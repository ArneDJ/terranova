cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 14)

project(terranova)

set(EXECUTABLE_OUTPUT_PATH "prototype")

file(GLOB all_SRCS
        "${PROJECT_SOURCE_DIR}/src/*.cpp"
        "${PROJECT_SOURCE_DIR}/src/*.h"
        "${PROJECT_SOURCE_DIR}/src/extern/loguru/*.cpp"
        "${PROJECT_SOURCE_DIR}/src/extern/loguru/*.h"
)

add_executable(${PROJECT_NAME} ${all_SRCS})
target_link_libraries(${PROJECT_NAME} pthread)
target_link_libraries(${PROJECT_NAME} dl)

