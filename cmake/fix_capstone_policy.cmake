# Removes cmake_policy(SET CMP0048 OLD) from capstone 4.0.2 CMakeLists.txt
file(READ "${DIR}/CMakeLists.txt" _content)
string(REPLACE
    "cmake_policy(SET CMP0048 OLD)"
    "# cmake_policy(SET CMP0048 OLD)"
    _content "${_content}")
file(WRITE "${DIR}/CMakeLists.txt" "${_content}")
