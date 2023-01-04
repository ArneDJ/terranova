cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 20)

project(terranova)

set(EXECUTABLE_OUTPUT_PATH "prototype")

find_package(fmt)
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED COMPONENTS OpenGL)
find_package(GLEW REQUIRED)
find_package(Freetype REQUIRED)
find_package(OpenMP REQUIRED)

include_directories(GLEW::GLEW)
include_directories("src/extern")
include_directories("src/extern/bullet")

file(GLOB all_SRCS
        "${PROJECT_SOURCE_DIR}/src/*.cpp"
        "${PROJECT_SOURCE_DIR}/src/*.h"
        "${PROJECT_SOURCE_DIR}/src/geometry/*.cpp"
        "${PROJECT_SOURCE_DIR}/src/geometry/*.h"
        "${PROJECT_SOURCE_DIR}/src/util/*.cpp"
        "${PROJECT_SOURCE_DIR}/src/util/*.h"
        "${PROJECT_SOURCE_DIR}/src/graphics/*.cpp"
        "${PROJECT_SOURCE_DIR}/src/graphics/*.h"
        "${PROJECT_SOURCE_DIR}/src/physics/*.cpp"
        "${PROJECT_SOURCE_DIR}/src/physics/*.h"
        "${PROJECT_SOURCE_DIR}/src/campaign/*.cpp"
        "${PROJECT_SOURCE_DIR}/src/campaign/*.h"
        "${PROJECT_SOURCE_DIR}/src/battle/*.cpp"
        "${PROJECT_SOURCE_DIR}/src/battle/*.h"
	"${PROJECT_SOURCE_DIR}/src/extern/imgui/*.cpp"
	"${PROJECT_SOURCE_DIR}/src/extern/imgui/*.h"
	"${PROJECT_SOURCE_DIR}/src/extern/fastnoise/*.cpp"
	"${PROJECT_SOURCE_DIR}/src/extern/recast/ChunkyTriMesh.cpp"
	"${PROJECT_SOURCE_DIR}/src/extern/namegen/*.cpp"
)

add_executable(${PROJECT_NAME} ${all_SRCS})
target_link_libraries(${PROJECT_NAME} fmt::fmt)
target_link_libraries(${PROJECT_NAME} pthread)
target_link_libraries(${PROJECT_NAME} OpenMP::OpenMP_CXX)
target_link_libraries(${PROJECT_NAME} dl)
target_link_libraries(${PROJECT_NAME} SDL2::SDL2 SDL2::SDL2main)
target_link_libraries(${PROJECT_NAME} GLEW::GLEW)
target_link_libraries(${PROJECT_NAME} OpenGL::OpenGL)
target_link_libraries(${PROJECT_NAME} Freetype::Freetype)

target_link_libraries(${PROJECT_NAME}  ${CMAKE_SOURCE_DIR}/lib/libBulletDynamics.a)
target_link_libraries(${PROJECT_NAME}  ${CMAKE_SOURCE_DIR}/lib/libBulletCollision.a)
target_link_libraries(${PROJECT_NAME}  ${CMAKE_SOURCE_DIR}/lib/libLinearMath.a)

target_link_libraries(${PROJECT_NAME}  ${CMAKE_SOURCE_DIR}/lib/libRecast.a)
target_link_libraries(${PROJECT_NAME}  ${CMAKE_SOURCE_DIR}/lib/libDetour.a)
target_link_libraries(${PROJECT_NAME}  ${CMAKE_SOURCE_DIR}/lib/libDetourCrowd.a)

target_link_libraries(${PROJECT_NAME}  ${CMAKE_SOURCE_DIR}/lib/libfreetype-gl.a)

target_link_libraries(${PROJECT_NAME}  ${CMAKE_SOURCE_DIR}/lib/libozz_animation.a)
target_link_libraries(${PROJECT_NAME}  ${CMAKE_SOURCE_DIR}/lib/libozz_base.a)
target_link_libraries(${PROJECT_NAME}  ${CMAKE_SOURCE_DIR}/lib/libozz_geometry.a)
