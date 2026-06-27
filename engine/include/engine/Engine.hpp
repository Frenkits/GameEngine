#pragma once
#include "engine/Window.hpp"
#include "engine/Renderer.hpp"
#include "engine/Camera.hpp"
#include "engine/Scene.hpp"
#include "engine/EditorUI.hpp"
#include "engine/Framebuffer.hpp"
#include "engine/Mesh.hpp"
#include <memory>
#include <string>
#include <unordered_map>

namespace engine {

// Punto di ingresso unico dell'engine: questa è la classe che esporremo
// a Python tramite pybind11. Tiene insieme finestra + renderer + editor UI
// e offre un loop "a step" pilotabile sia da C++ che da Python.
class Engine {
public:
    Engine(int width, int height, const std::string& title, const std::string& projectPath = "");
    ~Engine();

    bool isRunning() const;
    void tick();

    void setClearColor(float r, float g, float b, float a = 1.0f);

    bool isKeyPressed(int keyCode) const;
    bool isMouseButtonPressed(int button) const;
    void getMousePosition(double& x, double& y) const;
    float getDeltaTime() const { return m_deltaTime; }

    void setTrianglePosition(float x, float y);

    Scene& scene() { return m_scene; }
    const Scene& scene() const { return m_scene; }
    ObjectId getSelectedObject() const { return m_selectedObject; }
    void setSelectedObject(ObjectId id) { m_selectedObject = id; }

    OrbitCamera& camera() { return m_camera; }

    const std::string& projectPath() const { return m_projectPath; }

    void saveScene();
    void loadScene();

    bool isPlaying() const { return m_isPlaying; }

private:
    std::unique_ptr<Window> m_window;
    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<EditorUI> m_editorUI;
    std::unique_ptr<Framebuffer> m_sceneFramebuffer;
    std::unique_ptr<Framebuffer> m_pickingFramebuffer; // render pass "invisibile" per il click-to-select
    Scene m_scene;
    Scene m_playSnapshot; // copia della scena presa appena prima di entrare in Play,
                          // ripristinata allo Stop: le modifiche fatte dagli script
                          // durante il gioco (posizioni, rotazioni...) non restano.
    OrbitCamera m_camera;
    ObjectId m_selectedObject = kInvalidId;
    std::string m_projectPath;
    bool m_isPlaying = false; // true = vista di gioco a schermo intero (Play mode)

    float m_clearColor[4] = {0.1f, 0.1f, 0.12f, 1.0f};
    double m_lastFrameTime = 0.0;
    float m_deltaTime = 0.0f;

    double m_lastMouseX = 0.0;
    double m_lastMouseY = 0.0;
    bool m_hasLastMouse = false;

    float m_lastViewportWidth = 1280.0f;
    float m_lastViewportHeight = 720.0f;
    bool m_lastViewportHovered = false;

    void updateCameraInput();
    void renderSceneToFramebuffer();

    // --- Gizmo di traslazione per l'oggetto selezionato (solo Edit) ---
    // Tre freccette colorate (X=rosso, Y=verde, Z=blu) trascinabili col mouse.
    // Limite noto: il movimento è lungo gli assi MONDO, applicato direttamente
    // alla posizione LOCALE dell'oggetto — per oggetti senza genitore (o con
    // genitore non ruotato) il risultato è esatto; con un genitore ruotato la
    // direzione potrebbe non essere perfettamente quella attesa.
    int m_gizmoDragAxis = -1; // -1 = nessun drag in corso, 0=X, 1=Y, 2=Z
    int m_gizmoHoverAxis = -1; // asse sotto il cursore in questo frame (per evidenziarlo), -1 = nessuno
    bool m_gizmoLeftMouseWasPressed = false;
    float m_gizmoDragStartMousePixelX = 0.0f, m_gizmoDragStartMousePixelY = 0.0f;
    Vec3 m_gizmoDragStartPosition{};
    float m_gizmoOriginPixelX = 0.0f, m_gizmoOriginPixelY = 0.0f;
    float m_gizmoTipPixelX[3] = {0.0f, 0.0f, 0.0f};
    float m_gizmoTipPixelY[3] = {0.0f, 0.0f, 0.0f};
    bool m_gizmoVisibleThisFrame = false;

    // Calcola le posizioni a schermo delle freccette (per disegnarle e per il
    // test di "click sopra l'asse"). Va chiamato con la stessa camera/dimensioni
    // usate per il render, così il gizmo disegnato e quello cliccabile coincidono.
    void computeGizmoScreenPositions(const Mat4& view, const Mat4& proj, int viewportW, int viewportH);

    // Disegna le 3 freccette nel framebuffer della scena.
    void renderTransformGizmo(const Mat4& view, const Mat4& proj);

    // Gestisce hit-test/drag del gizmo con i dati di QUESTO frame (mouse,
    // pulsanti). Ritorna true se il click di questo frame è stato "consumato"
    // dal gizmo (va quindi saltata la normale selezione tramite color-picking).
    bool updateTransformGizmoInteraction(float mouseFractionX, float mouseFractionY,
                                         int viewportW, int viewportH, bool viewportHovered);

    // Drag "sul corpo": cliccando e tenendo premuto direttamente sull'oggetto
    // (non sulle freccette del gizmo), scorre liberamente sul piano orizzontale
    // alla sua altezza attuale — comodo per spostamenti rapidi su X/Z, mentre
    // il gizmo resta per il controllo preciso (incluso l'asse verticale Y).
    bool m_objectBodyDragActive = false;
    float m_objectBodyDragPlaneY = 0.0f;
    void updateObjectBodyDrag(float mouseFractionX, float mouseFractionY, float aspect, bool viewportHovered);

    // Ritorna le matrici view/projection da usare per questo frame: in Play,
    // se esiste un oggetto Camera nella scena, usa lui; altrimenti (o in Edit)
    // usa la camera orbitale dell'editor.
    void getActiveCameraMatrices(float aspect, Mat4& outView, Mat4& outProj, Vec3& outEyePos);

    // Calcola dove piazzare un oggetto appena trascinato dagli Assets:
    // lancia un raggio dalla camera attraverso il punto esatto del rilascio
    // (fractionX/Y, 0..1) e lo interseca con il piano del terreno (Y=0).
    Vec3 computeDropWorldPosition(float fractionX, float fractionY, float aspect, float planeY = 0.0f);

    // Composizione gerarchica delle trasformazioni: la matrice "mondo" di un
    // figlio è (matrice mondo del genitore) * (sua matrice locale). Così
    // muovere/ruotare/scalare un oggetto contenitore (es. la cartella radice
    // creata importando un .obj) sposta automaticamente tutti i suoi pezzi.
    void collectWorldMatrices(ObjectId id, const Mat4& parentWorld, std::unordered_map<ObjectId, Mat4>& out);
    std::unordered_map<ObjectId, Mat4> computeWorldMatrices();

    // Cache delle mesh caricate da file .obj. Chiave: "<path>::<nomeGruppo>"
    // per l'import multi-oggetto, oppure "<path>|<excludedGroupsCsv>" per il
    // vecchio flusso a mesh-singola (retrocompatibilità).
    std::unordered_map<std::string, std::shared_ptr<Mesh>> m_meshCache;
    std::shared_ptr<Mesh> getOrLoadMesh(const std::string& path, const std::string& excludedGroupsCsv);
    std::shared_ptr<Mesh> getOrLoadMeshGroup(const std::string& path, const std::string& groupName);
    std::shared_ptr<Mesh> getMeshForObject(const GameObject& obj);

    // Color picking: disegna ogni oggetto con un colore che codifica il suo
    // id, legge il pixel sotto il cursore, decodifica l'id. Ritorna kInvalidId
    // se il click è su uno sfondo vuoto.
    ObjectId pickObjectAt(float fractionX, float fractionY, int viewportWidth, int viewportHeight);
};

} // namespace engine
