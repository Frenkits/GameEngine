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
out vec3 vWorldPos;

void main() {
    vNormalWorld = mat3(uModel) * aNormal;
    vec4 worldPos4 = uModel * vec4(aPos, 1.0);
    vWorldPos = worldPos4.xyz;
    gl_Position = uProjection * uView * worldPos4;
}
)";

    const char* kMeshFragmentSrc = R"(
#version 330 core
in vec3 vNormalWorld;
in vec3 vWorldPos;
uniform vec3 uColor;
out vec4 FragColor;

// Luce vera, impostata una volta per frame da Engine in base all'oggetto
// "Luce" presente nella scena (posizione/colore/intensità modificabili
// dall'Inspector). Se non c'è nessuna luce nella scena, Engine passa una
// posizione/colore di default così la scena non resta mai completamente buia.
uniform vec3 uLightPos;
uniform vec3 uLightColor;
uniform float uLightIntensity;
uniform float uAmbient;

void main() {
    vec3 N = normalize(vNormalWorld);
    vec3 L = normalize(uLightPos - vWorldPos);
    float diffuse = max(dot(N, L), 0.0);
    vec3 lighting = vec3(uAmbient) + (1.0 - uAmbient) * diffuse * uLightColor * uLightIntensity;
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

void Mesh::setGlobalLight(float lightX, float lightY, float lightZ,
                          float colorR, float colorG, float colorB,
                          float intensity, float ambient) {
    Shader& shader = meshShader();
    shader.bind();
    shader.setUniform3f("uLightPos", lightX, lightY, lightZ);
    shader.setUniform3f("uLightColor", colorR, colorG, colorB);
    shader.setUniform1f("uLightIntensity", intensity);
    shader.setUniform1f("uAmbient", ambient);
}

} // namespace engine
