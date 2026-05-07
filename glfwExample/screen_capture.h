#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

struct RecorderConfig;

/// 默认显示器 WGC 捕获：D3D11 + 帧池；CPU 缓冲为 RGBA（自 BGRA 转换）。供 main / N-API 等复用。
class MonitorCapture {
    struct Impl;
    std::unique_ptr<Impl> impl_;

public:
    MonitorCapture();
    ~MonitorCapture();

    MonitorCapture(const MonitorCapture&) = delete;
    MonitorCapture& operator=(const MonitorCapture&) = delete;
    MonitorCapture(MonitorCapture&&) = delete;
    MonitorCapture& operator=(MonitorCapture&&) = delete;

    bool prepare_default_monitor();
    void begin_capture();
    void stop();

    bool is_prepared() const;
    int width() const;
    int height() const;

    std::mutex& buffer_mutex();
    std::vector<uint8_t>& pixel_buffer();
    std::atomic<bool>& frame_dirty();

    void set_preview_enabled(bool enabled);
    bool preview_enabled() const;

    void set_recording_enabled(bool enabled);
    bool recording_enabled() const;

    /// 输出 MP4（MediaFoundation）。须已 prepare + begin_capture；path 为 UTF-16 本地路径。
    bool start_recording(const std::wstring& output_path, const RecorderConfig& config);
    void stop_recording();
    bool is_recording() const;
};
