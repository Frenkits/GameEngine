#pragma once
#include <string>

namespace engine {

// Piccola finestra indipendente mostrata PRIMA dell'editor vero e proprio:
// elenca i progetti esistenti (sottocartelle di "projects/") e permette di
// crearne uno nuovo. Ha una sua finestra GLFW e un suo contesto ImGui,
// separati e completamente chiusi prima che Engine apra la finestra dell'editor.
class Launcher {
public:
    Launcher();
    ~Launcher();

    // Esegue il loop del launcher finché l'utente non scegliere un progetto
    // o chiude la finestra. Ritorna il percorso del progetto scelto, oppure
    // stringa vuota se l'utente ha chiuso senza scegliere.
    std::string run();

private:
    std::string m_projectsRoot = "projects";
};

} // namespace engine
