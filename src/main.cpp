#include <chrono>
#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <string>
#include <fstream>
#include <sstream>
#include <thread>

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

bool fullscreen = false;
int windowedX, windowedY, windowedWidth, windowedHeight;

void toggleFullscreen(GLFWwindow* window) {
    fullscreen = !fullscreen;

    if (fullscreen) {
        glfwGetWindowPos(window, &windowedX, &windowedY);
        glfwGetWindowSize(window, &windowedWidth, &windowedHeight);

        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);

        //set fullscreen
        glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    } else {
        glfwSetWindowMonitor(window, nullptr, windowedX, windowedY, windowedWidth, windowedHeight, 0);
    }
}

std::string readShaderFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "ERROR: Unable to open shader file: " << filepath << std::endl;
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

unsigned int compileShader(unsigned int type, const std::string& source) {
    unsigned int shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    // Check for compilation errors
    int success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "ERROR: Shader Compilation Failed\n" << infoLog << std::endl;
    }
    return shader;
}

unsigned int createShaderProgram(const std::string& vertexSource, const std::string& fragmentSource) {
    unsigned int vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    unsigned int fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    // Check for linking errors
    int success;
    char infoLog[512];
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        std::cerr << "ERROR: Program Linking Failed\n" << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

// global  mouse position
double mouseX = 0.0, mouseY = 0.0;

// callback mouse movement
void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    mouseX = xpos;
    mouseY = ypos;
}

int main() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "ERROR: GLFW Initialization Failed\n";
        return -1;
    }
    // Set GLFW window hints for OpenGL version and profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    float width = 1800.0f, height = 1300.0f;

    // Create a windowed OpenGL context
    GLFWwindow* window = glfwCreateWindow(width, height, "OpenGL Shader Example", nullptr, nullptr);
    if (!window) {
        std::cerr << "ERROR: GLFW Window Creation Failed\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(0); // Disable V-Sync
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // Load OpenGL functions with GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "ERROR: GLAD Initialization Failed\n";
        glfwTerminate();
        return -1;
    }

    // mouse callback
    glfwSetCursorPosCallback(window, mouse_callback);

    // load shaders
    std::string vertexShaderSource = readShaderFile("../src/vertex_shader.glsl");

    std::vector<std::string> fragmentFilepaths = {
        readShaderFile("../src/fragment_shader1.glsl"),
        readShaderFile("../src/fragment_shader2.glsl"),
        readShaderFile("../src/fragment_shader3.glsl"),
        readShaderFile("../src/fragment_shader4.glsl"),
        readShaderFile("../src/fragment_shader5.glsl")

    };

    std::vector<unsigned int> shaderPrograms ={
        createShaderProgram(vertexShaderSource, fragmentFilepaths[0]),
        createShaderProgram(vertexShaderSource, fragmentFilepaths[1]),
        createShaderProgram(vertexShaderSource, fragmentFilepaths[2]),
        createShaderProgram(vertexShaderSource, fragmentFilepaths[3]),
        createShaderProgram(vertexShaderSource, fragmentFilepaths[4])
    };


    // Set up the VAO and VBO for a full-screen quad
    float vertices[] = {
        // Positions (x, y)    // Texture Coordinates (u, v)
        -1.0f, -1.0f,          0.0f, 0.0f, // Bottom-left
         1.0f, -1.0f,          1.0f, 0.0f, // Bottom-right
         1.0f,  1.0f,          1.0f, 1.0f, // Top-right
        -1.0f,  1.0f,          0.0f, 1.0f  // Top-left
    };
    unsigned int indices[] = {
        0, 1, 2, // First triangle
        2, 3, 0,  // Second triangle
    };

    unsigned int VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    // Bind and set up VAO
    glBindVertexArray(VAO);

    // Bind and fill the VBO
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Bind and fill the EBO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // Define the vertex attribute pointers
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), static_cast<void *>(nullptr)); // Position attribute
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float))); // Texture coordinate attribute
    glEnableVertexAttribArray(1);

    // Unbind the VAO
    glBindVertexArray(1);

    float time1 = 0.0f;

    using Clock = std::chrono::high_resolution_clock;
    auto previousTime = Clock::now();

    float deltaTime = 0.0f;
    float timeCountTo1 = 0.0f;

    unsigned int activeProgram = shaderPrograms[4];
    int frameCount = 0;
    // shader switching 1-4
    while (!glfwWindowShouldClose(window)) {

        //izjemno
        for (int i = 0; i < 9; ++i) {
            if (glfwGetKey(window, GLFW_KEY_1 +i) == GLFW_PRESS) {
              activeProgram = shaderPrograms[i];
            }
        }


        if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
          std::cout << mouseX/width << " " << (height - mouseY)/height<< std::endl;
        }

        static bool pressed = false;
        if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
            if (!pressed) {
                toggleFullscreen(window);
                pressed = true;
            }
        } else {
            pressed = false;
        }

        int width, height;
        auto currentTime = Clock::now();
        deltaTime = std::chrono::duration<float>(currentTime - previousTime).count();
        previousTime = currentTime;

        glfwGetFramebufferSize(window, &width, &height);

        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(activeProgram);

        // Pass uniforms
        glUniform1f(glGetUniformLocation(activeProgram, "u_time"), time1);
        glUniform2f(glGetUniformLocation(activeProgram, "u_resolution"), width, height);
        glUniform2f(glGetUniformLocation(activeProgram, "u_mouse"), mouseX, height - mouseY); // Flip Y-axis

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

        glfwSwapBuffers(window);
        glfwPollEvents();
        frameCount++;

        if (timeCountTo1 >= 1.0f) {
            std::cout << frameCount <<" FPS" << std::endl;
            frameCount = 0;
            timeCountTo1 = 0.0f;
        }
        time1 += 0.1f * deltaTime;
        timeCountTo1 += deltaTime;
    }

    // Clean up
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteProgram(activeProgram);

    glfwTerminate();
    return 0;
}
