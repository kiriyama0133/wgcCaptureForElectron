#pragma once
// com winrt
#include <winrt/base.h>
#include <unknwn.h>

// winrt投影文件
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>


// DirectX 11 原生接口
#include <d3d11.h>
#include <d3d11_4.h> // 包含 D3D11 的所有原生 COM 接口
#include <Windows.Graphics.DirectX.Direct3D11.interop.h>
#include <Windows.Graphics.Capture.Interop.h>

// OpenGL 与窗口库
#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>