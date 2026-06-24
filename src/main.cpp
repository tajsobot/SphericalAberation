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
namespace {
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
}

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
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    glBindImageTexture(0, 0, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);

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
    GLuint inputTex    = loadInputTexture("../input/pi.png");

    // dummy VAO — required by core profile even if we don't use vertex buffers
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // ── run compute once (initial save) ──
    glBindImageTexture(0, outputTex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
    glUseProgram(computeProg);
    glUniform1f(glGetUniformLocation(computeProg, "u_r"), 0.0f);
    glUniform1i(glGetUniformLocation(computeProg, "u_input"), 1);
    glUniform1f(glGetUniformLocation(computeProg, "u_n"), 0.0f);
    glUniform1f(glGetUniformLocation(computeProg, "u_a"), 0.0f);
    glUniform1f(glGetUniformLocation(computeProg, "u_b"), 0.0f);
    glUniform1f(glGetUniformLocation(computeProg, "u_xLensPos"), 0.0f);
    glUniform1f(glGetUniformLocation(computeProg, "u_yLensPos"), 0.0f);
    glUniform1f(glGetUniformLocation(computeProg, "u_yObjPos"), 0.0f);
    glUniform1f(glGetUniformLocation(computeProg, "u_yObjPos"), 0.0f);
    glUniform1f(glGetUniformLocation(computeProg, "u_ObjScale"), 0.0f);


    glUniform2f(glGetUniformLocation(computeProg, "u_resolution"), IMAGE_W, IMAGE_H);
    glDispatchCompute(IMAGE_W / LOCAL_SIZE, IMAGE_H / LOCAL_SIZE, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
    savePNG(outputTex, IMAGE_W, IMAGE_H, "../output/firstFrame.png");

    //                            u_a je v neskoncnosti zaradi paralelnih zarkov!!!
    float u_r = 1.0f, u_n = 1.2f, u_a = 10.0f, u_b = 5.0f, u_xLensPos = 0.0f, u_yLensPos = 0.0f, u_xObjPos = 0.0f, u_yObjPos = 0.0f, u_ObjScale = 1.0f;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        if (glfwGetKey(window, GLFW_KEY_UP)    == GLFW_PRESS) {u_b += 0.01f; std::cout << "b:" << u_b << std::endl;}
        if (glfwGetKey(window, GLFW_KEY_DOWN)  == GLFW_PRESS) {u_b -= 0.01f; std::cout << "b:" << u_b << std::endl;}
        if (glfwGetKey(window, GLFW_KEY_LEFT)  == GLFW_PRESS) {u_r -= 0.01f; std::cout << "r:" << u_r << std::endl;}
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {u_r += 0.01f; std::cout << "r:" << u_r << std::endl;}
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {u_xLensPos += 0.01f; std::cout << "x_lens:" << u_xLensPos << std::endl;}
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {u_xLensPos -= 0.01f; std::cout << "x_lens:" << u_xLensPos << std::endl;}
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {u_yLensPos += 0.01f; std::cout << "y_lens:" << u_yLensPos << std::endl;}
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {u_yLensPos -= 0.01f; std::cout << "y_lens:" << u_yLensPos << std::endl;}
        if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS) {u_xObjPos += 0.01f; std::cout << "x_obj:" << u_xObjPos << std::endl;}
        if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS) {u_xObjPos -= 0.01f; std::cout << "x_obj:" << u_xObjPos << std::endl;}
        if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS) {u_yObjPos += 0.01f; std::cout << "y_obj:" << u_yObjPos << std::endl;}
        if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS) {u_yObjPos -= 0.01f; std::cout << "y_obj:" << u_yObjPos << std::endl;}
        if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS) {u_ObjScale += 0.01f; std::cout << "objScale:" << u_ObjScale << std::endl;}
        if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) {u_ObjScale -= 0.01f; std::cout << "objScale:" << u_ObjScale << std::endl;}
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {u_n += 0.001f; std::cout << "n:" << u_n << std::endl;}
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {u_n -= 0.001f; std::cout << "n:" << u_n << std::endl;}

        if (glfwGetKey(window, GLFW_KEY_C)     == GLFW_PRESS) {
            savePNG(outputTex, IMAGE_W, IMAGE_H, "../output/activeFrame.png"); //todo izpisi vse parametre za sliko
            std::printf(
                "=== saved frame ===\n"
                "u_r       = %.3f\n"
                "u_n       = %.3f\n"
                "u_a       = %.3f\n"
                "u_b       = %.3f\n"
                "u_xLensPos= %.3f\n"
                "u_yLensPos= %.3f\n"
                "u_xObjPos = %.3f\n"
                "u_yObjPos = %.3f\n"
                "u_ObjScale= %.3f\n"
                "===================\n",
                u_r, u_n, u_a, u_b,
                u_xLensPos, u_yLensPos,
                u_xObjPos, u_yObjPos,
                u_ObjScale
            );
        }

        // ── FIX 1: set compute uniforms while computeProg is active ──
        // glUniform* writes into whichever program is currently bound.
        // All compute uniforms must be set before dispatching computeProg.
        glBindImageTexture(0, outputTex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
        glUseProgram(computeProg);
        glUniform1f(glGetUniformLocation(computeProg, "u_r"), u_r);
        glUniform1f(glGetUniformLocation(computeProg, "u_n"), u_n);
        glUniform1f(glGetUniformLocation(computeProg, "u_a"), u_a);
        glUniform1f(glGetUniformLocation(computeProg, "u_b"), u_b);
        glUniform1f(glGetUniformLocation(computeProg, "u_xLensPos"), u_xLensPos);
        glUniform1f(glGetUniformLocation(computeProg, "u_yLensPos"), u_yLensPos);
        glUniform1f(glGetUniformLocation(computeProg, "u_xObjPos"), u_xObjPos);
        glUniform1f(glGetUniformLocation(computeProg, "u_yObjPos"), u_yObjPos);
        glUniform1f(glGetUniformLocation(computeProg, "u_ObjScale"), u_ObjScale);


        glUniform2f(glGetUniformLocation(computeProg, "u_resolution"), IMAGE_W, IMAGE_H);

        // ── FIX 2: actually dispatch the compute shader each frame ──
        // Without this call the uniforms above are uploaded but nothing runs —
        // outputTex never updates and you just keep seeing the initial frame.
        glDispatchCompute(IMAGE_W / LOCAL_SIZE, IMAGE_H / LOCAL_SIZE, 1);

        // Wait for compute writes to finish before the fragment shader samples outputTex.
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

        int winW, winH;
        glfwGetFramebufferSize(window, &winW, &winH);

        // black bars
        glViewport(0, 0, winW, winH);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        // letterbox viewport
        float target = (float)IMAGE_W / IMAGE_H;
        float actual = (float)winW / winH;
        int vpW, vpH, vpX, vpY;
        if (actual > target) {
            vpH = winH;
            vpW = (int)(winH * target);
            vpX = (winW - vpW) / 2;
            vpY = 0;
        } else {
            vpW = winW;
            vpH = (int)(winW / target);
            vpX = 0;
            vpY = (winH - vpH) / 2;
        }
        glViewport(vpX, vpY, vpW, vpH);

        // ── FIX 3: bind textures to their correct units before setting uniforms ──
        // glActiveTexture + glBindTexture must be called in order — the active unit
        // at the time of glBindTexture determines which slot the texture lands in.
        // inputTex goes to unit 1, outputTex to unit 0.
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, inputTex);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, outputTex);

        // ── FIX 1 (continued): set quad uniforms while quadProg is active ──
        // u_input was previously being set against computeProg (wrong program).
        // Switch to quadProg first, then upload both sampler uniforms.
        glUseProgram(quadProg);
        glUniform1i(glGetUniformLocation(quadProg, "u_tex"),   0); // outputTex on unit 0
        glUniform1i(glGetUniformLocation(quadProg, "u_input"), 1); // inputTex  on unit 1

        glDrawArrays(GL_TRIANGLES, 0, 3);
        glfwSwapBuffers(window);
    }

    // ── cleanup ──
    glDeleteTextures(1, &outputTex);
    glDeleteTextures(1, &inputTex);
    glDeleteProgram(computeProg);
    glDeleteProgram(quadProg);
    glDeleteVertexArrays(1, &vao);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}