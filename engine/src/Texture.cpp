#include "engine/Texture.hpp"
#include <glad/glad.h>
#include <iostream>

// stb_image è header-only: in UNA sola unità di compilazione va definita
// STB_IMAGE_IMPLEMENTATION prima di includerlo, per generare l'implementazione.
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace engine {

Texture::Texture(const std::string& path) {
    stbi_set_flip_vertically_on_load(true); // le UV OBJ hanno origine in basso, come OpenGL

    int width, height, channels;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 0);
    if (!data) {
        std::cout << "[Texture] Impossibile caricare l'immagine: " << path << "\n";
        return;
    }

    GLenum format = GL_RGB;
    if (channels == 1) format = GL_RED;
    else if (channels == 3) format = GL_RGB;
    else if (channels == 4) format = GL_RGBA;

    glGenTextures(1, &m_textureId);
    glBindTexture(GL_TEXTURE_2D, m_textureId);

    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);

    std::cout << "[Texture] Caricata: " << path << " (" << width << "x" << height << ", " << channels << " canali)\n";
}

Texture::~Texture() {
    if (m_textureId) glDeleteTextures(1, &m_textureId);
}

} // namespace engine
