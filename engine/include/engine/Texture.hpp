#pragma once
#include <string>

namespace engine {

// Carica un'immagine da file (PNG/JPG/BMP/...) come texture OpenGL, usando
// stb_image (libreria header-only, nessuna dipendenza pesante).
class Texture {
public:
    explicit Texture(const std::string& path);
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    bool isValid() const { return m_textureId != 0; }
    unsigned int id() const { return m_textureId; }

private:
    unsigned int m_textureId = 0;
};

} // namespace engine
