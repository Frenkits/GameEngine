add_library(engine_core STATIC
    src/Window.cpp
    src/Shader.cpp
    src/Renderer.cpp
    src/Scene.cpp
    src/EditorUI.cpp
    src/Framebuffer.cpp
    src/Launcher.cpp
    src/ObjLoader.cpp
    src/Mesh.cpp
    src/Texture.cpp
    src/Engine.cpp
)

target_include_directories(engine_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(engine_core PUBLIC
    glad
    glfw
    OpenGL::GL
    imgui
    stb_image
)
