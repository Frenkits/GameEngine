#include "engine/Window.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace engine {

namespace {
    bool g_glfwInitialized = false;
}

Window::Window(int width, int height, const std::string& title)
    : m_width(width), m_height(height) {

#ifdef _WIN32
    // Senza questo, su schermi con ridimensionamento Windows >100% (molto
    // comune su laptop, es. 125%/150%), Windows applica uno scaling automatico
    // alla finestra che disallinea le coordinate del mouse riportate da GLFW
    // rispetto ai pixel reali renderizzati: causa esattamente un offset di
    // click sistematico nel color-picking (e in generale in qualsiasi
    // interazione mouse-su-contenuto-renderizzato). Va chiamato una sola
    // volta, prima di creare qualsiasi finestra.
    static bool dpiAwarenessSet = false;
    if (!dpiAwarenessSet) {
        SetProcessDPIAware();
        dpiAwarenessSet = true;
    }
#endif

    if (!g_glfwInitialized) {
        if (!glfwInit()) {
            throw std::runtime_error("Impossibile inizializzare GLFW");
        }
        g_glfwInitialized = true;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

    m_handle = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!m_handle) {
        glfwTerminate();
        throw std::runtime_error("Impossibile creare la finestra GLFW");
    }

    glfwMakeContextCurrent(m_handle);
    glfwSetWindowUserPointer(m_handle, this);
    glfwSetFramebufferSizeCallback(m_handle, framebufferSizeCallback);
    glfwSetScrollCallback(m_handle, scrollCallback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        throw std::runtime_error("Impossibile inizializzare GLAD");
    }

    glViewport(0, 0, width, height);
    std::cout << "OpenGL " << glGetString(GL_VERSION) << " inizializzato.\n";
}

Window::~Window() {
    if (m_handle) {
        glfwDestroyWindow(m_handle);
    }
    // glfwTerminate() viene chiamato una sola volta dal processo se vuoi
    // gestire più finestre; qui per semplicità lo lasciamo al sistema operativo
    // alla chiusura del processo. Se crei/distruggi molte finestre, valuta
    // un contatore globale + glfwTerminate() quando arriva a zero.
}

bool Window::shouldClose() const {
    return glfwWindowShouldClose(m_handle);
}

void Window::requestClose() {
    glfwSetWindowShouldClose(m_handle, GLFW_TRUE);
}

void Window::pollEvents() {
    glfwPollEvents();
}

void Window::swapBuffers() {
    glfwSwapBuffers(m_handle);
}

void Window::setResizeCallback(std::function<void(int, int)> cb) {
    m_resizeCallback = std::move(cb);
}

bool Window::isKeyPressed(int keyCode) const {
    return glfwGetKey(m_handle, keyCode) == GLFW_PRESS;
}

bool Window::isMouseButtonPressed(int button) const {
    return glfwGetMouseButton(m_handle, button) == GLFW_PRESS;
}

void Window::getMousePosition(double& x, double& y) const {
    glfwGetCursorPos(m_handle, &x, &y);
}

double Window::getTime() {
    return glfwGetTime();
}

double Window::consumeScrollDelta() {
    double v = m_scrollAccum;
    m_scrollAccum = 0.0;
    return v;
}

void Window::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (!self) return;
    self->m_scrollAccum += yoffset;
}

void Window::framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (!self) return;
    self->m_width = width;
    self->m_height = height;
    glViewport(0, 0, width, height);
    if (self->m_resizeCallback) {
        self->m_resizeCallback(width, height);
    }
}

} // namespace engine
