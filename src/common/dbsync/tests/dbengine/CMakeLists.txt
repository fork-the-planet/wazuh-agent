cmake_minimum_required(VERSION 3.22)

project(dbengine_unit_test)

if(COVERITY)
  add_definitions(-D__GNUC__=8)
endif(COVERITY)

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  set(CMAKE_CXX_FLAGS "-Wall -Wextra -std=c++14 --coverage")
else()
  set(CMAKE_CXX_FLAGS "/W4 /permissive- /std:c++14 /MT /EHsc")
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../include/ ${CMAKE_CURRENT_SOURCE_DIR}/../../../sqliteWrapper/tests/mocks)
link_directories(${CMAKE_BINARY_DIR}/lib)

file(GLOB DBENGINE_UNITTEST_SRC
    "*.cpp")

file(GLOB SQLITE_ENGINE_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/../../src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../src/sqlite/*.cpp")

add_executable(dbengine_unit_test
    ${DBENGINE_UNITTEST_SRC}
    ${SQLITE_ENGINE_SRC})
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_link_libraries(dbengine_unit_test
        dbsync
        GTest::gtest
        GTest::gmock
        GTest::gtest_main
        GTest::gmock_main
        cjson
    )
else()
    target_link_libraries(dbengine_unit_test
        dbsync
        GTest::gtest
        GTest::gmock
        GTest::gtest_main
        GTest::gmock_main
        cjson
        pthread
        dl
    )
endif(CMAKE_SYSTEM_NAME STREQUAL "Windows")

add_test(NAME dbengine_unit_test
         COMMAND dbengine_unit_test)
