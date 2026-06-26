#pragma once

namespace engine {

// Renderizza la scena 3D in una texture invece che direttamente sullo schermo.
// Necessario per mostrare il viewport come un pannello ImGui dockabile
// (la "finestra Scena" al centro dell'editor) invece che a schermo intero.
class Framebuffer {
public:
    Framebuffer(int width, int height);
    ~Framebuffer();

    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

    // Ricrea le risorse GPU solo se le dimensioni sono effettivamente cambiate
    void resize(int width, int height);

    void bind() const;
    static void unbind();

    unsigned int colorTexture() const { return m_colorTexture; }
    int width() const { return m_width; }
    int height() const { return m_height; }

private:
    unsigned int m_fbo = 0;
    unsigned int m_colorTexture = 0;
    unsigned int m_depthRbo = 0;
    int m_width = 0;
    int m_height = 0;

    void create();
    void destroy();
};

} // namespace engine
