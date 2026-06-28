#include "engine/Engine.hpp"
#include "engine/ObjLoader.hpp"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <filesystem>
#include <sstream>
#include <iostream>
#include <cmath>
#include <vector>
#include <algorithm>
#include <utility>

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

    // Trasforma una DIREZIONE (non un punto: ignora la traslazione) tramite
    // la parte di rotazione/scala 3x3 della matrice. Usata per calcolare dove
    // "guarda" una camera usando ESATTAMENTE la stessa rotazione completa
    // (X, Y, Z) applicata al corpo visivo dell'oggetto (Transform::getMatrix()),
    // invece di una formula semplificata solo yaw/pitch che potrebbe non
    // coincidere su tutti gli assi (in particolare il roll, asse Z).
    Vec3 transformDirection(const Mat4& m, const Vec3& v) {
        return Vec3{
            m.m[0] * v.x + m.m[4] * v.y + m.m[8] * v.z,
            m.m[1] * v.x + m.m[5] * v.y + m.m[9] * v.z,
            m.m[2] * v.x + m.m[6] * v.y + m.m[10] * v.z
        };
    }

    // Come transformDirection ma per un PUNTO (tiene conto anche della
    // traslazione, colonna 3 della matrice): usata per portare un centro
    // mesh locale in coordinate mondo.
    Vec3 transformPoint(const Mat4& m, const Vec3& p) {
        return Vec3{
            m.m[0] * p.x + m.m[4] * p.y + m.m[8] * p.z + m.m[12],
            m.m[1] * p.x + m.m[5] * p.y + m.m[9] * p.z + m.m[13],
            m.m[2] * p.x + m.m[6] * p.y + m.m[10] * p.z + m.m[14]
        };
    }

    // Calcola forward/right/up "di sguardo" di un oggetto usando la sua
    // rotazione COMPLETA (stessa composizione Rz*Ry*Rx di Transform::getMatrix()).
    void computeLookVectors(const Transform& transform, Vec3& outForward, Vec3& outRight, Vec3& outUp) {
        Mat4 rx = Mat4::rotateX(radians(transform.rotationDegrees.x));
        Mat4 ry = Mat4::rotateY(radians(transform.rotationDegrees.y));
        Mat4 rz = Mat4::rotateZ(radians(transform.rotationDegrees.z));
        Mat4 rot = rz * ry * rx;

        outForward = transformDirection(rot, Vec3{0.0f, 0.0f, 1.0f}).normalized();
        outRight = transformDirection(rot, Vec3{1.0f, 0.0f, 0.0f}).normalized();
        outUp = transformDirection(rot, Vec3{0.0f, 1.0f, 0.0f}).normalized();
    }

    // Proietta un punto mondo in coordinate pixel (convenzione "OpenGL": origine
    // in basso a sinistra, stessa usata da pickObjectAt), usando view*projection.
    void projectToPixel(const Vec3& worldPos, const Mat4& view, const Mat4& proj,
                        int viewportW, int viewportH, float& outX, float& outY) {
        Mat4 vp = proj * view;
        float x = worldPos.x, y = worldPos.y, z = worldPos.z;
        float clipX = vp.m[0] * x + vp.m[4] * y + vp.m[8] * z + vp.m[12];
        float clipY = vp.m[1] * x + vp.m[5] * y + vp.m[9] * z + vp.m[13];
        float clipW = vp.m[3] * x + vp.m[7] * y + vp.m[11] * z + vp.m[15];
        if (std::fabs(clipW) < 1e-6f) clipW = 1e-6f;
        float ndcX = clipX / clipW;
        float ndcY = clipY / clipW;
        outX = (ndcX * 0.5f + 0.5f) * static_cast<float>(viewportW);
        outY = (ndcY * 0.5f + 0.5f) * static_cast<float>(viewportH);
    }

    // Distanza minima (in pixel) tra il punto (px,py) e il segmento [a,b].
    float pointSegmentDistance2D(float px, float py, float ax, float ay, float bx, float by) {
        float dx = bx - ax, dy = by - ay;
        float lenSq = dx * dx + dy * dy;
        float t = lenSq > 1e-6f ? ((px - ax) * dx + (py - ay) * dy) / lenSq : 0.0f;
        t = std::max(0.0f, std::min(1.0f, t));
        float cx = ax + t * dx, cy = ay + t * dy;
        float ddx = px - cx, ddy = py - cy;
        return std::sqrt(ddx * ddx + ddy * ddy);
    }

    // Calcola gli assi mondo (X,Y,Z) di un collider Box/Capsula, combinando la
    // rotazione PROPRIA dell'oggetto (così il collider segue l'oggetto quando
    // questo ruota, es. una macchina che gira) con l'eventuale rotazione extra
    // del collider stesso (colliderRotation, per casi particolari).
    void getColliderWorldAxes(const Mat4& worldMatrix, const float colliderRotationDeg[3], Vec3 outAxes[3]) {
        Mat4 rx = Mat4::rotateX(radians(colliderRotationDeg[0]));
        Mat4 ry = Mat4::rotateY(radians(colliderRotationDeg[1]));
        Mat4 rz = Mat4::rotateZ(radians(colliderRotationDeg[2]));
        Mat4 ownRot = rz * ry * rx;

        Vec3 localX = transformDirection(ownRot, Vec3{1.0f, 0.0f, 0.0f});
        Vec3 localY = transformDirection(ownRot, Vec3{0.0f, 1.0f, 0.0f});
        Vec3 localZ = transformDirection(ownRot, Vec3{0.0f, 0.0f, 1.0f});

        outAxes[0] = transformDirection(worldMatrix, localX).normalized();
        outAxes[1] = transformDirection(worldMatrix, localY).normalized();
        outAxes[2] = transformDirection(worldMatrix, localZ).normalized();
    }

    // Test di separazione (SAT) tra due Box orientati (OBB): true se si
    // sovrappongono, false se esiste un asse che li separa. Tiene conto della
    // rotazione di entrambi, a differenza del semplice confronto AABB.
    bool obbOverlap(const Vec3& centerA, const Vec3 axisA[3], const float halfA[3],
                    const Vec3& centerB, const Vec3 axisB[3], const float halfB[3]) {
        Vec3 d = centerB - centerA;

        auto testAxis = [&](Vec3 axis) -> bool {
            float len = axis.length();
            if (len < 1e-6f) return true; // assi quasi paralleli: asse degenere, non separa

            axis = axis * (1.0f / len);
            float rA = std::fabs(Vec3::dot(axisA[0], axis)) * halfA[0]
                     + std::fabs(Vec3::dot(axisA[1], axis)) * halfA[1]
                     + std::fabs(Vec3::dot(axisA[2], axis)) * halfA[2];
            float rB = std::fabs(Vec3::dot(axisB[0], axis)) * halfB[0]
                     + std::fabs(Vec3::dot(axisB[1], axis)) * halfB[1]
                     + std::fabs(Vec3::dot(axisB[2], axis)) * halfB[2];
            float dist = std::fabs(Vec3::dot(d, axis));
            return dist <= (rA + rB); // true = si sovrappongono su questo asse (non è separante)
        };

        // 15 assi candidati: 3 facce di A, 3 facce di B, 9 prodotti incrociati
        for (int i = 0; i < 3; ++i) if (!testAxis(axisA[i])) return false;
        for (int i = 0; i < 3; ++i) if (!testAxis(axisB[i])) return false;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                if (!testAxis(Vec3::cross(axisA[i], axisB[j]))) return false;

        return true; // nessun asse separante trovato: i due OBB si toccano/sovrappongono
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
            ObjectId lightId = m_scene.createObject("Luce");
            if (auto* light = m_scene.getObject(lightId)) {
                light->isLight = true;
                light->transform.position = {5.0f, 8.0f, 5.0f};
            }
        }
    } else {
        m_scene.createObject("Cubo");
        ObjectId lightId = m_scene.createObject("Luce");
        if (auto* light = m_scene.getObject(lightId)) {
            light->isLight = true;
            light->transform.position = {5.0f, 8.0f, 5.0f};
        }
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

void Engine::getActiveCameraMatrices(float aspect, Mat4& outView, Mat4& outProj, Vec3& outEyePos) {
    if (m_isPlaying) {
        std::unordered_map<ObjectId, Mat4> worldMatrices = computeWorldMatrices();

        for (const auto& [id, obj] : m_scene.getAllObjects()) {
            if (obj.isCamera) {
                // IMPORTANTE: usiamo la matrice MONDO (genitore incluso), non
                // solo la trasformazione locale dell'oggetto — altrimenti una
                // camera "dentro" un altro oggetto (es. un personaggio) non
                // lo seguirebbe quando quello si muove/ruota.
                auto it = worldMatrices.find(id);
                Mat4 worldMatrix = (it != worldMatrices.end()) ? it->second : obj.transform.getMatrix();

                Vec3 eye{worldMatrix.m[12], worldMatrix.m[13], worldMatrix.m[14]};
                Vec3 forward = transformDirection(worldMatrix, Vec3{0.0f, 0.0f, 1.0f}).normalized();
                Vec3 up = transformDirection(worldMatrix, Vec3{0.0f, 1.0f, 0.0f}).normalized();
                Vec3 target = eye + forward;

                outView = Mat4::lookAt(eye, target, up);
                outProj = Mat4::perspective(radians(obj.cameraFov), aspect, 0.05f, 500.0f);
                outEyePos = eye;
                return; // usiamo la prima camera trovata
            }
        }
    }

    // Nessuna camera di gioco trovata (o siamo in Edit): usa quella orbitale.
    outView = m_camera.getViewMatrix();
    outProj = m_camera.getProjectionMatrix(aspect);
    outEyePos = m_camera.getEyePosition();
}

Vec3 Engine::getGizmoAnchorWorldPos(ObjectId id) {
    const GameObject* obj = m_scene.getObject(id);
    if (!obj) return Vec3{};

    std::unordered_map<ObjectId, Mat4> worldMatrices = computeWorldMatrices();
    auto it = worldMatrices.find(id);
    Mat4 worldMatrix = (it != worldMatrices.end()) ? it->second : obj->transform.getMatrix();

    std::shared_ptr<Mesh> mesh = getMeshForObject(*obj);
    if (mesh && mesh->isValid()) {
        // Ha una mesh: ancora al centro VISIVO della sua geometria, non alla
        // Transform (spesso a 0,0,0 per i pezzi importati da un .obj multi-oggetto).
        return transformPoint(worldMatrix, mesh->getLocalCenter());
    }

    // Nessuna mesh (luce, camera, vuoto, contenitore...): usa la Transform.
    return Vec3{worldMatrix.m[12], worldMatrix.m[13], worldMatrix.m[14]};
}

void Engine::computeGizmoScreenPositions(const Mat4& view, const Mat4& proj, int viewportW, int viewportH) {
    m_gizmoVisibleThisFrame = false;
    if (m_selectedObject == kInvalidId) return;

    const GameObject* obj = m_scene.getObject(m_selectedObject);
    if (!obj) return;

    Vec3 origin = getGizmoAnchorWorldPos(m_selectedObject);

    // Lunghezza del gizmo proporzionale alla distanza dalla camera: resta
    // leggibile sia da vicino che da lontano (altrimenti a distanza diventa
    // microscopico o, da vicino, esagerato).
    Vec3 eyePos = m_camera.getEyePosition();
    float dist = (origin - eyePos).length();
    float gizmoLen = std::max(0.5f, dist * 0.15f);

    static const Vec3 kAxisDirs[3] = {
        Vec3{1.0f, 0.0f, 0.0f},
        Vec3{0.0f, 1.0f, 0.0f},
        Vec3{0.0f, 0.0f, 1.0f}
    };

    projectToPixel(origin, view, proj, viewportW, viewportH, m_gizmoOriginPixelX, m_gizmoOriginPixelY);
    for (int i = 0; i < 3; ++i) {
        Vec3 tip = origin + kAxisDirs[i] * gizmoLen;
        projectToPixel(tip, view, proj, viewportW, viewportH, m_gizmoTipPixelX[i], m_gizmoTipPixelY[i]);
    }

    m_gizmoVisibleThisFrame = true;
}

void Engine::renderTransformGizmo(const Mat4& view, const Mat4& proj) {
    if (m_selectedObject == kInvalidId) return;
    const GameObject* obj = m_scene.getObject(m_selectedObject);
    if (!obj) return;

    Vec3 origin = getGizmoAnchorWorldPos(m_selectedObject);

    Vec3 eyePos = m_camera.getEyePosition();
    float dist = (origin - eyePos).length();
    float gizmoLen = std::max(0.5f, dist * 0.15f);

    static const Vec3 kAxisDirs[3] = {
        Vec3{1.0f, 0.0f, 0.0f},
        Vec3{0.0f, 1.0f, 0.0f},
        Vec3{0.0f, 0.0f, 1.0f}
    };
    // Colore base per asse (X=rosso, Y=verde, Z=blu), più acceso se in hover/drag.
    static const float kAxisColor[3][3] = {
        {0.9f, 0.2f, 0.2f},
        {0.2f, 0.9f, 0.2f},
        {0.25f, 0.45f, 0.95f}
    };

    for (int i = 0; i < 3; ++i) {
        Vec3 tip = origin + kAxisDirs[i] * gizmoLen;
        std::vector<float> line = {origin.x, origin.y, origin.z, tip.x, tip.y, tip.z};

        bool active = (m_gizmoDragAxis == i) || (m_gizmoDragAxis == -1 && m_gizmoHoverAxis == i);
        float r = kAxisColor[i][0], g = kAxisColor[i][1], b = kAxisColor[i][2];
        if (active) {
            r = r * 0.5f + 0.5f; g = g * 0.5f + 0.5f; b = b * 0.5f + 0.5f; // schiarisce per evidenziare
        }
        m_renderer->drawLines(line, view, proj, r, g, b);
    }
}

bool Engine::updateTransformGizmoInteraction(float mouseFractionX, float mouseFractionY,
                                              int viewportW, int viewportH, bool viewportHovered) {
    if (m_isPlaying) {
        m_gizmoDragAxis = -1;
        m_gizmoHoverAxis = -1;
        return false;
    }

    bool leftPressed = m_window->isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);
    bool justPressed = leftPressed && !m_gizmoLeftMouseWasPressed;
    bool justReleased = !leftPressed && m_gizmoLeftMouseWasPressed;
    m_gizmoLeftMouseWasPressed = leftPressed;

    if (justReleased) {
        m_gizmoDragAxis = -1;
    }

    if (!m_gizmoVisibleThisFrame || m_selectedObject == kInvalidId) {
        m_gizmoDragAxis = -1;
        m_gizmoHoverAxis = -1;
        return false;
    }

    float mousePixelX = mouseFractionX * static_cast<float>(viewportW);
    float mousePixelY = (1.0f - mouseFractionY) * static_cast<float>(viewportH); // bottom-up

    constexpr float kHitRadius = 14.0f; // pixel di tolleranza per "centrare" un asse

    // Già in drag: continua a muovere lungo l'asse scelto all'inizio,
    // indipendentemente dalla distanza attuale dall'asse stesso.
    if (m_gizmoDragAxis != -1) {
        if (!leftPressed) {
            m_gizmoDragAxis = -1;
            return false;
        }

        float screenDx = m_gizmoTipPixelX[m_gizmoDragAxis] - m_gizmoOriginPixelX;
        float screenDy = m_gizmoTipPixelY[m_gizmoDragAxis] - m_gizmoOriginPixelY;
        float screenLen = std::sqrt(screenDx * screenDx + screenDy * screenDy);
        if (screenLen < 1e-3f) return true; // asse praticamente invisibile a schermo (vista quasi parallela): ignora

        float axisDirX = screenDx / screenLen, axisDirY = screenDy / screenLen;
        float mouseDeltaX = mousePixelX - m_gizmoDragStartMousePixelX;
        float mouseDeltaY = mousePixelY - m_gizmoDragStartMousePixelY;
        float scalarPixels = mouseDeltaX * axisDirX + mouseDeltaY * axisDirY;

        // "gizmoLen" mondo corrisponde a "screenLen" pixel su quell'asse:
        // converte lo spostamento in pixel in unità mondo con questo rapporto.
        GameObject* obj = m_scene.getObject(m_selectedObject);
        if (obj) {
            Vec3 eyePos = m_camera.getEyePosition();
            Vec3 origin{m_gizmoDragStartPosition.x, m_gizmoDragStartPosition.y, m_gizmoDragStartPosition.z};
            float dist = (origin - eyePos).length();
            float gizmoLen = std::max(0.5f, dist * 0.15f);

            static const Vec3 kAxisDirs[3] = {
                Vec3{1.0f, 0.0f, 0.0f}, Vec3{0.0f, 1.0f, 0.0f}, Vec3{0.0f, 0.0f, 1.0f}
            };
            float worldDelta = scalarPixels * (gizmoLen / screenLen);
            Vec3 newPos = m_gizmoDragStartPosition + kAxisDirs[m_gizmoDragAxis] * worldDelta;
            obj->transform.position = newPos;
        }
        return true; // questo click/drag è "del gizmo": non toccare la selezione
    }

    // Non in drag: aggiorna solo l'hover (per evidenziare l'asse più vicino al cursore)
    m_gizmoHoverAxis = -1;
    if (viewportHovered) {
        float bestDist = kHitRadius;
        for (int i = 0; i < 3; ++i) {
            float d = pointSegmentDistance2D(mousePixelX, mousePixelY,
                                              m_gizmoOriginPixelX, m_gizmoOriginPixelY,
                                              m_gizmoTipPixelX[i], m_gizmoTipPixelY[i]);
            if (d < bestDist) {
                bestDist = d;
                m_gizmoHoverAxis = i;
            }
        }
    }

    if (justPressed && m_gizmoHoverAxis != -1) {
        GameObject* obj = m_scene.getObject(m_selectedObject);
        if (obj) {
            m_gizmoDragAxis = m_gizmoHoverAxis;
            m_gizmoDragStartMousePixelX = mousePixelX;
            m_gizmoDragStartMousePixelY = mousePixelY;
            m_gizmoDragStartPosition = obj->transform.position;
            return true; // consuma il click: non far partire la normale selezione
        }
    }

    return false;
}

void Engine::updateObjectBodyDrag(float mouseFractionX, float mouseFractionY, float aspect, bool viewportHovered) {
    if (m_isPlaying) {
        m_objectBodyDragActive = false;
        return;
    }

    bool leftPressed = m_window->isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);
    if (!leftPressed) {
        m_objectBodyDragActive = false;
        return;
    }

    if (!m_objectBodyDragActive) return; // nessun drag in corso, niente da fare

    GameObject* obj = m_scene.getObject(m_selectedObject);
    if (!obj) {
        m_objectBodyDragActive = false;
        return;
    }

    // Raggio dalla camera attraverso il mouse, intersecato col piano
    // orizzontale all'altezza ATTUALE dell'oggetto (così scorre alla sua
    // quota, non "cade" a terra). Aggiungiamo l'offset calcolato al momento
    // del click, così il movimento è relativo e non "scatta" sotto il cursore.
    Vec3 hit = computeDropWorldPosition(mouseFractionX, mouseFractionY, aspect, m_objectBodyDragPlaneY);
    obj->transform.position.x = hit.x + m_objectBodyDragOffsetX;
    obj->transform.position.z = hit.z + m_objectBodyDragOffsetZ;
}

void Engine::computeColliderGizmoScreenPositions(const Mat4& view, const Mat4& proj, int viewportW, int viewportH) {
    m_colliderGizmoVisibleThisFrame = false;
    if (m_selectedObject == kInvalidId) return;

    const GameObject* obj = m_scene.getObject(m_selectedObject);
    if (!obj || obj->colliderType == 0) return;

    std::unordered_map<ObjectId, Mat4> worldMatrices = computeWorldMatrices();
    auto it = worldMatrices.find(m_selectedObject);
    Mat4 wm = (it != worldMatrices.end()) ? it->second : obj->transform.getMatrix();
    Vec3 objWorldPos{wm.m[12], wm.m[13], wm.m[14]};
    Vec3 center = objWorldPos + Vec3{obj->colliderOffset[0], obj->colliderOffset[1], obj->colliderOffset[2]};

    Vec3 eyePos = m_camera.getEyePosition();
    float dist = (center - eyePos).length();
    float gizmoLen = std::max(0.5f, dist * 0.15f);

    static const Vec3 kAxisDirs[3] = {
        Vec3{1.0f, 0.0f, 0.0f}, Vec3{0.0f, 1.0f, 0.0f}, Vec3{0.0f, 0.0f, 1.0f}
    };

    projectToPixel(center, view, proj, viewportW, viewportH, m_colliderGizmoOriginPixelX, m_colliderGizmoOriginPixelY);
    for (int i = 0; i < 3; ++i) {
        Vec3 tip = center + kAxisDirs[i] * gizmoLen;
        projectToPixel(tip, view, proj, viewportW, viewportH, m_colliderGizmoTipPixelX[i], m_colliderGizmoTipPixelY[i]);
    }

    m_colliderGizmoVisibleThisFrame = true;
}

void Engine::renderColliderGizmo(const Mat4& view, const Mat4& proj) {
    if (m_selectedObject == kInvalidId) return;
    const GameObject* obj = m_scene.getObject(m_selectedObject);
    if (!obj || obj->colliderType == 0) return;

    std::unordered_map<ObjectId, Mat4> worldMatrices = computeWorldMatrices();
    auto it = worldMatrices.find(m_selectedObject);
    Mat4 wm = (it != worldMatrices.end()) ? it->second : obj->transform.getMatrix();
    Vec3 objWorldPos{wm.m[12], wm.m[13], wm.m[14]};
    Vec3 center = objWorldPos + Vec3{obj->colliderOffset[0], obj->colliderOffset[1], obj->colliderOffset[2]};

    Vec3 eyePos = m_camera.getEyePosition();
    float dist = (center - eyePos).length();
    float gizmoLen = std::max(0.5f, dist * 0.15f);

    static const Vec3 kAxisDirs[3] = {
        Vec3{1.0f, 0.0f, 0.0f}, Vec3{0.0f, 1.0f, 0.0f}, Vec3{0.0f, 0.0f, 1.0f}
    };
    // Stessi colori per asse del gizmo di posizione (X=rosso,Y=verde,Z=blu),
    // ma leggermente più scuri di base per distinguerlo visivamente da quello
    // della posizione quando entrambi sono visibili insieme.
    static const float kAxisColor[3][3] = {
        {0.75f, 0.15f, 0.15f},
        {0.15f, 0.75f, 0.15f},
        {0.2f, 0.35f, 0.8f}
    };

    for (int i = 0; i < 3; ++i) {
        Vec3 tip = center + kAxisDirs[i] * gizmoLen;
        std::vector<float> line = {center.x, center.y, center.z, tip.x, tip.y, tip.z};

        bool active = (m_colliderGizmoDragAxis == i) || (m_colliderGizmoDragAxis == -1 && m_colliderGizmoHoverAxis == i);
        float r = kAxisColor[i][0], g = kAxisColor[i][1], b = kAxisColor[i][2];
        if (active) {
            r = r * 0.5f + 0.5f; g = g * 0.5f + 0.5f; b = b * 0.5f + 0.5f;
        }
        m_renderer->drawLines(line, view, proj, r, g, b);
    }
}

bool Engine::updateColliderGizmoInteraction(float mouseFractionX, float mouseFractionY,
                                             int viewportW, int viewportH, bool viewportHovered) {
    bool leftPressed = m_window->isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);
    bool justPressed = leftPressed && !m_colliderGizmoLeftMouseWasPressed;
    bool justReleased = !leftPressed && m_colliderGizmoLeftMouseWasPressed;
    m_colliderGizmoLeftMouseWasPressed = leftPressed;

    if (justReleased) {
        m_colliderGizmoDragAxis = -1;
    }

    if (!m_showColliderGizmos || !m_colliderGizmoVisibleThisFrame || m_selectedObject == kInvalidId) {
        m_colliderGizmoDragAxis = -1;
        m_colliderGizmoHoverAxis = -1;
        return false;
    }

    float mousePixelX = mouseFractionX * static_cast<float>(viewportW);
    float mousePixelY = (1.0f - mouseFractionY) * static_cast<float>(viewportH);
    constexpr float kHitRadius = 14.0f;

    if (m_colliderGizmoDragAxis != -1) {
        if (!leftPressed) {
            m_colliderGizmoDragAxis = -1;
            return false;
        }

        float screenDx = m_colliderGizmoTipPixelX[m_colliderGizmoDragAxis] - m_colliderGizmoOriginPixelX;
        float screenDy = m_colliderGizmoTipPixelY[m_colliderGizmoDragAxis] - m_colliderGizmoOriginPixelY;
        float screenLen = std::sqrt(screenDx * screenDx + screenDy * screenDy);
        if (screenLen < 1e-3f) return true;

        float axisDirX = screenDx / screenLen, axisDirY = screenDy / screenLen;
        float mouseDeltaX = mousePixelX - m_colliderGizmoDragStartMousePixelX;
        float mouseDeltaY = mousePixelY - m_colliderGizmoDragStartMousePixelY;
        float scalarPixels = mouseDeltaX * axisDirX + mouseDeltaY * axisDirY;

        GameObject* obj = m_scene.getObject(m_selectedObject);
        if (obj) {
            std::unordered_map<ObjectId, Mat4> worldMatrices = computeWorldMatrices();
            auto it = worldMatrices.find(m_selectedObject);
            Mat4 wm = (it != worldMatrices.end()) ? it->second : obj->transform.getMatrix();
            Vec3 objWorldPos{wm.m[12], wm.m[13], wm.m[14]};
            Vec3 center = objWorldPos + m_colliderGizmoDragStartOffset;

            Vec3 eyePos = m_camera.getEyePosition();
            float dist = (center - eyePos).length();
            float gizmoLen = std::max(0.5f, dist * 0.15f);

            static const Vec3 kAxisDirs[3] = {
                Vec3{1.0f, 0.0f, 0.0f}, Vec3{0.0f, 1.0f, 0.0f}, Vec3{0.0f, 0.0f, 1.0f}
            };
            float worldDelta = scalarPixels * (gizmoLen / screenLen);
            Vec3 newOffset = m_colliderGizmoDragStartOffset + kAxisDirs[m_colliderGizmoDragAxis] * worldDelta;
            obj->colliderOffset[0] = newOffset.x;
            obj->colliderOffset[1] = newOffset.y;
            obj->colliderOffset[2] = newOffset.z;
        }
        return true;
    }

    m_colliderGizmoHoverAxis = -1;
    if (viewportHovered) {
        float bestDist = kHitRadius;
        for (int i = 0; i < 3; ++i) {
            float d = pointSegmentDistance2D(mousePixelX, mousePixelY,
                                              m_colliderGizmoOriginPixelX, m_colliderGizmoOriginPixelY,
                                              m_colliderGizmoTipPixelX[i], m_colliderGizmoTipPixelY[i]);
            if (d < bestDist) {
                bestDist = d;
                m_colliderGizmoHoverAxis = i;
            }
        }
    }

    if (justPressed && m_colliderGizmoHoverAxis != -1) {
        GameObject* obj = m_scene.getObject(m_selectedObject);
        if (obj) {
            m_colliderGizmoDragAxis = m_colliderGizmoHoverAxis;
            m_colliderGizmoDragStartMousePixelX = mousePixelX;
            m_colliderGizmoDragStartMousePixelY = mousePixelY;
            m_colliderGizmoDragStartOffset = Vec3{obj->colliderOffset[0], obj->colliderOffset[1], obj->colliderOffset[2]};
            return true;
        }
    }

    return false;
}

Vec3 Engine::computeDropWorldPosition(float fractionX, float fractionY, float aspect, float planeY) {
    // Ricostruiamo il raggio dalla camera attraverso il punto di rilascio
    // usando direttamente i vettori della camera orbitale (niente bisogno di
    // invertire matrici): forward/right/up + FOV/aspect bastano.
    Vec3 eye = m_camera.getEyePosition();
    Vec3 forward = (m_camera.target - eye).normalized();
    Vec3 worldUp{0.0f, 1.0f, 0.0f};
    Vec3 right = Vec3::cross(forward, worldUp).normalized();
    Vec3 up = Vec3::cross(right, forward).normalized();

    float ndcX = fractionX * 2.0f - 1.0f;       // -1 (sinistra) .. +1 (destra)
    float ndcY = 1.0f - fractionY * 2.0f;       // +1 (in alto) .. -1 (in basso)
    float tanHalfFov = std::tan(radians(m_camera.fovDegrees * 0.5f));

    Vec3 rayDir = (forward
                   + right * (ndcX * tanHalfFov * aspect)
                   + up * (ndcY * tanHalfFov)).normalized();

    // Interseca col piano orizzontale ad altezza 'planeY' (di default il
    // terreno, Y=0; il drag sul corpo passa invece l'altezza attuale
    // dell'oggetto, così scorre alla sua quota senza "cadere" a terra). Se il
    // raggio è quasi parallelo al piano o punta "all'indietro" (sopra
    // l'orizzonte), usa il target della camera come ripiego.
    const float kMinRayY = 1e-4f;
    if (std::fabs(rayDir.y) > kMinRayY) {
        float t = (planeY - eye.y) / rayDir.y;
        if (t > 0.0f && t < 1000.0f) {
            return eye + rayDir * t;
        }
    }
    return m_camera.target;
}

void Engine::renderSceneToFramebuffer() {
    m_sceneFramebuffer->resize(static_cast<int>(m_lastViewportWidth), static_cast<int>(m_lastViewportHeight));
    m_sceneFramebuffer->bind();

    m_renderer->clear(m_clearColor[0], m_clearColor[1], m_clearColor[2], m_clearColor[3]);

    float aspect = m_lastViewportHeight > 0.0f ? (m_lastViewportWidth / m_lastViewportHeight) : 1.0f;
    Mat4 view, proj;
    Vec3 eyePos;
    getActiveCameraMatrices(aspect, view, proj, eyePos);

    m_renderer->drawGrid(view, proj, eyePos);

    std::unordered_map<ObjectId, Mat4> worldMatrices = computeWorldMatrices();

    // Cerca il primo oggetto "luce" nella scena: la sua posizione/colore/
    // intensità (impostabili dall'Inspector) guidano l'illuminazione di tutta
    // la scena per questo frame. Se non c'è nessuna luce, usiamo un default
    // ragionevole così la scena non resta mai completamente al buio.
    Vec3 lightPos{10.0f, 20.0f, 10.0f};
    Vec3 lightColor{1.0f, 1.0f, 1.0f};
    float lightIntensity = 1.0f;
    const float ambient = 0.35f;

    for (const auto& [id, obj] : m_scene.getAllObjects()) {
        if (obj.isLight) {
            lightPos = obj.transform.position;
            lightColor = {obj.lightColor[0], obj.lightColor[1], obj.lightColor[2]};
            lightIntensity = obj.lightIntensity;
            break; // per ora una sola luce attiva: la prima trovata
        }
    }

    m_renderer->setGlobalLight(lightPos.x, lightPos.y, lightPos.z,
                                lightColor.x, lightColor.y, lightColor.z,
                                lightIntensity, ambient);
    Mesh::setGlobalLight(lightPos.x, lightPos.y, lightPos.z,
                          lightColor.x, lightColor.y, lightColor.z,
                          lightIntensity, ambient);

    for (const auto& [id, obj] : m_scene.getAllObjects()) {
        // In modalità Play la camera attiva non disegna il proprio corpo
        // (altrimenti ci si ritrova "dentro" la sua geometria). In Edit resta
        // visibile e selezionabile come qualsiasi altro oggetto.
        if (obj.isCamera && m_isPlaying) continue;

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

        // Gizmo "cono di visione": mostra quale porzione di scena la camera
        // inquadra, in base a FOV/direzione. Solo in Edit (in Play la camera
        // attiva è il punto di vista stesso, non avrebbe senso disegnarlo).
        if (obj.isCamera && !m_isPlaying) {
            // Stessa worldMatrix usata per disegnare l'oggetto: così il gizmo
            // segue correttamente anche la trasformazione di un eventuale
            // genitore (es. una camera "dentro" un personaggio).
            Vec3 eye{worldMatrix.m[12], worldMatrix.m[13], worldMatrix.m[14]};
            Vec3 forward = transformDirection(worldMatrix, Vec3{0.0f, 0.0f, 1.0f}).normalized();
            Vec3 right = transformDirection(worldMatrix, Vec3{1.0f, 0.0f, 0.0f}).normalized();
            Vec3 camUp = transformDirection(worldMatrix, Vec3{0.0f, 1.0f, 0.0f}).normalized();

            float gizmoDist = 3.0f; // lunghezza fissa del gizmo (non è il far plane reale)
            float gizmoAspect = (m_lastViewportHeight > 0.0f) ? (m_lastViewportWidth / m_lastViewportHeight) : (16.0f / 9.0f);
            float halfH = std::tan(radians(obj.cameraFov * 0.5f)) * gizmoDist;
            float halfW = halfH * gizmoAspect;

            Vec3 c1 = eye + forward * gizmoDist + camUp * halfH + right * halfW;
            Vec3 c2 = eye + forward * gizmoDist + camUp * halfH - right * halfW;
            Vec3 c3 = eye + forward * gizmoDist - camUp * halfH - right * halfW;
            Vec3 c4 = eye + forward * gizmoDist - camUp * halfH + right * halfW;

            std::vector<float> lines;
            auto addLine = [&](const Vec3& a, const Vec3& b2) {
                lines.push_back(a.x); lines.push_back(a.y); lines.push_back(a.z);
                lines.push_back(b2.x); lines.push_back(b2.y); lines.push_back(b2.z);
            };
            addLine(eye, c1);
            addLine(eye, c2);
            addLine(eye, c3);
            addLine(eye, c4);
            addLine(c1, c2);
            addLine(c2, c3);
            addLine(c3, c4);
            addLine(c4, c1);

            float gr = isSelected ? 1.0f : 0.95f;
            float gg = isSelected ? 0.85f : 0.85f;
            float gb = isSelected ? 0.1f : 0.2f;
            m_renderer->drawLines(lines, view, proj, gr, gg, gb);
        }

        // Gizmo del collider: mostra la forma usata per il rilevamento
        // collisioni (Engine::checkCollision), in arancione. Solo in Edit, e
        // solo se il toggle "Visualizza > Avanzate > Collisioni" è attivo.
        if (obj.colliderType != 0 && !m_isPlaying && m_showColliderGizmos) {
            Vec3 center = Vec3{worldMatrix.m[12], worldMatrix.m[13], worldMatrix.m[14]}
                        + Vec3{obj.colliderOffset[0], obj.colliderOffset[1], obj.colliderOffset[2]};
            std::vector<float> colliderLines;
            auto addColLine = [&](const Vec3& a, const Vec3& b) {
                colliderLines.push_back(a.x); colliderLines.push_back(a.y); colliderLines.push_back(a.z);
                colliderLines.push_back(b.x); colliderLines.push_back(b.y); colliderLines.push_back(b.z);
            };

            if (obj.colliderType == 1) {
                // Box: 12 spigoli del parallelepipedo. Gli assi seguono SIA la
                // rotazione dell'oggetto (es. la macchina che gira) SIA
                // l'eventuale rotazione extra propria del collider.
                float hx = obj.colliderBoxSize[0] * 0.5f, hy = obj.colliderBoxSize[1] * 0.5f, hz = obj.colliderBoxSize[2] * 0.5f;

                Vec3 axis[3];
                getColliderWorldAxes(worldMatrix, obj.colliderRotation, axis);
                auto rotOffset = [&](float x, float y, float z) { return axis[0]*x + axis[1]*y + axis[2]*z; };

                Vec3 c[8] = {
                    center + rotOffset(-hx,-hy,-hz), center + rotOffset( hx,-hy,-hz),
                    center + rotOffset( hx, hy,-hz), center + rotOffset(-hx, hy,-hz),
                    center + rotOffset(-hx,-hy, hz), center + rotOffset( hx,-hy, hz),
                    center + rotOffset( hx, hy, hz), center + rotOffset(-hx, hy, hz)
                };
                addColLine(c[0],c[1]); addColLine(c[1],c[2]); addColLine(c[2],c[3]); addColLine(c[3],c[0]);
                addColLine(c[4],c[5]); addColLine(c[5],c[6]); addColLine(c[6],c[7]); addColLine(c[7],c[4]);
                addColLine(c[0],c[4]); addColLine(c[1],c[5]); addColLine(c[2],c[6]); addColLine(c[3],c[7]);

            } else if (obj.colliderType == 2) {
                // Sfera: 3 cerchi ortogonali (XY, XZ, YZ)
                float r = obj.colliderSphereRadius;
                constexpr int kSegs = 24;
                for (int axis = 0; axis < 3; ++axis) {
                    Vec3 prev{};
                    for (int i = 0; i <= kSegs; ++i) {
                        float t = (static_cast<float>(i) / kSegs) * 2.0f * PI;
                        Vec3 p;
                        if (axis == 0) p = center + Vec3{0.0f, std::cos(t) * r, std::sin(t) * r};
                        else if (axis == 1) p = center + Vec3{std::cos(t) * r, 0.0f, std::sin(t) * r};
                        else p = center + Vec3{std::cos(t) * r, std::sin(t) * r, 0.0f};
                        if (i > 0) addColLine(prev, p);
                        prev = p;
                    }
                }

            } else if (obj.colliderType == 3) {
                // Capsula: due cerchi orizzontali alle estremità + 4 raccordi
                // verticali. Stessa combinazione rotazione oggetto + propria
                // del Box: l'asse "lungo" segue la macchina quando gira.
                float r = obj.colliderCapsuleRadius;
                float halfH = std::max(0.0f, obj.colliderCapsuleHeight * 0.5f - r);

                Vec3 axis[3];
                getColliderWorldAxes(worldMatrix, obj.colliderRotation, axis);
                Vec3 right = axis[0], up = axis[1], fwd = axis[2];

                Vec3 top = center + up * halfH;
                Vec3 bottom = center - up * halfH;
                constexpr int kSegs = 16;
                for (int which = 0; which < 2; ++which) {
                    Vec3 c = (which == 0) ? top : bottom;
                    Vec3 prev{};
                    for (int i = 0; i <= kSegs; ++i) {
                        float t = (static_cast<float>(i) / kSegs) * 2.0f * PI;
                        Vec3 p = c + right * (std::cos(t) * r) + fwd * (std::sin(t) * r);
                        if (i > 0) addColLine(prev, p);
                        prev = p;
                    }
                }
                for (int i = 0; i < 4; ++i) {
                    float t = (static_cast<float>(i) / 4.0f) * 2.0f * PI;
                    Vec3 offs = right * (std::cos(t) * r) + fwd * (std::sin(t) * r);
                    addColLine(top + offs, bottom + offs);
                }
            }

            m_renderer->drawLines(colliderLines, view, proj, 1.0f, 0.55f, 0.1f); // arancione
        }
    }

    // Gizmo di traslazione per l'oggetto selezionato: solo in Edit (in Play
    // non avrebbe senso, e comunque la Hierarchy/Inspector non sono visibili).
    if (!m_isPlaying) {
        computeGizmoScreenPositions(view, proj, static_cast<int>(m_lastViewportWidth), static_cast<int>(m_lastViewportHeight));
        renderTransformGizmo(view, proj);
        if (m_showColliderGizmos) {
            computeColliderGizmoScreenPositions(view, proj, static_cast<int>(m_lastViewportWidth), static_cast<int>(m_lastViewportHeight));
            renderColliderGizmo(view, proj);
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
    Mat4 view, proj;
    Vec3 eyePosUnused;
    getActiveCameraMatrices(aspect, view, proj, eyePosUnused);

    std::unordered_map<ObjectId, Mat4> worldMatrices = computeWorldMatrices();

    for (const auto& [id, obj] : m_scene.getAllObjects()) {
        if (obj.isCamera && m_isPlaying) continue; // stesso motivo del rendering visibile

        float r, g, b;
        idToColor(id, r, g, b);

        auto worldIt = worldMatrices.find(id);
        Mat4 worldMatrix = (worldIt != worldMatrices.end()) ? worldIt->second : obj.transform.getMatrix();

        std::shared_ptr<Mesh> mesh = getMeshForObject(obj);
        if (mesh && mesh->isValid()) {
            mesh->drawUnlit(worldMatrix, view, proj, r, g, b);
        } else if (obj.children.empty()) {
            m_renderer->drawCubeUnlit(worldMatrix, view, proj, r, g, b);
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

    // Consumati subito (l'evento del sistema operativo può arrivare in
    // qualsiasi momento), ma li useremo più sotto, dopo aver disegnato i
    // pannelli: solo allora sappiamo qual è la cartella Assets corrente.
    std::vector<std::string> externallyDroppedFiles = m_window->consumeDroppedFiles();

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
        m_scene, m_selectedObject, m_sceneFramebuffer->colorTexture(), m_projectPath, m_isPlaying);
    m_editorUI->endFrame();

    m_lastViewportWidth = result.viewportWidth;
    m_lastViewportHeight = result.viewportHeight;
    m_lastViewportHovered = result.viewportHovered;
    m_showColliderGizmos = result.showColliderGizmos;

    if (result.saveRequested) saveScene();
    if (result.openRequested) loadScene();
    if (result.quitRequested) m_window->requestClose();
    if (result.togglePlayRequested) {
        if (!m_isPlaying) {
            // Si sta per ENTRARE in Play: salva una copia della scena.
            m_playSnapshot = m_scene;
            m_isPlaying = true;
        } else {
            // Si sta per USCIRE dal Play: ripristina la copia salvata, così
            // tutto quello che gli script hanno modificato durante il gioco
            // (posizioni, rotazioni...) torna come prima di premere Play.
            m_scene = m_playSnapshot;
            m_isPlaying = false;
        }
    }

    // Il gizmo (posizione + maniglie collider) va interrogato SEMPRE (non solo
    // al click) per gestire bene il drag continuo: ritorna true se questo
    // frame "appartiene" al gizmo (hover, inizio drag, o drag in corso), nel
    // qual caso non si deve toccare la selezione con il normale color-picking.
    bool gizmoConsumedInput = updateTransformGizmoInteraction(
        result.viewportMouseFractionX, result.viewportMouseFractionY,
        renderedWidth, renderedHeight, result.viewportHovered);

    bool colliderHandleConsumedInput = updateColliderGizmoInteraction(
        result.viewportMouseFractionX, result.viewportMouseFractionY,
        renderedWidth, renderedHeight, result.viewportHovered);

    gizmoConsumedInput = gizmoConsumedInput || colliderHandleConsumedInput;

    // Continua il drag sul corpo (se attivo) anche nei frame successivi al
    // click, mentre il tasto resta premuto e il mouse si muove.
    float dragAspect = renderedHeight > 0 ? static_cast<float>(renderedWidth) / static_cast<float>(renderedHeight) : 1.0f;

    if (result.clickedInViewport && !gizmoConsumedInput) {
        m_selectedObject = pickObjectAt(result.clickFractionX, result.clickFractionY, renderedWidth, renderedHeight);

        if (m_selectedObject != kInvalidId) {
            // Avvia anche un drag "sul corpo": se l'utente continua a tenere
            // premuto e muove il mouse, l'oggetto scorre liberamente sul
            // piano orizzontale alla sua altezza attuale — un modo rapido di
            // spostarlo, in aggiunta al gizmo per il controllo asse-per-asse.
            if (const GameObject* picked = m_scene.getObject(m_selectedObject)) {
                m_objectBodyDragActive = true;
                m_objectBodyDragPlaneY = picked->transform.position.y;

                // Offset tra la posizione reale e il punto cliccato sul
                // terreno: senza questo, anche il semplice click selezionerebbe
                // E SPOSTEREBBE l'oggetto fino al punto esatto cliccato.
                Vec3 hitAtClick = computeDropWorldPosition(result.clickFractionX, result.clickFractionY,
                                                            dragAspect, m_objectBodyDragPlaneY);
                m_objectBodyDragOffsetX = picked->transform.position.x - hitAtClick.x;
                m_objectBodyDragOffsetZ = picked->transform.position.z - hitAtClick.z;
            }
        }

        if (const GameObject* picked = m_scene.getObject(m_selectedObject)) {
            std::cout << "[Picking] Frazione (" << result.clickFractionX << "," << result.clickFractionY
                       << ") su framebuffer " << renderedWidth << "x" << renderedHeight
                       << " -> selezionato \"" << picked->name << "\" (id=" << picked->id << ")\n";
        } else {
            std::cout << "[Picking] Frazione (" << result.clickFractionX << "," << result.clickFractionY
                       << ") -> sfondo vuoto, deselezionato\n";
        }
    }

    updateObjectBodyDrag(result.viewportMouseFractionX, result.viewportMouseFractionY, dragAspect, result.viewportHovered);

    if (result.draggedToSceneId != kInvalidId) {
        m_scene.setParent(result.draggedToSceneId, kInvalidId);
    }

    // File trascinati dentro l'editor da Esplora File (o altro programma del
    // sistema operativo): li copiamo nella cartella Assets attualmente
    // mostrata. Da lì l'utente può poi trascinarli nella Scena come già fa
    // con qualsiasi altro asset (drag&drop interno, già esistente).
    if (!externallyDroppedFiles.empty()) {
        std::string destFolder = !result.currentAssetsFolder.empty()
            ? result.currentAssetsFolder
            : (m_projectPath.empty() ? "assets" : (m_projectPath + "/assets"));

        std::error_code ec;
        fs::create_directories(destFolder, ec);

        for (const std::string& srcPath : externallyDroppedFiles) {
            std::error_code fileEc;
            if (!fs::is_regular_file(srcPath, fileEc)) {
                std::cout << "[Import] Ignorato (non è un file): " << srcPath << "\n";
                continue;
            }
            fs::path dst = fs::path(destFolder) / fs::path(srcPath).filename();
            fs::copy_file(srcPath, dst, fs::copy_options::overwrite_existing, fileEc);
            if (fileEc) {
                std::cout << "[Import] Errore copiando \"" << srcPath << "\": " << fileEc.message() << "\n";
            } else {
                std::cout << "[Import] Copiato in Assets: " << dst.string() << "\n";
            }
        }
    }

    if (!result.droppedAssetPath.empty()) {
        std::string filename = fs::path(result.droppedAssetPath).filename().string();

        // Calcola dove piazzare l'oggetto: un raggio dalla camera attraverso
        // il punto esatto del rilascio, intersecato col piano del terreno.
        // Senza questo, ogni oggetto importato finirebbe sempre nello stesso
        // punto (il target della camera), ammassandosi tutti insieme.
        float dropAspect = renderedHeight > 0 ? static_cast<float>(renderedWidth) / static_cast<float>(renderedHeight) : 1.0f;
        Vec3 dropPosition = computeDropWorldPosition(result.dropFractionX, result.dropFractionY, dropAspect);

        // Oggetto "contenitore": raggruppa nella Hierarchy tutti i pezzi del
        // file importato, ma non disegna nulla di suo (vedi renderSceneToFramebuffer).
        ObjectId rootId = m_scene.createObject(filename);
        if (auto* root = m_scene.getObject(rootId)) {
            root->transform.position = dropPosition;
        }

        std::vector<ObjGroup> groups;
        if (loadObjFileGrouped(result.droppedAssetPath, groups)) {
            for (auto& g : groups) {
                if (g.vertices.empty()) continue;

                ObjectId childId = m_scene.createObject(g.name, rootId);
                if (auto* child = m_scene.getObject(childId)) {
                    child->meshPath = result.droppedAssetPath;
                    child->groupName = g.name;
                    // Colore originale letto dal file .mtl (se presente), altrimenti
                    // resta il grigio di default: modificabile comunque dall'Inspector.
                    child->baseColor[0] = g.color[0];
                    child->baseColor[1] = g.color[1];
                    child->baseColor[2] = g.color[2];
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

bool Engine::checkCollision(ObjectId a, ObjectId b) {
    const GameObject* objA = m_scene.getObject(a);
    const GameObject* objB = m_scene.getObject(b);
    if (!objA || !objB) return false;
    if (objA->colliderType == 0 || objB->colliderType == 0) return false;

    std::unordered_map<ObjectId, Mat4> worldMatrices = computeWorldMatrices();

    auto getWorldMatrix = [&](const GameObject* obj, ObjectId id) -> Mat4 {
        auto it = worldMatrices.find(id);
        return (it != worldMatrices.end()) ? it->second : obj->transform.getMatrix();
    };
    auto getCenter = [&](const GameObject* obj, const Mat4& wm) -> Vec3 {
        return Vec3{wm.m[12], wm.m[13], wm.m[14]}
             + Vec3{obj->colliderOffset[0], obj->colliderOffset[1], obj->colliderOffset[2]};
    };

    Mat4 wmA = getWorldMatrix(objA, a);
    Mat4 wmB = getWorldMatrix(objB, b);
    Vec3 centerA = getCenter(objA, wmA);
    Vec3 centerB = getCenter(objB, wmB);

    // Box-Box: vero test OBB-OBB (Separating Axis Theorem), tiene conto della
    // rotazione di entrambi (sia quella dell'oggetto che l'eventuale extra
    // del collider stesso).
    if (objA->colliderType == 1 && objB->colliderType == 1) {
        Vec3 axisA[3], axisB[3];
        getColliderWorldAxes(wmA, objA->colliderRotation, axisA);
        getColliderWorldAxes(wmB, objB->colliderRotation, axisB);
        float halfA[3] = {objA->colliderBoxSize[0] * 0.5f, objA->colliderBoxSize[1] * 0.5f, objA->colliderBoxSize[2] * 0.5f};
        float halfB[3] = {objB->colliderBoxSize[0] * 0.5f, objB->colliderBoxSize[1] * 0.5f, objB->colliderBoxSize[2] * 0.5f};
        return obbOverlap(centerA, axisA, halfA, centerB, axisB, halfB);
    }

    // Sfera-Sfera: distanza ESATTA tra i centri (non influenzata dalla rotazione).
    if (objA->colliderType == 2 && objB->colliderType == 2) {
        float dist = (centerA - centerB).length();
        return dist <= (objA->colliderSphereRadius + objB->colliderSphereRadius);
    }

    // Box-Sfera (in entrambi gli ordini): punto più vicino sul Box ORIENTATO
    // (non più solo l'AABB) al centro della sfera, poi distanza — ESATTO,
    // tiene conto della rotazione del box.
    if ((objA->colliderType == 1 && objB->colliderType == 2) || (objA->colliderType == 2 && objB->colliderType == 1)) {
        bool aIsBox = (objA->colliderType == 1);
        const GameObject* boxObj = aIsBox ? objA : objB;
        const Mat4& boxWm = aIsBox ? wmA : wmB;
        Vec3 boxCenter = aIsBox ? centerA : centerB;
        const GameObject* sphereObj = aIsBox ? objB : objA;
        Vec3 sphereCenter = aIsBox ? centerB : centerA;

        Vec3 axis[3];
        getColliderWorldAxes(boxWm, boxObj->colliderRotation, axis);
        float half[3] = {boxObj->colliderBoxSize[0] * 0.5f, boxObj->colliderBoxSize[1] * 0.5f, boxObj->colliderBoxSize[2] * 0.5f};

        Vec3 d = sphereCenter - boxCenter;
        float local[3] = { Vec3::dot(d, axis[0]), Vec3::dot(d, axis[1]), Vec3::dot(d, axis[2]) };
        for (int i = 0; i < 3; ++i) {
            local[i] = std::max(-half[i], std::min(local[i], half[i]));
        }
        Vec3 closest = boxCenter + axis[0] * local[0] + axis[1] * local[1] + axis[2] * local[2];

        float dist = (closest - sphereCenter).length();
        return dist <= sphereObj->colliderSphereRadius;
    }

    // Qualsiasi caso che coinvolge una Capsula: resta l'approssimazione a
    // sfera avvolgente (raggio capsula + metà altezza) — meno precisa, ma la
    // Capsula non è coinvolta nel caso più comune (Box contro Box).
    auto boundingRadius = [&](const GameObject* obj) -> float {
        if (obj->colliderType == 1) {
            float hx = obj->colliderBoxSize[0] * 0.5f, hy = obj->colliderBoxSize[1] * 0.5f, hz = obj->colliderBoxSize[2] * 0.5f;
            return std::sqrt(hx * hx + hy * hy + hz * hz);
        } else if (obj->colliderType == 2) {
            return obj->colliderSphereRadius;
        } else if (obj->colliderType == 3) {
            return obj->colliderCapsuleRadius + obj->colliderCapsuleHeight * 0.5f;
        }
        return 0.5f;
    };
    float radiusA = boundingRadius(objA);
    float radiusB = boundingRadius(objB);
    float dist = (centerA - centerB).length();
    return dist <= (radiusA + radiusB);
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
