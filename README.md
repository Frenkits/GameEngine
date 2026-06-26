# MyGameEngine

Game engine con core in **C++** (logica pesante, math, rendering OpenGL 3.3 core
via GLAD + GLFW) esposto a **Python** tramite **pybind11**, così la logica di
gioco/scripting si scrive in Python ma le parti critiche girano native.

## Struttura

```
GameEngine/
├── CMakeLists.txt              # build principale
├── engine/                     # core C++ (libreria statica engine_core)
│   ├── include/engine/
│   │   ├── Engine.hpp          # facade: Window+Renderer, tick()
│   │   ├── Window.hpp          # wrapper GLFW + init GLAD
│   │   ├── Renderer.hpp        # clear + draw di test
│   │   ├── Shader.hpp          # compilazione/link shader GLSL
│   │   └── Math.hpp            # Vec2/Vec3/Mat4 senza dipendenze esterne
│   └── src/                    # implementazioni .cpp
├── bindings/                   # modulo pyengine (pybind11)
│   └── bindings.cpp
├── python/
│   └── examples/main.py        # esempio: loop di gioco in Python
└── third_party/
    └── glad/                   # vedi README in questa cartella: file da generare
```

## Prerequisiti (sul tuo PC, non in questa sandbox)

- CMake >= 3.16, compilatore C++17 (già hai git+cmake ✅)
- Python 3.8+ con header di sviluppo (`python3-dev` su Linux)
- GLFW3:
  - Linux: `sudo apt install libglfw3-dev`
  - macOS: `brew install glfw`
  - Windows: vcpkg (`vcpkg install glfw3`) oppure lascialo scaricare in automatico
    dal FetchContent già presente nel CMakeLists (richiede solo connessione internet)
- I file generati di **GLAD** (vedi `third_party/glad/README.md`)

## Build

```bash
# 1. genera i file GLAD seguendo third_party/glad/README.md, poi:

mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j
```

Questo produce:
- `engine_core` (libreria statica C++)
- `pyengine*.so` / `pyengine*.pyd` copiato automaticamente in `python/`

## Eseguire l'esempio Python

```bash
cd python
python examples/main.py
```

Dovresti vedere una finestra con un triangolo colorato: è la pipeline
GLFW → GLAD → OpenGL → pybind11 → Python che funziona end-to-end.

## Prossimi passi suggeriti

1. **Math con SIMD/glm**: se la matematica diventa pesante, sostituisci `Math.hpp`
   con [glm](https://github.com/g-truc/glm) (header-only, facile da integrare via FetchContent).
2. **ECS**: aggiungi `engine/include/engine/Scene.hpp` con un Entity-Component-System
   semplice (es. array di struct + sparse set), esposto poi a Python.
3. **Input**: wrappa `glfwGetKey`/`glfwSetKeyCallback` in `Window` ed esponilo
   in `bindings.cpp` così la logica input la scrivi in Python.
4. **Asset loading**: aggiungi una libreria immagini (stb_image, header-only)
   per le texture.
5. **Build wheel Python**: se vuoi distribuire il modulo come pacchetto pip,
   usa `scikit-build-core` al posto della chiamata cmake manuale.
