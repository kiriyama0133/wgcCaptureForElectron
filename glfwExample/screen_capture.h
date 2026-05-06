#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

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

    /// 创建 D3D 设备、staging、帧池并注册 FrameArrived（尚未 StartCapture）。
    bool prepare_default_monitor();
    /// 在消费者就绪后调用：CreateCaptureSession + StartCapture。
    void begin_capture();
    void stop();

    bool is_prepared() const;
    int width() const;
    int height() const;

    std::mutex& buffer_mutex();
    std::vector<uint8_t>& pixel_buffer();
    std::atomic<bool>& frame_dirty();
};
