"""
Editor 3D con docking, import OBJ multi-oggetto, illuminazione/camera, e
ora anche SCRIPT PYTHON per-oggetto: durante il Play, ogni GameObject con
uno script assegnato (creato dal pannello Assets, trascinato sul campo
"Script Python" nell'Inspector) viene caricato ed eseguito automaticamente.

Formato di uno script:
    def on_start(engine, obj_id):
        ...  # chiamata una volta, appena entri in Play

    def on_update(engine, obj_id, dt):
        ...  # chiamata ogni frame, mentre sei in Play

Dentro lo script puoi usare l'API di "engine" per leggere/modificare la
scena, es: engine.get_position(obj_id), engine.set_position(obj_id, x,y,z).

Eseguire dalla cartella python/ dopo la build:
    python examples/main.py
"""
import sys
import os
import importlib.util

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

import pyengine


# --- Runtime script: carica/esegue gli script Python assegnati agli oggetti ---

_loaded_scripts = {}   # obj_id -> modulo Python caricato
_was_playing = False


def _load_script_module(path, obj_id):
    """Importa un file .py come modulo Python isolato (uno per oggetto, così
    più oggetti possono usare lo stesso file .py senza condividere stato globale)."""
    module_name = f"_user_script_{obj_id}"
    spec = importlib.util.spec_from_file_location(module_name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def update_scripts(engine, dt):
    global _was_playing
    playing = engine.is_playing()

    if playing and not _was_playing:
        # Si è appena entrati in Play: carica gli script di tutti gli oggetti che ne hanno uno
        _loaded_scripts.clear()
        all_ids = engine.get_all_object_ids()
        print(f"[Script] Entrato in Play. Oggetti nella scena: {len(all_ids)}")
        for obj_id in all_ids:
            script_path = engine.get_script_path(obj_id)
            name = engine.get_object_name(obj_id)
            if script_path:
                print(f"[Script] \"{name}\" (id={obj_id}) ha script: {script_path}")
                if not os.path.exists(script_path):
                    print(f"[Script]   ERRORE: il file non esiste piu' a quel percorso!")
                    continue
                try:
                    module = _load_script_module(script_path, obj_id)
                    _loaded_scripts[obj_id] = module
                    has_start = hasattr(module, "on_start")
                    has_update = hasattr(module, "on_update")
                    print(f"[Script]   Caricato OK. on_start={has_start} on_update={has_update}")
                    if has_start:
                        module.on_start(engine, obj_id)
                except Exception as e:
                    print(f"[Script]   ERRORE caricando \"{script_path}\" per \"{name}\": {e}")

    if playing:
        for obj_id, module in list(_loaded_scripts.items()):
            if hasattr(module, "on_update"):
                try:
                    module.on_update(engine, obj_id, dt)
                except Exception as e:
                    name = engine.get_object_name(obj_id)
                    print(f"[Script] Errore in on_update di \"{name}\": {e}")

    if not playing and _was_playing:
        _loaded_scripts.clear()  # usciti dal Play: scarica tutto, si ricarica al prossimo Play

    _was_playing = playing


def main():
    launcher = pyengine.Launcher()
    project_path = launcher.run()

    if not project_path:
        print("Nessun progetto selezionato, uscita.")
        return

    print(f"Progetto selezionato: {project_path}")

    engine = pyengine.Engine(1600, 900, "Il Mio Game Engine - Editor", project_path)
    engine.set_clear_color(0.12, 0.12, 0.14, 1.0)

    while engine.is_running():
        if engine.is_key_pressed(pyengine.KEY_ESCAPE):
            break

        # dt dell'ultimo frame: usato per gli script PRIMA del prossimo tick(),
        # così le modifiche fatte dagli script si vedono già nel render di questo frame.
        dt = engine.get_delta_time()
        update_scripts(engine, dt)

        engine.tick()


if __name__ == "__main__":
    main()
