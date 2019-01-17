cmake_minimum_required(VERSION 3.5)

set(USE_VANILLA_DEFAULT OFF)
set(USE_MLX5 OFF)

# Options
option(USE_VANILLA "Use vanilla version of gloo" ${USE_VANILLA_DEFAULT})
option(USE_MLX5 "Use MLX5 and ibverbs" ${USE_MLX5_DEFAULT})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fPIC -O3 -DNDEBUG")
add_definitions("-DHIREDIS_NESTED_INCLUDE")

link_directories(${CMAKE_SOURCE_DIR}/../../build/gloo/)
include_directories(${CMAKE_SOURCE_DIR}/../../build)
include_directories(${CMAKE_SOURCE_DIR}/../../)

if(NOT USE_VANILLA)
    include_directories(${CMAKE_SOURCE_DIR}/../../daiet/build/include)
    link_directories(${CMAKE_SOURCE_DIR}/../../daiet/build)
    link_directories(${CMAKE_SOURCE_DIR}/../../daiet/lib/dpdk/build/lib)
else()
    message(WARNING "Compiling with vanilla gloo")
endif()

add_executable(exp1 main.cc)

target_link_libraries(exp1 gloo hiredis pthread)

if(NOT USE_VANILLA)
    target_link_libraries(exp1 -Wl,--whole-archive daiet dpdk -Wl,--no-whole-archive dl numa boost_chrono boost_system boost_thread boost_program_options)
endif()

if(USE_MLX5)
    target_link_libraries(exp1 ibverbs mlx5 mnl)
endif()
