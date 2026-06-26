#include "engine/Engine.hpp"
#include "engine/ObjLoader.hpp"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <filesystem>
#include <sstream>
#include <iostream>

namespace fs = std::filesystem;

namespace engine {

namespace {
    // Codifica/decodifica un ObjectId in un colore RGB a 8 bit per canale,
    // usato per il "color picking": disegniamo ogni oggetto con un colore
    // univoco su un framebuffer invisibile, poi leggiamo il pixel sotto il
    // cursore del mouse per scoprire quale oggetto è stato cliccato.
    void idToColor(ObjectId id, float& r, float& g, float& b) {
        unsigned int v = static_cast<unsigned int>(id) + 1; // +1: 0 resta riservato allo sfondo
        r = ((v >> 0) & 0xFF) / 255.0f;
        g = ((v >> 8) & 0xFF) / 255.0f;
        b = ((v >> 16) & 0xFF) / 255.0f;
    }

    ObjectId colorToId(unsigned char r, unsigned char g, unsigned char b) {
        unsigned int v = static_cast<unsigned int>(r)
                        | (static_cast<unsigned int>(g) << 8)
                        | (static_cast<unsigned int>(b) << 16);
        if (v == 0) return kInvalidId; // sfondo: nessun oggetto sotto al cursore
        return static_cast<ObjectId>(v - 1);
    }
}

Engine::Engine(int width, int height, const std::string& title, const std::string& projectPath)
    : m_projectPath(projectPath) {

    m_window = std::make_unique<Window>(width, height, title);
    m_renderer = std::make_unique<Renderer>();
    m_editorUI = std::make_unique<EditorUI>(m_window->handle());
    m_sceneFramebuffer = std::make_unique<Framebuffer>(
        static_cast<int>(m_lastViewportWidth), static_cast<int>(m_lastViewportHeight));

    if (!m_projectPath.empty()) {
        std::error_code ec;
        fs::create_directories(m_projectPath, ec);
        fs::create_directories(m_projectPath + "/assets", ec);

        std::string scenePath = m_projectPath + "/scene.txt";
        if (fs::exists(scenePath, ec)) {
            m_scene.loadFromFile(scenePath);
        } else {
            m_scene.createObject("Cubo");
            m_scene.createObject("Luce");
        }
    } else {
        m_scene.createObject("Cubo");
        m_scene.createObject("Luce");
    }
}

Engine::~Engine() = default;

bool Engine::isRunning() const {
    return !m_window->shouldClose();
}

void Engine::saveScene() {
    std::string path = m_projectPath.empty() ? "scene.txt" : (m_projectPath + "/scene.txt");
    m_scene.saveToFile(path);
}

void Engine::loadScene() {
    std::string path = m_projectPath.empty() ? "scene.txt" : (m_projectPath + "/scene.txt");
    m_scene.loadFromFile(path);
    m_selectedObject = kInvalidId;
}

void Engine::updateCameraInput() {
    double mx, my;
    m_window->getMousePosition(mx, my);

    if (!m_hasLastMouse) {
        m_lastMouseX = mx;
        m_lastMouseY = my;
        m_hasLastMouse = true;
    }

    double dx = mx - m_lastMouseX;
    double dy = my - m_lastMouseY;
    m_lastMouseX = mx;
    m_lastMouseY = my;

    bool overViewport = m_lastViewportHovered;

    if (overViewport && m_window->isMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT)) {
        m_camera.orbit(static_cast<float>(dx), static_cast<float>(dy));
    }
    if (overViewport && m_window->isMouseButtonPressed(GLFW_MOUSE_BUTTON_MIDDLE)) {
        m_camera.pan(static_cast<float>(dx), static_cast<float>(dy));
    }

    double scroll = m_window->consumeScrollDelta();
    if (overViewport && scroll != 0.0) {
        m_camera.zoom(static_cast<float>(scroll));
    }
}

std::shared_ptr<Mesh> Engine::getOrLoadMesh(const std::string& path, const std::string& excludedGroupsCsv) {
    std::string cacheKey = path + "|" + excludedGroupsCsv;

    auto it = m_meshCache.find(cacheKey);
    if (it != m_meshCache.end()) return it->second;

    std::vector<std::string> excludeList;
    std::stringstream ss(excludedGroupsCsv);
    std::string token;
    while (std::getline(ss, token, ',')) {
        size_t start = token.find_first_not_of(' ');
        size_t end = token.find_last_not_of(' ');
        if (start != std::string::npos) {
            excludeList.push_back(token.substr(start, end - start + 1));
        }
    }

    std::vector<float> vertices;
    if (!loadObjFile(path, vertices, excludeList)) {
        m_meshCache[cacheKey] = nullptr;
        return nullptr;
    }

    auto mesh = std::make_shared<Mesh>(vertices);
    m_meshCache[cacheKey] = mesh;
    return mesh;
}

std::shared_ptr<Mesh> Engine::getOrLoadMeshGroup(const std::string& path, const std::string& groupName) {
    std::string cacheKey = path + "::" + groupName;

    auto it = m_meshCache.find(cacheKey);
    if (it != m_meshCache.end()) return it->second;

    // Cache miss: il file probabilmente non è ancora stato parsato in questa
    // sessione (es. scena ricaricata da disco). Carichiamo TUTTI i gruppi in
    // un colpo, popolando la cache per tutti: le richieste successive per
    // altri gruppi dello stesso file saranno istantanee.
    std::vector<ObjGroup> groups;
    if (loadObjFileGrouped(path, groups)) {
        for (auto& g : groups) {
            m_meshCache[path + "::" + g.name] = std::make_shared<Mesh>(g.vertices);
        }
    }

    auto it2 = m_meshCache.find(cacheKey);
    return it2 != m_meshCache.end() ? it2->second : nullptr;
}

std::shared_ptr<Mesh> Engine::getMeshForObject(const GameObject& obj) {
    if (obj.meshPath.empty()) return nullptr;
    if (!obj.groupName.empty()) return getOrLoadMeshGroup(obj.meshPath, obj.groupName);
    return getOrLoadMesh(obj.meshPath, obj.excludedGroups); // retrocompatibilità vecchie scene
}

void Engine::collectWorldMatrices(ObjectId id, const Mat4& parentWorld, std::unordered_map<ObjectId, Mat4>& out) {
    const GameObject* obj = m_scene.getObject(id);
    if (!obj) return;

    Mat4 world = parentWorld * obj->transform.getMatrix();
    out[id] = world;

    for (ObjectId childId : obj->children) {
        collectWorldMatrices(childId, world, out);
    }
}

std::unordered_map<ObjectId, Mat4> Engine::computeWorldMatrices() {
    std::unordered_map<ObjectId, Mat4> result;
    Mat4 identity = Mat4::identity();
    for (ObjectId rootId : m_scene.getRootObjects()) {
        collectWorldMatrices(rootId, identity, result);
    }
    return result;
}

void Engine::renderSceneToFramebuffer() {
    m_sceneFramebuffer->resize(static_cast<int>(m_lastViewportWidth), static_cast<int>(m_lastViewportHeight));
    m_sceneFramebuffer->bind();

    m_renderer->clear(m_clearColor[0], m_clearColor[1], m_clearColor[2], m_clearColor[3]);

    float aspect = m_lastViewportHeight > 0.0f ? (m_lastViewportWidth / m_lastViewportHeight) : 1.0f;
    Mat4 view = m_camera.getViewMatrix();
    Mat4 proj = m_camera.getProjectionMatrix(aspect);

    m_renderer->drawGrid(view, proj);

    std::unordered_map<ObjectId, Mat4> worldMatrices = computeWorldMatrices();

    for (const auto& [id, obj] : m_scene.getAllObjects()) {
        bool isSelected = (id == m_selectedObject);

        // Colore materiale dell'oggetto, con un tint verso l'arancio quando
        // selezionato (feedback visivo, sostituisce lo schema fisso precedente).
        float r = obj.baseColor[0], g = obj.baseColor[1], b = obj.baseColor[2];
        if (isSelected) {
            r = r * 0.5f + 1.0f * 0.5f;
            g = g * 0.5f + 0.65f * 0.5f;
            b = b * 0.5f + 0.1f * 0.5f;
        }

        auto worldIt = worldMatrices.find(id);
        Mat4 worldMatrix = (worldIt != worldMatrices.end()) ? worldIt->second : obj.transform.getMatrix();

        std::shared_ptr<Mesh> mesh = getMeshForObject(obj);
        if (mesh && mesh->isValid()) {
            mesh->draw(worldMatrix, view, proj, r, g, b);
        } else if (obj.children.empty()) {
            // Solo i "veri" oggetti vuoti mostrano il cubo segnaposto: i
            // contenitori creati dall'import multi-oggetto (la cartella radice
            // che raggruppa i pezzi importati) restano invisibili, è giusto così.
            m_renderer->drawCube(worldMatrix, view, proj, r, g, b);
        }
    }

    Framebuffer::unbind();
    glViewport(0, 0, m_window->width(), m_window->height());
}

ObjectId Engine::pickObjectAt(float fractionX, float fractionY, int viewportWidth, int viewportHeight) {
    int w = viewportWidth;
    int h = viewportHeight;
    if (w <= 0 || h <= 0) return kInvalidId;

    if (!m_pickingFramebuffer) {
        m_pickingFramebuffer = std::make_unique<Framebuffer>(w, h);
    }
    m_pickingFramebuffer->resize(w, h);
    m_pickingFramebuffer->bind();

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // nero = id 0 = "nessun oggetto"
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);   // forziamo la SCRITTURA della profondità: se questo
                            // stato fosse rimasto disabilitato da una passata
                            // ImGui precedente, il test di profondità confronta
                            // sempre con un buffer "vuoto" mai aggiornato, e
                            // vince semplicemente l'ultimo oggetto disegnato
                            // indipendentemente da chi sia davanti o dietro
                            // (esattamente il sintomo "seleziona quello dietro").
    glDepthFunc(GL_LESS);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float aspect = h > 0 ? static_cast<float>(w) / static_cast<float>(h) : 1.0f;
    Mat4 view = m_camera.getViewMatrix();
    Mat4 proj = m_camera.getProjectionMatrix(aspect);

    std::unordered_map<ObjectId, Mat4> worldMatrices = computeWorldMatrices();

    for (const auto& [id, obj] : m_scene.getAllObjects()) {
        float r, g, b;
        idToColor(id, r, g, b);

        auto worldIt = worldMatrices.find(id);
        Mat4 worldMatrix = (worldIt != worldMatrices.end()) ? worldIt->second : obj.transform.getMatrix();

        std::shared_ptr<Mesh> mesh = getMeshForObject(obj);
        if (mesh && mesh->isValid()) {
            mesh->draw(worldMatrix, view, proj, r, g, b);
        } else if (obj.children.empty()) {
            m_renderer->drawCube(worldMatrix, view, proj, r, g, b);
        }
    }

    // Stessa identica logica di mappatura usata per renderizzare/visualizzare
    // questo frame: frazione 0..1 -> pixel nel framebuffer "appena disegnato"
    // (w,h passati dal chiamante = le dimensioni REALI di QUESTO framebuffer,
    // non quelle (potenzialmente diverse) del pannello UI di un frame diverso).
    // Stessa identica logica di mappatura usata per renderizzare/visualizzare
    // questo frame: frazione 0..1 -> pixel nel framebuffer "appena disegnato".
    int centerX = static_cast<int>(fractionX * static_cast<float>(w));
    int centerY = static_cast<int>((1.0f - fractionY) * static_cast<float>(h)); // flip: OpenGL ha origine in basso

    // Leggiamo un piccolo blocco (non un singolo pixel) e prendiamo l'id PIÙ
    // FREQUENTE al suo interno. Modelli con geometria sovrapposta/duplicata
    // (comune nei file .obj scaricati: pannelli doppi, interni nascosti)
    // possono soffrire di z-fighting, dove la GPU risolve le profondità
    // quasi-identiche in modo leggermente diverso tra la passata visibile e
    // quella di picking. Un voto di maggioranza su un'area piccola è molto
    // più stabile del singolo pixel.
    constexpr int kSampleRadius = 3;
    std::unordered_map<int, int> votes;

    for (int dy = -kSampleRadius; dy <= kSampleRadius; ++dy) {
        for (int dx = -kSampleRadius; dx <= kSampleRadius; ++dx) {
            int px = centerX + dx;
            int py = centerY + dy;
            if (px < 0 || px >= w || py < 0 || py >= h) continue;

            unsigned char pixel[3] = {0, 0, 0};
            glReadPixels(px, py, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, pixel);
            ObjectId id = colorToId(pixel[0], pixel[1], pixel[2]);
            if (id == kInvalidId) continue; // non contiamo i voti per "sfondo"

            votes[id]++;
        }
    }

    Framebuffer::unbind();
    glViewport(0, 0, m_window->width(), m_window->height());

    if (votes.empty()) return kInvalidId; // tutta l'area era sfondo

    int bestId = kInvalidId;
    int bestCount = 0;
    for (const auto& [id, count] : votes) {
        if (count > bestCount) {
            bestCount = count;
            bestId = id;
        }
    }
    return static_cast<ObjectId>(bestId);
}

void Engine::tick() {
    double now = Window::getTime();
    m_deltaTime = m_lastFrameTime > 0.0 ? static_cast<float>(now - m_lastFrameTime) : 0.0f;
    m_lastFrameTime = now;

    m_window->pollEvents();
    updateCameraInput();

    renderSceneToFramebuffer();

    // Dimensioni REALI con cui la scena è appena stata renderizzata in questo
    // frame (quelle che l'utente vedrà nell'immagine): il picking DEVE usare
    // esattamente queste, non quelle (potenzialmente diverse) che otterremo
    // dal pannello UI qualche riga più sotto, altrimenti il click si "scollega"
    // dall'immagine effettivamente mostrata.
    int renderedWidth = m_sceneFramebuffer->width();
    int renderedHeight = m_sceneFramebuffer->height();

    m_renderer->clear(0.05f, 0.05f, 0.06f, 1.0f);

    m_editorUI->beginFrame();
    EditorUI::FrameResult result = m_editorUI->drawPanels(
        m_scene, m_selectedObject, m_sceneFramebuffer->colorTexture(), m_projectPath);
    m_editorUI->endFrame();

    m_lastViewportWidth = result.viewportWidth;
    m_lastViewportHeight = result.viewportHeight;
    m_lastViewportHovered = result.viewportHovered;

    if (result.saveRequested) saveScene();
    if (result.openRequested) loadScene();
    if (result.quitRequested) m_window->requestClose();

    if (result.clickedInViewport) {
        m_selectedObject = pickObjectAt(result.clickFractionX, result.clickFractionY, renderedWidth, renderedHeight);

        if (const GameObject* picked = m_scene.getObject(m_selectedObject)) {
            std::cout << "[Picking] Frazione (" << result.clickFractionX << "," << result.clickFractionY
                       << ") su framebuffer " << renderedWidth << "x" << renderedHeight
                       << " -> selezionato \"" << picked->name << "\" (id=" << picked->id << ")\n";
        } else {
            std::cout << "[Picking] Frazione (" << result.clickFractionX << "," << result.clickFractionY
                       << ") -> sfondo vuoto, deselezionato\n";
        }
    }

    if (!result.droppedAssetPath.empty()) {
        std::string filename = fs::path(result.droppedAssetPath).filename().string();

        // Oggetto "contenitore": raggruppa nella Hierarchy tutti i pezzi del
        // file importato, ma non disegna nulla di suo (vedi renderSceneToFramebuffer).
        ObjectId rootId = m_scene.createObject(filename);

        std::vector<ObjGroup> groups;
        if (loadObjFileGrouped(result.droppedAssetPath, groups)) {
            for (auto& g : groups) {
                if (g.vertices.empty()) continue;

                ObjectId childId = m_scene.createObject(g.name, rootId);
                if (auto* child = m_scene.getObject(childId)) {
                    child->meshPath = result.droppedAssetPath;
                    child->groupName = g.name;
                }

                // Popoliamo subito la cache: evita di riparsare il file da capo
                // alla prima renderSceneToFramebuffer() del prossimo frame.
                m_meshCache[result.droppedAssetPath + "::" + g.name] = std::make_shared<Mesh>(g.vertices);
            }
        }

        m_selectedObject = rootId;
    }

    m_window->swapBuffers();
}

void Engine::setClearColor(float r, float g, float b, float a) {
    m_clearColor[0] = r;
    m_clearColor[1] = g;
    m_clearColor[2] = b;
    m_clearColor[3] = a;
}

bool Engine::isKeyPressed(int keyCode) const {
    return m_window->isKeyPressed(keyCode);
}

bool Engine::isMouseButtonPressed(int button) const {
    return m_window->isMouseButtonPressed(button);
}

void Engine::getMousePosition(double& x, double& y) const {
    m_window->getMousePosition(x, y);
}

void Engine::setTrianglePosition(float x, float y) {
    m_renderer->setOffset(x, y);
}

} // namespace engine
