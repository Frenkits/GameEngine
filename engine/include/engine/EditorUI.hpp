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
        bool togglePlayRequested = false; // utente ha cliccato Play/Stop in questo frame
        // Percorso del file .obj che l'utente ha trascinato dal pannello Assets
        // dentro il pannello Scena in questo frame. Vuoto se nessun drop.
        std::string droppedAssetPath;
        // Posizione del rilascio, come frazione (0..1) del pannello: usata da
        // Engine per calcolare DOVE nella scena 3D piazzare l'oggetto importato
        // (invece di metterlo sempre nello stesso punto fisso).
        float dropFractionX = 0.5f;
        float dropFractionY = 0.5f;

        // Click sinistro dentro il pannello Scena in questo frame: usato per
        // la selezione "alla Unreal" cliccando direttamente sull'oggetto 3D.
        // Espresso come FRAZIONE (0..1) della dimensione del pannello, non
        // pixel assoluti: vedi commento in drawSceneWindow per il perché.
        bool clickedInViewport = false;
        float clickFractionX = 0.0f; // 0 = sinistra, 1 = destra
        float clickFractionY = 0.0f; // 0 = in alto, 1 = in basso

        // Id di un oggetto della Hierarchy trascinato e rilasciato sul
        // pannello Scena: viene "staccato" da qualsiasi genitore (come
        // trascinarlo nella zona vuota in fondo alla Hierarchy). kInvalidId
        // se nessun drag di questo tipo è avvenuto in questo frame.
        ObjectId draggedToSceneId = kInvalidId;
    };

    explicit EditorUI(GLFWwindow* windowHandle);
    ~EditorUI();

    void beginFrame();

    // sceneTextureId: texture a colori del Framebuffer in cui è stata
    // renderizzata la scena 3D in questo frame. isPlaying: true se l'editor è
    // in modalità Play (mostra solo la vista di gioco a schermo intero).
    FrameResult drawPanels(Scene& scene, ObjectId& selectedId,
                           unsigned int sceneTextureId, const std::string& projectPath,
                           bool isPlaying);

    void endFrame();

private:
    bool m_dockingLayoutBuilt = false;

    // --- Stato del browser Assets (stile "Content Browser" di Unreal) ---
    std::string m_assetCurrentRelPath;  // percorso corrente relativo a "<progetto>/assets", "" = radice
    std::string m_assetSearchFilter;    // testo di ricerca per filtrare i nomi
    std::string m_assetRenamingPath;    // percorso assoluto dell'elemento in fase di rinomina (vuoto = nessuno)
    char m_assetRenameBuffer[256] = "";
    std::string m_assetPendingDelete;   // percorso assoluto in attesa di conferma eliminazione

    // Riassegno genitore via drag&drop nella Hierarchy: applicato DOPO aver
    // disegnato l'intero albero (non subito durante l'iterazione), altrimenti
    // modificare le liste children/m_rootObjects mentre le stiamo scorrendo
    // invaliderebbe l'iterazione in corso.
    bool m_hasPendingReparent = false;
    ObjectId m_pendingReparentChild = kInvalidId;
    ObjectId m_pendingReparentNewParent = kInvalidId;

    void setupDockingLayout();
    void drawMenuBar(FrameResult& result, bool isPlaying);
    void handleShortcuts(Scene& scene, ObjectId& selectedId);
    void drawHierarchyNode(Scene& scene, ObjectId id, ObjectId& selectedId);
    void drawHierarchyWindow(Scene& scene, ObjectId& selectedId);
    void drawInspectorWindow(Scene& scene, ObjectId selectedId);
    void drawSceneWindow(FrameResult& result, unsigned int sceneTextureId);
    void drawPlayWindow(FrameResult& result, unsigned int sceneTextureId);
    void drawViewportImageAndInput(FrameResult& result, unsigned int sceneTextureId);
    void drawAssetsWindow(const std::string& projectPath);
    void drawAssetBreadcrumb(const std::string& assetsRoot);
    void drawAssetGridItem(const std::string& fullPath, const std::string& displayName, bool isFolder);
};

} // namespace engine
