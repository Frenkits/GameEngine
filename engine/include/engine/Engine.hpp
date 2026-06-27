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

    // Ritorna le matrici view/projection da usare per questo frame: in Play,
    // se esiste un oggetto Camera nella scena, usa lui; altrimenti (o in Edit)
    // usa la camera orbitale dell'editor.
    void getActiveCameraMatrices(float aspect, Mat4& outView, Mat4& outProj, Vec3& outEyePos);

    // Calcola dove piazzare un oggetto appena trascinato dagli Assets:
    // lancia un raggio dalla camera attraverso il punto esatto del rilascio
    // (fractionX/Y, 0..1) e lo interseca con il piano del terreno (Y=0).
    Vec3 computeDropWorldPosition(float fractionX, float fractionY, float aspect);

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
