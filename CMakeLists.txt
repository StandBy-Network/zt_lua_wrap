cmake_minimum_required(VERSION 3.17.0)

project(lua_wrapper)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "--std=c++17 -g -pthread")

set(SOURCES lib/zt_lua_wrap.cc lib/config_reader.cc)
set(TEST_SOURCES app/test.cc)

add_library(zt_lua_wrap STATIC ${SOURCES})

target_include_directories(zt_lua_wrap PUBLIC "./ext/libzt/include")
target_include_directories(zt_lua_wrap PUBLIC "./ext/lua-5.3.5/src")
target_include_directories(zt_lua_wrap PUBLIC "./lib")

add_executable(test ${TEST_SOURCES})

target_include_directories(test PUBLIC "./ext/libzt/include")
target_include_directories(test PUBLIC "./ext/lua-5.3.5/src")
target_include_directories(test PUBLIC "./lib")

target_link_directories(test PUBLIC "./ext/libzt/lib/debug/linux-x86_64")
target_link_directories(test PUBLIC "./ext/lua-5.3.5/src")

target_link_libraries(test zt)
target_link_libraries(test zt_lua_wrap)
target_link_libraries(test lua)
target_link_libraries(test dl)