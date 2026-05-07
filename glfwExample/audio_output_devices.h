#pragma once

#include <string>
#include <vector>

struct AudioOutputDeviceInfo {
    std::wstring id;
    std::wstring friendlyName;
};

/** 枚举当前可用的音频播放（渲染）端点，用于 loopback 录制选择设备。须在 COM 已初始化的线程调用。 */
std::vector<AudioOutputDeviceInfo> EnumerateAudioRenderDevices();
