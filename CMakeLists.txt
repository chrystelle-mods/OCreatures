cmake_minimum_required(VERSION 3.21)
message("Using toolchain file ${CMAKE_TOOLCHAIN_FILE}.")

project(
    OCreatures
    VERSION 2.0.0
    DESCRIPTION "OCreatures - creature support for OStim Standalone."
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

macro(set_from_environment VARIABLE)
    if (NOT DEFINED ${VARIABLE} AND DEFINED ENV{${VARIABLE}})
        set(${VARIABLE} $ENV{${VARIABLE}})
    endif ()
endmacro()
set_from_environment(VCPKG_ROOT)

add_compile_definitions(SKYRIM)

########################################################################################################################
## CommonLibSSE-NG (alandtse/CommonLibVR branch ng). Uses the extern/CommonLibSSE-NG submodule when present,
## otherwise the shared v8.4.0 clone.
########################################################################################################################
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/extern/CommonLibSSE-NG/CMakeLists.txt")
    set(COMMONLIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/CommonLibSSE-NG" CACHE PATH "CommonLibSSE-NG source")
else ()
    set(COMMONLIB_DIR "D:/Development/CommonLibSSE-NG" CACHE PATH "CommonLibSSE-NG source")
endif ()
message(STATUS "CommonLibSSE-NG: ${COMMONLIB_DIR}")

set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(${COMMONLIB_DIR} CommonLibSSE EXCLUDE_FROM_ALL)

########################################################################################################################
## Plugin
########################################################################################################################
set(sources
    src/main.cpp
    src/SkinMap.cpp
    src/Swaps.cpp
    src/OStimBridge.cpp
    src/BeastStart.cpp
)

add_library(${PROJECT_NAME} SHARED ${sources}
    src/PCH.h src/SkinMap.h src/Swaps.h src/OStimBridge.h src/BeastStart.h)

target_include_directories(${PROJECT_NAME}
    PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/extern/OStim>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/extern/SKSEMenuFramework>)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_compile_options(${PROJECT_NAME} PRIVATE /Zc:preprocessor)

find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE CommonLibSSE::CommonLibSSE nlohmann_json::nlohmann_json)

target_precompile_headers(${PROJECT_NAME} PRIVATE src/PCH.h)

########################################################################################################################
## Deploy the DLL + PDB into the MO2 mod folder
########################################################################################################################
set(OUTPUT_FOLDER "C:/Steam/steamapps/common/Skyrim Special Edition/ModOrganizer Data/mods/OCreatures"
    CACHE PATH "Mod folder to deploy the built DLL/PDB into")
set(DLL_FOLDER "${OUTPUT_FOLDER}/SKSE/Plugins")
message(STATUS "SKSE plugin output folder: ${DLL_FOLDER}")

add_custom_command(
    TARGET ${PROJECT_NAME}
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${DLL_FOLDER}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${PROJECT_NAME}>" "${DLL_FOLDER}/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_PDB_FILE:${PROJECT_NAME}>" "${DLL_FOLDER}/$<TARGET_PDB_FILE_NAME:${PROJECT_NAME}>"
    VERBATIM)
