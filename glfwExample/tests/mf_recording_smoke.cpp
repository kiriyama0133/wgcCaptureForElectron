// CTest：验证 BeginWriting + 多帧 PushVideoFrame 后 MP4 非 0 字节（不依赖 Electron/WGC）。
#include <Windows.h>

#include <cstdio>
#include <string>

#include <d3d11.h>
#include <wrl/client.h>

#include "media_recorder.h"

using Microsoft::WRL::ComPtr;

static HRESULT CreateDevice(ComPtr<ID3D11Device>& dev, ComPtr<ID3D11DeviceContext>& ctx)
{
    D3D_FEATURE_LEVEL fl{};
    return D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &dev,
        &fl,
        &ctx);
}

static HRESULT CreateBGRATexture(
    ID3D11Device* dev,
    UINT width,
    UINT height,
    ComPtr<ID3D11Texture2D>& out)
{
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    return dev->CreateTexture2D(&desc, nullptr, out.GetAddressOf());
}

static unsigned long long FileSizeBytes(const std::wstring& path)
{
    WIN32_FILE_ATTRIBUTE_DATA fad{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad))
        return 0;
    ULARGE_INTEGER u{};
    u.LowPart = fad.nFileSizeLow;
    u.HighPart = fad.nFileSizeHigh;
    return u.QuadPart;
}

int main()
{
    const HRESULT coinit = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(coinit)) {
        std::fprintf(stderr, "[mf_smoke] CoInitializeEx -> 0x%08lX\n", static_cast<unsigned long>(coinit));
        return 1;
    }

    int exitCode = 0;
    HRESULT hr = S_OK;
    {
        ComPtr<ID3D11Device> dev;
        ComPtr<ID3D11DeviceContext> ctx;
        hr = CreateDevice(dev, ctx);
        if (FAILED(hr)) {
            std::fprintf(stderr, "[mf_smoke] D3D11CreateDevice -> 0x%08lX\n", static_cast<unsigned long>(hr));
            exitCode = 2;
        }
        else {
            wchar_t tmpDir[MAX_PATH]{};
            const DWORD n = GetTempPathW(MAX_PATH, tmpDir);
            if (n == 0 || n >= MAX_PATH) {
                std::fprintf(stderr, "[mf_smoke] GetTempPathW failed (%lu)\n", static_cast<unsigned long>(GetLastError()));
                exitCode = 3;
            }
            else {
                const std::wstring path = std::wstring(tmpDir) + L"mf_recording_smoke.mp4";
                DeleteFileW(path.c_str());

                RecorderConfig cfg{};
                cfg.width = 640;
                cfg.height = 360;
                cfg.fps = 30;
                cfg.enableAudio = false;

                std::fprintf(stderr, "[mf_smoke] output path: ");
                std::fwprintf(stderr, L"%ls\n", path.c_str());

                ComPtr<ID3D11Texture2D> tex;
                hr = CreateBGRATexture(dev.Get(), cfg.width, cfg.height, tex);
                if (FAILED(hr)) {
                    std::fprintf(stderr, "[mf_smoke] CreateTexture2D -> 0x%08lX\n", static_cast<unsigned long>(hr));
                    exitCode = 5;
                }
                else {
                    MediaRecorder recorder(dev, cfg);
                    hr = recorder.StartRecording(path, cfg);
                    if (FAILED(hr)) {
                        std::fprintf(
                            stderr,
                            "[mf_smoke] StartRecording -> 0x%08lX (see [MediaRecorder] lines above)\n",
                            static_cast<unsigned long>(hr));
                        exitCode = 4;
                    }
                    else {
                        LARGE_INTEGER freq{};
                        LARGE_INTEGER t0{};
                        QueryPerformanceFrequency(&freq);
                        QueryPerformanceCounter(&t0);

                        constexpr int kFrames = 60;
                        const UINT32 fps = cfg.fps < 1u ? 30u : cfg.fps;

                        for (int i = 0; i < kFrames; ++i) {
                            LARGE_INTEGER t{};
                            t.QuadPart = t0.QuadPart + static_cast<LONGLONG>(i) * freq.QuadPart / static_cast<LONGLONG>(fps);
                            hr = recorder.PushVideoFrame(tex, static_cast<uint64_t>(t.QuadPart));
                            if (FAILED(hr)) {
                                std::fprintf(
                                    stderr,
                                    "[mf_smoke] PushVideoFrame frame %d -> 0x%08lX\n",
                                    i,
                                    static_cast<unsigned long>(hr));
                                exitCode = 6;
                                break;
                            }
                        }

                        recorder.StopRecording();

                        const unsigned long long sz = FileSizeBytes(path);
                        std::fprintf(stderr, "[mf_smoke] file size after %d video frames: %llu bytes\n", kFrames, sz);
                        if (exitCode == 0 && sz == 0ULL) {
                            std::fprintf(
                                stderr,
                                "[mf_smoke] FAIL: MP4 still 0 bytes (WriteSample did not persist / Finalize issue)\n");
                            exitCode = 7;
                        }
                        else if (exitCode == 0) {
                            std::fprintf(stderr, "[mf_smoke] OK\n");
                        }
                    }
                }
            }
        }
    }

    CoUninitialize();
    return exitCode;
}
