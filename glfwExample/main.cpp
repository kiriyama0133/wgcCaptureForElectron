#include <fmt/color.h>
#include "winrt_c.h"
#include "logger.h"
#include "screen_capture.h"
#include <memory>
#include <mutex>
#include <atomic>
#include <vector>

using namespace Logger;
GLFWwindow* g_window = nullptr;

struct AppRenderState {
    GLFWwindow* window{};
    GLuint shaderProgram{};
    GLuint VAO{};
    GLuint texture{};
    GLint screenTextureLoc{};
    std::mutex* bufferMutex{};
    std::vector<uint8_t>* pixelBuffer{};
    std::atomic<bool>* frameDirty{};
    int texWidth{};
    int texHeight{};
    bool ready{};
};

static AppRenderState g_render;

void static processInput(GLFWwindow* window);

static void draw_present_frame(AppRenderState& s) {
    if (!s.ready || !s.window)
        return;

    glfwMakeContextCurrent(s.window);

    if (s.frameDirty->load()) {
        std::lock_guard<std::mutex> lock(*s.bufferMutex);
        glBindTexture(GL_TEXTURE_2D, s.texture);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, s.texWidth, s.texHeight,
            GL_RGBA, GL_UNSIGNED_BYTE, s.pixelBuffer->data());
        s.frameDirty->store(false);
    }

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(s.shaderProgram);
    glUniform1i(s.screenTextureLoc, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s.texture);
    glBindVertexArray(s.VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glfwSwapBuffers(s.window);
}

// 编译单个着色器的辅助函数
GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    // 检查编译是否成功
    int success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        // 这里用了你代码里的 logger，如果没有就改用 printf
        fmt::print(fg(fmt::color::red), "ERROR::SHADER::COMPILATION_FAILED\n{}\n", infoLog);
    }
    return shader;
}

// 链接着色器程序的辅助函数
GLuint createShaderProgram(const char* vShaderCode, const char* fShaderCode) {
    GLuint vertex = compileShader(GL_VERTEX_SHADER, vShaderCode);
    GLuint fragment = compileShader(GL_FRAGMENT_SHADER, fShaderCode);

    GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);

    // 检查链接是否成功
    int success;
    char infoLog[512];
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        fmt::print(fg(fmt::color::red), "ERROR::SHADER::PROGRAM::LINKING_FAILED\n{}\n", infoLog);
    }

    // 链接完就可以删掉中间件了
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    return program;
}

void CALLBACK TimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
    if (g_render.ready && g_window) {
        draw_present_frame(g_render);
    }
}

void static framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void static window_refresh_callback(GLFWwindow* window) {
    processInput(window);
    draw_present_frame(g_render);
}

void static window_focus_callback(GLFWwindow* window, int focused) {
    if (focused)
        fmt::print(fg(fmt::color::green) | fmt::emphasis::italic, "Window gained focus\n");
    else
        fmt::print(fg(fmt::color::red) | fmt::emphasis::italic, "Window lost focus\n");
}

void static processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

int main() {

    winrt::init_apartment();

    MonitorCapture capture;
    if (!capture.prepare_default_monitor()) {
        fmt::print(fg(fmt::color::red), "Monitor capture prepare failed.\n");
        return -1;
    }

    auto logger = std::make_shared<LoggerClass>();

    if (!glfwInit()) return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    g_window = glfwCreateWindow(800, 600, "Hello World", nullptr, nullptr);
    glfwMakeContextCurrent(g_window);
    if (g_window == nullptr) {
        logger->Log(LoggerLevel::error, "Failed to create GLFW window");
        glfwTerminate();
        return -1;
    }
    // 初始化 GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        logger->Log(LoggerLevel::error, "Failed to initialize GLAD");
        return -1;
    }
    GLuint glTextureHandle;
    glGenTextures(1, &glTextureHandle);
    glBindTexture(GL_TEXTURE_2D, glTextureHandle);

    // 过滤参数
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // 预分配纹理内存
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, capture.width(), capture.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    //SetTimer(glfwGetWin32Window(g_window), 1, 16, (TIMERPROC)TimerProc);
    
    //着色器绘制矩形
    const char* vertexShaderSource = R"(
    #version 460 core
    layout (location = 0) in vec2 aPos;
    layout (location = 1) in vec2 aTexCoords;
    out vec2 TexCoords;
    void main() {
        gl_Position = vec4(aPos, 0.0, 1.0);
        // 翻转 Y 轴：1.0 - y
        TexCoords = vec2(aTexCoords.x, 1.0 - aTexCoords.y);
    }
)";

    const char* fragmentShaderSource = R"(
    #version 460 core
    out vec4 FragColor;
    in vec2 TexCoords;
    uniform sampler2D screenTexture;
    void main() {
        vec4 sampledColor = texture(screenTexture, TexCoords);
        // 关键：只取 RGB，Alpha 强行设为 1.0
        FragColor = vec4(sampledColor.rgb, 1.0); 
    }
)";

    GLuint shaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);
    float vertices[] = {
        // 位置      // 纹理坐标
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,

        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };
    GLuint VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // 位置属性
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // 纹理坐标属性
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    g_render.window = g_window;
    g_render.shaderProgram = shaderProgram;
    g_render.VAO = VAO;
    g_render.texture = glTextureHandle;
    g_render.screenTextureLoc = glGetUniformLocation(shaderProgram, "screenTexture");
    g_render.bufferMutex = &capture.buffer_mutex();
    g_render.pixelBuffer = &capture.pixel_buffer();
    g_render.frameDirty = &capture.frame_dirty();
    g_render.texWidth = capture.width();
    g_render.texHeight = capture.height();
    g_render.ready = true;

    // 注册回调
    glfwSetFramebufferSizeCallback(g_window, framebuffer_size_callback);
    glfwSetWindowFocusCallback(g_window, window_focus_callback);
    glfwSetWindowRefreshCallback(g_window, window_refresh_callback);

    glViewport(0, 0, 800, 600);

    capture.begin_capture();

    while (!glfwWindowShouldClose(g_window)) {
        processInput(g_window);
        draw_present_frame(g_render);
        glfwPollEvents();
    }

    capture.stop();
    glfwTerminate();
    return 0;
}
