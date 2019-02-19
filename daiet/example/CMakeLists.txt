cmake_minimum_required(VERSION 3.5)

set(USE_MLX5 OFF)
set(DEBUG OFF)

# Options
option(USE_MLX5 "Use MLX5 and ibverbs" ${USE_MLX5_DEFAULT})
option(DEBUG "Compile in debug mode" OFF)

if(DEBUG)
    message(WARNING "Compile in debug mode")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fPIC -g -O0")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fPIC -O3 -DNDEBUG")
endif()

add_definitions("-DHIREDIS_NESTED_INCLUDE")

include_directories(${CMAKE_SOURCE_DIR}/../build/include)
include_directories(${CMAKE_SOURCE_DIR}/../..)
link_directories(${CMAKE_SOURCE_DIR}/../build)
link_directories(${CMAKE_SOURCE_DIR}/../lib/dpdk/build/lib)

add_executable(example main.cc)

target_link_libraries(example -Wl,--whole-archive daiet dpdk -Wl,--no-whole-archive dl numa boost_chrono boost_system boost_thread boost_program_options pthread)

if(USE_MLX5)
    target_link_libraries(example ibverbs mlx5 mnl)
endif()
