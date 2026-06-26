"""
Flusso completo: Launcher (scegli/crea progetto) -> Editor 3D.

L'editor ha un vero layout dockabile:
- Hierarchy a sinistra
- Inspector a destra
- Scena al centro (il viewport 3D, con camera orbitale: tasto destro = ruota,
  rotella = zoom, tasto centrale = pan; funziona solo quando il mouse è SOPRA
  il pannello "Scena")
- Assets in basso (lista file della cartella assets/ del progetto, importa
  file copiandoli dentro)
- Menu "File" in alto: Salva Scena (Ctrl+S), Apri Scena, Esci

Eseguire dalla cartella python/ dopo la build:
    python examples/main.py
"""
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

import pyengine


def main():
    # 1. Launcher: l'utente scegli o crea un progetto
    launcher = pyengine.Launcher()
    project_path = launcher.run()

    if not project_path:
        print("Nessun progetto selezionato, uscita.")
        return

    print(f"Progetto selezionato: {project_path}")

    # 2. Editor: si apre sul progetto scelto (carica scene.txt se esiste)
    engine = pyengine.Engine(1600, 900, "Il Mio Game Engine - Editor", project_path)
    engine.set_clear_color(0.12, 0.12, 0.14, 1.0)

    while engine.is_running():
        if engine.is_key_pressed(pyengine.KEY_ESCAPE):
            break
        engine.tick()


if __name__ == "__main__":
    main()
