file(READ "${SOURCE_DIR}/cmake/capstone.cmake.in" _content)

# Insert PATCH_COMMAND before CONFIGURE_COMMAND in the ExternalProject_Add.
# Bracket args prevent cmake from substituting ${...} while writing this file.
string(REPLACE
    "    CONFIGURE_COMMAND \"\""
    [=[    PATCH_COMMAND "${CMAKE_COMMAND}" -DDIR="${CMAKE_CURRENT_BINARY_DIR}/capstone-src" -P "${CAPSTONE_FIX_SCRIPT}"
    CONFIGURE_COMMAND ""]=]
    _content "${_content}")

file(WRITE "${SOURCE_DIR}/cmake/capstone.cmake.in" "${_content}")
