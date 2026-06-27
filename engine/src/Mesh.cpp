#include "engine/Mesh.hpp"
#include "engine/Shader.hpp"
#include <glad/glad.h>
#include <memory>

namespace engine {

namespace {
    const char* kMeshVertexSrc = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vNormalWorld;

void main() {
    // Approssimazione "normal matrix" valida per scale uniformi (sufficiente
    // per l'editor: se in futuro servono scale non uniformi estreme, va
    // sostituita con transpose(inverse(mat3(uModel))) ).
    vNormalWorld = mat3(uModel) * aNormal;
    gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0);
}
)";

    const char* kMeshFragmentSrc = R"(
#version 330 core
in vec3 vNormalWorld;
uniform vec3 uColor;
out vec4 FragColor;

// Luce direzionale fissa + ambient: semplice ma dà già la sensazione di
// "volume" rispetto al flat color puro di prima.
const vec3 kLightDir = normalize(vec3(0.4, -1.0, 0.35));
const float kAmbient = 0.35;

void main() {
    vec3 N = normalize(vNormalWorld);
    float diffuse = max(dot(N, -kLightDir), 0.0);
    float lighting = kAmbient + (1.0 - kAmbient) * diffuse;
    FragColor = vec4(uColor * lighting, 1.0);
}
)";

    Shader& meshShader() {
        static std::unique_ptr<Shader> shader = std::make_unique<Shader>(kMeshVertexSrc, kMeshFragmentSrc);
        return *shader;
    }

    // Shader "flat": nessuna illuminazione, il colore passato esce identico.
    // Usato SOLO per il color-picking, dove il colore è in realtà un id
    // codificato che non deve essere alterato in alcun modo.
    const char* kUnlitVertexSrc = R"(
#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

void main() {
    gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0);
}
)";

    const char* kUnlitFragmentSrc = R"(
#version 330 core
uniform vec3 uColor;
out vec4 FragColor;

void main() {
    FragColor = vec4(uColor, 1.0);
}
)";

    Shader& meshUnlitShader() {
        static std::unique_ptr<Shader> shader = std::make_unique<Shader>(kUnlitVertexSrc, kUnlitFragmentSrc);
        return *shader;
    }
}

Mesh::Mesh(const std::vector<float>& vertices) {
    if (vertices.empty()) return;

    constexpr int kFloatsPerVertex = 6; // x,y,z, nx,ny,nz
    m_vertexCount = static_cast<int>(vertices.size() / kFloatsPerVertex);

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, kFloatsPerVertex * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, kFloatsPerVertex * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

Mesh::~Mesh() {
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
}

void Mesh::draw(const Mat4& model, const Mat4& view, const Mat4& projection,
                float r, float g, float b) const {
    if (!isValid()) return;

    Shader& shader = meshShader();
    shader.bind();
    shader.setUniformMat4("uModel", model.data());
    shader.setUniformMat4("uView", view.data());
    shader.setUniformMat4("uProjection", projection.data());
    shader.setUniform3f("uColor", r, g, b);

    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, m_vertexCount);
    glBindVertexArray(0);
}

void Mesh::drawUnlit(const Mat4& model, const Mat4& view, const Mat4& projection,
                     float r, float g, float b) const {
    if (!isValid()) return;

    Shader& shader = meshUnlitShader();
    shader.bind();
    shader.setUniformMat4("uModel", model.data());
    shader.setUniformMat4("uView", view.data());
    shader.setUniformMat4("uProjection", projection.data());
    shader.setUniform3f("uColor", r, g, b);

    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, m_vertexCount);
    glBindVertexArray(0);
}

} // namespace engine
