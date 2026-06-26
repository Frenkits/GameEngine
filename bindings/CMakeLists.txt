pybind11_add_module(pyengine bindings.cpp)

target_link_libraries(pyengine PRIVATE engine_core)

# Copia automaticamente il modulo compilato (.so/.pyd) nella cartella python/
# così "import pyengine" funziona subito dagli esempi senza installazione.
set_target_properties(pyengine PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/python
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/python
)

# Su generatori multi-config (Visual Studio, Xcode) le proprietà sopra vengono
# ignorate e Cmake aggiunge sempre una sottocartella Debug/Release/... :
# qui la fissiamo esplicitamente per ogni configurazione possibile.
foreach(OUTPUTCONFIG Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG_UPPER)
    set_target_properties(pyengine PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER} ${CMAKE_SOURCE_DIR}/python
        RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG_UPPER} ${CMAKE_SOURCE_DIR}/python
    )
endforeach()
