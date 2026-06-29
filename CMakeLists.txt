cmake_minimum_required(VERSION 3.16)
project(MyGameEngine LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Esporta compile_commands.json (utile per clangd / editor)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_PYTHON_BINDINGS "Compila i binding Python con pybind11" ON)

# --- Dipendenze di sistema ---
find_package(OpenGL REQUIRED)

# GLFW: lo cerchiamo come pacchetto di sistema (vcpkg / apt / brew).
# Se non lo trova, scommenta il blocco FetchContent qui sotto.
find_package(glfw3 QUIET)

if(NOT glfw3_FOUND)
    message(STATUS "glfw3 non trovato come pacchetto di sistema: lo scarico con FetchContent")
    include(FetchContent)
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
        GIT_SHALLOW TRUE
    )
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glfw)
endif()

# --- GLAD (loader OpenGL 3.3 core, generato da glad.dav1d.de) ---
add_subdirectory(third_party/glad)

# --- Dear ImGui (per Hierarchy/Inspector dell'editor) ---
include(FetchContent)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG docking
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
# imgui_impl_glfw.cpp include <GLFW/glfw3.h>: il target glfw è già definito sopra
target_link_libraries(imgui PUBLIC glfw OpenGL::GL)

# --- stb_image (libreria header-only per caricare PNG/JPG/ecc., per le texture) ---
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(stb)

add_library(stb_image INTERFACE)
target_include_directories(stb_image INTERFACE ${stb_SOURCE_DIR})

# --- Core engine C++ ---
add_subdirectory(engine)

# --- Binding Python (pybind11) ---
if(BUILD_PYTHON_BINDINGS)
    include(FetchContent)
    FetchContent_Declare(
        pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11.git
        GIT_TAG v2.13.6
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(pybind11)

    add_subdirectory(bindings)
endif()
