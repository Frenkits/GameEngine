#pragma once
#include "engine/Math.hpp"
#include <memory>

namespace engine {

class Shader;

// Renderer minimale: pulisce lo schermo e sa disegnare un triangolo/quad di test.
// È il punto da cui espandere batch rendering, mesh, texture, ecc.
class Renderer {
public:
    Renderer();
    ~Renderer();

    void clear(float r, float g, float b, float a = 1.0f) const;

    // Disegna un triangolo colorato a centro schermo (per validare la pipeline)
    void drawDebugTriangle();

    // Sposta il triangolo di debug: x,y in coordinate normalizzate (-1..1)
    void setOffset(float x, float y) { m_offsetX = x; m_offsetY = y; }

    // Griglia di riferimento sul piano XZ, tipica degli editor 3D
    void drawGrid(const Mat4& view, const Mat4& projection, int halfLines = 20, float spacing = 1.0f);

    // Cubo unitario (1x1x1) trasformato da 'model': usato per rappresentare
    // ogni GameObject nella scena finché non hai mesh vere e proprie
    void drawCube(const Mat4& model, const Mat4& view, const Mat4& projection,
                  float r, float g, float b);

private:
    unsigned int m_vao = 0;
    unsigned int m_vbo = 0;
    std::unique_ptr<Shader> m_shader;
    float m_offsetX = 0.0f;
    float m_offsetY = 0.0f;

    // Griglia
    unsigned int m_gridVao = 0;
    unsigned int m_gridVbo = 0;
    std::unique_ptr<Shader> m_gridShader;
    int m_gridLineCount = 0;
    void setupGrid(int halfLines, float spacing);

    // Cubo
    unsigned int m_cubeVao = 0;
    unsigned int m_cubeVbo = 0;
    std::unique_ptr<Shader> m_cubeShader;
    void setupCube();

    void setupDebugTriangle();
};

} // namespace engine

