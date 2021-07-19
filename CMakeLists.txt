cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 14)

project(terranova)

set(EXECUTABLE_OUTPUT_PATH "prototype")

find_package(SDL2 REQUIRED)

find_package(OpenGL REQUIRED COMPONENTS OpenGL)

find_package(GLEW REQUIRED)
include_directories(GLEW::GLEW)

file(GLOB all_SRCS
        "${PROJECT_SOURCE_DIR}/src/*.cpp"
        "${PROJECT_SOURCE_DIR}/src/*.h"
        "${PROJECT_SOURCE_DIR}/src/util/*.cpp"
        "${PROJECT_SOURCE_DIR}/src/util/*.h"
        "${PROJECT_SOURCE_DIR}/src/gpu/*.cpp"
        "${PROJECT_SOURCE_DIR}/src/gpu/*.h"
        "${PROJECT_SOURCE_DIR}/src/extern/loguru/*.cpp"
        "${PROJECT_SOURCE_DIR}/src/extern/loguru/*.h"
	"${PROJECT_SOURCE_DIR}/src/extern/imgui/*.cpp"
	"${PROJECT_SOURCE_DIR}/src/extern/imgui/*.h"
)

add_executable(${PROJECT_NAME} ${all_SRCS})
target_link_libraries(${PROJECT_NAME} pthread)
target_link_libraries(${PROJECT_NAME} dl)
target_link_libraries(${PROJECT_NAME} SDL2::SDL2 SDL2::SDL2main)
target_link_libraries(${PROJECT_NAME} GLEW::GLEW)
target_link_libraries(${PROJECT_NAME} OpenGL::OpenGL)

