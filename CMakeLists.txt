cmake_minimum_required(VERSION 3.20)

if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

project(qatt LANGUAGES CXX)

option(QATT_STATIC_RUNTIME "Statically link the C++ runtime into executables" ON)

if(MSVC)
    if(QATT_STATIC_RUNTIME)
        set(CMAKE_MSVC_RUNTIME_LIBRARY
            "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    else()
        set(CMAKE_MSVC_RUNTIME_LIBRARY
            "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    endif()
elseif(MINGW AND QATT_STATIC_RUNTIME)
    add_link_options(-static)
endif()

add_library(qatt_core STATIC
    src/mapping.cpp
    src/telex.cpp
)

target_compile_features(qatt_core PUBLIC cxx_std_20)
target_include_directories(qatt_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

if(MSVC)
    target_compile_options(qatt_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(qatt_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/data/mapping.tsv"
    "${CMAKE_CURRENT_BINARY_DIR}/mapping.tsv"
    COPYONLY
)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/qatt.ico"
    "${CMAKE_CURRENT_BINARY_DIR}/qatt.ico"
    COPYONLY
)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config.ini"
    "${CMAKE_CURRENT_BINARY_DIR}/config.ini"
    COPYONLY
)

add_executable(qatt_cli src/main.cpp)
target_link_libraries(qatt_cli PRIVATE qatt_core)

if(WIN32)
    add_executable(qatt_tray WIN32 src/windows_tray.cpp)
    target_link_libraries(qatt_tray PRIVATE qatt_core shell32 user32 winmm)
    if(MSVC)
        target_compile_options(qatt_tray PRIVATE /W4 /permissive-)
    else()
        target_compile_options(qatt_tray PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    if(MINGW)
        target_link_options(qatt_tray PRIVATE -municode)
    endif()
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(qatt_tests tests/tests.cpp)
    target_link_libraries(qatt_tests PRIVATE qatt_core)
    add_test(NAME qatt_tests COMMAND qatt_tests
        "${CMAKE_CURRENT_BINARY_DIR}/mapping.tsv")
endif()
