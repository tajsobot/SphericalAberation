cmake_minimum_required(VERSION 3.30)
project(SphericalAberation)

set(CMAKE_CXX_STANDARD 20)

add_executable(SphericalAberation src/main.cpp)

set(SOURCES
        src/main.cpp
        src/glad.c
        src/stb_image.h
        src/stb_image_write.h
)

#glfw
add_subdirectory(libs/glfw)
include_directories(libs/glfw/include/GLFW)

#glad
add_library(glad STATIC src/glad.c
)
target_include_directories(glad PUBLIC libs/glad/include)

find_package(OpenGL REQUIRED)
target_link_libraries(SphericalAberation glfw OpenGL::GL glad)

target_include_directories(SphericalAberation PRIVATE libs/glfw/include)
target_include_directories(SphericalAberation PRIVATE libs/glad/include)

target_link_libraries(SphericalAberation glfw OpenGL::GL glad -static-libgcc -static-libstdc++)
