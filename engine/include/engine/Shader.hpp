#pragma once
#include <string>

namespace engine {

class Shader {
public:
    Shader(const std::string& vertexSrc, const std::string& fragmentSrc);
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    void bind() const;
    void unbind() const;

    void setUniformMat4(const std::string& name, const float* matrix) const;
    void setUniform3f(const std::string& name, float x, float y, float z) const;
    void setUniform1f(const std::string& name, float value) const;

    unsigned int id() const { return m_id; }

private:
    unsigned int m_id = 0;

    static unsigned int compile(unsigned int type, const std::string& src);
};

} // namespace engine
