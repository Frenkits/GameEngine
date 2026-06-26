#include "engine/Renderer.hpp"
#include "engine/Shader.hpp"
#include <glad/glad.h>
#include <vector>

namespace engine {

static const char* kVertexSrc = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;

uniform vec2 uOffset;

out vec3 vColor;

void main() {
    vColor = aColor;
    gl_Position = vec4(aPos.x + uOffset.x, aPos.y + uOffset.y, aPos.z, 1.0);
}
)";

static const char* kFragmentSrc = R"(
#version 330 core
in vec3 vColor;
out vec4 FragColor;

void main() {
    FragColor = vec4(vColor, 1.0);
}
)";

static const char* kGridVertexSrc = R"(
#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 uView;
uniform mat4 uProjection;

void main() {
    gl_Position = uProjection * uView * vec4(aPos, 1.0);
}
)";

static const char* kGridFragmentSrc = R"(
#version 330 core
out vec4 FragColor;

void main() {
    FragColor = vec4(0.45, 0.45, 0.45, 1.0);
}
)";

static const char* kCubeVertexSrc = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vNormalWorld;

void main() {
    vNormalWorld = mat3(uModel) * aNormal;
    gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0);
}
)";

static const char* kCubeFragmentSrc = R"(
#version 330 core
in vec3 vNormalWorld;
uniform vec3 uColor;
out vec4 FragColor;

const vec3 kLightDir = normalize(vec3(0.4, -1.0, 0.35));
const float kAmbient = 0.35;

void main() {
    vec3 N = normalize(vNormalWorld);
    float diffuse = max(dot(N, -kLightDir), 0.0);
    float lighting = kAmbient + (1.0 - kAmbient) * diffuse;
    FragColor = vec4(uColor * lighting, 1.0);
}
)";

Renderer::Renderer() {
    setupDebugTriangle();
    setupGrid(20, 1.0f);
    setupCube();
}

Renderer::~Renderer() {
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_gridVbo) glDeleteBuffers(1, &m_gridVbo);
    if (m_gridVao) glDeleteVertexArrays(1, &m_gridVao);
    if (m_cubeVbo) glDeleteBuffers(1, &m_cubeVbo);
    if (m_cubeVao) glDeleteVertexArrays(1, &m_cubeVao);
}

void Renderer::clear(float r, float g, float b, float a) const {
    glEnable(GL_DEPTH_TEST);
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::setupDebugTriangle() {
    m_shader = std::make_unique<Shader>(kVertexSrc, kFragmentSrc);

    // x, y, z,   r, g, b
    float vertices[] = {
        -0.5f, -0.5f, 0.0f,   1.0f, 0.0f, 0.0f,
         0.5f, -0.5f, 0.0f,   0.0f, 1.0f, 0.0f,
         0.0f,  0.5f, 0.0f,   0.0f, 0.0f, 1.0f,
    };

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void Renderer::drawDebugTriangle() {
    m_shader->bind();
    int loc = glGetUniformLocation(m_shader->id(), "uOffset");
    glUniform2f(loc, m_offsetX, m_offsetY);

    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

void Renderer::setupGrid(int halfLines, float spacing) {
    m_gridShader = std::make_unique<Shader>(kGridVertexSrc, kGridFragmentSrc);

    std::vector<float> verts;
    float extent = halfLines * spacing;
    for (int i = -halfLines; i <= halfLines; ++i) {
        float pos = i * spacing;
        // linea parallela all'asse X (varia Z)
        verts.insert(verts.end(), {-extent, 0.0f, pos,  extent, 0.0f, pos});
        // linea parallela all'asse Z (varia X)
        verts.insert(verts.end(), {pos, 0.0f, -extent,  pos, 0.0f, extent});
    }
    m_gridLineCount = static_cast<int>(verts.size() / 3); // numero di vertici totali

    glGenVertexArrays(1, &m_gridVao);
    glGenBuffers(1, &m_gridVbo);
    glBindVertexArray(m_gridVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_gridVbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void Renderer::drawGrid(const Mat4& view, const Mat4& projection, int /*halfLines*/, float /*spacing*/) {
    m_gridShader->bind();
    m_gridShader->setUniformMat4("uView", view.data());
    m_gridShader->setUniformMat4("uProjection", projection.data());

    glBindVertexArray(m_gridVao);
    glDrawArrays(GL_LINES, 0, m_gridLineCount);
    glBindVertexArray(0);
}

void Renderer::setupCube() {
    m_cubeShader = std::make_unique<Shader>(kCubeVertexSrc, kCubeFragmentSrc);

    // Cubo unitario centrato sull'origine: posizione (3) + normale (3) per
    // vertice. Una normale per faccia (flat shading), niente index buffer
    // per semplicità: leggermente ridondante in memoria ma il codice resta semplice.
    static const float vertices[] = {
        // back face (normale -Z)
        -0.5f,-0.5f,-0.5f,  0,0,-1,   0.5f,-0.5f,-0.5f,  0,0,-1,   0.5f, 0.5f,-0.5f,  0,0,-1,
         0.5f, 0.5f,-0.5f,  0,0,-1,  -0.5f, 0.5f,-0.5f,  0,0,-1,  -0.5f,-0.5f,-0.5f,  0,0,-1,
        // front face (normale +Z)
        -0.5f,-0.5f, 0.5f,  0,0, 1,   0.5f,-0.5f, 0.5f,  0,0, 1,   0.5f, 0.5f, 0.5f,  0,0, 1,
         0.5f, 0.5f, 0.5f,  0,0, 1,  -0.5f, 0.5f, 0.5f,  0,0, 1,  -0.5f,-0.5f, 0.5f,  0,0, 1,
        // left face (normale -X)
        -0.5f, 0.5f, 0.5f, -1,0,0,   -0.5f, 0.5f,-0.5f, -1,0,0,   -0.5f,-0.5f,-0.5f, -1,0,0,
        -0.5f,-0.5f,-0.5f, -1,0,0,   -0.5f,-0.5f, 0.5f, -1,0,0,   -0.5f, 0.5f, 0.5f, -1,0,0,
        // right face (normale +X)
         0.5f, 0.5f, 0.5f,  1,0,0,    0.5f, 0.5f,-0.5f,  1,0,0,    0.5f,-0.5f,-0.5f,  1,0,0,
         0.5f,-0.5f,-0.5f,  1,0,0,    0.5f,-0.5f, 0.5f,  1,0,0,    0.5f, 0.5f, 0.5f,  1,0,0,
        // bottom face (normale -Y)
        -0.5f,-0.5f,-0.5f,  0,-1,0,   0.5f,-0.5f,-0.5f,  0,-1,0,   0.5f,-0.5f, 0.5f,  0,-1,0,
         0.5f,-0.5f, 0.5f,  0,-1,0,  -0.5f,-0.5f, 0.5f,  0,-1,0,  -0.5f,-0.5f,-0.5f,  0,-1,0,
        // top face (normale +Y)
        -0.5f, 0.5f,-0.5f,  0,1,0,    0.5f, 0.5f,-0.5f,  0,1,0,    0.5f, 0.5f, 0.5f,  0,1,0,
         0.5f, 0.5f, 0.5f,  0,1,0,   -0.5f, 0.5f, 0.5f,  0,1,0,   -0.5f, 0.5f,-0.5f,  0,1,0,
    };

    glGenVertexArrays(1, &m_cubeVao);
    glGenBuffers(1, &m_cubeVbo);
    glBindVertexArray(m_cubeVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_cubeVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

void Renderer::drawCube(const Mat4& model, const Mat4& view, const Mat4& projection,
                         float r, float g, float b) {
    m_cubeShader->bind();
    m_cubeShader->setUniformMat4("uModel", model.data());
    m_cubeShader->setUniformMat4("uView", view.data());
    m_cubeShader->setUniformMat4("uProjection", projection.data());
    m_cubeShader->setUniform3f("uColor", r, g, b);

    glBindVertexArray(m_cubeVao);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

} // namespace engine
