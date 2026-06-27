#include "engine/ObjLoader.hpp"
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <set>
#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <filesystem>
#include <sstream>
#include <array>

namespace engine {

namespace {

// Legge un file .mtl (materiali OBJ) e ritorna nome materiale -> colore
// diffuse (riga "Kd r g b"). Usato per colorare i pezzi importati col loro
// colore originale invece del grigio di default.
std::unordered_map<std::string, std::array<float, 3>> parseMtlFile(const std::string& mtlPath) {
    std::unordered_map<std::string, std::array<float, 3>> result;
    std::ifstream file(mtlPath);
    if (!file.is_open()) return result;

    std::string currentName;
    std::string line;
    while (std::getline(file, line)) {
        if (line.size() < 2) continue;

        if (line.rfind("newmtl", 0) == 0) {
            std::string rest = line.substr(6);
            size_t s = rest.find_first_not_of(' ');
            currentName = (s == std::string::npos) ? "" : rest.substr(s);
            while (!currentName.empty() && (currentName.back() == '\r' || currentName.back() == ' ')) {
                currentName.pop_back();
            }
        } else if (line.rfind("Kd", 0) == 0 && (line.size() == 2 || line[2] == ' ')) {
            const char* p = line.c_str() + 2;
            char* end;
            float r = std::strtof(p, &end); p = end;
            float g = std::strtof(p, &end); p = end;
            float b = std::strtof(p, &end); p = end;
            if (!currentName.empty()) {
                result[currentName] = {r, g, b};
            }
        }
    }
    return result;
}

}

// Normale del triangolo (flat shading: stessa normale per i 3 vertici della
// faccia). Calcolata sempre noi stessi via prodotto vettoriale, indipendentemente
// da eventuali "vn" nel file: più semplice e robusto di parsare/indicizzare le
// normali originali, al costo di un'illuminazione "a facce" non "a superficie morbida".
void computeFaceNormal(const float* p0, const float* p1, const float* p2, float out[3]) {
    float e1[3] = { p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2] };
    float e2[3] = { p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2] };
    out[0] = e1[1] * e2[2] - e1[2] * e2[1];
    out[1] = e1[2] * e2[0] - e1[0] * e2[2];
    out[2] = e1[0] * e2[1] - e1[1] * e2[0];
    float len = std::sqrt(out[0]*out[0] + out[1]*out[1] + out[2]*out[2]);
    if (len > 1e-8f) { out[0] /= len; out[1] /= len; out[2] /= len; }
}

// Parser interno condiviso da loadObjFile e loadObjFileGrouped. Parsing
// manuale (niente std::stringstream per riga): su file da 2+ milioni di
// righe lo stream-based parsing è il vero collo di bottiglia.
//
// Formato di uscita per ogni vertice: x,y,z, nx,ny,nz (6 float interleaved),
// pronto per un VAO con due attributi (posizione + normale).
bool parseObjGrouped(const std::string& path, std::vector<ObjGroup>& outGroups) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cout << "[ObjLoader] Impossibile aprire il file: " << path << "\n";
        return false;
    }

    std::vector<float> positions; // x,y,z grezzi di TUTTO il file (condivisi tra gruppi)
    outGroups.clear();

    std::unordered_map<std::string, size_t> nameToIndex;
    auto ensureGroup = [&](const std::string& name) -> size_t {
        auto it = nameToIndex.find(name);
        if (it != nameToIndex.end()) return it->second;
        outGroups.push_back(ObjGroup{name, {}});
        size_t idx = outGroups.size() - 1;
        nameToIndex[name] = idx;
        return idx;
    };

    size_t currentGroupIdx = ensureGroup("(senza nome)");
    std::vector<int> faceIndices; // riutilizzato per ogni faccia, evita realloc
    std::unordered_map<std::string, std::array<float, 3>> materialColors; // nome materiale -> colore Kd

    std::string line;
    int vCount = 0, fCount = 0;

    while (std::getline(file, line)) {
        if (line.size() < 2) continue;

        if (line[0] == 'v' && line[1] == ' ') {
            const char* p = line.c_str() + 2;
            char* end;
            float x = std::strtof(p, &end); p = end;
            float y = std::strtof(p, &end); p = end;
            float z = std::strtof(p, &end); p = end;
            positions.push_back(x);
            positions.push_back(y);
            positions.push_back(z);
            ++vCount;

        } else if ((line[0] == 'o' || line[0] == 'g') && line[1] == ' ') {
            std::string name = line.substr(2);
            while (!name.empty() && (name.back() == '\r' || name.back() == ' ')) name.pop_back();
            if (!name.empty()) {
                currentGroupIdx = ensureGroup(name);
            }

        } else if (line.rfind("mtllib", 0) == 0 && (line.size() == 6 || line[6] == ' ')) {
            // Riga tipo "mtllib nomefile.mtl" (raramente più file separati da spazio).
            // Il file .mtl si trova nella stessa cartella del file .obj.
            std::string rest = line.substr(6);
            std::stringstream ss(rest); // riga rara, non un hot-path: ok usare stringstream qui
            std::string mtlFile;
            std::filesystem::path objDir = std::filesystem::path(path).parent_path();
            while (ss >> mtlFile) {
                std::filesystem::path mtlFullPath = objDir / mtlFile;
                auto parsed = parseMtlFile(mtlFullPath.string());
                for (auto& [matName, color] : parsed) {
                    materialColors[matName] = color;
                }
            }

        } else if (line.rfind("usemtl", 0) == 0 && (line.size() == 6 || line[6] == ' ')) {
            std::string matName = line.substr(6);
            size_t s = matName.find_first_not_of(' ');
            matName = (s == std::string::npos) ? "" : matName.substr(s);
            while (!matName.empty() && (matName.back() == '\r' || matName.back() == ' ')) matName.pop_back();

            auto it = materialColors.find(matName);
            if (it != materialColors.end()) {
                outGroups[currentGroupIdx].color[0] = it->second[0];
                outGroups[currentGroupIdx].color[1] = it->second[1];
                outGroups[currentGroupIdx].color[2] = it->second[2];
            }

        } else if (line[0] == 'f' && line[1] == ' ') {
            ++fCount;
            faceIndices.clear();
            const char* p = line.c_str() + 2;
            int verticesSoFar = static_cast<int>(positions.size() / 3);

            while (*p) {
                while (*p == ' ') ++p;
                if (!*p) break;
                char* end;
                long rawIdx = std::strtol(p, &end, 10);
                if (end == p) break;
                p = end;
                while (*p && *p != ' ') ++p; // salta l'eventuale /vt/vn dopo l'indice

                int idx0Based = (rawIdx > 0) ? static_cast<int>(rawIdx - 1)
                                              : static_cast<int>(verticesSoFar + rawIdx);
                faceIndices.push_back(idx0Based);
            }

            std::vector<float>& outVerts = outGroups[currentGroupIdx].vertices;

            // Triangolazione a ventaglio: (0,1,2), (0,2,3), (0,3,4), ...
            for (size_t i = 1; i + 1 < faceIndices.size(); ++i) {
                int idxs[3] = { faceIndices[0], faceIndices[i], faceIndices[i + 1] };
                bool valid = true;
                for (int idx : idxs) {
                    if (idx < 0 || static_cast<size_t>(idx) * 3 + 2 >= positions.size()) {
                        valid = false;
                        break;
                    }
                }
                if (!valid) continue;

                const float* p0 = &positions[idxs[0] * 3];
                const float* p1 = &positions[idxs[1] * 3];
                const float* p2 = &positions[idxs[2] * 3];

                float normal[3];
                computeFaceNormal(p0, p1, p2, normal);

                for (const float* pv : { p0, p1, p2 }) {
                    outVerts.push_back(pv[0]);
                    outVerts.push_back(pv[1]);
                    outVerts.push_back(pv[2]);
                    outVerts.push_back(normal[0]);
                    outVerts.push_back(normal[1]);
                    outVerts.push_back(normal[2]);
                }
            }
        }
        // altri tag (vn, vt, usemtl, mtllib, s...) ignorati
    }

    outGroups.erase(std::remove_if(outGroups.begin(), outGroups.end(),
        [](const ObjGroup& g) { return g.vertices.empty(); }), outGroups.end());

    std::cout << "[ObjLoader] File: " << path << "\n";
    std::cout << "[ObjLoader] Vertici totali: " << vCount << ", Facce totali: " << fCount << "\n";
    std::cout << "[ObjLoader] Gruppi con geometria: " << outGroups.size() << "\n";
    for (const auto& g : outGroups) {
        std::cout << "[ObjLoader]   - \"" << g.name << "\": " << (g.vertices.size() / 18) << " triangoli\n";
    }

    return !outGroups.empty();
}

bool loadObjFileGrouped(const std::string& path, std::vector<ObjGroup>& outGroups) {
    return parseObjGrouped(path, outGroups);
}

bool loadObjFile(const std::string& path, std::vector<float>& outVertices,
                  const std::vector<std::string>& excludeGroups) {
    std::vector<ObjGroup> groups;
    if (!parseObjGrouped(path, groups)) return false;

    std::set<std::string> excludeSet(excludeGroups.begin(), excludeGroups.end());
    outVertices.clear();
    for (const auto& g : groups) {
        if (excludeSet.count(g.name)) continue;
        outVertices.insert(outVertices.end(), g.vertices.begin(), g.vertices.end());
    }
    return !outVertices.empty();
}

} // namespace engine
