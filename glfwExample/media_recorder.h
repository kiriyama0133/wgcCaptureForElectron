#pragma once
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mfreadwrite.lib")

// win32 / com
#include <windows.h>
#include <unknwn.h>
#include <wrl/client.h>
#include <Audioclient.h>

// Media Foundation 的核心基础
#include <mfapi.h>
#include <mfidl.h>
#include <mmdeviceapi.h>  // IMMDeviceEnumerator 在这里定义
#include <endpointvolume.h> 
#include <Functiondiscoverykeys_devpkey.h> // 如果你需要获取设备友好名称

// 读写接口（IMFSinkWriter 在这里定义）
#include <mfreadwrite.h>
#include <mfobjects.h>
#include <avrt.h>

// DirectX 基础
#include <d3d11.h>

// 基础库
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <cstdint>
#include <atomic>

using Microsoft::WRL::ComPtr;
using AudioDataCallback = std::function<void(const uint8_t*, size_t*, uint64_t*)> ;

struct RecorderConfig {
	uint32_t width;
	uint32_t height;
	uint32_t fps;
	uint32_t videoBitrate = 10000000; // 默认 10Mbps
	uint32_t audioSampleRate = 48000;
	uint32_t audioChannels = 2;
	/** false：仅封装 H.264 视频轨（无 AAC），可绕过部分机型上 AAC/PCM 协商失败 */
	bool enableAudio = true;
	/** 空：默认播放设备；loopback 录制使用 IMMDevice::GetId() 字符串 */
	std::wstring audioOutputDeviceId;
};

class MediaRecorder {

public:

	MediaRecorder(ComPtr<ID3D11Device> device, const RecorderConfig& config);
	~MediaRecorder();

	uint64_t GetQPCFrequency();
	uint64_t GetCurrentQPC();

	// 初始化：创建 SinkWriter 并配置音视频流
	HRESULT StartRecording(const std::wstring& outputPath, const RecorderConfig& config);

	// 接收视频：传入 WGC 的纹理和对应的 QPC 时间戳
	HRESULT PushVideoFrame(ComPtr<ID3D11Texture2D> texture, uint64_t qpcTime);

	// 接收音频：传入原始 PCM 数据和对应的 QPC 时间戳
	HRESULT PushAudioData(const uint8_t* data, size_t size, uint64_t qpcTime);

	void StopRecording();

	std::atomic<bool> m_isRecording{ false };
	uint64_t m_startTimeQPC = 0; // 保存点击“开始录制”那一刻的高精度时间戳
	uint64_t m_qpcFrequency = 0; // QPC 频率，供 PTS 换算

private:
	HRESULT SetupVideoStreaming(const RecorderConfig& config); // 设置画质、分辨率等信息
	HRESULT SetupAudioStreaming(const RecorderConfig& config);
	HRESULT PushVideoFrame_RgbMemory(ComPtr<ID3D11Texture2D> texture, LONGLONG pts, LONGLONG frameDur100ns);

	ComPtr<ID3D11Device> m_pDevice; //实现零拷贝使用的设备
	ComPtr<ID3D11DeviceContext> m_pImmediateContext;
	ComPtr<ID3D11Texture2D> m_stagingReadback;
	bool m_useRgbMemoryPath = false;
	ComPtr<IMFDXGIDeviceManager> m_pDeviceManager; // Media Foundation 的 DXGI 设备管理器
	ComPtr<IMFSinkWriter> m_sinkWriter; // media foundation导出器，它吐出 MP4 文件。

	RecorderConfig m_config;
	DWORD m_videoStreamIndex = 0; // SinkWriter 内部流的索引
	DWORD m_audioStreamIndex = 0;
	bool m_hasAudioStream = false;
};



class WasapiLoopback {
	
public:
	WasapiLoopback();
	~WasapiLoopback();

	// 禁止copy
	WasapiLoopback(const WasapiLoopback&) = delete;
	WasapiLoopback& operator=(const WasapiLoopback&) = delete;

	/** deviceId 为空或 nullptr 时使用默认渲染端点（控制台）。 */
	HRESULT Initialize(LPCWSTR deviceId = nullptr);

	// start/stop
	void Start(AudioDataCallback callback);
	void Stop();

	WAVEFORMATEX* GetFormat() const { return m_pwfx; }

private:
	void CaptureThread();
	ComPtr<IAudioClient> m_audioClient;
	ComPtr<IAudioCaptureClient> m_captureClient;
	WAVEFORMATEX* m_pwfx = nullptr;
	AudioDataCallback m_onData;
	std::thread m_captureThread;
	std::atomic<bool> m_isRunning{ false };
	uint32_t m_frameSize = 0; // 每帧占用字节数
};