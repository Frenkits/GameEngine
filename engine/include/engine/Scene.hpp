#pragma once
#include "engine/GameObject.hpp"
#include <unordered_map>
#include <vector>
#include <string>

namespace engine {

// Tiene tutti i GameObject della scena corrente. Pensata per essere pilotata
// sia dall'editor C++ (Hierarchy/Inspector via ImGui) sia da Python
// (creazione procedurale di oggetti, scripting di gameplay).
class Scene {
public:
    ObjectId createObject(const std::string& name, ObjectId parent = kInvalidId);
    void destroyObject(ObjectId id);

    // Crea una copia dell'oggetto (stesso parent, stesso transform, nome + " (Copia)")
    ObjectId duplicateObject(ObjectId id);

    // Sposta 'id' sotto un nuovo genitore (kInvalidId = lo rende un oggetto
    // di primo livello, "staccato" da qualsiasi genitore). Usato dal drag&drop
    // nella Hierarchy. Rifiuta l'operazione (ritorna false, nessun effetto) se
    // creerebbe un ciclo (es. rendere un oggetto figlio di un suo discendente).
    bool setParent(ObjectId id, ObjectId newParent);
    GameObject* getObject(ObjectId id);
    const GameObject* getObject(ObjectId id) const;

    // Oggetti senza parent: il punto di partenza per disegnare l'albero della Hierarchy
    const std::vector<ObjectId>& getRootObjects() const { return m_rootObjects; }

    const std::unordered_map<ObjectId, GameObject>& getAllObjects() const { return m_objects; }

    size_t count() const { return m_objects.size(); }

    // Salva/carica la scena in un formato testuale semplice (non JSON, zero
    // dipendenze esterne). Pensato per il bottone "Salva Scena" del menu File.
    bool saveToFile(const std::string& path) const;
    bool loadFromFile(const std::string& path);

private:
    std::unordered_map<ObjectId, GameObject> m_objects;
    std::vector<ObjectId> m_rootObjects;
    ObjectId m_nextId = 0;

    void removeFromParentList(ObjectId id);
    bool isAncestorOf(ObjectId potentialAncestor, ObjectId node) const;
};

} // namespace engine
