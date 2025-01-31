cmake_minimum_required(VERSION 3.22)

project(Communicator)

include(../../cmake/CommonSettings.cmake)
set_common_settings()

find_package(OpenSSL REQUIRED)
find_package(Boost REQUIRED COMPONENTS asio beast system url)
find_package(nlohmann_json CONFIG REQUIRED)
find_path(JWT_CPP_INCLUDE_DIRS "jwt-cpp/base.h")

if(WIN32)
    set(VERIFY_UTILS_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src/https_socket_verify_utils_win.cpp")
    list(APPEND VERIFY_UTILS_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src/https_verifier_win.cpp")
elseif(APPLE)
    set(VERIFY_UTILS_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src/https_socket_verify_utils_mac.cpp")
    list(APPEND VERIFY_UTILS_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src/https_verifier_mac.cpp")
else()
    set(VERIFY_UTILS_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src/https_socket_verify_utils_lin.cpp")
endif()

add_library(Communicator src/communicator.cpp src/http_client.cpp src/http_request_params.cpp ${VERIFY_UTILS_FILE})

target_include_directories(Communicator PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    SYSTEM PRIVATE
    ${JWT_CPP_INCLUDE_DIRS})

target_compile_definitions(Communicator PRIVATE -DJWT_DISABLE_PICOJSON=ON)
target_link_libraries(Communicator PUBLIC Config ConfigurationParser Boost::asio Boost::beast Boost::system Boost::url Logger PRIVATE OpenSSL::SSL OpenSSL::Crypto nlohmann_json::nlohmann_json)

if(WIN32)
    target_link_libraries(Communicator PRIVATE Crypt32)
elseif(APPLE)
    target_link_libraries(Communicator PRIVATE "-framework Security" "-framework CoreFoundation")
endif()

include(../../cmake/ConfigureTarget.cmake)
configure_target(Communicator)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
