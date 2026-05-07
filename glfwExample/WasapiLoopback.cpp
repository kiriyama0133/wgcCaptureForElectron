#include "media_recorder.h"
#pragma comment(lib, "avrt.lib")
#pragma comment(lib, "Mmdevapi.lib")

#include <mferror.h>

#include <mmreg.h>

#include <cstring>
#include <cstdio>

namespace {

void MFLogFail(const char* stage, HRESULT hr)
{
    char buf[384];
    std::snprintf(
        buf,
        sizeof(buf),
        "[MediaRecorder] %s -> HRESULT 0x%08lX\n",
        stage,
        static_cast<unsigned long>(hr));
    OutputDebugStringA(buf);
    std::fprintf(stderr, "%s", buf);
}

} // namespace

WasapiLoopback::WasapiLoopback() : m_pwfx(nullptr) {}

WasapiLoopback::~WasapiLoopback()
{
    Stop();
    m_captureClient.Reset();
    m_audioClient.Reset();
    if (m_pwfx)
        CoTaskMemFree(m_pwfx);
    m_pwfx = nullptr;
}

HRESULT WasapiLoopback::Initialize(LPCWSTR deviceId)
{
    Stop();
    if (m_pwfx) {
        CoTaskMemFree(m_pwfx);
        m_pwfx = nullptr;
    }
    m_captureClient.Reset();
    m_audioClient.Reset();

    ComPtr<IMMDeviceEnumerator> pEnumerator;
    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        (void**)pEnumerator.GetAddressOf());
    if (FAILED(hr))
        return hr;

    ComPtr<IMMDevice> pDevice;
    if (deviceId != nullptr && deviceId[0] != L'\0')
        hr = pEnumerator->GetDevice(deviceId, pDevice.GetAddressOf());
    else
        hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, pDevice.GetAddressOf());
    if (FAILED(hr))
        return hr;

    hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)m_audioClient.GetAddressOf());
    if (FAILED(hr))
        return hr;

    hr = m_audioClient->GetMixFormat(&m_pwfx);
    if (FAILED(hr))
        return hr;

    // Loopback：捕获「该播放设备」上听到的混音（与桌面音频一致须选扬声器/默认设备）
    constexpr REFERENCE_TIME kBuffer100ns = 10 * 10000 * 1000; // 10ms @ 100ns units... wait 1ms = 10000 * 100ns
    // 500ms 环形缓冲，降低欠载
    constexpr REFERENCE_TIME kBufferDuration = 500 * 10000; // 500 * 1e-5 sec = 500ms in 100ns units
    hr = m_audioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK,
        kBufferDuration,
        0,
        m_pwfx,
        nullptr);
    if (FAILED(hr))
        return hr;

    hr = m_audioClient->GetService(__uuidof(IAudioCaptureClient), (void**)m_captureClient.GetAddressOf());
    if (FAILED(hr))
        return hr;

    m_frameSize = m_pwfx->nBlockAlign;
    return S_OK;
}

void WasapiLoopback::Start(AudioDataCallback callback) {
    if (m_isRunning) return;
    m_onData = callback;
    m_isRunning = true;
    m_captureThread = std::thread(&WasapiLoopback::CaptureThread, this);
}

void WasapiLoopback::Stop()
{
    if (!m_isRunning.exchange(false, std::memory_order_acq_rel))
        return;
    if (m_audioClient)
        m_audioClient->Stop();
    if (m_captureThread.joinable())
        m_captureThread.join();
    m_captureClient.Reset();
    m_onData = nullptr;
}

void WasapiLoopback::CaptureThread() {
    CoInitializeEx(NULL, COINIT_MULTITHREADED); // 初始化com

    // 提升线程优先级
    DWORD taskIndex = 0;
    HANDLE hTask = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);

    m_audioClient->Start();
    while (m_isRunning) {
        UINT32 packetLength = 0;
        HRESULT hr = m_captureClient->GetNextPacketSize(&packetLength);
        while (packetLength > 0) {
            BYTE* pData;
            UINT32 numFramesAvailable;
            DWORD flags;
            uint64_t devicePosition, qpcPosition;

            // 从驱动缓冲区抓取PCM
            hr = m_captureClient->GetBuffer(&pData, &numFramesAvailable, &flags, &devicePosition, &qpcPosition);
            if (SUCCEEDED(hr)) {
                if (m_onData) {
                    size_t sizeInBytes = numFramesAvailable * m_frameSize;
                    if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) && pData && sizeInBytes > 0)
                        std::memset(pData, 0, sizeInBytes);
                    m_onData(pData, &sizeInBytes, &qpcPosition);
                }
                m_captureClient->ReleaseBuffer(numFramesAvailable);
            }
            m_captureClient->GetNextPacketSize(&packetLength);
        }

        Sleep(1); // 休眠，通常位音频缓冲区长度一般 比如5ms
    }

    if (hTask) AvRevertMmThreadCharacteristics(hTask);
    CoUninitialize();
}

MediaRecorder::MediaRecorder(ComPtr<ID3D11Device> device, const RecorderConfig& config)
    : m_pDevice(device), m_config(config), m_isRecording(false), m_startTimeQPC(0) {
	MFStartup(MF_VERSION);
    ID3D11DeviceContext* ctx = nullptr;
    m_pDevice->GetImmediateContext(&ctx);
    if (ctx)
        m_pImmediateContext.Attach(ctx);
    UINT resetToken; // 创建dxgi设备管理
    HRESULT hr = MFCreateDXGIDeviceManager(&resetToken, &m_pDeviceManager);
    if (SUCCEEDED(hr)) {
        hr = m_pDeviceManager->ResetDevice(m_pDevice.Get(), resetToken);
	}
}

MediaRecorder::~MediaRecorder() {
    // 确保退出时资源被释放，防止内存泄漏或文件损坏
    StopRecording();
    MFShutdown();
}

HRESULT MediaRecorder::PushVideoFrame_RgbMemory(ComPtr<ID3D11Texture2D> texture, LONGLONG pts, LONGLONG frameDur100ns)
{
    if (!m_stagingReadback || !m_pImmediateContext || !m_sinkWriter)
        return E_FAIL;

    m_pImmediateContext->CopyResource(m_stagingReadback.Get(), texture.Get());
    D3D11_MAPPED_SUBRESOURCE map{};
    HRESULT hr = m_pImmediateContext->Map(m_stagingReadback.Get(), 0, D3D11_MAP_READ, 0, &map);
    if (FAILED(hr)) {
        MFLogFail("Map staging readback", hr);
        return hr;
    }

    const UINT packedPitch = m_config.width * 4u;
    const DWORD packedSize = packedPitch * m_config.height;
    ComPtr<IMFMediaBuffer> buf;
    hr = MFCreateMemoryBuffer(packedSize, &buf);
    if (FAILED(hr)) {
        m_pImmediateContext->Unmap(m_stagingReadback.Get(), 0);
        return hr;
    }

    BYTE* dst = nullptr;
    hr = buf->Lock(&dst, nullptr, nullptr);
    if (FAILED(hr)) {
        m_pImmediateContext->Unmap(m_stagingReadback.Get(), 0);
        return hr;
    }
    const auto* src = static_cast<const BYTE*>(map.pData);
    for (UINT y = 0; y < m_config.height; ++y)
        std::memcpy(dst + static_cast<size_t>(y) * packedPitch, src + static_cast<size_t>(y) * map.RowPitch, packedPitch);
    buf->Unlock();
    buf->SetCurrentLength(packedSize);
    m_pImmediateContext->Unmap(m_stagingReadback.Get(), 0);

    ComPtr<IMFSample> sample;
    hr = MFCreateSample(&sample);
    if (FAILED(hr))
        return hr;
    hr = sample->AddBuffer(buf.Get());
    if (FAILED(hr))
        return hr;
    sample->SetSampleTime(pts);
    sample->SetSampleDuration(frameDur100ns);
    hr = m_sinkWriter->WriteSample(m_videoStreamIndex, sample.Get());
    if (FAILED(hr))
        MFLogFail("WriteSample (video RGB memory)", hr);
    return hr;
}

HRESULT MediaRecorder::PushVideoFrame(ComPtr<ID3D11Texture2D> texture, uint64_t qpcTime) {
    if (!m_isRecording || !m_sinkWriter) return E_FAIL;

    D3D11_TEXTURE2D_DESC td{};
    texture->GetDesc(&td);
    if (td.Width != m_config.width || td.Height != m_config.height) {
        char stage[192]{};
        std::snprintf(
            stage,
            sizeof(stage),
            "PushVideoFrame: texture %ux%u != MF_MT_FRAME_SIZE %ux%u (start_recording 须与捕获分辨率一致)",
            td.Width,
            td.Height,
            m_config.width,
            m_config.height);
        MFLogFail(stage, MF_E_INVALIDMEDIATYPE);
        return MF_E_INVALIDMEDIATYPE;
    }

    if (m_startTimeQPC == 0) {
        m_startTimeQPC = qpcTime; // 初始化基准
    }
    // Media Foundation 的时间单位是 100 纳秒 (1s = 10^7 units)
    // 公式: PTS = (当前QPC - 起始QPC) * 10^7 / QPC频率
    const LONGLONG pts = (qpcTime - m_startTimeQPC) * 10000000 / static_cast<LONGLONG>(m_qpcFrequency);
    const UINT32 fps = (m_config.fps < 1u) ? 30u : m_config.fps;
    const LONGLONG frameDur100ns =
        (10000000LL + static_cast<LONGLONG>(fps) / 2LL) / static_cast<LONGLONG>(fps);

    if (m_useRgbMemoryPath)
        return PushVideoFrame_RgbMemory(texture, pts, frameDur100ns);

    ComPtr<IMFMediaBuffer> pBuffer;
    HRESULT hr = MFCreateDXGISurfaceBuffer(__uuidof(ID3D11Texture2D), texture.Get(), 0, TRUE, &pBuffer);
    if (FAILED(hr)) {
        MFLogFail("MFCreateDXGISurfaceBuffer", hr);
        if (m_stagingReadback) {
            MFLogFail("Trying RGB memory path after MFCreateDXGISurfaceBuffer failure", hr);
            m_useRgbMemoryPath = true;
            return PushVideoFrame_RgbMemory(texture, pts, frameDur100ns);
        }
        return hr;
    }
    ComPtr<IMFSample> pSample;
    hr = MFCreateSample(&pSample);
    if (FAILED(hr)) return hr;
    hr = pSample->AddBuffer(pBuffer.Get());
    if (FAILED(hr)) return hr;

    pSample->SetSampleTime(pts);
    pSample->SetSampleDuration(frameDur100ns);

    hr = m_sinkWriter->WriteSample(m_videoStreamIndex, pSample.Get());
    if (FAILED(hr)) {
        constexpr HRESULT kInvalidArg = static_cast<HRESULT>(0x80070057);
        if (hr == kInvalidArg && m_stagingReadback) {
            MFLogFail("WriteSample (video DXGI) E_INVALIDARG -> switching to RGB memory copy", hr);
            m_useRgbMemoryPath = true;
            return PushVideoFrame_RgbMemory(texture, pts, frameDur100ns);
        }
        MFLogFail("WriteSample (video DXGI)", hr);
    }
    return hr;
}

HRESULT MediaRecorder::PushAudioData(const uint8_t* data, size_t size, uint64_t qpcTime) {
    if (!m_hasAudioStream)
        return S_OK;
    if (!m_isRecording || !m_sinkWriter) return E_FAIL;
    if (m_startTimeQPC == 0) {
        m_startTimeQPC = qpcTime;
    }
    LONGLONG pts = (qpcTime - m_startTimeQPC) * 10000000 / m_qpcFrequency;
    // 内存缓冲区
    ComPtr<IMFMediaBuffer> pBuffer;
    HRESULT hr = MFCreateMemoryBuffer(static_cast<DWORD>(size), &pBuffer);
    if (FAILED(hr)) return hr;
    // pcm-copy
    BYTE* pDest = nullptr;
    hr = pBuffer->Lock(&pDest, nullptr, nullptr); // 锁住来写入
    if (SUCCEEDED(hr)) {
        memcpy(pDest, data, size);
        pBuffer->Unlock();
        pBuffer->SetCurrentLength(static_cast<DWORD>(size)); // 更新缓冲里的长度
    }
    else {
        return hr;
    }

    // get sample，打包buffer
    ComPtr<IMFSample> pSample;
    hr = MFCreateSample(&pSample);
    if (FAILED(hr)) return hr;
    hr = pSample->AddBuffer(pBuffer.Get());
    if (FAILED(hr)) return hr;
    hr = pSample->SetSampleTime(pts);
    hr = m_sinkWriter->WriteSample(m_audioStreamIndex, pSample.Get());
    return hr;
}

HRESULT MediaRecorder::SetupVideoStreaming(const RecorderConfig& config) {
    ComPtr<IMFMediaType> pTypeOut; // 输出格式h264
	ComPtr<IMFMediaType> pTypeIn;  // 输入格式bgra

	HRESULT hr = MFCreateMediaType(&pTypeOut);
	hr = pTypeOut->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	hr = pTypeOut->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
	hr = pTypeOut->SetUINT32(MF_MT_AVG_BITRATE, config.videoBitrate);
    hr = pTypeOut->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);

    // 设置分辨率、帧率
	hr = MFSetAttributeSize(pTypeOut.Get(), MF_MT_FRAME_SIZE, config.width, config.height);
	hr = MFSetAttributeRatio(pTypeOut.Get(), MF_MT_FRAME_RATE, config.fps, 1);

    // 输出流添加到sinkwriter
	hr = m_sinkWriter->AddStream(pTypeOut.Get(), &m_videoStreamIndex);
    if (FAILED(hr)) {
        MFLogFail("SetupVideoStreaming AddStream (H.264 out)", hr);
        return hr;
    }
	hr = MFCreateMediaType(&pTypeIn);
    // 配置输入格式
	hr = pTypeIn->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    hr = pTypeIn->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32); // 即 BGRA，对应 WGC 的输出
    hr = MFSetAttributeSize(pTypeIn.Get(), MF_MT_FRAME_SIZE, config.width, config.height);
    hr = MFSetAttributeRatio(pTypeIn.Get(), MF_MT_FRAME_RATE, config.fps, 1);
    hr = pTypeIn->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    // 未压缩 RGB32：缺少 stride / PAR 时 Sink Writer 常在 WriteSample 返回 E_INVALIDARG(0x80070057)
    hr = MFSetAttributeRatio(pTypeIn.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    if (FAILED(hr))
        return hr;
    hr = pTypeIn->SetUINT32(MF_MT_DEFAULT_STRIDE, config.width * 4u);
    if (FAILED(hr))
        return hr;
    hr = pTypeIn->SetUINT32(MF_MT_VIDEO_NOMINAL_RANGE, 1u); // MFNominalRange_Normal
    if (FAILED(hr))
        return hr;
    // 绑定输入格式
	hr = m_sinkWriter->SetInputMediaType(m_videoStreamIndex, pTypeIn.Get(), nullptr);
    if (FAILED(hr))
        MFLogFail("SetupVideoStreaming SetInputMediaType (BGRA in)", hr);
    return hr;
}

HRESULT MediaRecorder::SetupAudioStreaming(const RecorderConfig& config) {
    ComPtr<IMFMediaType> pTypeOut; // 输出aac
    ComPtr<IMFMediaType> pTypeIn;  // 输入pcm
    HRESULT hr = MFCreateMediaType(&pTypeOut);
    if (FAILED(hr))
        return hr;
    hr = pTypeOut->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    if (FAILED(hr))
        return hr;
    hr = pTypeOut->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC);
    if (FAILED(hr))
        return hr;
    hr = pTypeOut->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    if (FAILED(hr))
        return hr;
    hr = pTypeOut->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, config.audioSampleRate);
    if (FAILED(hr))
        return hr;
    hr = pTypeOut->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, config.audioChannels);
    if (FAILED(hr))
        return hr;
    // 压缩 AAC 轨：此处应为「压缩后」平均字节/秒，勿填 PCM 口径（易触发编码器/PCM 协商异常）
    constexpr UINT32 kAacBitrateBitsPerSec = 128000;
    hr = pTypeOut->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, kAacBitrateBitsPerSec / 8);
    if (FAILED(hr))
        return hr;

    hr = m_sinkWriter->AddStream(pTypeOut.Get(), &m_audioStreamIndex);
    if (FAILED(hr)) {
        MFLogFail("SetupAudioStreaming AddStream (AAC out)", hr);
        return hr;
    }

    UINT32 nCh = config.audioChannels;
    if (nCh < 1u)
        nCh = 1u;
    if (nCh > 8u)
        nCh = 8u;

    WAVEFORMATEX wfx{};
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = static_cast<WORD>(nCh);
    wfx.nSamplesPerSec = config.audioSampleRate;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = static_cast<WORD>(wfx.nChannels * wfx.wBitsPerSample / 8);
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    wfx.cbSize = 0;

    hr = MFCreateMediaType(&pTypeIn);
    if (FAILED(hr))
        return hr;
    hr = MFInitMediaTypeFromWaveFormatEx(pTypeIn.Get(), &wfx, sizeof(wfx));
    if (FAILED(hr)) {
        MFLogFail("MFInitMediaTypeFromWaveFormatEx (PCM)", hr);
        return hr;
    }

    hr = m_sinkWriter->SetInputMediaType(m_audioStreamIndex, pTypeIn.Get(), nullptr);
    if (FAILED(hr))
        MFLogFail("SetupAudioStreaming SetInputMediaType (PCM in)", hr);
    return hr;
}

HRESULT MediaRecorder::StartRecording(const std::wstring& path, const RecorderConfig& config) {
    m_config = config;
    m_hasAudioStream = false;
    m_useRgbMemoryPath = false;
    m_stagingReadback.Reset();
    LARGE_INTEGER fq{};
    QueryPerformanceFrequency(&fq);
    m_qpcFrequency = fq.QuadPart;
    m_startTimeQPC = 0;

    ComPtr<IMFAttributes> pAttributes;
    HRESULT hr = MFCreateAttributes(&pAttributes, 3);
    if (FAILED(hr)) {
        MFLogFail("MFCreateAttributes", hr);
        return hr;
    }
    // 硬件编码器/DXGI RGB 路径在部分机型上 WriteSample 报 E_INVALIDARG；先用软件管线保证可写
    hr = pAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, FALSE);
    if (FAILED(hr)) {
        MFLogFail("MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS", hr);
        return hr;
    }
    hr = pAttributes->SetUnknown(MF_SINK_WRITER_D3D_MANAGER, m_pDeviceManager.Get());
    if (FAILED(hr)) {
        MFLogFail("MF_SINK_WRITER_D3D_MANAGER (check DXGI device manager / D3D device)", hr);
        return hr;
    }
    hr = pAttributes->SetUINT32(MF_SINK_WRITER_DISABLE_THROTTLING, true);
    if (FAILED(hr)) {
        MFLogFail("MF_SINK_WRITER_DISABLE_THROTTLING", hr);
        return hr;
    }

    hr = MFCreateSinkWriterFromURL(path.c_str(), nullptr, pAttributes.Get(), &m_sinkWriter);
    if (FAILED(hr)) {
        MFLogFail("MFCreateSinkWriterFromURL (path / codec registration / permissions)", hr);
        return hr;
    }

    hr = SetupVideoStreaming(config);
    if (FAILED(hr))
        return hr;
    if (m_config.enableAudio) {
        hr = SetupAudioStreaming(config);
        if (FAILED(hr))
            return hr;
        m_hasAudioStream = true;
    }

    hr = m_sinkWriter->BeginWriting();
    if (SUCCEEDED(hr)) {
        if (m_pDevice && m_pImmediateContext && m_config.width > 0 && m_config.height > 0) {
            D3D11_TEXTURE2D_DESC sd{};
            sd.Width = m_config.width;
            sd.Height = m_config.height;
            sd.MipLevels = 1;
            sd.ArraySize = 1;
            sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            sd.SampleDesc.Count = 1;
            sd.Usage = D3D11_USAGE_STAGING;
            sd.BindFlags = 0;
            sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            sd.MiscFlags = 0;
            const HRESULT hTex = m_pDevice->CreateTexture2D(&sd, nullptr, m_stagingReadback.GetAddressOf());
            if (FAILED(hTex)) {
                MFLogFail("CreateTexture2D staging (RGB fallback)", hTex);
            }
        }
        m_isRecording.store(true, std::memory_order_release);
        return S_OK;
    }
    MFLogFail("BeginWriting", hr);
    return hr;
}

void MediaRecorder::StopRecording() {
	if (!m_isRecording.exchange(false, std::memory_order_acq_rel)) return; // 已经停止了
    if (m_sinkWriter) {
        HRESULT hr = m_sinkWriter->Finalize(); // 确保所有数据都写入文件
        if (FAILED(hr)) {
            OutputDebugStringA("Failed to finalize recording: ");
        }
    }
	m_sinkWriter.Reset();
    m_stagingReadback.Reset();
    m_useRgbMemoryPath = false;
	m_startTimeQPC = 0;
	OutputDebugStringA("Recording stopped and finalized.\n");
}

uint64_t MediaRecorder::GetCurrentQPC() {
    LARGE_INTEGER qpc;
    QueryPerformanceCounter(&qpc);
    return qpc.QuadPart;
}

uint64_t MediaRecorder::GetQPCFrequency() {
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    return freq.QuadPart;
}