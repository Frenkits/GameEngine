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

    // Versione SENZA illuminazione: usata esclusivamente per il color-picking.
    // Lì il "colore" è in realtà l'id dell'oggetto codificato esattamente nei
    // byte RGB — se venisse moltiplicato per un fattore di illuminazione
    // (come fa draw()), il valore si corromperebbe e decodificheremmo un id
    // sbagliato. Qui il colore passa inalterato, pixel per pixel.
    void drawUnlit(const Mat4& model, const Mat4& view, const Mat4& projection,
                   float r, float g, float b) const;

    // Imposta la luce usata da TUTTE le Mesh (shader condiviso): chiamalo una
    // volta per frame, prima di disegnare qualsiasi oggetto illuminato.
    static void setGlobalLight(float lightX, float lightY, float lightZ,
                               float colorR, float colorG, float colorB,
                               float intensity, float ambient);

    bool isValid() const { return m_vertexCount > 0; }

    // Centro (del bounding box) della mesh in spazio LOCALE: utile per
    // ancorare gizmo/strumenti al centro VISIVO del pezzo, dato che molti
    // oggetti importati hanno la Transform a (0,0,0) e tutta la forma reale
    // dentro ai vertici della mesh stessa.
    Vec3 getLocalCenter() const { return m_localCenter; }

private:
    unsigned int m_vao = 0;
    unsigned int m_vbo = 0;
    int m_vertexCount = 0;
    Vec3 m_localCenter{};
};

} // namespace engine
