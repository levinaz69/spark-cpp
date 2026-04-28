cmake_minimum_required(VERSION 3.14)
project(spark-cpp VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(SPARK_BUILD_VIEWER "Build the viewer application" ON)
option(SPARK_BUILD_EXAMPLES "Build example applications" ON)
option(SPARK_BUILD_TESTS "Build tests" OFF)

# --- Dependencies (managed by CPM) ---
set(CPM_SOURCE_CACHE "${CMAKE_CURRENT_SOURCE_DIR}/_deps" CACHE PATH "Directory for CPM source cache")
include(cmake/CPM.cmake)

# GLM (header-only math library, needed by core)
CPMAddPackage(
    NAME glm
    GITHUB_REPOSITORY g-truc/glm
    GIT_TAG 1.0.1
)

# OpenGL
find_package(OpenGL REQUIRED)

# GLAD (bundled)
add_library(glad STATIC third_party/glad/src/glad.c)
target_include_directories(glad PUBLIC third_party/glad/include)

# stb (header-only, bundled)
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE third_party/stb)

# miniz (bundled)
add_library(miniz STATIC third_party/miniz/miniz.c)
target_include_directories(miniz PUBLIC third_party/miniz)

# --- Spark Core Library (no GLFW/ImGui dependency) ---
file(GLOB_RECURSE SPARK_CORE_SOURCES
    src/core/*.cpp
    src/formats/*.cpp
    src/sort/*.cpp
    src/raycast/*.cpp
    src/render/*.cpp
    src/shader/*.cpp
    src/scene/*.cpp
    src/lod/*.cpp
    src/utils/*.cpp
    src/constructors/*.cpp
)

add_library(spark_core STATIC ${SPARK_CORE_SOURCES})
target_include_directories(spark_core PUBLIC src)
target_link_libraries(spark_core PUBLIC
    glad
    glm::glm
    OpenGL::GL
    stb
    miniz
)

# --- Interactive targets (viewer, examples) need GLFW ---
if(SPARK_BUILD_VIEWER OR SPARK_BUILD_EXAMPLES)
    # GLFW (windowing, only for interactive targets)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    CPMAddPackage(
        NAME glfw
        GITHUB_REPOSITORY glfw/glfw
        GIT_TAG 3.4
    )
endif()

# --- Viewer Application ---
if(SPARK_BUILD_VIEWER)
    add_executable(spark_viewer
        src/viewer/main.cpp
        src/controls/spark_controls.cpp
    )
    target_link_libraries(spark_viewer PRIVATE
        spark_core
        glfw
    )
endif()

# --- Examples ---
if(SPARK_BUILD_EXAMPLES)
    # Dear ImGui (via CPM)
    CPMAddPackage(
        NAME imgui
        GITHUB_REPOSITORY ocornut/imgui
        GIT_TAG v1.91.8
        DOWNLOAD_ONLY YES
    )
    if(imgui_ADDED)
        add_library(imgui STATIC
            ${imgui_SOURCE_DIR}/imgui.cpp
            ${imgui_SOURCE_DIR}/imgui_demo.cpp
            ${imgui_SOURCE_DIR}/imgui_draw.cpp
            ${imgui_SOURCE_DIR}/imgui_tables.cpp
            ${imgui_SOURCE_DIR}/imgui_widgets.cpp
            ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
            ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
        )
        target_include_directories(imgui PUBLIC
            ${imgui_SOURCE_DIR}
            ${imgui_SOURCE_DIR}/backends
        )
        target_link_libraries(imgui PUBLIC glfw glad OpenGL::GL)
    endif()

    # SOGS Example
    add_executable(spark_sogs_example examples/sogs/main.cpp)
    target_link_libraries(spark_sogs_example PRIVATE
        spark_core
        glfw
        imgui
    )
endif()
