#pragma once
#include "engine/Transform.hpp"
#include <string>
#include <vector>

namespace engine {

using ObjectId = int;
constexpr ObjectId kInvalidId = -1;

// Nodo della scena: ogni oggetto nella Hierarchy è un GameObject.
// Il parent/children permette di costruire un albero (es. "Auto" contiene
// "Ruota1", "Ruota2"...), proprio come la Hierarchy di Unity/Godot.
struct GameObject {
    ObjectId id = kInvalidId;
    std::string name = "GameObject";
    Transform transform;

    // Colore "materiale" della superficie (albedo), modificabile dall'Inspector.
    // Combinato con l'illuminazione direzionale fissa nello shader.
    float baseColor[3] = {0.7f, 0.7f, 0.75f};

    // Se true, questo oggetto è una sorgente di luce reale: Engine la userà
    // per illuminare la scena (al posto/oltre la luce di default), invece
    // di essere solo un cubo decorativo come prima.
    bool isLight = false;
    float lightColor[3] = {1.0f, 1.0f, 1.0f};
    float lightIntensity = 1.0f;

    // Se true, questo oggetto è una camera di gioco: in modalità Play,
    // Engine userà la sua posizione/rotazione (Transform sopra) invece della
    // camera orbitale dell'editor. Rotazione: X = pitch (su/giù), Y = yaw
    // (sinistra/destra), stessa convenzione della camera orbitale.
    bool isCamera = false;
    float cameraFov = 60.0f;

    // Percorso di uno script Python assegnato a questo oggetto (vuoto = nessuno).
    // Durante il Play, se il file definisce on_start(engine, obj_id) e/o
    // on_update(engine, obj_id, dt), vengono chiamati automaticamente da Python.
    std::string scriptPath;

    // Percorso del file .obj importato (vuoto = disegna il cubo segnaposto).
    std::string meshPath;

    // Nomi di gruppi/oggetti OBJ (tag "o"/"g" nel file) da escludere dal
    // caricamento, separati da virgola. Usato solo dal vecchio flusso a
    // mesh-singola (retrocompatibilità con scene salvate in precedenza).
    std::string excludedGroups;

    // Nome del gruppo OBJ specifico che questo oggetto rappresenta, quando
    // fa parte di un import multi-oggetto (un file .obj -> più GameObject
    // figli, uno per pezzo). Vuoto = non fa parte di un import a gruppi.
    std::string groupName;

    ObjectId parent = kInvalidId;
    std::vector<ObjectId> children;
};

} // namespace engine
