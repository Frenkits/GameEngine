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

out vec3 vWorldPos;

void main() {
    // Il quad è già definito direttamente in coordinate mondo (vedi setupGrid):
    // niente matrice modello, la posizione del vertice È la posizione mondo.
    vWorldPos = aPos;
    gl_Position = uProjection * uView * vec4(aPos, 1.0);
}
)";

static const char* kGridFragmentSrc = R"(
#version 330 core
in vec3 vWorldPos;
uniform vec3 uCameraPos;
out vec4 FragColor;

// Griglia procedurale "infinita" (tecnica classica: calcola le linee nello
// shader invece di generare migliaia di segmenti via CPU). Antialiasing via
// derivate (fwidth), così le linee restano nitide a qualsiasi distanza/zoom.
float gridLineFactor(vec2 coord, float cellSize) {
    vec2 g = coord / cellSize;
    vec2 d = fwidth(g);
    vec2 lineDist = abs(fract(g - 0.5) - 0.5) / d;
    float line = min(lineDist.x, lineDist.y);
    return 1.0 - min(line, 1.0);
}

void main() {
    vec2 coord = vWorldPos.xz;

    float minor = gridLineFactor(coord, 1.0);   // una linea ogni unità
    float major = gridLineFactor(coord, 10.0);  // una linea più marcata ogni 10 unità

    float dist = length(vWorldPos - uCameraPos);
    float fade = clamp(1.0 - dist / 150.0, 0.0, 1.0); // dissolvenza, non finisce di colpo

    vec3 minorColor = vec3(0.32);
    vec3 majorColor = vec3(0.6);

    float intensity = max(minor * 0.5, major);
    vec3 color = mix(minorColor, majorColor, step(0.5, major));

    float alpha = intensity * fade;
    if (alpha < 0.01) discard; // niente da disegnare: lascia vedere lo sfondo

    FragColor = vec4(color, alpha);
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
out vec3 vWorldPos;

void main() {
    vNormalWorld = mat3(uModel) * aNormal;
    vec4 worldPos4 = uModel * vec4(aPos, 1.0);
    vWorldPos = worldPos4.xyz;
    gl_Position = uProjection * uView * worldPos4;
}
)";

static const char* kCubeFragmentSrc = R"(
#version 330 core
in vec3 vNormalWorld;
in vec3 vWorldPos;
uniform vec3 uColor;
out vec4 FragColor;

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

// Shader "flat" senza illuminazione, usato SOLO per il color-picking (vedi
// commento su Mesh::drawUnlit: qui il colore è un id codificato che non deve
// essere alterato in alcun modo).
static const char* kCubeUnlitVertexSrc = R"(
#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

void main() {
    gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0);
}
)";

static const char* kCubeUnlitFragmentSrc = R"(
#version 330 core
uniform vec3 uColor;
out vec4 FragColor;

void main() {
    FragColor = vec4(uColor, 1.0);
}
)";

// Shader per le linee dei gizmo (es. cono di visione delle camere): colore
// piatto modificabile, nessuna illuminazione.
static const char* kLineVertexSrc = R"(
#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 uView;
uniform mat4 uProjection;

void main() {
    gl_Position = uProjection * uView * vec4(aPos, 1.0);
}
)";

static const char* kLineFragmentSrc = R"(
#version 330 core
uniform vec3 uColor;
out vec4 FragColor;

void main() {
    FragColor = vec4(uColor, 1.0);
}
)";

Renderer::Renderer() {
    setupDebugTriangle();
    setupGrid();
    setupCube();
    setupLines();
    m_cubeUnlitShader = std::make_unique<Shader>(kCubeUnlitVertexSrc, kCubeUnlitFragmentSrc);
}

Renderer::~Renderer() {
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_gridVbo) glDeleteBuffers(1, &m_gridVbo);
    if (m_gridVao) glDeleteVertexArrays(1, &m_gridVao);
    if (m_cubeVbo) glDeleteBuffers(1, &m_cubeVbo);
    if (m_cubeVao) glDeleteVertexArrays(1, &m_cubeVao);
    if (m_lineVbo) glDeleteBuffers(1, &m_lineVbo);
    if (m_lineVao) glDeleteVertexArrays(1, &m_lineVao);
}

void Renderer::clear(float r, float g, float b, float a) const {
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE); // vedi commento in Engine::pickObjectAt: protezione contro
                           // stato GL ereditato da passate precedenti (es. ImGui)
    glDepthFunc(GL_LESS);
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

void Renderer::setupGrid() {
    m_gridShader = std::make_unique<Shader>(kGridVertexSrc, kGridFragmentSrc);

    // Un quad enorme sul piano Y=0: niente più migliaia di segmenti via CPU,
    // le linee della griglia sono calcolate dal fragment shader (vedi sopra).
    constexpr float kExtent = 1000.0f;
    const float verts[] = {
        -kExtent, 0.0f, -kExtent,
         kExtent, 0.0f, -kExtent,
         kExtent, 0.0f,  kExtent,

         kExtent, 0.0f,  kExtent,
        -kExtent, 0.0f,  kExtent,
        -kExtent, 0.0f, -kExtent,
    };
    m_gridVertexCount = 6;

    glGenVertexArrays(1, &m_gridVao);
    glGenBuffers(1, &m_gridVbo);
    glBindVertexArray(m_gridVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_gridVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void Renderer::drawGrid(const Mat4& view, const Mat4& projection, const Vec3& cameraPos) {
    // Il quad è semi-trasparente (alpha calcolato nello shader per le linee,
    // 0 altrove con discard): serve il blending per fonderlo correttamente
    // con quello che c'è dietro/davanti, invece di un riquadro opaco.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_gridShader->bind();
    m_gridShader->setUniformMat4("uView", view.data());
    m_gridShader->setUniformMat4("uProjection", projection.data());
    m_gridShader->setUniform3f("uCameraPos", cameraPos.x, cameraPos.y, cameraPos.z);

    glBindVertexArray(m_gridVao);
    glDrawArrays(GL_TRIANGLES, 0, m_gridVertexCount);
    glBindVertexArray(0);

    glDisable(GL_BLEND);
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

void Renderer::drawCubeUnlit(const Mat4& model, const Mat4& view, const Mat4& projection,
                              float r, float g, float b) {
    m_cubeUnlitShader->bind();
    m_cubeUnlitShader->setUniformMat4("uModel", model.data());
    m_cubeUnlitShader->setUniformMat4("uView", view.data());
    m_cubeUnlitShader->setUniformMat4("uProjection", projection.data());
    m_cubeUnlitShader->setUniform3f("uColor", r, g, b);

    glBindVertexArray(m_cubeVao);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

void Renderer::setGlobalLight(float lightX, float lightY, float lightZ,
                              float colorR, float colorG, float colorB,
                              float intensity, float ambient) {
    m_cubeShader->bind();
    m_cubeShader->setUniform3f("uLightPos", lightX, lightY, lightZ);
    m_cubeShader->setUniform3f("uLightColor", colorR, colorG, colorB);
    m_cubeShader->setUniform1f("uLightIntensity", intensity);
    m_cubeShader->setUniform1f("uAmbient", ambient);
}

void Renderer::setupLines() {
    m_lineShader = std::make_unique<Shader>(kLineVertexSrc, kLineFragmentSrc);

    glGenVertexArrays(1, &m_lineVao);
    glGenBuffers(1, &m_lineVbo);

    glBindVertexArray(m_lineVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_lineVbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void Renderer::drawLines(const std::vector<float>& positions, const Mat4& view, const Mat4& projection,
                         float r, float g, float b) {
    if (positions.empty()) return;

    glBindBuffer(GL_ARRAY_BUFFER, m_lineVbo);
    glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(float), positions.data(), GL_DYNAMIC_DRAW);

    m_lineShader->bind();
    m_lineShader->setUniformMat4("uView", view.data());
    m_lineShader->setUniformMat4("uProjection", projection.data());
    m_lineShader->setUniform3f("uColor", r, g, b);

    glBindVertexArray(m_lineVao);
    glDrawArrays(GL_LINES, 0, static_cast<int>(positions.size() / 3));
    glBindVertexArray(0);
}

} // namespace engine
