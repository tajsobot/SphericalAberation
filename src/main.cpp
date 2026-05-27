// main.cpp

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
// add at top
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"   // same vendor folder, grab from github.com/nothings/stb

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// ─── config ───────────────────────────────────────────────────────────────────

constexpr int IMAGE_W    = 1024;
constexpr int IMAGE_H    = 1024;
constexpr int LOCAL_SIZE = 16;

// ─── shader loaders ───────────────────────────────────────────────────────────

static std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) { std::cerr << "cannot open: " << path << "\n"; std::exit(1); }
    std::ostringstream ss; ss << f.rdbuf(); return ss.str();
}

static GLuint compileComputeShader(const std::string& path) {
    std::string src = readFile(path);
    const char* cstr = src.c_str();
    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(shader, 1, &cstr, nullptr);
    glCompileShader(shader);
    GLint ok; glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len; glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0');
        glGetShaderInfoLog(shader, len, nullptr, log.data());
        std::cerr << "[compute] " << log << "\n"; std::exit(1);
    }
    GLuint prog = glCreateProgram();
    glAttachShader(prog, shader); glLinkProgram(prog); glDeleteShader(shader);
    return prog;
}

static GLuint compileRasterShader(const char* vertSrc, const char* fragSrc) {
    auto compile = [](GLenum type, const char* src) {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            GLint len; glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
            std::string log(len, '\0');
            glGetShaderInfoLog(s, len, nullptr, log.data());
            std::cerr << "[raster] " << log << "\n"; std::exit(1);
        }
        return s;
    };
    GLuint vert = compile(GL_VERTEX_SHADER,   vertSrc);
    GLuint frag = compile(GL_FRAGMENT_SHADER, fragSrc);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert); glAttachShader(prog, frag);
    glLinkProgram(prog);
    glDeleteShader(vert); glDeleteShader(frag);
    return prog;
}

// ─── fullscreen quad shaders (embedded strings, no extra files needed) ────────

static const char* QUAD_VERT = R"(
#version 430 core
out vec2 uv;
void main() {
    // generate a clip-space triangle that covers the screen, no VBO needed
    vec2 pos[3] = vec2[](vec2(-1,-1), vec2(3,-1), vec2(-1,3));
    gl_Position = vec4(pos[gl_VertexID], 0.0, 1.0);
    uv = pos[gl_VertexID] * 0.5 + 0.5;
}
)";

static const char* QUAD_FRAG = R"(
#version 430 core
in vec2 uv;
out vec4 fragColor;
uniform sampler2D u_tex;
void main() {
    fragColor = texture(u_tex, uv);
}
)";

// ─── texture ──────────────────────────────────────────────────────────────────

static GLuint createOutputTexture(int w, int h) {
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, w, h);
    // linear filter so the quad draw looks nice if window is resized
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    return tex;
}

// ─── PNG save ─────────────────────────────────────────────────────────────────

static void savePNG(GLuint tex, int w, int h, const std::string& path) {
    std::vector<float> floats(w * h * 4);
    glBindTexture(GL_TEXTURE_2D, tex);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, floats.data());

    std::vector<unsigned char> bytes(w * h * 4);
    for (int i = 0; i < w * h * 4; i++) {
        float v = std::max(0.f, std::min(1.f, floats[i]));
        bytes[i] = (unsigned char)(v * 255.f + 0.5f);
    }
    stbi_flip_vertically_on_write(1);
    stbi_write_png(path.c_str(), w, h, 4, bytes.data(), w * 4);
    std::cout << "saved: " << path << "\n";
}
// ─── load input texture ───────────────────────────────────────────────────────

static GLuint loadInputTexture(const std::string& path) {
    int w, h, channels;
    stbi_set_flip_vertically_on_load(1);  // match OpenGL's bottom-left origin
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 4); // force RGBA
    if (!data) {
        std::cerr << "failed to load image: " << path << "\n";
        std::exit(1);
    }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    stbi_image_free(data);
    std::cout << "loaded: " << path << " (" << w << "x" << h << ")\n";
    return tex;
}

// ─── main ─────────────────────────────────────────────────────────────────────

int main() {
    // ── window ──
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(IMAGE_W, IMAGE_H, "Spherical Aberration", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    // ── resources ──
    GLuint outputTex   = createOutputTexture(IMAGE_W, IMAGE_H);
    GLuint computeProg = compileComputeShader("../shaders/raytrace.comp");
    GLuint quadProg    = compileRasterShader(QUAD_VERT, QUAD_FRAG);

    // dummy VAO — required by core profile even if we don't use vertex buffers
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // ── run compute once ──
    glBindImageTexture(0, outputTex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

    glUseProgram(computeProg);
    glUniform1f(glGetUniformLocation(computeProg, "u_r"), 1.0f);
    glUniform1f(glGetUniformLocation(computeProg, "u_n"), 1.5f);
    glUniform1f(glGetUniformLocation(computeProg, "u_a"), 5.0f);
    glUniform1f(glGetUniformLocation(computeProg, "u_b"), 3.0f);
    glUniform2f(glGetUniformLocation(computeProg, "u_resolution"), IMAGE_W, IMAGE_H);

    glDispatchCompute(IMAGE_W / LOCAL_SIZE, IMAGE_H / LOCAL_SIZE, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

    // ── save immediately after compute ──
    savePNG(outputTex, IMAGE_W, IMAGE_H, "../output/frame.png");

    // ── display loop — just keeps showing the same frame ──
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // close on Escape
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        int winW, winH;
        glfwGetFramebufferSize(window, &winW, &winH);
        glViewport(0, 0, winW, winH);

        glClear(GL_COLOR_BUFFER_BIT);

        // draw the compute output texture as a fullscreen quad
        glUseProgram(quadProg);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, outputTex);
        glUniform1i(glGetUniformLocation(quadProg, "u_tex"), 0);
        glDrawArrays(GL_TRIANGLES, 0, 3);  // 3 verts, no VBO

        glfwSwapBuffers(window);
    }

    // ── cleanup ──
    glDeleteTextures(1, &outputTex);
    glDeleteProgram(computeProg);
    glDeleteProgram(quadProg);
    glDeleteVertexArrays(1, &vao);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}