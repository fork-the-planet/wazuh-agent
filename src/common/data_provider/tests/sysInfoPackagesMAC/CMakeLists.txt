cmake_minimum_required(VERSION 3.22)

project(sysInfoMacPackage_unit_test)

# Copy input files to the build directory
file(COPY input_files DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

set(CMAKE_CXX_FLAGS_DEBUG "-g --coverage")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../mocks)

file(GLOB sysinfo_UNIT_TEST_SRC
    "*.cpp")

file(GLOB SYSINFO_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/../../src/packages/packageMac.cpp")

add_executable(sysInfoMacPackage_unit_test
    ${sysinfo_UNIT_TEST_SRC}
    ${SYSINFO_SRC})

target_link_libraries(sysInfoMacPackage_unit_test PRIVATE
    sysinfo
    GTest::gtest
    GTest::gmock
    GTest::gtest_main
    GTest::gmock_main
)

add_test(NAME sysInfoMacPackage_unit_test
         COMMAND sysInfoMacPackage_unit_test)
