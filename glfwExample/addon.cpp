#include <napi.h>
#include <winrt/base.h>

#include <memory>
#include <mutex>

#include "screen_capture.h"

namespace {

    std::unique_ptr<MonitorCapture> g_capture;
    std::once_flag g_winrt_apartment_once;

    /** 勿在 NODE_API_MODULE 入口调 init_apartment：会与 Electron 主进程已有 COM 冲突并直接崩溃。 */
    static void ensure_winrt_apartment()
    {
        std::call_once(g_winrt_apartment_once, []() {
            try {
                winrt::init_apartment(winrt::apartment_type::multi_threaded);
            }
            catch (winrt::hresult_error const& e) {
                const auto c = static_cast<uint32_t>(e.code());
                if (c != 0x80010106U) {
                    throw;
                }
                // RPC_E_CHANGED_MODE：当前线程已由 Chromium/Electron 初始化过 COM，继续用现有单元。
            }
        });
    }

    Napi::Value Start(const Napi::CallbackInfo& info)
    {
        Napi::Env env = info.Env();
        try {
            ensure_winrt_apartment();
            if (g_capture) {
                g_capture->stop();
                g_capture.reset();
            }
            g_capture = std::make_unique<MonitorCapture>();
            if (!g_capture->prepare_default_monitor()) {
                Napi::Error::New(env, "WGC prepare_default_monitor failed").ThrowAsJavaScriptException();
                return env.Null();
            }
            g_capture->begin_capture();
            return env.Undefined();
        }
        catch (const Napi::Error&) {
            throw;
        }
        catch (const std::exception& e) {
            Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
            return env.Null();
        }
    }
    
    Napi::Value GetFrame(const Napi::CallbackInfo& info) {
        Napi::Env env = info.Env();
        if (!g_capture) {
            return env.Null();
        }
        auto& buffer = g_capture->pixel_buffer();
        auto& mutex = g_capture->buffer_mutex();
        std::lock_guard<std::mutex> lock(mutex);
        return Napi::Buffer<uint8_t>::Copy(env, buffer.data(), buffer.size());
    }

    Napi::Value Stop(const Napi::CallbackInfo& info)
    {
        Napi::Env env = info.Env();
        if (g_capture) {
            g_capture->stop();
            g_capture.reset();
        }
        return env.Undefined();
    }

    Napi::Value GetSize(const Napi::CallbackInfo& info)
    {
        Napi::Env env = info.Env();
        Napi::Object o = Napi::Object::New(env);
        if (!g_capture || !g_capture->is_prepared()) {
            o.Set("width", Napi::Number::New(env, 0));
            o.Set("height", Napi::Number::New(env, 0));
            return o;
        }
        o.Set("width", Napi::Number::New(env, g_capture->width()));
        o.Set("height", Napi::Number::New(env, g_capture->height()));
        return o;
    }

} // namespace

Napi::Object InitializeModule(Napi::Env env, Napi::Object exports)
{
    exports.Set("start", Napi::Function::New(env, Start));
    exports.Set("stop", Napi::Function::New(env, Stop));
    exports.Set("getFrame", Napi::Function::New(env, GetFrame));
    exports.Set("getSize", Napi::Function::New(env, GetSize));
    return exports;
}

// 与 CMake target / 输出 capture_addon.node 一致；勿依赖可能未定义的 NODE_GYP_MODULE_NAME
NODE_API_MODULE(capture_addon, InitializeModule)
