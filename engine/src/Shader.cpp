#include "engine/Shader.hpp"
#include <glad/glad.h>

#include <stdexcept>
#include <vector>
#include <iostream>

namespace engine {

unsigned int Shader::compile(unsigned int type, const std::string& src) {
    unsigned int shader = glCreateShader(type);
    const char* csrc = src.c_str();
    glShaderSource(shader, 1, &csrc, nullptr);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        int len;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(len);
        glGetShaderInfoLog(shader, len, nullptr, log.data());
        throw std::runtime_error("Errore compilazione shader: " + std::string(log.data()));
    }
    return shader;
}

Shader::Shader(const std::string& vertexSrc, const std::string& fragmentSrc) {
    unsigned int vs = compile(GL_VERTEX_SHADER, vertexSrc);
    unsigned int fs = compile(GL_FRAGMENT_SHADER, fragmentSrc);

    m_id = glCreateProgram();
    glAttachShader(m_id, vs);
    glAttachShader(m_id, fs);
    glLinkProgram(m_id);

    int success;
    glGetProgramiv(m_id, GL_LINK_STATUS, &success);
    if (!success) {
        int len;
        glGetProgramiv(m_id, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(len);
        glGetProgramInfoLog(m_id, len, nullptr, log.data());
        glDeleteShader(vs);
        glDeleteShader(fs);
        throw std::runtime_error("Errore link shader program: " + std::string(log.data()));
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
}

Shader::~Shader() {
    if (m_id) glDeleteProgram(m_id);
}

void Shader::bind() const { glUseProgram(m_id); }
void Shader::unbind() const { glUseProgram(0); }

void Shader::setUniformMat4(const std::string& name, const float* matrix) const {
    int loc = glGetUniformLocation(m_id, name.c_str());
    glUniformMatrix4fv(loc, 1, GL_FALSE, matrix);
}

void Shader::setUniform3f(const std::string& name, float x, float y, float z) const {
    int loc = glGetUniformLocation(m_id, name.c_str());
    glUniform3f(loc, x, y, z);
}

void Shader::setUniform1f(const std::string& name, float value) const {
    int loc = glGetUniformLocation(m_id, name.c_str());
    glUniform1f(loc, value);
}

} // namespace engine
