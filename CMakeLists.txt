cmake_minimum_required(VERSION 3.16)
project(spark-cpp VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(SPARK_BUILD_VIEWER "Build the viewer application" ON)
option(SPARK_BUILD_TESTS "Build tests" OFF)

# --- Dependencies ---

# GLFW
find_package(glfw3 3.3 QUIET)
if(NOT glfw3_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
    )
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glfw)
endif()

# GLM
find_package(glm QUIET)
if(NOT glm_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.1
    )
    FetchContent_MakeAvailable(glm)
endif()

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

# --- Spark Core Library ---
file(GLOB_RECURSE SPARK_CORE_SOURCES
    src/core/*.cpp
    src/formats/*.cpp
    src/sort/*.cpp
    src/raycast/*.cpp
    src/render/*.cpp
    src/shader/*.cpp
    src/scene/*.cpp
    src/controls/*.cpp
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

# --- Viewer Application ---
if(SPARK_BUILD_VIEWER)
    add_executable(spark_viewer src/viewer/main.cpp)
    target_link_libraries(spark_viewer PRIVATE
        spark_core
        glfw
    )
endif()
