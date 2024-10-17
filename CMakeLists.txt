cmake_minimum_required (VERSION 2.8)

# 设定项目名称和使用的C++标准
project(macdaatr)
set(CMAKE_CXX_STANDARD 11)

# 设定交叉编译工具链（如果不是在CMake工具链文件中指定的话）
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# set(CMAKE_CXX_COMPILER g++)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)

# 设定编译选项，可以根据需求修改
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w -Wextra")

set(SRC_DIR ${PROJECT_SOURCE_DIR}/sources)
set(INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)
set(OBJ_DIR ${PROJECT_SOURCE_DIR}/obj)

# 设定包含头文件的路径
include_directories(${INCLUDE_DIR})

# 查找所有的源文件
file(GLOB SOURCES ${SRC_DIR}/*.cpp)

# 设定目标可执行文件，并链接源文件
add_executable(${PROJECT_NAME} ${SOURCES})

# 指定目标生成的可执行文件名
set_target_properties(
    ${PROJECT_NAME} PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}
    OUTPUT_NAME macdaatr
)

# 设置生成的对象文件输出路径
# set_target_properties(
#     ${PROJECT_NAME} PROPERTIES
#     RUNTIME_OUTPUT_DIRECTORY ${OBJ_DIR}
# )

# 链接pthread和rt库
target_link_libraries(${PROJECT_NAME} pthread rt)

# 添加自定义的clean目标
add_custom_target(clean
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${PROJECT_SOURCE_DIR}/obj
    COMMAND ${CMAKE_COMMAND} -E remove ${PROJECT_SOURCE_DIR}/macdaatr
)