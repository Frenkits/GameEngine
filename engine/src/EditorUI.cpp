#include "engine/EditorUI.hpp"

#include <imgui.h>
#include <imgui_internal.h> // necessario per le funzioni ImGui::DockBuilder*
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace engine {

namespace {
    constexpr const char* kDockspaceName = "EditorDockspace";
    constexpr const char* kHierarchyName = "Hierarchy";
    constexpr const char* kInspectorName = "Inspector";
    constexpr const char* kSceneName = "Scena";
    constexpr const char* kAssetsName = "Assets";
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

void EditorUI::drawMenuBar(FrameResult& result) {
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
                                            unsigned int sceneTextureId, const std::string& projectPath) {
    FrameResult result;

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

    drawMenuBar(result);

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
    drawAssetsWindow(projectPath);

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
        selectedId = scene.createObject("Luce");
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

void EditorUI::drawSceneWindow(FrameResult& result, unsigned int sceneTextureId) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin(kSceneName);

    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x < 1.0f) avail.x = 1.0f;
    if (avail.y < 1.0f) avail.y = 1.0f;

    result.viewportWidth = avail.x;
    result.viewportHeight = avail.y;
    result.viewportHovered = ImGui::IsWindowHovered();

    ImVec2 imageScreenPos = ImGui::GetCursorScreenPos(); // angolo in alto a sx dell'immagine, coord. schermo

    // Flip verticale (V invertita) perché la texture OpenGL ha origine in
    // basso a sinistra, mentre ImGui disegna le immagini con origine in alto.
    ImGui::Image((ImTextureID)(intptr_t)sceneTextureId, avail, ImVec2(0, 1), ImVec2(1, 0));

    // Click sinistro = selezione oggetto (color picking, gestito da Engine).
    // Il tasto destro resta libero per orbitare la camera, quindi nessun conflitto.
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        ImVec2 mouse = ImGui::GetMousePos();
        float localX = mouse.x - imageScreenPos.x;
        float localY = mouse.y - imageScreenPos.y; // origine in alto, Y verso il basso

        result.clickedInViewport = true;
        result.clickPixelX = localX;
        result.clickPixelY = avail.y - localY; // flip: l'OpenGL framebuffer ha origine in basso
    }

    // Punto di rilascio per il drag&drop: trascinando un file dal pannello
    // Assets e lasciandolo qui sopra, si crea un nuovo GameObject con quella mesh.
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_FILE_PATH")) {
            result.droppedAssetPath = std::string(static_cast<const char*>(payload->Data));
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

void EditorUI::drawAssetsWindow(const std::string& projectPath) {
    ImGui::Begin(kAssetsName);

    std::string assetsDir = projectPath.empty() ? "assets" : (projectPath + "/assets");

    static char importPathBuf[260] = "";
    ImGui::TextDisabled("Cartella: %s", assetsDir.c_str());
    ImGui::InputText("Percorso file da importare", importPathBuf, sizeof(importPathBuf));
    ImGui::SameLine();
    if (ImGui::Button("Importa")) {
        std::error_code ec;
        fs::create_directories(assetsDir, ec);
        fs::path src(importPathBuf);
        if (!ec && fs::exists(src)) {
            fs::path dst = fs::path(assetsDir) / src.filename();
            fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
        }
    }

    ImGui::Separator();

    std::error_code ec;
    if (fs::exists(assetsDir, ec) && fs::is_directory(assetsDir, ec)) {
        for (const auto& entry : fs::directory_iterator(assetsDir, ec)) {
            std::string fullPath = entry.path().string();
            std::string filename = entry.path().filename().string();

            ImGui::Selectable(filename.c_str());

            // Sorgente del drag&drop: rilasciandolo sopra il pannello "Scena"
            // crea un GameObject con questa mesh (solo i file .obj hanno
            // effetto per ora; altri tipi restano semplicemente non gestiti).
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                ImGui::SetDragDropPayload("ASSET_FILE_PATH", fullPath.c_str(),
                                          fullPath.size() + 1); // +1 per il terminatore '\0'
                ImGui::Text("%s", filename.c_str());
                ImGui::EndDragDropSource();
            }
        }
    } else {
        ImGui::TextDisabled("(cartella assets vuota o non ancora creata)");
    }

    ImGui::End();
}

} // namespace engine
