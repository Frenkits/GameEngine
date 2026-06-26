#pragma once
#include "engine/Scene.hpp"
#include <string>

struct GLFWwindow;

namespace engine {

// Inizializza ImGui (backend GLFW + OpenGL3, con docking) e disegna il
// layout standard di un editor: Hierarchy (sinistra), Inspector (destra),
// Scena (centro, mostra il framebuffer renderizzato), Assets (in basso),
// più una menu bar con File > Salva/Apri Scena.
class EditorUI {
public:
    // Risultato della UI di questo frame: l'Engine lo usa per sapere quanto
    // deve essere grande il framebuffer della scena e se l'utente ha chiesto
    // di salvare/caricare.
    struct FrameResult {
        float viewportWidth = 1280.0f;
        float viewportHeight = 720.0f;
        bool viewportHovered = false;
        bool saveRequested = false;
        bool openRequested = false;
        bool quitRequested = false;
        // Percorso del file .obj che l'utente ha trascinato dal pannello Assets
        // dentro il pannello Scena in questo frame. Vuoto se nessun drop.
        std::string droppedAssetPath;

        // Click sinistro dentro il pannello Scena in questo frame: usato per
        // la selezione "alla Unreal" cliccando direttamente sull'oggetto 3D.
        // Espresso come FRAZIONE (0..1) della dimensione del pannello, non
        // pixel assoluti: vedi commento in drawSceneWindow per il perché.
        bool clickedInViewport = false;
        float clickFractionX = 0.0f; // 0 = sinistra, 1 = destra
        float clickFractionY = 0.0f; // 0 = in alto, 1 = in basso
    };

    explicit EditorUI(GLFWwindow* windowHandle);
    ~EditorUI();

    void beginFrame();

    // sceneTextureId: texture a colori del Framebuffer in cui è stata
    // renderizzata la scena 3D in questo frame (verrà mostrata nel pannello "Scena").
    FrameResult drawPanels(Scene& scene, ObjectId& selectedId,
                           unsigned int sceneTextureId, const std::string& projectPath);

    void endFrame();

private:
    bool m_dockingLayoutBuilt = false;

    void setupDockingLayout();
    void drawMenuBar(FrameResult& result);
    void handleShortcuts(Scene& scene, ObjectId& selectedId);
    void drawHierarchyNode(Scene& scene, ObjectId id, ObjectId& selectedId);
    void drawHierarchyWindow(Scene& scene, ObjectId& selectedId);
    void drawInspectorWindow(Scene& scene, ObjectId selectedId);
    void drawSceneWindow(FrameResult& result, unsigned int sceneTextureId);
    void drawAssetsWindow(const std::string& projectPath);
};

} // namespace engine
