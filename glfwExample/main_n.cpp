#pragma comment(lib, "windowsapp.lib")
#pragma comment(lib, "d3d11.lib")

#include "screen_capture.h"

#include <Windows.h>
#include <d3d11_4.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <Windows.Graphics.Capture.Interop.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>

#ifndef WGC_NATIVE_NODE_ADDON
#include <fmt/color.h>
#endif

namespace winrt_capture = winrt::Windows::Graphics::Capture;
using namespace Windows::Graphics::DirectX::Direct3D11;
using namespace winrt::Windows::Graphics::DirectX;

struct MonitorCapture::Impl {
    winrt::com_ptr<ID3D11Device> d3dDevice;
    winrt::com_ptr<ID3D11DeviceContext> d3dContext;
    winrt::com_ptr<ID3D11Texture2D> stagingTexture;

    std::mutex bufferMutex;
    std::vector<uint8_t> pixelBuffer;
    std::atomic<bool> frameDirty{ false };

    int width{};
    int height{};
    bool prepared{};
    bool sessionActive{};

    winrt::Windows::Graphics::Capture::GraphicsCaptureItem item{ nullptr };
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool framePool{ nullptr };
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession session{ nullptr };

    void on_frame_arrived(winrt_capture::Direct3D11CaptureFramePool const& pool);
};

void MonitorCapture::Impl::on_frame_arrived(winrt_capture::Direct3D11CaptureFramePool const& pool)
{
    try {
        auto frame = pool.TryGetNextFrame();
        if (!frame)
            return;

        auto surface = frame.Surface();
        auto access = surface.as<IDirect3DDxgiInterfaceAccess>();

        winrt::com_ptr<ID3D11Texture2D> sourceTexture;
        winrt::check_hresult(access->GetInterface(__uuidof(ID3D11Texture2D), sourceTexture.put_void()));

        if (!sourceTexture)
            return;

        d3dContext->CopyResource(stagingTexture.get(), sourceTexture.get());
        d3dContext->Flush();

        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (!SUCCEEDED(d3dContext->Map(stagingTexture.get(), 0, D3D11_MAP_READ, 0, &mapped)))
            return;

        {
            std::lock_guard<std::mutex> lock(bufferMutex);
            auto* src = static_cast<uint8_t*>(mapped.pData);
            uint8_t* dst = pixelBuffer.data();

            auto size = frame.ContentSize();
            const int w = size.Width;
            const int h = size.Height;
            const size_t widthInBytes = static_cast<size_t>(w) * 4u;

            // Staging 纹理为 BGRA；写入 pixelBuffer 时转为 RGBA（供 Canvas ImageData / 前端直接使用）
            for (int y = 0; y < h; ++y) {
                const uint8_t* rowSrc = src + static_cast<size_t>(y) * mapped.RowPitch;
                uint8_t* rowDst = dst + static_cast<size_t>(y) * widthInBytes;
                for (int x = 0; x < w; ++x) {
                    const size_t i = static_cast<size_t>(x) * 4u;
                    rowDst[i + 0] = rowSrc[i + 2]; // R
                    rowDst[i + 1] = rowSrc[i + 1]; // G
                    rowDst[i + 2] = rowSrc[i + 0]; // B
                    rowDst[i + 3] = rowSrc[i + 3]; // A
                }
            }
        }
        d3dContext->Unmap(stagingTexture.get(), 0);
        frameDirty.store(true);
    }
    catch (winrt::hresult_error const& ex) {
#ifdef WGC_NATIVE_NODE_ADDON
        (void)ex;
#else
        fmt::print(fg(fmt::color::red), "Interop Error: {}\n", winrt::to_string(ex.message()));
#endif
    }
}

MonitorCapture::MonitorCapture() : impl_(std::make_unique<Impl>()) {}

MonitorCapture::~MonitorCapture()
{
    stop();
}

bool MonitorCapture::prepare_default_monitor()
{
    if (impl_->prepared)
        return true;

    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION,
        impl_->d3dDevice.put(), nullptr, impl_->d3dContext.put());
    if (FAILED(hr))
        return false;

    auto multithread = impl_->d3dDevice.as<::ID3D11Multithread>();
    multithread->SetMultithreadProtected(TRUE);

    winrt::com_ptr<IDXGIDevice> dxgiDevice = impl_->d3dDevice.as<IDXGIDevice>();
    winrt::com_ptr<::IInspectable> inspectable;
    hr = CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.get(), inspectable.put());
    if (FAILED(hr))
        return false;

    auto winrtDevice = inspectable.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();

    HMONITOR hMonitor = MonitorFromWindow(GetDesktopWindow(), MONITOR_DEFAULTTONEAREST);
    auto interop_factory = winrt::get_activation_factory<winrt::Windows::Graphics::Capture::GraphicsCaptureItem, IGraphicsCaptureItemInterop>();
    winrt::check_hresult(interop_factory->CreateForMonitor(hMonitor,
        winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(),
        winrt::put_abi(impl_->item)));

    auto itemSize = impl_->item.Size();
    impl_->width = itemSize.Width;
    impl_->height = itemSize.Height;
    impl_->pixelBuffer.resize(static_cast<size_t>(impl_->width) * static_cast<size_t>(impl_->height) * 4u);

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = static_cast<UINT>(impl_->width);
    desc.Height = static_cast<UINT>(impl_->height);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.BindFlags = 0;
    hr = impl_->d3dDevice->CreateTexture2D(&desc, nullptr, impl_->stagingTexture.put());
    if (FAILED(hr))
        return false;

    impl_->framePool = winrt_capture::Direct3D11CaptureFramePool::CreateFreeThreaded(
        winrtDevice,
        DirectXPixelFormat::B8G8R8A8UIntNormalized,
        2,
        itemSize);

    Impl* raw = impl_.get();
    impl_->framePool.FrameArrived([raw](auto&& pool, auto&&) {
        raw->on_frame_arrived(pool);
    });

    impl_->prepared = true;
    return true;
}

void MonitorCapture::begin_capture()
{
    if (!impl_->prepared || impl_->sessionActive)
        return;

    impl_->session = impl_->framePool.CreateCaptureSession(impl_->item);
    impl_->session.StartCapture();
    impl_->sessionActive = true;
}

void MonitorCapture::stop()
{
    if (!impl_)
        return;

    if (impl_->sessionActive) {
        impl_->session.Close();
        impl_->session = nullptr;
        impl_->sessionActive = false;
    }

    impl_->framePool = nullptr;
    impl_->stagingTexture = nullptr;
    impl_->d3dContext = nullptr;
    impl_->d3dDevice = nullptr;
    impl_->item = nullptr;
    impl_->pixelBuffer.clear();
    impl_->width = impl_->height = 0;
    impl_->prepared = false;
}

bool MonitorCapture::is_prepared() const
{
    return impl_ && impl_->prepared;
}

int MonitorCapture::width() const
{
    return impl_ ? impl_->width : 0;
}

int MonitorCapture::height() const
{
    return impl_ ? impl_->height : 0;
}

std::mutex& MonitorCapture::buffer_mutex()
{
    return impl_->bufferMutex;
}

std::vector<uint8_t>& MonitorCapture::pixel_buffer()
{
    return impl_->pixelBuffer;
}

std::atomic<bool>& MonitorCapture::frame_dirty()
{
    return impl_->frameDirty;
}
