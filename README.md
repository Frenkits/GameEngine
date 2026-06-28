# MyGameEngine

Editor 3D con core in **C++** (rendering OpenGL 3.3 core via GLAD + GLFW,
ImGui per l'interfaccia) esposto a **Python** tramite **pybind11**: il
rendering/import/picking girano nativi in C++, la logica di gioco si scrive
in script Python collegati agli oggetti.

## Cosa fa, ad oggi

### Editor
- **Launcher**: finestra iniziale per scegliere/creare un progetto.
- **Editor con docking** (Hierarchy a sinistra, Inspector a destra, Scena al
  centro, Assets in basso), layout ridimensionabile e salvato tra le sessioni.
- **Scena 3D**: griglia infinita procedurale (stile Unreal/Unity, con linee
  principali ogni 10 unità e dissolvenza in distanza), camera orbitale (tasto
  destro = ruota, rotella = zoom, tasto centrale = pan).
- **Gizmo di traslazione** (freccette X/Y/Z) per ogni oggetto selezionato,
  ancorato al centro visivo della mesh (non solo alla Transform, spesso a
  zero per i pezzi importati). Trascinare l'oggetto direttamente nel
  viewport lo sposta liberamente sul piano orizzontale.
- **Selezione**: dalla Hierarchy, o cliccando direttamente nel viewport 3D
  (color-picking).
- **Drag&drop nella Hierarchy** per riassegnare genitori (es. metti una
  Camera dentro un personaggio perché lo segua muovendosi).
- **Drag&drop da Esplora File** direttamente nella finestra dell'editor:
  copia il file nella cartella Assets corrente.

### Scena e oggetti
- Crea oggetti vuoti/cubi/luci/camere dalla Hierarchy; trasformazioni
  gerarchiche (muovere un genitore sposta i figli); colore materiale per
  oggetto; duplicazione (Ctrl+D); eliminazione (Canc).
- **Import di modelli `.obj`**: drag&drop da Assets alla Scena. Ogni gruppo
  del file diventa un GameObject figlio separato (puoi eliminare pezzi
  indesiderati come pavimenti/luci da studio incluse per errore). Legge il
  `.mtl` associato per i colori originali dei materiali.
- **Illuminazione**: una sorgente di luce posizionabile (ambient + diffuse).
- **Oggetto Camera**: gizmo a piramide che mostra il FOV in Edit; usata come
  punto di vista reale durante il Play (segue anche la rotazione di un
  eventuale genitore, es. un personaggio che gira).
- **Collider** (Box/Sfera/Capsula): forma, dimensioni, centro (offset) e
  rotazione propria configurabili dall'Inspector; wireframe arancione
  disattivabile da Visualizza → Avanzate → Collisioni. Rilevamento
  collisioni preciso per Box-Box (vero test OBB-OBB con SAT, tiene conto
  della rotazione) e Sfera-Sfera/Box-Sfera; approssimato se coinvolge una
  Capsula.

### Script e gameplay
- **Script Python per-oggetto**: crea un file `.py` dal pannello Assets
  ("+ Script"), trascinalo sul campo "Script Python" nell'Inspector. Durante
  il Play, `on_start(engine, obj_id)` e `on_update(engine, obj_id, dt)`
  vengono chiamati automaticamente. Doppio click su un `.py` (o sul campo
  nell'Inspector) lo apre con l'editor di testo predefinito del sistema.
- **API esposta agli script**: posizione/rotazione/nome/script-path di
  qualsiasi oggetto, input tastiera/mouse, `check_collision(id_a, id_b)`.
- **Play/Stop**: vista di gioco a schermo intero (usa la Camera della scena
  se presente). Allo Stop, la scena torna esattamente come prima del Play
  (snapshot automatico di oggetti e nome scena) — le modifiche fatte dagli
  script durante il gioco non restano nella scena che stai editando.
- **Scene multiple** (come i "livelli" di Unity): File → Nuova
  Scena/Salva come/Apri Scena. Richiamabili anche dagli script
  (`engine.load_scene_by_name("Level2")`) per cambiare scena durante il
  gioco, es. quando il giocatore tocca una porta/trigger.

### Assets
- Content Browser con navigazione cartelle (breadcrumb), griglia con icone,
  ricerca, rinomina/elimina, creazione cartelle, import file.

## Struttura

```
GameEngine/
├── CMakeLists.txt              # build principale (scarica GLFW/pybind11/ImGui)
├── engine/                     # core C++ (libreria statica engine_core)
│   ├── CMakeLists.txt
│   ├── include/engine/
│   │   ├── Engine.hpp          # facade: Window+Renderer+EditorUI, tick(),
│   │   │                       # Play/Stop, multi-scena, gizmo, collisioni
│   │   ├── Window.hpp          # wrapper GLFW + init GLAD + input + drop file OS
│   │   ├── Launcher.hpp        # finestra di selezione progetto (pre-editor)
│   │   ├── EditorUI.hpp        # Hierarchy/Inspector/Scena/Assets (ImGui + docking)
│   │   ├── Scene.hpp           # contenitore GameObject (albero, save/load)
│   │   ├── GameObject.hpp      # nodo scena: transform, mesh, luce, camera,
│   │   │                       # colore, collider, script
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
│   └── examples/main.py        # Launcher -> Engine, loop + runtime script Python
├── third_party/
│   └── glad/                   # vedi README in questa cartella: file da generare
└── projects/                   # progetti creati dal Launcher (esclusi da git)
    └── <progetto>/
        ├── assets/             # file importati (.obj, .py, ...)
        └── scenes/             # una scena per file (<nome>.txt)
```

## Prerequisiti

- CMake >= 3.16, compilatore C++17 (su Windows: Visual Studio Build Tools)
- Python 3.8+ con header di sviluppo
- Connessione internet alla prima configurazione (CMake scarica GLFW,
  pybind11 e Dear ImGui via FetchContent, in modalità shallow/leggera)
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

## Scrivere uno script per un oggetto

```python
# Creato dal pannello Assets con "+ Script", poi trascinato sul campo
# "Script Python" nell'Inspector dell'oggetto.

def on_start(engine, obj_id):
    pass  # chiamato una volta, appena entri in Play

def on_update(engine, obj_id, dt):
    x, y, z = engine.get_position(obj_id)
    engine.set_position(obj_id, x, y + dt, z)  # sale di 1 unità al secondo
```

API principali disponibili su `engine` dentro uno script: `get/set_position`,
`get/set_rotation`, `get_all_object_ids`, `get_object_name`, `is_key_pressed`,
`check_collision(id_a, id_b)`, `load_scene_by_name`, `get_current_scene_name`.

## Note di sviluppo

- **GLAD non è incluso nel repository** in chiaro: va generato per la
  combinazione di versione/profilo OpenGL desiderata, segui
  `third_party/glad/README.md`.
- Il formato di salvataggio scena è testuale e volutamente semplice (non
  JSON): un campo per riga, separato da `;`. Ogni nuova feature che aggiunge
  dati per-oggetto ha esteso il formato in modo retrocompatibile (campi
  assenti = valori di default).
- Il color-picking usa uno shader "flat" dedicato (`drawUnlit`/`drawCubeUnlit`)
  separato da quello con illuminazione: il colore-id deve arrivare nel
  framebuffer **esattamente** invariato, senza essere moltiplicato da nessun
  fattore di luce.
- Le collisioni Box-Box usano un vero test OBB-OBB (Separating Axis
  Theorem): tengono conto della rotazione di entrambi gli oggetti. Box-Sfera
  è esatto contro il box orientato. Qualsiasi caso con una Capsula usa
  un'approssimazione a sfera avvolgente (meno precisa).
- Allo Stop del Play, Engine ripristina sia gli oggetti della scena sia il
  **nome** della scena corrente (uno snapshot preso all'avvio del Play):
  necessario perché uno script potrebbe cambiare scena durante il gioco
  (`load_scene_by_name`), e senza ripristinare anche il nome un salvataggio
  successivo scriverebbe il contenuto sbagliato sotto il nome sbagliato.

## Prossimi passi possibili

1. **Controllo camera in Play**: muoverla con WASD/mouse durante il Play
   (attualmente è statica, usa solo la posizione impostata in Edit).
2. **Multi-luce**: oggi viene usata solo la prima luce trovata nella scena.
3. **Smooth shading**: le normali sono calcolate per-triangolo (flat shading);
   per superfici curve più "morbide" servirebbe leggere/mediare le normali
   originali del file `.obj`.
4. **Texture/materiali**: caricamento immagini (es. stb_image) per applicare
   texture invece del solo colore piatto.
5. **Gizmo di rotazione/scala**: oggi il gizmo nel viewport gestisce solo la
   traslazione; rotazione e scala restano editabili solo via numeri.
6. **Collisioni Capsula precise**: oggi qualsiasi test che coinvolge una
   Capsula usa un'approssimazione a sfera avvolgente.
