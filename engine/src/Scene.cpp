#include "engine/Scene.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>

namespace engine {

ObjectId Scene::createObject(const std::string& name, ObjectId parent) {
    ObjectId id = m_nextId++;

    GameObject obj;
    obj.id = id;
    obj.name = name;
    obj.parent = parent;

    m_objects[id] = obj;

    if (parent == kInvalidId) {
        m_rootObjects.push_back(id);
    } else if (auto* parentObj = getObject(parent)) {
        parentObj->children.push_back(id);
    }

    return id;
}

ObjectId Scene::duplicateObject(ObjectId id) {
    const GameObject* src = getObject(id);
    if (!src) return kInvalidId;

    ObjectId newId = createObject(src->name + " (Copia)", src->parent);
    if (auto* dst = getObject(newId)) {
        dst->transform = src->transform;
    }
    return newId;
}

void Scene::destroyObject(ObjectId id) {
    auto* obj = getObject(id);
    if (!obj) return;

    // Distruggi prima i figli (ricorsivo), poi rimuovi dal parent e infine sé stesso.
    // Copiamo la lista perché destroyObject modifica obj->children indirettamente.
    std::vector<ObjectId> childrenCopy = obj->children;
    for (ObjectId child : childrenCopy) {
        destroyObject(child);
    }

    removeFromParentList(id);
    m_objects.erase(id);
}

void Scene::removeFromParentList(ObjectId id) {
    auto* obj = getObject(id);
    if (!obj) return;

    if (obj->parent == kInvalidId) {
        m_rootObjects.erase(std::remove(m_rootObjects.begin(), m_rootObjects.end(), id), m_rootObjects.end());
    } else if (auto* parentObj = getObject(obj->parent)) {
        auto& c = parentObj->children;
        c.erase(std::remove(c.begin(), c.end(), id), c.end());
    }
}

GameObject* Scene::getObject(ObjectId id) {
    auto it = m_objects.find(id);
    return it != m_objects.end() ? &it->second : nullptr;
}

const GameObject* Scene::getObject(ObjectId id) const {
    auto it = m_objects.find(id);
    return it != m_objects.end() ? &it->second : nullptr;
}

// Formato testuale semplice, una riga per oggetto:
// id;parent;nome;px,py,pz;rx,ry,rz;sx,sy,sz;meshPath
// Il nome e meshPath non possono contenere ';' (limitazione accettabile per ora).
bool Scene::saveToFile(const std::string& path) const {
    std::ofstream out(path);
    if (!out.is_open()) return false;

    for (const auto& [id, obj] : m_objects) {
        out << id << ';' << obj.parent << ';' << obj.name << ';'
            << obj.transform.position.x << ',' << obj.transform.position.y << ',' << obj.transform.position.z << ';'
            << obj.transform.rotationDegrees.x << ',' << obj.transform.rotationDegrees.y << ',' << obj.transform.rotationDegrees.z << ';'
            << obj.transform.scale.x << ',' << obj.transform.scale.y << ',' << obj.transform.scale.z << ';'
            << obj.meshPath << ';' << obj.excludedGroups << ';' << obj.groupName << ';'
            << obj.baseColor[0] << ',' << obj.baseColor[1] << ',' << obj.baseColor[2] << ';'
            << (obj.isLight ? 1 : 0) << ';'
            << obj.lightColor[0] << ',' << obj.lightColor[1] << ',' << obj.lightColor[2] << ';'
            << obj.lightIntensity << ';'
            << (obj.isCamera ? 1 : 0) << ';'
            << obj.cameraFov << ';'
            << obj.scriptPath << '\n';
    }
    return true;
}

namespace {
    Vec3 parseVec3(const std::string& s) {
        Vec3 v{};
        std::stringstream ss(s);
        std::string token;
        if (std::getline(ss, token, ',')) v.x = std::stof(token);
        if (std::getline(ss, token, ',')) v.y = std::stof(token);
        if (std::getline(ss, token, ',')) v.z = std::stof(token);
        return v;
    }
}

bool Scene::loadFromFile(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) return false;

    m_objects.clear();
    m_rootObjects.clear();
    m_nextId = 0;

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string idStr, parentStr, name, posStr, rotStr, scaleStr, meshPathStr, excludedGroupsStr, groupNameStr, colorStr;
        std::string isLightStr, lightColorStr, lightIntensityStr;
        std::string isCameraStr, cameraFovStr, scriptPathStr;
        std::getline(ss, idStr, ';');
        std::getline(ss, parentStr, ';');
        std::getline(ss, name, ';');
        std::getline(ss, posStr, ';');
        std::getline(ss, rotStr, ';');
        std::getline(ss, scaleStr, ';');
        std::getline(ss, meshPathStr, ';');
        std::getline(ss, excludedGroupsStr, ';');
        std::getline(ss, groupNameStr, ';');
        std::getline(ss, colorStr, ';');
        std::getline(ss, isLightStr, ';');
        std::getline(ss, lightColorStr, ';');
        std::getline(ss, lightIntensityStr, ';');
        std::getline(ss, isCameraStr, ';');
        std::getline(ss, cameraFovStr, ';');
        std::getline(ss, scriptPathStr, ';'); // assente nei salvataggi vecchi: resta vuoto

        GameObject obj;
        obj.id = std::stoi(idStr);
        obj.parent = std::stoi(parentStr);
        obj.name = name;
        obj.transform.position = parseVec3(posStr);
        obj.transform.rotationDegrees = parseVec3(rotStr);
        obj.transform.scale = parseVec3(scaleStr);
        obj.meshPath = meshPathStr;
        obj.excludedGroups = excludedGroupsStr;
        obj.groupName = groupNameStr;
        if (!colorStr.empty()) {
            Vec3 c = parseVec3(colorStr);
            obj.baseColor[0] = c.x;
            obj.baseColor[1] = c.y;
            obj.baseColor[2] = c.z;
        }
        if (!isLightStr.empty()) {
            obj.isLight = (isLightStr == "1");
        }
        if (!lightColorStr.empty()) {
            Vec3 lc = parseVec3(lightColorStr);
            obj.lightColor[0] = lc.x;
            obj.lightColor[1] = lc.y;
            obj.lightColor[2] = lc.z;
        }
        if (!lightIntensityStr.empty()) {
            obj.lightIntensity = std::stof(lightIntensityStr);
        }
        if (!isCameraStr.empty()) {
            obj.isCamera = (isCameraStr == "1");
        }
        if (!cameraFovStr.empty()) {
            obj.cameraFov = std::stof(cameraFovStr);
        }
        obj.scriptPath = scriptPathStr;

        m_objects[obj.id] = obj;
        m_nextId = std::max(m_nextId, obj.id + 1);
    }

    // Seconda passata: ricostruisci le liste children/root ora che tutti gli oggetti esistono
    for (auto& [id, obj] : m_objects) {
        if (obj.parent == kInvalidId) {
            m_rootObjects.push_back(id);
        } else if (auto it = m_objects.find(obj.parent); it != m_objects.end()) {
            it->second.children.push_back(id);
        }
    }

    return true;
}

} // namespace engine
