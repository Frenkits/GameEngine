#pragma once
#include "engine/Math.hpp"
#include <vector>

namespace engine {

// Mesh generica: un VAO/VBO di sole posizioni + uno shader "flat color"
// condiviso da tutte le istanze (lazy-init alla prima Mesh creata).
// Usata per disegnare i modelli .obj importati negli Assets, al posto
// del cubo segnaposto.
class Mesh {
public:
    // vertices: posizioni già triangolate e appiattite (x,y,z, x,y,z, ...)
    explicit Mesh(const std::vector<float>& vertices);
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    void draw(const Mat4& model, const Mat4& view, const Mat4& projection,
              float r, float g, float b) const;

    bool isValid() const { return m_vertexCount > 0; }

private:
    unsigned int m_vao = 0;
    unsigned int m_vbo = 0;
    int m_vertexCount = 0;
};

} // namespace engine
