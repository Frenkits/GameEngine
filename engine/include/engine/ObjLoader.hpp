#pragma once
#include <string>
#include <vector>

namespace engine {

// Un "pezzo" del file OBJ (tag "o"/"g"): la macchina intera scaricata online
// è quasi sempre composta da decine di questi (carrozzeria, ruote, fari,
// interni...). Ogni ObjGroup diventa un GameObject figlio separato nella
// Hierarchy, così puoi selezionare/eliminare singoli pezzi indesiderati
// (pavimento, luci da studio, ecc.) esattamente come in Unreal/Unity.
struct ObjGroup {
    std::string name;
    std::vector<float> vertices; // 8 float per vertice: x,y,z, nx,ny,nz, u,v (triangolato e appiattito)

    // Colore del materiale originale (letto dal file .mtl associato, se
    // presente). Default: grigio neutro, come il colore di default dei
    // GameObject creati senza import.
    float color[3] = {0.7f, 0.7f, 0.75f};
};

// Carica TUTTI i gruppi del file separatamente. Stampa anche un riassunto
// in console (nome gruppo + numero triangoli) per diagnostica.
bool loadObjFileGrouped(const std::string& path, std::vector<ObjGroup>& outGroups);

// Versione "merge singolo": carica tutti i gruppi e li fonde in un'unica
// mesh, escludendo quelli il cui nome è in excludeGroups. Mantenuta per
// retrocompatibilità con le scene salvate prima dell'import multi-oggetto.
bool loadObjFile(const std::string& path, std::vector<float>& outVertices,
                  const std::vector<std::string>& excludeGroups = {});

} // namespace engine
