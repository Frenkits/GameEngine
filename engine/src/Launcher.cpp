#include "engine/Launcher.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <filesystem>
#include <vector>
#include <stdexcept>

namespace fs = std::filesystem;

namespace engine {

Launcher::Launcher() {
    std::error_code ec;
    fs::create_directories(m_projectsRoot, ec);
}

Launcher::~Launcher() = default;

std::string Launcher::run() {
    // Il launcher gestisce GLFW/GLAD/ImGui "a mano" (senza la classe Window
    // o EditorUI) perché è una finestra a parte, più semplice, mostrata e
    // distrutta completamente prima che l'editor apra la sua.
    if (!glfwInit()) {
        throw std::runtime_error("Impossibile inizializzare GLFW per il Launcher");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(560, 420, "Seleziona Progetto", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        throw std::runtime_error("Impossibile creare la finestra del Launcher");
    }
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        throw std::runtime_error("Impossibile inizializzare GLAD nel Launcher");
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    std::string chosenProject;
    char newProjectName[128] = "NuovoProgetto";

    while (!glfwWindowShouldClose(window) && chosenProject.empty()) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::Begin("Launcher", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

        ImGui::TextUnformatted("Progetti esistenti");
        ImGui::Separator();

        std::vector<std::string> projectNames;
        std::error_code ec;
        if (fs::exists(m_projectsRoot, ec)) {
            for (const auto& entry : fs::directory_iterator(m_projectsRoot, ec)) {
                if (entry.is_directory()) {
                    projectNames.push_back(entry.path().filename().string());
                }
            }
        }

        if (projectNames.empty()) {
            ImGui::TextDisabled("(nessun progetto trovato in \"%s/\")", m_projectsRoot.c_str());
        }

        for (const auto& name : projectNames) {
            if (ImGui::Button(("Apri: " + name).c_str(), ImVec2(-1, 0))) {
                chosenProject = m_projectsRoot + "/" + name;
            }
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Crea nuovo progetto");
        ImGui::InputText("Nome progetto", newProjectName, sizeof(newProjectName));
        if (ImGui::Button("Crea e apri", ImVec2(-1, 0)) && newProjectName[0] != '\0') {
            std::string path = m_projectsRoot + "/" + std::string(newProjectName);
            fs::create_directories(path, ec);
            fs::create_directories(path + "/assets", ec);
            chosenProject = path;
        }

        ImGui::End();

        ImGui::Render();
        int fbWidth, fbHeight;
        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
        glViewport(0, 0, fbWidth, fbHeight);
        glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate(); // il Launcher è indipendente: chiude GLFW del tutto.
                      // Window (engine editor) lo re-inizializzerà da capo.

    return chosenProject;
}

} // namespace engine
