
# Target
set(target antianti-painters)
message(STATUS "Example ${target}")


# External libraries

# ...
find_package(assimp REQUIRED)


# Includes

include_directories(
    BEFORE
    ${ASSIMP_INCLUDE_DIRS}
    "C:/projects/glkernel/source/glkernel/include"
    ${CMAKE_CURRENT_SOURCE_DIR}
)


# Libraries

set(libs
    ${GLEXAMPLES_DEPENDENCY_LIBRARIES}
    ${ASSIMP_LIBRARIES}
)


# Compiler definitions

# for compatibility between glm 0.9.4 and 0.9.5
add_definitions("-DGLM_FORCE_RADIANS")


# Sources

set(include_path "${CMAKE_CURRENT_SOURCE_DIR}/")
set(source_path "${CMAKE_CURRENT_SOURCE_DIR}/")

set(sources
    ${source_path}/plugin.cpp
    ${source_path}/antianti/AntiAnti.cpp
    ${source_path}/antianti/ImageLoader.cpp
    ${source_path}/antianti/PerfCounter.cpp
    ${source_path}/antianti/PostProcessing.cpp
    ${source_path}/antianti/SceneLoader.cpp
)

set(api_includes
    ${include_path}/antianti/AntiAnti.h
    ${include_path}/antianti/ImageLoader.h
    ${include_path}/antianti/PerfCounter.h
    ${include_path}/antianti/PostProcessing.h
    ${include_path}/antianti/SceneLoader.h
)

# Group source files
set(header_group "Header Files (API)")
set(source_group "Source Files")
source_group_by_path(${include_path} "\\\\.h$|\\\\.hpp$"
    ${header_group} ${api_includes})
source_group_by_path(${source_path} "\\\\.cpp$|\\\\.c$|\\\\.h$|\\\\.hpp$"
    ${source_group} ${sources})


# Build library

add_library(${target} SHARED ${api_includes} ${sources})

target_link_libraries(${target} ${libs})

target_compile_options(${target} PRIVATE ${DEFAULT_COMPILE_FLAGS})

set_target_properties(${target}
    PROPERTIES
    LINKER_LANGUAGE              CXX
    FOLDER                      "${IDE_FOLDER}"
    COMPILE_DEFINITIONS_DEBUG   "${DEFAULT_COMPILE_DEFS_DEBUG}"
    COMPILE_DEFINITIONS_RELEASE "${DEFAULT_COMPILE_DEFS_RELEASE}"
    LINK_FLAGS_DEBUG            "${DEFAULT_LINKER_FLAGS_DEBUG}"
    LINK_FLAGS_RELEASE          "${DEFAULT_LINKER_FLAGS_RELEASE}"
    DEBUG_POSTFIX               "d${DEBUG_POSTFIX}"
    INCLUDE_PATH                ${include_path})


# Deployment

# Plugin library (release)
install(TARGETS ${target}
    RUNTIME DESTINATION ${INSTALL_PLUGINS}       CONFIGURATIONS Release
    LIBRARY DESTINATION ${INSTALL_PLUGINS}       CONFIGURATIONS Release
    ARCHIVE DESTINATION ${INSTALL_LIB}
)

# Plugin library (debug)
install(TARGETS ${target}
    RUNTIME DESTINATION ${INSTALL_PLUGINS_DEBUG} CONFIGURATIONS Debug
    LIBRARY DESTINATION ${INSTALL_PLUGINS_DEBUG} CONFIGURATIONS Debug
)
