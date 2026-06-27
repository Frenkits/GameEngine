#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <GLFW/glfw3.h>
#include "engine/Engine.hpp"
#include "engine/Launcher.hpp"

namespace py = pybind11;

PYBIND11_MODULE(pyengine, m) {
    m.doc() = "Binding Python del core C++ del game engine + editor";

    py::class_<engine::Engine>(m, "Engine")
        .def(py::init<int, int, const std::string&, const std::string&>(),
             py::arg("width"), py::arg("height"), py::arg("title"), py::arg("project_path") = "",
             "Crea finestra + contesto OpenGL 3.3 core, renderer, editor UI. "
             "project_path: cartella del progetto (scelta dal Launcher)")
        .def("is_running", &engine::Engine::isRunning,
             "True finché la finestra non viene chiusa")
        .def("tick", &engine::Engine::tick,
             "Esegue un frame completo: input camera, render scena, editor UI, swap buffer")
        .def("set_clear_color", &engine::Engine::setClearColor,
             py::arg("r"), py::arg("g"), py::arg("b"), py::arg("a") = 1.0f,
             "Imposta il colore di background")
        .def("is_key_pressed", &engine::Engine::isKeyPressed,
             py::arg("key_code"),
             "True se il tasto (costante pyengine.KEY_*) è premuto in questo frame")
        .def("is_mouse_button_pressed", &engine::Engine::isMouseButtonPressed,
             py::arg("button"),
             "True se il pulsante del mouse (pyengine.MOUSE_BUTTON_*) è premuto")
        .def("get_mouse_position", [](const engine::Engine& self) {
                double x, y;
                self.getMousePosition(x, y);
                return py::make_tuple(x, y);
             }, "Ritorna (x, y) della posizione del mouse nella finestra")
        .def("get_delta_time", &engine::Engine::getDeltaTime,
             "Secondi trascorsi dall'ultimo tick(): usalo per movimento fluido")
        .def("set_triangle_position", &engine::Engine::setTrianglePosition,
             py::arg("x"), py::arg("y"))

        // --- Scena / Hierarchy ---
        .def("create_object", [](engine::Engine& self, const std::string& name, engine::ObjectId parent) {
                return self.scene().createObject(name, parent);
             }, py::arg("name"), py::arg("parent") = engine::kInvalidId,
             "Crea un GameObject nella scena (visibile nella Hierarchy), ritorna il suo id")
        .def("destroy_object", [](engine::Engine& self, engine::ObjectId id) {
                self.scene().destroyObject(id);
             }, py::arg("id"))
        .def("set_position", [](engine::Engine& self, engine::ObjectId id, float x, float y, float z) {
                if (auto* obj = self.scene().getObject(id)) obj->transform.position = {x, y, z};
             }, py::arg("id"), py::arg("x"), py::arg("y"), py::arg("z"))
        .def("set_rotation", [](engine::Engine& self, engine::ObjectId id, float x, float y, float z) {
                if (auto* obj = self.scene().getObject(id)) obj->transform.rotationDegrees = {x, y, z};
             }, py::arg("id"), py::arg("x"), py::arg("y"), py::arg("z"),
             "Rotazione in gradi (Euler XYZ)")
        .def("set_scale", [](engine::Engine& self, engine::ObjectId id, float x, float y, float z) {
                if (auto* obj = self.scene().getObject(id)) obj->transform.scale = {x, y, z};
             }, py::arg("id"), py::arg("x"), py::arg("y"), py::arg("z"))
        .def("get_object_count", [](const engine::Engine& self) {
                return self.scene().count();
             })
        .def("get_selected_object", &engine::Engine::getSelectedObject,
             "Id dell'oggetto attualmente selezionato nella Hierarchy (-1 se nessuno)")
        .def("set_selected_object", &engine::Engine::setSelectedObject, py::arg("id"))
        .def("save_scene", &engine::Engine::saveScene, "Salva la scena nel progetto corrente")
        .def("load_scene", &engine::Engine::loadScene, "Carica la scena dal progetto corrente")
        .def("get_project_path", &engine::Engine::projectPath)
        .def("is_playing", &engine::Engine::isPlaying,
             "True se l'editor è in modalità Play (vista di gioco a schermo intero)")

        // --- Lettura generica oggetti/script, usata dal runtime script Python ---
        .def("get_all_object_ids", [](const engine::Engine& self) {
                std::vector<engine::ObjectId> ids;
                for (const auto& [id, obj] : self.scene().getAllObjects()) ids.push_back(id);
                return ids;
             }, "Lista degli id di tutti i GameObject nella scena")
        .def("get_object_name", [](const engine::Engine& self, engine::ObjectId id) {
                const auto* obj = self.scene().getObject(id);
                return obj ? obj->name : std::string();
             }, py::arg("id"))
        .def("get_script_path", [](const engine::Engine& self, engine::ObjectId id) {
                const auto* obj = self.scene().getObject(id);
                return obj ? obj->scriptPath : std::string();
             }, py::arg("id"), "Percorso dello script Python assegnato (vuoto se nessuno)")
        .def("get_position", [](const engine::Engine& self, engine::ObjectId id) {
                const auto* obj = self.scene().getObject(id);
                if (!obj) return py::make_tuple(0.0f, 0.0f, 0.0f);
                return py::make_tuple(obj->transform.position.x, obj->transform.position.y, obj->transform.position.z);
             }, py::arg("id"))
        .def("get_rotation", [](const engine::Engine& self, engine::ObjectId id) {
                const auto* obj = self.scene().getObject(id);
                if (!obj) return py::make_tuple(0.0f, 0.0f, 0.0f);
                return py::make_tuple(obj->transform.rotationDegrees.x, obj->transform.rotationDegrees.y, obj->transform.rotationDegrees.z);
             }, py::arg("id"))
        .def("get_scale", [](const engine::Engine& self, engine::ObjectId id) {
                const auto* obj = self.scene().getObject(id);
                if (!obj) return py::make_tuple(1.0f, 1.0f, 1.0f);
                return py::make_tuple(obj->transform.scale.x, obj->transform.scale.y, obj->transform.scale.z);
             }, py::arg("id"));

    py::class_<engine::Launcher>(m, "Launcher")
        .def(py::init<>())
        .def("run", &engine::Launcher::run,
             "Mostra la finestra di selezione/creazione progetto. "
             "Ritorna il percorso del progetto scelto, o stringa vuota se l'utente ha chiuso senza scegliere.");

    // --- Costanti tasti più comuni (valori GLFW_KEY_*) ---
    m.attr("KEY_A") = GLFW_KEY_A;
    m.attr("KEY_B") = GLFW_KEY_B;
    m.attr("KEY_C") = GLFW_KEY_C;
    m.attr("KEY_D") = GLFW_KEY_D;
    m.attr("KEY_E") = GLFW_KEY_E;
    m.attr("KEY_F") = GLFW_KEY_F;
    m.attr("KEY_G") = GLFW_KEY_G;
    m.attr("KEY_H") = GLFW_KEY_H;
    m.attr("KEY_I") = GLFW_KEY_I;
    m.attr("KEY_J") = GLFW_KEY_J;
    m.attr("KEY_K") = GLFW_KEY_K;
    m.attr("KEY_L") = GLFW_KEY_L;
    m.attr("KEY_M") = GLFW_KEY_M;
    m.attr("KEY_N") = GLFW_KEY_N;
    m.attr("KEY_O") = GLFW_KEY_O;
    m.attr("KEY_P") = GLFW_KEY_P;
    m.attr("KEY_Q") = GLFW_KEY_Q;
    m.attr("KEY_R") = GLFW_KEY_R;
    m.attr("KEY_S") = GLFW_KEY_S;
    m.attr("KEY_T") = GLFW_KEY_T;
    m.attr("KEY_U") = GLFW_KEY_U;
    m.attr("KEY_V") = GLFW_KEY_V;
    m.attr("KEY_W") = GLFW_KEY_W;
    m.attr("KEY_X") = GLFW_KEY_X;
    m.attr("KEY_Y") = GLFW_KEY_Y;
    m.attr("KEY_Z") = GLFW_KEY_Z;
    m.attr("KEY_UP") = GLFW_KEY_UP;
    m.attr("KEY_DOWN") = GLFW_KEY_DOWN;
    m.attr("KEY_LEFT") = GLFW_KEY_LEFT;
    m.attr("KEY_RIGHT") = GLFW_KEY_RIGHT;
    m.attr("KEY_SPACE") = GLFW_KEY_SPACE;
    m.attr("KEY_ESCAPE") = GLFW_KEY_ESCAPE;
    m.attr("KEY_ENTER") = GLFW_KEY_ENTER;
    m.attr("KEY_LEFT_SHIFT") = GLFW_KEY_LEFT_SHIFT;
    m.attr("MOUSE_BUTTON_LEFT") = GLFW_MOUSE_BUTTON_LEFT;
    m.attr("MOUSE_BUTTON_RIGHT") = GLFW_MOUSE_BUTTON_RIGHT;
    m.attr("MOUSE_BUTTON_MIDDLE") = GLFW_MOUSE_BUTTON_MIDDLE;
}
