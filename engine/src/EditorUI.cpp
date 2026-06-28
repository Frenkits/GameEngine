#include "engine/EditorUI.hpp"

#include <imgui.h>
#include <imgui_internal.h> // necessario per le funzioni ImGui::DockBuilder*
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <filesystem>
#include <fstream>
#include <cstdio>
#include <sstream>
#include <algorithm>
#include <vector>
#include <cctype>
#include <cstdlib>

namespace fs = std::filesystem;

namespace engine {

namespace {
    constexpr const char* kDockspaceName = "EditorDockspace";
    constexpr const char* kHierarchyName = "Hierarchy";
    constexpr const char* kInspectorName = "Inspector";
    constexpr const char* kSceneName = "Scena";
    constexpr const char* kAssetsName = "Assets";

    // Apre un file con il programma predefinito del sistema (es. Notepad/VS
    // Code per i .py su Windows, in base alle associazioni file dell'utente).
    void openFileInDefaultEditor(const std::string& path) {
#ifdef _WIN32
        std::string command = "start \"\" \"" + path + "\"";
#elif __APPLE__
        std::string command = "open \"" + path + "\"";
#else
        std::string command = "xdg-open \"" + path + "\"";
#endif
        std::system(command.c_str());
    }
}

EditorUI::EditorUI(GLFWwindow* windowHandle) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(windowHandle, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");
}

EditorUI::~EditorUI() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void EditorUI::beginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void EditorUI::endFrame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

// Costruisce la disposizione dei pannelli SOLO la prima volta che il
// dockspace viene creato (se esiste già un imgui.ini con un layout salvato,
// ImGui lo ripristina da solo e questa funzione non viene richiamata).
void EditorUI::setupDockingLayout() {
    ImGuiID dockspaceId = ImGui::GetID(kDockspaceName);

    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->Size);

    ImGuiID center = dockspaceId;
    ImGuiID left, right, bottom;

    left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.22f, nullptr, &center);
    right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.25f, nullptr, &center);
    bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.28f, nullptr, &center);

    ImGui::DockBuilderDockWindow(kHierarchyName, left);
    ImGui::DockBuilderDockWindow(kInspectorName, right);
    ImGui::DockBuilderDockWindow(kAssetsName, bottom);
    ImGui::DockBuilderDockWindow(kSceneName, center);

    ImGui::DockBuilderFinish(dockspaceId);
}

void EditorUI::drawMenuBar(FrameResult& result, bool isPlaying) {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Salva Scena", "Ctrl+S")) {
                result.saveRequested = true;
            }
            if (ImGui::MenuItem("Apri Scena")) {
                result.openRequested = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Esci")) {
                result.quitRequested = true;
            }
            ImGui::EndMenu();
        }

        ImGui::Separator();
        if (!isPlaying) {
            if (ImGui::MenuItem("> PLAY")) {
                result.togglePlayRequested = true;
            }
        } else {
            if (ImGui::MenuItem("[] STOP")) {
                result.togglePlayRequested = true;
            }
        }

        ImGui::EndMenuBar();
    }

    ImGuiIO& io = ImGui::GetIO();
    bool ctrl = io.KeyCtrl;
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
        result.saveRequested = true;
    }
}

void EditorUI::handleShortcuts(Scene& scene, ObjectId& selectedId) {
    if (ImGui::GetIO().WantTextInput) return;
    if (selectedId == kInvalidId) return;

    if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        scene.destroyObject(selectedId);
        selectedId = kInvalidId;
        return;
    }

    bool ctrl = ImGui::GetIO().KeyCtrl;
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_D)) {
        selectedId = scene.duplicateObject(selectedId);
    }
}

EditorUI::FrameResult EditorUI::drawPanels(Scene& scene, ObjectId& selectedId,
                                            unsigned int sceneTextureId, const std::string& projectPath,
                                            bool isPlaying) {
    FrameResult result;

    // Durante il Play non creiamo affatto la finestra "contenitore" dell'editor
    // (quella che ospita la menu bar e il dockspace): evita qualunque rischio
    // che il suo sfondo (anche solo semi-trasparente) finisca sopra la vista
    // di gioco per questioni di ordine di disegno tra finestre ImGui.
    if (isPlaying) {
        drawPlayWindow(result, sceneTextureId);
        return result;
    }

    // --- Dockspace a schermo intero, con menu bar ---
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags hostFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_MenuBar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("EditorRoot", nullptr, hostFlags);
    ImGui::PopStyleVar(3);

    drawMenuBar(result, isPlaying);

    ImGuiID dockspaceId = ImGui::GetID(kDockspaceName);
    if (!m_dockingLayoutBuilt) {
        setupDockingLayout();
        m_dockingLayoutBuilt = true;
    }
    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

    ImGui::End(); // EditorRoot

    handleShortcuts(scene, selectedId);
    drawHierarchyWindow(scene, selectedId);
    drawInspectorWindow(scene, selectedId);
    drawSceneWindow(result, sceneTextureId);
    drawAssetsWindow(result, projectPath);

    return result;
}

void EditorUI::drawHierarchyNode(Scene& scene, ObjectId id, ObjectId& selectedId) {
    GameObject* obj = scene.getObject(id);
    if (!obj) return;

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (obj->children.empty()) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (id == selectedId) flags |= ImGuiTreeNodeFlags_Selected;

    bool opened = ImGui::TreeNodeEx((void*)(intptr_t)id, flags, "%s", obj->name.c_str());
    if (ImGui::IsItemClicked()) {
        selectedId = id;
    }

    // Sorgente del drag: trascina questo oggetto per riassegnargli un nuovo
    // genitore (rilascialo su un altro nodo, o nella zona vuota/Scena per
    // "staccarlo" e renderlo di nuovo un oggetto di primo livello).
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        ImGui::SetDragDropPayload("HIERARCHY_OBJECT_ID", &id, sizeof(ObjectId));
        ImGui::Text("%s", obj->name.c_str());
        ImGui::EndDragDropSource();
    }

    // Destinazione del drag: rilasciando qui un altro oggetto della Hierarchy,
    // diventa figlio di QUESTO nodo (es. metti una Camera dentro un personaggio
    // per farla muovere insieme a lui).
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_OBJECT_ID")) {
            ObjectId draggedId = *static_cast<const ObjectId*>(payload->Data);
            m_hasPendingReparent = true;
            m_pendingReparentChild = draggedId;
            m_pendingReparentNewParent = id;
        }
        ImGui::EndDragDropTarget();
    }

    if (opened && !obj->children.empty()) {
        for (ObjectId childId : obj->children) {
            drawHierarchyNode(scene, childId, selectedId);
        }
        ImGui::TreePop();
    }
}

void EditorUI::drawHierarchyWindow(Scene& scene, ObjectId& selectedId) {
    ImGui::Begin(kHierarchyName);

    if (ImGui::Button("+ Vuoto")) {
        selectedId = scene.createObject("GameObject");
    }
    ImGui::SameLine();
    if (ImGui::Button("+ Cubo")) {
        selectedId = scene.createObject("Cubo");
    }
    ImGui::SameLine();
    if (ImGui::Button("+ Luce")) {
        ObjectId id = scene.createObject("Luce");
        if (auto* obj = scene.getObject(id)) {
            obj->isLight = true;
            obj->transform.position = {5.0f, 8.0f, 5.0f}; // posizione di default: in alto, ben visibile
        }
        selectedId = id;
    }
    ImGui::SameLine();
    if (ImGui::Button("+ Camera")) {
        ObjectId id = scene.createObject("Camera");
        if (auto* obj = scene.getObject(id)) {
            obj->isCamera = true;
            obj->transform.position = {0.0f, 2.0f, 8.0f};
        }
        selectedId = id;
    }

    if (ImGui::Button("Duplica (Ctrl+D)") && selectedId != kInvalidId) {
        selectedId = scene.duplicateObject(selectedId);
    }
    ImGui::SameLine();
    if (ImGui::Button("Elimina (Canc)") && selectedId != kInvalidId) {
        scene.destroyObject(selectedId);
        selectedId = kInvalidId;
    }

    ImGui::Separator();
    ImGui::TextDisabled("%zu oggetti nella scena", scene.count());
    ImGui::Separator();

    for (ObjectId rootId : scene.getRootObjects()) {
        drawHierarchyNode(scene, rootId, selectedId);
    }

    // Zona vuota in fondo alla lista: trascinando qui un oggetto lo si
    // "stacca" da qualsiasi genitore (torna un oggetto di primo livello).
    ImGui::Dummy(ImVec2(-1, 40));
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_OBJECT_ID")) {
            ObjectId draggedId = *static_cast<const ObjectId*>(payload->Data);
            m_hasPendingReparent = true;
            m_pendingReparentChild = draggedId;
            m_pendingReparentNewParent = kInvalidId;
        }
        ImGui::EndDragDropTarget();
    }
    ImGui::TextDisabled("(trascina qui un oggetto per togliergli il genitore)");

    // Applica ORA l'eventuale riassegno richiesto: l'intero albero è già
    // stato disegnato, quindi modificare le liste children/m_rootObjects è sicuro.
    if (m_hasPendingReparent) {
        scene.setParent(m_pendingReparentChild, m_pendingReparentNewParent);
        m_hasPendingReparent = false;
    }

    ImGui::End();
}

void EditorUI::drawInspectorWindow(Scene& scene, ObjectId selectedId) {
    ImGui::Begin(kInspectorName);

    GameObject* obj = scene.getObject(selectedId);
    if (!obj) {
        ImGui::TextDisabled("Nessun oggetto selezionato");
        ImGui::TextDisabled("Seleziona un oggetto nella Hierarchy");
        ImGui::End();
        return;
    }

    ImGui::Text("Id oggetto: %d", obj->id);
    ImGui::Separator();

    char nameBuf[128];
    snprintf(nameBuf, sizeof(nameBuf), "%s", obj->name.c_str());
    if (ImGui::InputText("Nome", nameBuf, sizeof(nameBuf))) {
        obj->name = nameBuf;
    }

    ImGui::Separator();
    ImGui::Text("Transform");

    ImGui::DragFloat3("Posizione", &obj->transform.position.x, 0.05f);
    ImGui::DragFloat3("Rotazione", &obj->transform.rotationDegrees.x, 1.0f);
    ImGui::DragFloat3("Scala", &obj->transform.scale.x, 0.05f, 0.01f, 100.0f);

    ImGui::Separator();
    ImGui::Text("Materiale");
    ImGui::ColorEdit3("Colore", obj->baseColor);

    ImGui::Separator();
    ImGui::Checkbox("E' una sorgente di luce", &obj->isLight);
    if (obj->isLight) {
        ImGui::ColorEdit3("Colore luce", obj->lightColor);
        ImGui::DragFloat("Intensita'", &obj->lightIntensity, 0.05f, 0.0f, 10.0f);
        ImGui::TextWrapped("La posizione di questo oggetto (Transform sopra) determina da dove arriva la luce.");
    }

    ImGui::Separator();
    ImGui::Checkbox("E' una camera di gioco", &obj->isCamera);
    if (obj->isCamera) {
        ImGui::DragFloat("FOV (campo visivo)", &obj->cameraFov, 0.5f, 10.0f, 120.0f);
        ImGui::TextWrapped("In modalita' Play, la prima camera trovata nella scena sostituisce "
                           "la camera orbitale dell'editor. Posizione/Rotazione (Transform sopra) "
                           "= dove si trova e dove guarda.");
    }

    ImGui::Separator();
    ImGui::Text("Script Python");
    if (ImGui::Button(obj->scriptPath.empty() ? "(trascina qui un file .py)" : obj->scriptPath.c_str(),
                       ImVec2(-1, 0)) && !obj->scriptPath.empty()) {
        openFileInDefaultEditor(obj->scriptPath);
    }
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_FILE_PATH")) {
            std::string path(static_cast<const char*>(payload->Data));
            if (path.size() >= 3 && path.substr(path.size() - 3) == ".py") {
                obj->scriptPath = path;
            }
        }
        ImGui::EndDragDropTarget();
    }
    if (!obj->scriptPath.empty() && ImGui::Button("Rimuovi script")) {
        obj->scriptPath.clear();
    }
    ImGui::TextWrapped("Lo script viene eseguito automaticamente durante il Play "
                       "(funzioni on_start/on_update). Crealo dal pannello Assets ('+ Script').");

    ImGui::Separator();
    ImGui::Text("Collisione");
    const char* colliderNames[] = {"Nessuna", "Box", "Sfera", "Capsula"};
    if (ImGui::Combo("Tipo collider", &obj->colliderType, colliderNames, 4)) {
        // niente da fare oltre all'assegnazione: il Combo scrive già su obj->colliderType
    }
    if (obj->colliderType != 0) {
        ImGui::DragFloat3("Centro (offset)", obj->colliderOffset, 0.05f);
    }
    if (obj->colliderType == 1 || obj->colliderType == 3) {
        ImGui::DragFloat3("Rotazione collider", obj->colliderRotation, 1.0f);
        ImGui::TextWrapped("Si combina con la rotazione dell'oggetto: se l'oggetto gira "
                           "(es. uno script che lo ruota), il collider gira con lui. "
                           "Usa questo campo solo per un disallineamento extra rispetto al corpo.");
    }
    if (obj->colliderType == 1) {
        ImGui::DragFloat3("Dimensioni Box", obj->colliderBoxSize, 0.05f, 0.01f, 100.0f);
    } else if (obj->colliderType == 2) {
        ImGui::DragFloat("Raggio Sfera", &obj->colliderSphereRadius, 0.05f, 0.01f, 50.0f);
    } else if (obj->colliderType == 3) {
        ImGui::DragFloat("Raggio Capsula", &obj->colliderCapsuleRadius, 0.05f, 0.01f, 50.0f);
        ImGui::DragFloat("Altezza Capsula", &obj->colliderCapsuleHeight, 0.05f, 0.01f, 100.0f);
    }
    if (obj->colliderType != 0) {
        ImGui::TextWrapped("Il gizmo arancione mostra la forma usata per le collisioni "
                           "(engine.check_collision(id_a, id_b) negli script). Le freccette "
                           "colorate al centro spostano il collider rispetto all'oggetto.");
    }

    ImGui::Separator();
    if (obj->meshPath.empty()) {
        ImGui::TextDisabled("Mesh: (cubo segnaposto)");
    } else {
        ImGui::Text("Mesh: %s", obj->meshPath.c_str());
        if (!obj->groupName.empty()) {
            ImGui::Text("Gruppo: %s", obj->groupName.c_str());
        }
        if (ImGui::Button("Rimuovi mesh (torna a cubo)")) {
            obj->meshPath.clear();
            obj->groupName.clear();
        }

        char excludeBuf[256];
        snprintf(excludeBuf, sizeof(excludeBuf), "%s", obj->excludedGroups.c_str());
        ImGui::TextWrapped("Escludi gruppi OBJ (solo per mesh non importate a pezzi):");
        if (ImGui::InputText("##ExcludeGroups", excludeBuf, sizeof(excludeBuf))) {
            obj->excludedGroups = excludeBuf;
        }
    }

    ImGui::Separator();
    if (ImGui::Button("Reset Transform")) {
        obj->transform = Transform{};
    }

    ImGui::End();
}

void EditorUI::drawViewportImageAndInput(FrameResult& result, unsigned int sceneTextureId) {
    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x < 1.0f) avail.x = 1.0f;
    if (avail.y < 1.0f) avail.y = 1.0f;

    result.viewportWidth = avail.x;
    result.viewportHeight = avail.y;
    result.viewportHovered = ImGui::IsWindowHovered();

    ImVec2 imageScreenPos = ImGui::GetCursorScreenPos(); // angolo in alto a sx dell'immagine, coord. schermo

    // Posizione del mouse SEMPRE (non solo al click): serve al gizmo di
    // trasformazione per il drag continuo (Engine lo usa per il calcolo,
    // qui ci limitiamo a riportare il dato).
    ImVec2 currentMouse = ImGui::GetMousePos();
    result.viewportMouseFractionX = (currentMouse.x - imageScreenPos.x) / avail.x;
    result.viewportMouseFractionY = (currentMouse.y - imageScreenPos.y) / avail.y;

    // Flip verticale (V invertita) perché la texture OpenGL ha origine in
    // basso a sinistra, mentre ImGui disegna le immagini con origine in alto.
    ImGui::Image((ImTextureID)(intptr_t)sceneTextureId, avail, ImVec2(0, 1), ImVec2(1, 0));

    // Click sinistro = selezione oggetto (color picking, gestito da Engine).
    // Il tasto destro resta libero per orbitare la camera, quindi nessun conflitto.
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        ImVec2 mouse = ImGui::GetMousePos();
        float localX = mouse.x - imageScreenPos.x;
        float localY = mouse.y - imageScreenPos.y; // origine in alto, Y verso il basso

        // IMPORTANTE: salviamo la posizione del click come FRAZIONE (0..1)
        // della dimensione del pannello, non come pixel assoluti: vedi
        // commento più dettagliato nella cronologia, in breve evita
        // disallineamenti se il framebuffer ha dimensioni leggermente diverse
        // dal pannello UI in un dato frame.
        result.clickedInViewport = true;
        result.clickFractionX = localX / avail.x;
        result.clickFractionY = localY / avail.y; // 0 = in alto, 1 = in basso
    }

    // Punto di rilascio per il drag&drop: trascinando un file dal pannello
    // Assets e lasciandolo qui sopra, si crea un nuovo GameObject con quella mesh.
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_FILE_PATH")) {
            result.droppedAssetPath = std::string(static_cast<const char*>(payload->Data));

            // Posizione esatta del rilascio (non sempre disponibile da
            // GetMousePos() durante un drop ImGui: usiamo la posizione
            // corrente del mouse, che in pratica coincide col punto di rilascio).
            ImVec2 mouse = ImGui::GetMousePos();
            float localX = mouse.x - imageScreenPos.x;
            float localY = mouse.y - imageScreenPos.y;
            result.dropFractionX = localX / avail.x;
            result.dropFractionY = localY / avail.y;
        }
        // Un oggetto della Hierarchy trascinato sulla Scena 3D: lo "stacca"
        // da qualsiasi genitore (stesso effetto della zona vuota in fondo
        // alla Hierarchy), comodo se preferisci trascinare direttamente nel
        // viewport invece di scorrere fino in fondo alla lista.
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_OBJECT_ID")) {
            result.draggedToSceneId = *static_cast<const ObjectId*>(payload->Data);
        }
        ImGui::EndDragDropTarget();
    }
}

void EditorUI::drawSceneWindow(FrameResult& result, unsigned int sceneTextureId) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin(kSceneName);

    drawViewportImageAndInput(result, sceneTextureId);

    ImGui::End();
    ImGui::PopStyleVar();
}

void EditorUI::drawPlayWindow(FrameResult& result, unsigned int sceneTextureId) {
    // Finestra a schermo intero (non dockabile): è la "finestra di gioco".
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("##PlayView", nullptr, flags);

    drawViewportImageAndInput(result, sceneTextureId);

    // Bottone "Stop" flottante sopra l'immagine (disegnato dopo, quindi
    // visivamente in primo piano nella stessa finestra).
    ImGui::SetCursorScreenPos(ImVec2(ImGui::GetWindowPos().x + 12, ImGui::GetWindowPos().y + 12));
    if (ImGui::Button("[] STOP")) {
        result.togglePlayRequested = true;
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

void EditorUI::drawAssetBreadcrumb(const std::string& assetsRoot) {
    // "Assets" (radice) + un bottone per ogni segmento del percorso corrente,
    // cliccabile per saltare direttamente a quel livello (come in Unreal).
    if (ImGui::SmallButton("Assets")) {
        m_assetCurrentRelPath.clear();
    }

    if (!m_assetCurrentRelPath.empty()) {
        std::string accumulated;
        std::stringstream ss(m_assetCurrentRelPath);
        std::string segment;
        while (std::getline(ss, segment, '/')) {
            if (segment.empty()) continue;
            ImGui::SameLine();
            ImGui::TextUnformatted("/");
            ImGui::SameLine();

            accumulated = accumulated.empty() ? segment : (accumulated + "/" + segment);
            ImGui::PushID(accumulated.c_str());
            if (ImGui::SmallButton(segment.c_str())) {
                m_assetCurrentRelPath = accumulated;
            }
            ImGui::PopID();
        }
    }
}

void EditorUI::drawAssetGridItem(const std::string& fullPath, const std::string& displayName, bool isFolder) {
    constexpr float kIconSize = 64.0f;
    ImGui::PushID(fullPath.c_str());

    ImVec2 cursorStart = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // "Icona": un rettangolo colorato (cartella = oro, .obj = blu, altro = grigio).
    // Niente thumbnail vere per ora: nessun loader di immagini nel motore ancora.
    ImVec4 iconColor = isFolder ? ImVec4(0.85f, 0.7f, 0.25f, 1.0f)
                      : (fullPath.size() > 4 && fullPath.substr(fullPath.size() - 4) == ".obj")
                          ? ImVec4(0.3f, 0.55f, 0.9f, 1.0f)
                      : (fullPath.size() > 3 && fullPath.substr(fullPath.size() - 3) == ".py")
                          ? ImVec4(0.3f, 0.75f, 0.35f, 1.0f)
                          : ImVec4(0.45f, 0.45f, 0.48f, 1.0f);

    bool clicked = ImGui::InvisibleButton("##icon", ImVec2(kIconSize, kIconSize));
    bool doubleClicked = ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
    bool hovered = ImGui::IsItemHovered();

    drawList->AddRectFilled(cursorStart, ImVec2(cursorStart.x + kIconSize, cursorStart.y + kIconSize),
                             ImGui::ColorConvertFloat4ToU32(iconColor), 6.0f);
    if (hovered) {
        drawList->AddRect(cursorStart, ImVec2(cursorStart.x + kIconSize, cursorStart.y + kIconSize),
                           ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, 0.6f)), 6.0f, 0, 2.0f);
    }

    // Drag&drop: solo i file (non le cartelle) possono essere trascinati nella Scena.
    if (!isFolder && ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        ImGui::SetDragDropPayload("ASSET_FILE_PATH", fullPath.c_str(), fullPath.size() + 1);
        ImGui::Text("%s", displayName.c_str());
        ImGui::EndDragDropSource();
    }

    // Nome sotto l'icona, troncato se troppo lungo, oppure campo di editing se in rinomina
    if (m_assetRenamingPath == fullPath) {
        ImGui::SetNextItemWidth(kIconSize);
        ImGui::SetKeyboardFocusHere();
        if (ImGui::InputText("##rename", m_assetRenameBuffer, sizeof(m_assetRenameBuffer),
                              ImGuiInputTextFlags_EnterReturnsTrue)) {
            std::error_code ec;
            fs::path newPath = fs::path(fullPath).parent_path() / m_assetRenameBuffer;
            fs::rename(fullPath, newPath, ec);
            m_assetRenamingPath.clear();
        }
        if (ImGui::IsItemDeactivated() && !ImGui::IsItemDeactivatedAfterEdit()) {
            m_assetRenamingPath.clear(); // Esc o click altrove: annulla
        }
    } else {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + kIconSize);
        ImGui::TextWrapped("%s", displayName.c_str());
        ImGui::PopTextWrapPos();
    }

    // Menu contestuale (tasto destro)
    if (ImGui::BeginPopupContextItem("##ctx")) {
        if (ImGui::MenuItem("Rinomina")) {
            m_assetRenamingPath = fullPath;
            snprintf(m_assetRenameBuffer, sizeof(m_assetRenameBuffer), "%s", displayName.c_str());
        }
        if (ImGui::MenuItem("Elimina")) {
            m_assetPendingDelete = fullPath;
        }
        ImGui::EndPopup();
    }

    if (isFolder && doubleClicked) {
        m_assetCurrentRelPath = m_assetCurrentRelPath.empty() ? displayName : (m_assetCurrentRelPath + "/" + displayName);
    } else if (!isFolder && doubleClicked) {
        openFileInDefaultEditor(fullPath);
    }
    (void)clicked;

    ImGui::PopID();
}

void EditorUI::drawAssetsWindow(FrameResult& result, const std::string& projectPath) {
    ImGui::Begin(kAssetsName);

    std::string assetsRoot = projectPath.empty() ? "assets" : (projectPath + "/assets");
    std::string currentDir = m_assetCurrentRelPath.empty() ? assetsRoot : (assetsRoot + "/" + m_assetCurrentRelPath);
    result.currentAssetsFolder = currentDir;

    std::error_code ec;
    fs::create_directories(assetsRoot, ec);

    // --- Barra in alto: breadcrumb, ricerca, nuova cartella, import ---
    drawAssetBreadcrumb(assetsRoot);

    ImGui::SetNextItemWidth(180);
    char searchBuf[128];
    snprintf(searchBuf, sizeof(searchBuf), "%s", m_assetSearchFilter.c_str());
    if (ImGui::InputTextWithHint("##search", "Cerca...", searchBuf, sizeof(searchBuf))) {
        m_assetSearchFilter = searchBuf;
    }
    ImGui::SameLine();
    if (ImGui::Button("+ Cartella")) {
        std::string baseName = "NuovaCartella";
        std::string finalName = baseName;
        int suffix = 1;
        while (fs::exists(fs::path(currentDir) / finalName)) {
            finalName = baseName + std::to_string(suffix++);
        }
        fs::create_directory(fs::path(currentDir) / finalName, ec);
    }
    ImGui::SameLine();
    if (ImGui::Button("+ Script")) {
        std::string baseName = "NuovoScript";
        std::string finalName = baseName + ".py";
        int suffix = 1;
        while (fs::exists(fs::path(currentDir) / finalName)) {
            finalName = baseName + std::to_string(suffix++) + ".py";
        }
        std::ofstream out(fs::path(currentDir) / finalName);
        out << "# Script collegato a un GameObject (trascinalo sul campo \"Script Python\"\n"
            << "# nell'Inspector per assegnarlo). Eseguito solo durante il Play.\n"
            << "#\n"
            << "# engine: l'istanza di pyengine.Engine, per leggere/modificare la scena\n"
            << "# obj_id: l'id di QUESTO oggetto (quello a cui lo script e' assegnato)\n\n"
            << "def on_start(engine, obj_id):\n"
            << "    pass\n\n\n"
            << "def on_update(engine, obj_id, dt):\n"
            << "    pass\n";
    }

    static char importPathBuf[260] = "";
    ImGui::SetNextItemWidth(260);
    ImGui::InputTextWithHint("##importpath", "Percorso file da importare...", importPathBuf, sizeof(importPathBuf));
    ImGui::SameLine();
    if (ImGui::Button("Importa")) {
        fs::path src(importPathBuf);
        if (fs::exists(src)) {
            fs::path dst = fs::path(currentDir) / src.filename();
            fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
            importPathBuf[0] = '\0';
        }
    }

    ImGui::TextDisabled("Doppio click su un file .py per modificarlo con l'editor di testo predefinito.");
    ImGui::Separator();

    // --- Griglia: prima le cartelle, poi i file, filtrate dalla ricerca ---
    std::vector<fs::directory_entry> folders, files;
    if (fs::exists(currentDir, ec) && fs::is_directory(currentDir, ec)) {
        for (const auto& entry : fs::directory_iterator(currentDir, ec)) {
            std::string name = entry.path().filename().string();
            if (!m_assetSearchFilter.empty()) {
                std::string nameLower = name, filterLower = m_assetSearchFilter;
                std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);
                if (nameLower.find(filterLower) == std::string::npos) continue;
            }
            if (entry.is_directory()) folders.push_back(entry);
            else files.push_back(entry);
        }
    }

    constexpr float kCellWidth = 80.0f;
    float availWidth = ImGui::GetContentRegionAvail().x;
    int columns = std::max(1, static_cast<int>(availWidth / kCellWidth));
    int col = 0;

    for (const auto& entry : folders) {
        drawAssetGridItem(entry.path().string(), entry.path().filename().string(), true);
        if (++col < columns) ImGui::SameLine(); else col = 0;
    }
    for (const auto& entry : files) {
        drawAssetGridItem(entry.path().string(), entry.path().filename().string(), false);
        if (++col < columns) ImGui::SameLine(); else col = 0;
    }

    if (folders.empty() && files.empty()) {
        ImGui::TextDisabled("(cartella vuota)");
    }

    // --- Conferma eliminazione ---
    if (!m_assetPendingDelete.empty()) {
        ImGui::OpenPopup("Conferma eliminazione");
    }
    if (ImGui::BeginPopupModal("Conferma eliminazione", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Eliminare \"%s\"?", fs::path(m_assetPendingDelete).filename().string().c_str());
        ImGui::TextDisabled("Operazione irreversibile.");
        if (ImGui::Button("Elimina", ImVec2(120, 0))) {
            std::error_code delEc;
            fs::remove_all(m_assetPendingDelete, delEc);
            m_assetPendingDelete.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Annulla", ImVec2(120, 0))) {
            m_assetPendingDelete.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::End();
}

} // namespace engine
