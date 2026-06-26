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
