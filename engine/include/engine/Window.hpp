#pragma once
#include <string>
#include <functional>
#include <vector>

struct GLFWwindow;

namespace engine {

class Window {
public:
    Window(int width, int height, const std::string& title);
    ~Window();

    // Non copiabile (possiede risorse GLFW/OpenGL)
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool shouldClose() const;
    void requestClose();
    void pollEvents();
    void swapBuffers();

    int width() const { return m_width; }
    int height() const { return m_height; }

    // Callback opzionale invocata ad ogni resize della finestra
    void setResizeCallback(std::function<void(int, int)> cb);

    // --- Input ---
    // keyCode usa le costanti GLFW_KEY_* (es. GLFW_KEY_W = 87), esposte
    // a Python come pyengine.KEY_W ecc. nel modulo bindings.
    bool isKeyPressed(int keyCode) const;
    bool isMouseButtonPressed(int button) const;

    // Posizione del mouse in coordinate finestra (origine in alto a sinistra)
    void getMousePosition(double& x, double& y) const;

    // Tempo trascorso (in secondi) da quando GLFW è stato inizializzato.
    // Utile per calcolare il delta time tra frame.
    static double getTime();

    // Scroll della rotella mouse accumulato dall'ultima chiamata: la lettura
    // resetta l'accumulo (consuma il valore), tipico pattern "delta per frame".
    double consumeScrollDelta();

    // File trascinati dentro la finestra da Esplora File (o altro programma
    // del sistema operativo): accumulati dall'ultima chiamata, la lettura
    // consuma (resetta) l'elenco, stesso pattern dello scroll.
    std::vector<std::string> consumeDroppedFiles();

    GLFWwindow* handle() const { return m_handle; }

private:
    GLFWwindow* m_handle = nullptr;
    int m_width;
    int m_height;
    std::function<void(int, int)> m_resizeCallback;
    double m_scrollAccum = 0.0;
    std::vector<std::string> m_droppedFiles;

    static void framebufferSizeCallback(GLFWwindow* window, int width, int height);
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    static void dropCallback(GLFWwindow* window, int count, const char** paths);
};

} // namespace engine
