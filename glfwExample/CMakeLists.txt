cmake_minimum_required(VERSION 3.15)



if(NOT WIN32)

  message(FATAL_ERROR "capture_addon: Windows only.")

endif()



project(capture_addon LANGUAGES CXX)



set(NODE_ADDON_API_DIR "${CMAKE_CURRENT_SOURCE_DIR}/node_modules/node-addon-api")

if(NOT EXISTS "${NODE_ADDON_API_DIR}/napi.h")

    set(NODE_ADDON_API_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../node_modules/node-addon-api")

endif()



# 必须与 ${CMAKE_JS_SRC}（win_delay_load_hook.cc）一起链接，否则 /DELAYLOAD:NODE.EXE 在 Electron 内无法正确解析，require(.node) 会崩溃。

# 参考 cmake-js README：add_library(... ${CMAKE_JS_SRC})

add_library(${PROJECT_NAME} SHARED

    "addon.cpp"

    "main_n.cpp"

    "WasapiLoopback.cpp"

    "audio_output_devices.cpp"

)

if(CMAKE_JS_SRC)

  target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_JS_SRC})

endif()



target_include_directories(${PROJECT_NAME} PRIVATE

  "${CMAKE_CURRENT_SOURCE_DIR}"

  "${NODE_ADDON_API_DIR}"    # 提供 napi.h

  "${CMAKE_JS_INC}"         # 提供 node_api.h (由 cmake-js 自动注入)

)



target_compile_definitions(${PROJECT_NAME} PRIVATE

  WGC_NATIVE_NODE_ADDON 

  NAPI_CPP_EXCEPTIONS 

  FMT_HEADER_ONLY 

  WIN32_LEAN_AND_MEAN

)



set_target_properties(${PROJECT_NAME} PROPERTIES 

    CXX_STANDARD 20

    CXX_STANDARD_REQUIRED ON

    PREFIX "" 

    SUFFIX ".node"

)



# 必须与 Node/Electron 一致使用动态 CRT（/MD）。cmake-js 默认 MultiThreaded(/MT) 时，require(.node) 常在 Electron 内立刻崩溃。

set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY

    "$<$<CONFIG:Debug>:MultiThreadedDebugDLL>$<$<CONFIG:Release>:MultiThreadedDLL>$<$<CONFIG:RelWithDebInfo>:MultiThreadedDLL>$<$<CONFIG:MinSizeRel>:MultiThreadedDLL>")



if(MSVC)

  # /await 用于支持 C++/WinRT 的异步操作

  target_compile_options(${PROJECT_NAME} PRIVATE /await /EHsc /permissive- /utf-8)

endif()



target_link_libraries(${PROJECT_NAME} PRIVATE 

    d3d11.lib 

    windowsapp.lib

    delayimp.lib              # 必须链接这个库来解决 __delayLoadHelper2 错误

    mfplat.lib

    mfuuid.lib

    mfreadwrite.lib

    avrt.lib

    Mmdevapi.lib

    "${CMAKE_JS_LIB}" 

)



# --- CTest：MF 录制管线冒烟（与 addon 同源编译 WasapiLoopback.cpp）---

enable_testing()



add_executable(mf_recording_smoke

    "tests/mf_recording_smoke.cpp"

    "WasapiLoopback.cpp"

)



target_include_directories(mf_recording_smoke PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")



target_compile_definitions(mf_recording_smoke PRIVATE WIN32_LEAN_AND_MEAN)



set_target_properties(mf_recording_smoke PROPERTIES

    CXX_STANDARD 20

    CXX_STANDARD_REQUIRED ON

)



set_property(TARGET mf_recording_smoke PROPERTY MSVC_RUNTIME_LIBRARY

    "$<$<CONFIG:Debug>:MultiThreadedDebugDLL>$<$<CONFIG:Release>:MultiThreadedDLL>$<$<CONFIG:RelWithDebInfo>:MultiThreadedDLL>$<$<CONFIG:MinSizeRel>:MultiThreadedDLL>")



if(MSVC)

    target_compile_options(mf_recording_smoke PRIVATE /EHsc /permissive- /utf-8)

endif()



target_link_libraries(mf_recording_smoke PRIVATE

    d3d11.lib

    mfplat.lib

    mfuuid.lib

    mfreadwrite.lib

    avrt.lib

    Mmdevapi.lib

)



add_test(NAME mf_recording_smoke COMMAND mf_recording_smoke)

