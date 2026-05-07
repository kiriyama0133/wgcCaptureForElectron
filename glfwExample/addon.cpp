#include <napi.h>
#include <winrt/base.h>

#include <Windows.h>

#include <memory>
#include <mutex>
#include <string>

#include "media_recorder.h"
#include "screen_capture.h"
#include "audio_output_devices.h"

namespace {

    std::unique_ptr<MonitorCapture> g_capture;
    std::once_flag g_winrt_apartment_once;

    static std::wstring Utf8ToWide(const std::string& u8)
    {
        if (u8.empty())
            return L"";
        const int n = MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), -1, nullptr, 0);
        if (n <= 0)
            return L"";
        std::wstring w(static_cast<size_t>(n), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), -1, w.data(), n);
        if (!w.empty() && w.back() == L'\0')
            w.pop_back();
        return w;
    }

    static std::string WideToUtf8(const std::wstring& w)
    {
        if (w.empty())
            return {};
        const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (n <= 0)
            return {};
        std::string out(static_cast<size_t>(n - 1), '\0');
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, out.data(), n, nullptr, nullptr);
        return out;
    }

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
            }
        });
    }

    static RecorderConfig ParseRecorderConfig(Napi::Object const& o)
    {
        RecorderConfig cfg{};
        cfg.width = o.Get("width").As<Napi::Number>().Uint32Value();
        cfg.height = o.Get("height").As<Napi::Number>().Uint32Value();
        cfg.fps = o.Get("fps").As<Napi::Number>().Uint32Value();
        if (o.Has("videoBitrate"))
            cfg.videoBitrate = o.Get("videoBitrate").As<Napi::Number>().Uint32Value();
        if (o.Has("audioSampleRate"))
            cfg.audioSampleRate = o.Get("audioSampleRate").As<Napi::Number>().Uint32Value();
        if (o.Has("audioChannels"))
            cfg.audioChannels = o.Get("audioChannels").As<Napi::Number>().Uint32Value();
        if (o.Has("enableAudio"))
            cfg.enableAudio = o.Get("enableAudio").As<Napi::Boolean>().Value();
        if (o.Has("audioOutputDeviceId")) {
            const std::string u8 = o.Get("audioOutputDeviceId").As<Napi::String>().Utf8Value();
            if (!u8.empty())
                cfg.audioOutputDeviceId = Utf8ToWide(u8);
        }
        return cfg;
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

    Napi::Value GetFrame(const Napi::CallbackInfo& info)
    {
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

    /** startRecording(outputPathUtf8: string, config: RecorderConfig) — path 为 UTF-8，如 C:\\\\out\\\\cap.mp4 */
    Napi::Value StartRecording(const Napi::CallbackInfo& info)
    {
        Napi::Env env = info.Env();
        try {
            ensure_winrt_apartment();
            if (info.Length() < 2 || !info[0].IsString() || !info[1].IsObject()) {
                Napi::TypeError::New(env, "startRecording(outputPath: string, config: object)").ThrowAsJavaScriptException();
                return env.Null();
            }
            if (!g_capture || !g_capture->is_prepared()) {
                Napi::Error::New(env, "capture not started; call start() first").ThrowAsJavaScriptException();
                return env.Null();
            }

            const std::string pathUtf8 = info[0].As<Napi::String>().Utf8Value();
            const RecorderConfig cfg = ParseRecorderConfig(info[1].As<Napi::Object>());
            const std::wstring path = Utf8ToWide(pathUtf8);

            if (!g_capture->start_recording(path, cfg)) {
                Napi::Error::New(env, "start_recording failed (HRESULT / MF)").ThrowAsJavaScriptException();
                return env.Null();
            }
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

    Napi::Value StopRecording(const Napi::CallbackInfo& info)
    {
        Napi::Env env = info.Env();
        if (g_capture)
            g_capture->stop_recording();
        return env.Undefined();
    }

    Napi::Value IsRecording(const Napi::CallbackInfo& info)
    {
        Napi::Env env = info.Env();
        if (!g_capture)
            return Napi::Boolean::New(env, false);
        return Napi::Boolean::New(env, g_capture->is_recording());
    }

    /** 返回 [{ id: string, name: string }, ...] — id 为 WASAPI 设备 ID（传入 audioOutputDeviceId） */
    Napi::Value GetAudioOutputDevices(const Napi::CallbackInfo& info)
    {
        Napi::Env env = info.Env();
        try {
            const auto devices = EnumerateAudioRenderDevices();
            Napi::Array arr = Napi::Array::New(env, devices.size());
            for (size_t i = 0; i < devices.size(); ++i) {
                Napi::Object row = Napi::Object::New(env);
                row.Set("id", Napi::String::New(env, WideToUtf8(devices[i].id)));
                row.Set("name", Napi::String::New(env, WideToUtf8(devices[i].friendlyName)));
                arr.Set(static_cast<uint32_t>(i), row);
            }
            return arr;
        }
        catch (const std::exception& e) {
            Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
            return env.Null();
        }
    }

} // namespace

Napi::Object InitializeModule(Napi::Env env, Napi::Object exports)
{
    exports.Set("start", Napi::Function::New(env, Start));
    exports.Set("stop", Napi::Function::New(env, Stop));
    exports.Set("getFrame", Napi::Function::New(env, GetFrame));
    exports.Set("getSize", Napi::Function::New(env, GetSize));
    exports.Set("startRecording", Napi::Function::New(env, StartRecording));
    exports.Set("stopRecording", Napi::Function::New(env, StopRecording));
    exports.Set("isRecording", Napi::Function::New(env, IsRecording));
    exports.Set("getAudioOutputDevices", Napi::Function::New(env, GetAudioOutputDevices));
    return exports;
}

NODE_API_MODULE(capture_addon, InitializeModule)
