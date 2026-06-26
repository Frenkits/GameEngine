# GLAD — file da generare

GLAD non si scarica via git/cmake: si genera online (oppure con il tool python glad2)
per la combinazione esatta di versione OpenGL + estensioni che vuoi.

## Come generarlo (Core profile 3.3)

1. Vai su https://gen.glad.sh/ (o https://glad.dav1d.de/ per la versione legacy)
2. Imposta:
   - Language: C/C++
   - Specification: OpenGL
   - API gl: Version 3.3
   - Profile: Core
   - Generator: C
3. Clicca "GENERATE", scarica lo zip.
4. Copia i file nello zip dentro questa cartella, mantenendo questa struttura:

```
third_party/glad/
├── CMakeLists.txt        (già presente)
├── include/
│   ├── glad/glad.h
│   └── KHR/khrplatform.h
└── src/
    └── glad.c
```

5. Fatto: il CMakeLists.txt già pronto compila automaticamente `src/glad.c`
   come libreria statica "glad" e la espone con gli include path corretti.

Alternativa via pip (genera in locale, comodo da riusare):

```bash
pip install glad2 --break-system-packages
python -m glad --profile core --api gl=3.3 --generator c --out-path third_party/glad
```

(con glad2 la struttura cartelle generata corrisponde già a quella sopra)
