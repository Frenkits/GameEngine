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

void computeFaceNormal(const float* p0, const float* p1, const float* p2, float out[3]) {
    float e1[3] = { p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2] };
    float e2[3] = { p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2] };
    out[0] = e1[1] * e2[2] - e1[2] * e2[1];
    out[1] = e1[2] * e2[0] - e1[0] * e2[2];
    out[2] = e1[0] * e2[1] - e1[1] * e2[0];
    float len = std::sqrt(out[0]*out[0] + out[1]*out[1] + out[2]*out[2]);
    if (len > 1e-8f) { out[0] /= len; out[1] /= len; out[2] /= len; }
}

// Estrae UN indice da un token faccia ("12", "12/3", "12/3/5", "12//5"...).
// which=0 -> indice posizione (prima del primo '/'); which=1 -> indice UV
// (tra il primo e il secondo '/', vuoto se assente, es. "12//5").
// Ritorna 0 se il campo richiesto è assente/vuoto (nessun indice).
long parseFaceIndexField(const std::string& token, int which) {
    size_t firstSlash = token.find('/');
    if (which == 0) {
        std::string field = (firstSlash == std::string::npos) ? token : token.substr(0, firstSlash);
        if (field.empty()) return 0;
        return std::strtol(field.c_str(), nullptr, 10);
    }
    // which == 1: campo UV, tra il primo e il secondo '/'
    if (firstSlash == std::string::npos) return 0; // "12" (nessuna UV)
    size_t secondSlash = token.find('/', firstSlash + 1);
    std::string field = (secondSlash == std::string::npos)
        ? token.substr(firstSlash + 1)
        : token.substr(firstSlash + 1, secondSlash - firstSlash - 1);
    if (field.empty()) return 0; // "12//5" (nessuna UV)
    return std::strtol(field.c_str(), nullptr, 10);
}

// Formato di uscita per ogni vertice: x,y,z, nx,ny,nz, u,v (8 float interleaved).
bool parseObjGrouped(const std::string& path, std::vector<ObjGroup>& outGroups) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cout << "[ObjLoader] Impossibile aprire il file: " << path << "\n";
        return false;
    }

    std::vector<float> positions;  // x,y,z di tutto il file
    std::vector<float> texcoords;  // u,v di tutto il file (vuoto se il file non ne ha)
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
    std::vector<long> faceIndices;   // indici posizione di una faccia
    std::vector<long> faceUvIndices; // indici UV della stessa faccia (0 = nessuna)
    std::unordered_map<std::string, std::array<float, 3>> materialColors;

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

        } else if (line[0] == 'v' && line[1] == 't' && (line.size() == 2 || line[2] == ' ')) {
            const char* p = line.c_str() + 2;
            char* end;
            float u = std::strtof(p, &end); p = end;
            float v = std::strtof(p, &end); p = end;
            texcoords.push_back(u);
            texcoords.push_back(v);

        } else if ((line[0] == 'o' || line[0] == 'g') && line[1] == ' ') {
            std::string name = line.substr(2);
            while (!name.empty() && (name.back() == '\r' || name.back() == ' ')) name.pop_back();
            if (!name.empty()) {
                currentGroupIdx = ensureGroup(name);
            }

        } else if (line.rfind("mtllib", 0) == 0 && (line.size() == 6 || line[6] == ' ')) {
            std::string rest = line.substr(6);
            std::stringstream ss(rest); // riga rara, non un hot-path
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
            faceUvIndices.clear();
            const char* p = line.c_str() + 2;
            int verticesSoFar = static_cast<int>(positions.size() / 3);
            int uvsSoFar = static_cast<int>(texcoords.size() / 2);

            while (*p) {
                while (*p == ' ') ++p;
                if (!*p) break;

                const char* tokenStart = p;
                while (*p && *p != ' ') ++p;
                std::string token(tokenStart, p);

                long rawIdx = parseFaceIndexField(token, 0);
                if (rawIdx == 0) continue;
                long idx0Based = (rawIdx > 0) ? (rawIdx - 1) : (verticesSoFar + rawIdx);
                faceIndices.push_back(idx0Based);

                long rawUv = parseFaceIndexField(token, 1);
                long uvIdx0Based = (rawUv == 0) ? -1 // nessuna UV per questo vertice
                                  : (rawUv > 0) ? (rawUv - 1) : (uvsSoFar + rawUv);
                faceUvIndices.push_back(uvIdx0Based);
            }

            std::vector<float>& outVerts = outGroups[currentGroupIdx].vertices;

            // Triangolazione a ventaglio: (0,1,2), (0,2,3), (0,3,4), ...
            for (size_t i = 1; i + 1 < faceIndices.size(); ++i) {
                long idxs[3] = { faceIndices[0], faceIndices[i], faceIndices[i + 1] };
                long uvIdxs[3] = { faceUvIndices[0], faceUvIndices[i], faceUvIndices[i + 1] };
                bool valid = true;
                for (long idx : idxs) {
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

                const float* positionsArr[3] = { p0, p1, p2 };
                for (int v = 0; v < 3; ++v) {
                    outVerts.push_back(positionsArr[v][0]);
                    outVerts.push_back(positionsArr[v][1]);
                    outVerts.push_back(positionsArr[v][2]);
                    outVerts.push_back(normal[0]);
                    outVerts.push_back(normal[1]);
                    outVerts.push_back(normal[2]);

                    long uvIdx = uvIdxs[v];
                    if (uvIdx >= 0 && static_cast<size_t>(uvIdx) * 2 + 1 < texcoords.size()) {
                        outVerts.push_back(texcoords[uvIdx * 2]);
                        outVerts.push_back(texcoords[uvIdx * 2 + 1]);
                    } else {
                        outVerts.push_back(0.0f);
                        outVerts.push_back(0.0f);
                    }
                }
            }
        }
    }

    outGroups.erase(std::remove_if(outGroups.begin(), outGroups.end(),
        [](const ObjGroup& g) { return g.vertices.empty(); }), outGroups.end());

    std::cout << "[ObjLoader] File: " << path << "\n";
    std::cout << "[ObjLoader] Vertici totali: " << vCount << ", Facce totali: " << fCount
               << ", Coordinate UV: " << (texcoords.size() / 2) << "\n";
    std::cout << "[ObjLoader] Gruppi con geometria: " << outGroups.size() << "\n";
    for (const auto& g : outGroups) {
        std::cout << "[ObjLoader]   - \"" << g.name << "\": " << (g.vertices.size() / 24) << " triangoli\n";
    }

    return !outGroups.empty();
}

} // namespace

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
