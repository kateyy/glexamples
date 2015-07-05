
# Generate meta-header
configure_file(glexamples-version.h.in ${CMAKE_CURRENT_BINARY_DIR}/glexamples-version.h)

# We do NOT want to deploy this header (at least not for binary packages)
#install(FILES ${CMAKE_CURRENT_BINARY_DIR}/template/template-version.h DESTINATION ${INSTALL_INCLUDE}/template)

# External libraries
find_package(OpenGL REQUIRED)
find_package(GLM REQUIRED)
find_package(glbinding REQUIRED)
find_package(globjects REQUIRED)
find_package(gloperate REQUIRED)
#find_package(glkernel REQUIRED)
find_package(libzeug REQUIRED)

# hack
set(GLKERNEL_INCLUDES "C:/Libraries/glkernel/source/glkernel/include" CACHE PATH "GLKERNEL include path")


# Qt5

# good resource: http://www.kdab.com/using-cmake-with-qt-5/

# http://qt-project.org/forums/viewthread/30006/
if(MSVC)
    cmake_policy(SET CMP0020 NEW)
endif()

find_package(Qt5OpenGL  5.4 REQUIRED)
find_package(Qt5Core    5.4 REQUIRED)
# find_package(Qt5Gui     5.4 REQUIRED)
find_package(Qt5Widgets 5.4 REQUIRED)

# Qt5 options

set(CMAKE_AUTOMOC ON)

if(MSVC)
    # -> msvc14 : declaration hides class member (problem in qt)
    set(DEFAULT_COMPILE_FLAGS ${DEFAULT_COMPILE_FLAGS} /wd4458)
endif()




# Include directories
include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}

    ${GLM_INCLUDE_DIR}
    ${GLBINDING_INCLUDES}
    ${GLOBJECTS_INCLUDES}
    ${GLOPERATE_INCLUDES}
    ${GLKERNEL_INCLUDES}
    ${LIBZEUG_INCLUDES}
)

# Define common libraries for all examples
set(GLEXAMPLES_DEPENDENCY_LIBRARIES
    ${GLBINDING_LIBRARIES}
    ${GLOBJECTS_LIBRARIES}
    ${GLOPERATE_LIBRARIES}
    ${LIBZEUG_LIBRARIES}
    Qt5::Core
    #Qt5::Gui
    Qt5::Widgets
    Qt5::OpenGL
)

# Applications
set(IDE_FOLDER "")
add_subdirectory(antianti)
add_subdirectory(emptyexample)
add_subdirectory(openglexample)
add_subdirectory(transparency)
add_subdirectory(progressive_transparency)
add_subdirectory(glexamples-viewer)

# Tests
set(IDE_FOLDER "Tests")
add_subdirectory(tests)
