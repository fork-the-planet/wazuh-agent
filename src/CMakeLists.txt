cmake_minimum_required(VERSION 3.22)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
get_filename_component(SRC_FOLDER ${CMAKE_SOURCE_DIR}/../ ABSOLUTE)
get_filename_component(CONFIG_FOLDER ${CMAKE_SOURCE_DIR}/../etc/config/ ABSOLUTE)
project(wazuh-agent)

include(cmake/CommonSettings.cmake)
set_common_settings()

add_subdirectory(common)
add_subdirectory(modules)
add_subdirectory(agent)

add_executable(wazuh-agent agent/src/main.cpp)
target_link_libraries(wazuh-agent Agent Logger)

if(WIN32)
    include(cmake/SetWindowsManifest.cmake)
    set_windows_manifest()
endif()

include(cmake/ConfigureTarget.cmake)
configure_target(wazuh-agent)

## Installation
set(INSTALL_ROOT "" CACHE PATH "Root directory for installation")

if("${INSTALL_ROOT}" STREQUAL "/")
    set(INSTALL_ROOT "")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(BIN_INSTALL_DIR "/usr/share/wazuh-agent/bin")
    set(CONFIG_INSTALL_DIR "/etc/wazuh-agent")
    set(SERVICE_INSTALL_DIR "/usr/lib/systemd/system")
    set(DATA_INSTALL_DIR "/var/lib/wazuh-agent")
    set(LIB_INSTALL_DIR "/usr/share/wazuh-agent/lib")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(BIN_INSTALL_DIR "/usr/share/wazuh-agent/bin")
    set(CONFIG_INSTALL_DIR "/etc/wazuh-agent")
    set(SERVICE_INSTALL_DIR "/etc/systemd/system")
    set(DATA_INSTALL_DIR "/var/lib/wazuh-agent")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(BIN_INSTALL_DIR "C:/Program Files/wazuh-agent/bin")
    set(CONFIG_INSTALL_DIR "C:/Program Files/wazuh-agent/config")
    set(DATA_INSTALL_DIR "C:/Program Files/wazuh-agent/var")
else()
    message(FATAL_ERROR "Not supported OS")
endif()

install(CODE "file(MAKE_DIRECTORY ${INSTALL_ROOT}${BIN_INSTALL_DIR})")

install(TARGETS wazuh-agent
        DESTINATION ${INSTALL_ROOT}${BIN_INSTALL_DIR})

install(CODE "file(MAKE_DIRECTORY ${INSTALL_ROOT}${DATA_INSTALL_DIR})")
install(CODE "file(MAKE_DIRECTORY ${INSTALL_ROOT}${CONFIG_INSTALL_DIR})")
install(FILES ${CONFIG_FOLDER}/wazuh.conf
        DESTINATION ${INSTALL_ROOT}${CONFIG_INSTALL_DIR})

message(STATUS "CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    #add_custom_command(
    #    OUTPUT ${CMAKE_BINARY_DIR}/wazuh-agent.service
    #    COMMAND sed "s|WAZUH_HOME|${INSTALL_ROOT}${BIN_INSTALL_DIR}|g" agent/service/wazuh-agent.service > ${CMAKE_BINARY_DIR}/wazuh-agent.service)
    install(FILES agent/service/wazuh-agent.service
        DESTINATION ${INSTALL_ROOT}${SERVICE_INSTALL_DIR})
    install(FILES
        ${CMAKE_BINARY_DIR}/lib/libdbsync.so
        ${CMAKE_BINARY_DIR}/lib/libsysinfo.so
        DESTINATION ${INSTALL_ROOT}${LIB_INSTALL_DIR})
endif()

if(BUILD_TESTS)
    enable_testing()
endif()
