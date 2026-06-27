# MyGameEngine

Editor 3D con core in **C++** (rendering OpenGL 3.3 core via GLAD + GLFW,
ImGui per l'interfaccia) esposto a **Python** tramite **pybind11**: la logica
pesante (rendering, import mesh, picking) gira nativa in C++, lo scripting di
alto livello si scrive in Python.

## Cosa fa, ad oggi

- **Launcher**: finestra iniziale per scegliere/creare un progetto (cartella
  in `projects/`), prima di aprire l'editor.
- **Editor con docking** (Hierarchy a sinistra, Inspector a destra, Scena al
  centro, Assets in basso), layout ridimensionabile e salvato tra le sessioni.
- **Scena 3D**: griglia di riferimento, camera orbitale (tasto destro = ruota,
  rotella = zoom, tasto centrale = pan), illuminazione (ambient + diffuse) con
  una sorgente di luce posizionabile.
- **Import di modelli `.obj`**: drag&drop da Assets alla Scena. Ogni "gruppo"
  del file (`o`/`g` nel formato OBJ) diventa un GameObject figlio separato
  nella Hierarchy, così puoi eliminare/isolare singoli pezzi indesiderati
  (es. pavimenti/luci da studio inclusi per errore nel file scaricato). Legge
  anche il file `.mtl` associato per i colori originali dei materiali.
- **Hierarchy/Inspector**: crea oggetti vuoti/cubi/luci/camere, trasformazioni
  gerarchiche (muovere un genitore sposta i figli), colore materiale per
  oggetto, duplicazione (Ctrl+D), eliminazione (Canc).
- **Selezione cliccando nel viewport 3D** (color-picking), oltre che dalla
  Hierarchy.
- **Oggetto Camera**: posizionabile/orientabile, con gizmo a piramide che
  mostra il campo visivo (FOV) in Edit; usata come punto di vista reale
  quando premi **Play**.
- **Play/Stop**: passa a una vista di gioco a schermo intero (usa la Camera
  della scena se presente, altrimenti la camera orbitale).
- **Assets browser** stile "Content Browser": navigazione cartelle con
  breadcrumb, griglia con icone, ricerca, rinomina/elimina, creazione cartelle,
  import file.
- **Salvataggio/caricamento scena** (menu File, Ctrl+S) in un formato testuale
  semplice dentro la cartella del progetto.

## Struttura

```
GameEngine/
├── CMakeLists.txt              # build principale (scarica GLFW/pybind11/ImGui)
├── engine/                     # core C++ (libreria statica engine_core)
│   ├── CMakeLists.txt
│   ├── include/engine/
│   │   ├── Engine.hpp          # facade: Window+Renderer+EditorUI, tick(), Play/Stop
│   │   ├── Window.hpp          # wrapper GLFW + init GLAD + input
│   │   ├── Launcher.hpp        # finestra di selezione progetto (pre-editor)
│   │   ├── EditorUI.hpp        # Hierarchy/Inspector/Scena/Assets (ImGui + docking)
│   │   ├── Scene.hpp           # contenitore GameObject (albero, save/load)
│   │   ├── GameObject.hpp      # nodo scena: transform, mesh, luce, camera, colore
│   │   ├── Transform.hpp       # posizione/rotazione/scala -> matrice
│   │   ├── Camera.hpp          # camera orbitale dell'editor
│   │   ├── Renderer.hpp        # griglia, cubo segnaposto, linee gizmo, luce
│   │   ├── Mesh.hpp            # mesh generica caricata da .obj (pos+normale)
│   │   ├── ObjLoader.hpp       # parser .obj (multi-gruppo) + .mtl (colori)
│   │   ├── Framebuffer.hpp     # render-to-texture (viewport + picking)
│   │   ├── Shader.hpp          # compilazione/link shader GLSL
│   │   └── Math.hpp            # Vec2/Vec3/Mat4 senza dipendenze esterne
│   └── src/                    # implementazioni .cpp
├── bindings/                   # modulo pyengine (pybind11)
│   └── bindings.cpp
├── python/
│   └── examples/main.py        # Launcher -> Engine, loop minimo
├── third_party/
│   └── glad/                   # vedi README in questa cartella: file da generare
└── projects/                   # progetti creati dal Launcher (esclusi da git)
```

## Prerequisiti

- CMake >= 3.16, compilatore C++17 (su Windows: Visual Studio Build Tools)
- Python 3.8+ con header di sviluppo
- Connessione internet alla prima configurazione (CMake scarica GLFW,
  pybind11 e Dear ImGui via FetchContent)
- I file generati di **GLAD** (vedi `third_party/glad/README.md`) — core
  profile, OpenGL 3.3

## Build

```bash
# 1. genera i file GLAD seguendo third_party/glad/README.md, poi:

mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j
```

Produce `engine_core` (libreria statica C++) e `pyengine*.pyd`/`*.so`,
copiato automaticamente in `python/`.

⚠️ Su Windows con generatore Visual Studio (multi-config), se cambi rami/file
in modo massiccio è più sicuro ricreare la cartella `build/` da zero
(`rmdir /s /q build`) per evitare problemi di cache incrementale.

## Eseguire l'editor

```bash
cd python/examples
python main.py
```

Si apre prima il **Launcher** (scegli o crea un progetto), poi l'editor.

## Note di sviluppo

- **GLAD non è incluso nel repository** in chiaro perché va generato per la
  combinazione di versione/profilo OpenGL desiderata: segui
  `third_party/glad/README.md`.
- Il formato di salvataggio scena è testuale e volutamente semplice (non
  JSON): un campo per riga, separato da `;`. Ogni nuova feature che aggiunge
  dati per-oggetto (colore, luce, camera...) ha esteso il formato in modo
  retrocompatibile (campi assenti = valori di default).
- Il color-picking usa uno shader "flat" dedicato (`drawUnlit`/`drawCubeUnlit`)
  separato da quello con illuminazione: necessario perché il colore-id deve
  arrivare nel framebuffer **esattamente** invariato, senza essere moltiplicato
  da nessun fattore di luce.

## Prossimi passi possibili

1. **Controllo camera in Play**: muoverla con WASD/mouse durante il Play
   (attualmente è statica, usa solo la posizione impostata in Edit).
2. **Multi-luce**: oggi viene usata solo la prima luce trovata nella scena.
3. **Smooth shading**: le normali sono calcolate per-triangolo (flat shading);
   per superfici curve più "morbide" servirebbe leggere/mediare le normali
   originali del file `.obj`.
4. **Texture/materiali**: caricamento immagini (es. stb_image) per applicare
   texture invece del solo colore piatto.
5. **Composizione gerarchica completa**: i figli ereditano già la trasformazione
   del genitore nel rendering; manca ancora un gizmo di trasformazione
   (freccette per spostare/ruotare col mouse) invece di editare solo i numeri
   nell'Inspector.
