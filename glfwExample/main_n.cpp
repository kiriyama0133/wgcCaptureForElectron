#pragma comment(lib, "windowsapp.lib")
#pragma comment(lib, "d3d11.lib")

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "screen_capture.h"
#include "media_recorder.h"
#include "utils/pixel_format.hpp"

#include <Windows.h>
#include <atomic>
#include <cstdio>
#include <algorithm>
#include <d3d11_4.h>
#include <mmreg.h>
#include <KsMedia.h>
#include <wrl/client.h>
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

namespace {

bool IsFloatWaveFormat(const WAVEFORMATEX* pwfx)
{
    if (!pwfx)
        return false;
    if (pwfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
        return true;
    if (pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        const auto* ex = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(pwfx);
        return IsEqualGUID(ex->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) != FALSE;
    }
    return false;
}

} // namespace

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

    std::atomic<bool> preview_enabled{ true };
    std::atomic<bool> recording_enabled{ false };

    std::unique_ptr<MediaRecorder> m_recorder;
    std::unique_ptr<WasapiLoopback> m_wasapi;
    std::vector<uint8_t> m_audioScratch;
    bool m_audioFromFloat = false;
    UINT32 m_audioChannelsFromDevice = 2;

    winrt::Windows::Graphics::Capture::GraphicsCaptureItem item{ nullptr };
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool framePool{ nullptr };
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession session{ nullptr };

    void feed_recording_frame(winrt::com_ptr<ID3D11Texture2D> const& sourceTexture);
    void feed_wasapi_audio(const uint8_t* data, size_t* sizeBytes, uint64_t* qpcHint);
    void on_frame_arrived(winrt_capture::Direct3D11CaptureFramePool const& pool);
};

void MonitorCapture::Impl::feed_wasapi_audio(const uint8_t* data, size_t* sizeBytes, uint64_t* qpcHint)
{
    if (!recording_enabled.load(std::memory_order_relaxed))
        return;
    if (!m_recorder || !m_recorder->m_isRecording.load(std::memory_order_acquire))
        return;
    if (!data || !sizeBytes || *sizeBytes == 0)
        return;

    uint64_t qpc = m_recorder->GetCurrentQPC();
    if (qpcHint && *qpcHint != 0)
        qpc = *qpcHint;

    if (m_audioFromFloat) {
        const UINT32 ch = std::max(1u, m_audioChannelsFromDevice);
        const size_t totalFloats = *sizeBytes / sizeof(float);
        const size_t frames = totalFloats / static_cast<size_t>(ch);
        const size_t outBytes = frames * static_cast<size_t>(ch) * sizeof(int16_t);
        m_audioScratch.resize(outBytes);
        const auto* fs = reinterpret_cast<const float*>(data);
        auto* dst = reinterpret_cast<int16_t*>(m_audioScratch.data());
        for (size_t i = 0; i < frames * static_cast<size_t>(ch); ++i) {
            float s = fs[i];
            if (s > 1.f)
                s = 1.f;
            else if (s < -1.f)
                s = -1.f;
            dst[i] = static_cast<int16_t>(s * 32767.f);
        }
        (void)m_recorder->PushAudioData(m_audioScratch.data(), outBytes, qpc);
    }
    else {
        (void)m_recorder->PushAudioData(data, *sizeBytes, qpc);
    }
}

void MonitorCapture::Impl::feed_recording_frame(winrt::com_ptr<ID3D11Texture2D> const& sourceTexture)
{
    if (!recording_enabled.load(std::memory_order_relaxed))
        return;
    if (!m_recorder || !m_recorder->m_isRecording.load(std::memory_order_acquire))
        return;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> tex{ sourceTexture.get() };
    const HRESULT hr = m_recorder->PushVideoFrame(tex, m_recorder->GetCurrentQPC());
    if (FAILED(hr)) {
        static std::atomic<uint32_t> push_fail_count{ 0 };
        const uint32_t n = push_fail_count.fetch_add(1, std::memory_order_relaxed);
        if (n < 8u) {
            char buf[160]{};
            std::snprintf(
                buf,
                sizeof(buf),
                "[MonitorCapture] PushVideoFrame failed 0x%08lX (常见原因：录制宽高与纹理不一致)\n",
                static_cast<unsigned long>(hr));
            OutputDebugStringA(buf);
        }
    }
}

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

        const auto content_extent = frame.ContentSize();
        const int w = content_extent.Width;
        const int h = content_extent.Height;

        feed_recording_frame(sourceTexture);

        if (!preview_enabled.load(std::memory_order_relaxed))
            return;

        d3dContext->CopyResource(stagingTexture.get(), sourceTexture.get());
        d3dContext->Flush();

        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (!SUCCEEDED(d3dContext->Map(stagingTexture.get(), 0, D3D11_MAP_READ, 0, &mapped)))
            return;

        {
            std::lock_guard<std::mutex> lock(bufferMutex);
            wgc::pixel::copy_mapped_bgra_to_rgba(
                pixelBuffer.data(),
                static_cast<const uint8_t*>(mapped.pData),
                w,
                h,
                mapped.RowPitch);
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

    stop_recording();

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

void MonitorCapture::set_preview_enabled(bool enabled)
{
    if (impl_)
        impl_->preview_enabled.store(enabled, std::memory_order_relaxed);
}

bool MonitorCapture::preview_enabled() const
{
    return impl_ && impl_->preview_enabled.load(std::memory_order_relaxed);
}

void MonitorCapture::set_recording_enabled(bool enabled)
{
    if (impl_)
        impl_->recording_enabled.store(enabled, std::memory_order_relaxed);
}

bool MonitorCapture::recording_enabled() const
{
    return impl_ && impl_->recording_enabled.load(std::memory_order_relaxed);
}

bool MonitorCapture::start_recording(const std::wstring& output_path, const RecorderConfig& config)
{
    if (!impl_ || !impl_->prepared || !impl_->d3dDevice.get())
        return false;

    stop_recording();

    Microsoft::WRL::ComPtr<ID3D11Device> dev{ impl_->d3dDevice.get() };

    // MF 输入类型里的分辨率必须与 WGC 纹理 GetDesc 一致，否则 MFCreateDXGISurfaceBuffer/WriteSample 失败 → MP4 一直 0 字节
    RecorderConfig cfg = config;
    cfg.width = static_cast<uint32_t>(impl_->width);
    cfg.height = static_cast<uint32_t>(impl_->height);

    std::unique_ptr<WasapiLoopback> wasapi;
    if (cfg.enableAudio) {
        wasapi = std::make_unique<WasapiLoopback>();
        const wchar_t* devId = cfg.audioOutputDeviceId.empty() ? nullptr : cfg.audioOutputDeviceId.c_str();
        const HRESULT hrLoop = wasapi->Initialize(devId);
        if (FAILED(hrLoop))
            return false;

        const WAVEFORMATEX* pwfx = wasapi->GetFormat();
        if (!pwfx)
            return false;
        cfg.audioSampleRate = pwfx->nSamplesPerSec;
        cfg.audioChannels = pwfx->nChannels;
        impl_->m_audioChannelsFromDevice = pwfx->nChannels;
        impl_->m_audioFromFloat = IsFloatWaveFormat(pwfx);
    }

    impl_->m_recorder = std::make_unique<MediaRecorder>(dev, cfg);
    HRESULT hr = impl_->m_recorder->StartRecording(output_path, cfg);
    if (FAILED(hr)) {
        impl_->m_recorder.reset();
        return false;
    }

    impl_->recording_enabled.store(true, std::memory_order_release);

    if (wasapi) {
        impl_->m_wasapi = std::move(wasapi);
        Impl* raw = impl_.get();
        impl_->m_wasapi->Start([raw](const uint8_t* data, size_t* sz, uint64_t* qpc) {
            raw->feed_wasapi_audio(data, sz, qpc);
        });
    }

    return true;
}

void MonitorCapture::stop_recording()
{
    if (!impl_)
        return;
    impl_->recording_enabled.store(false, std::memory_order_release);
    if (impl_->m_wasapi) {
        impl_->m_wasapi->Stop();
        impl_->m_wasapi.reset();
    }
    if (impl_->m_recorder) {
        impl_->m_recorder->StopRecording();
        impl_->m_recorder.reset();
    }
}

bool MonitorCapture::is_recording() const
{
    return impl_ && impl_->m_recorder && impl_->m_recorder->m_isRecording.load(std::memory_order_acquire);
}
