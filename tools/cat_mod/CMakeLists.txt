cmake_minimum_required(VERSION 3.25)
project(cat_carry_mod CXX)

set(DUSK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." CACHE PATH "Path to dusk source root")
add_subdirectory("${DUSK_DIR}" dusk EXCLUDE_FROM_ALL)

set(DUSK_MODS_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/mods" CACHE PATH "Directory to write .dusk packages into")

add_dusk_mod(cat_carry_mod
    SOURCES  src/mod.cpp
    MOD_JSON mod.json
    RES_DIR  res
)
