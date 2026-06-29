#include "engine/Mesh.hpp"
#include "engine/Shader.hpp"
#include <glad/glad.h>
#include <memory>
#include <algorithm>

namespace engine {

namespace {
    const char* kMeshVertexSrc = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vNormalWorld;
out vec3 vWorldPos;
out vec2 vUV;

void main() {
    vNormalWorld = mat3(uModel) * aNormal;
    vec4 worldPos4 = uModel * vec4(aPos, 1.0);
    vWorldPos = worldPos4.xyz;
    vUV = aUV;
    gl_Position = uProjection * uView * worldPos4;
}
)";

    const char* kMeshFragmentSrc = R"(
#version 330 core
in vec3 vNormalWorld;
in vec3 vWorldPos;
in vec2 vUV;
uniform vec3 uColor;
uniform sampler2D uTexture;
uniform bool uHasTexture;
out vec4 FragColor;

uniform vec3 uLightPos;
uniform vec3 uLightColor;
uniform float uLightIntensity;
uniform float uAmbient;

void main() {
    vec3 albedo = uHasTexture ? texture(uTexture, vUV).rgb : uColor;

    vec3 N = normalize(vNormalWorld);
    vec3 L = normalize(uLightPos - vWorldPos);
    float diffuse = max(dot(N, L), 0.0);
    vec3 lighting = vec3(uAmbient) + (1.0 - uAmbient) * diffuse * uLightColor * uLightIntensity;
    FragColor = vec4(albedo * lighting, 1.0);
}
)";

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

    Shader& meshShader() {
        static std::unique_ptr<Shader> shader = std::make_unique<Shader>(kMeshVertexSrc, kMeshFragmentSrc);
        return *shader;
    }

    Shader& meshUnlitShader() {
        static std::unique_ptr<Shader> shader = std::make_unique<Shader>(kUnlitVertexSrc, kUnlitFragmentSrc);
        return *shader;
    }
}

Mesh::Mesh(const std::vector<float>& vertices) {
    if (vertices.empty()) return;

    constexpr int kFloatsPerVertex = 8; // x,y,z, nx,ny,nz, u,v
    m_vertexCount = static_cast<int>(vertices.size() / kFloatsPerVertex);

    // Centro del bounding box in spazio locale: serve ad ancorare il gizmo
    // di trasformazione al centro VISIVO del pezzo.
    float minX = vertices[0], maxX = vertices[0];
    float minY = vertices[1], maxY = vertices[1];
    float minZ = vertices[2], maxZ = vertices[2];
    for (size_t i = 0; i < vertices.size(); i += kFloatsPerVertex) {
        minX = std::min(minX, vertices[i]);     maxX = std::max(maxX, vertices[i]);
        minY = std::min(minY, vertices[i + 1]); maxY = std::max(maxY, vertices[i + 1]);
        minZ = std::min(minZ, vertices[i + 2]); maxZ = std::max(maxZ, vertices[i + 2]);
    }
    m_localCenter = Vec3{(minX + maxX) * 0.5f, (minY + maxY) * 0.5f, (minZ + maxZ) * 0.5f};

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, kFloatsPerVertex * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, kFloatsPerVertex * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, kFloatsPerVertex * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

Mesh::~Mesh() {
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
}

void Mesh::draw(const Mat4& model, const Mat4& view, const Mat4& projection,
                float r, float g, float b, unsigned int textureId) const {
    if (!isValid()) return;

    Shader& shader = meshShader();
    shader.bind();
    shader.setUniformMat4("uModel", model.data());
    shader.setUniformMat4("uView", view.data());
    shader.setUniformMat4("uProjection", projection.data());
    shader.setUniform3f("uColor", r, g, b);

    if (textureId != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureId);
    }
    int hasTexLoc = glGetUniformLocation(shader.id(), "uHasTexture");
    glUniform1i(hasTexLoc, textureId != 0 ? 1 : 0);
    int texLoc = glGetUniformLocation(shader.id(), "uTexture");
    glUniform1i(texLoc, 0); // sempre texture unit 0

    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, m_vertexCount);
    glBindVertexArray(0);

    if (textureId != 0) {
        glBindTexture(GL_TEXTURE_2D, 0);
    }
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
