set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread")
add_definitions(-DHIREDIS_NESTED_INCLUDE)
add_executable(example1 example1.cc)
target_link_libraries(example1 gloo daiet $ENV{RTE_SDK}/install/lib/libdpdk.a dl numa boost_chrono boost_system boost_thread boost_program_options hiredis)
