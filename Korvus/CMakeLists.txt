cmake_minimum_required(VERSION 3.16)

project(Korvus LANGUAGES CXX)



set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)


find_package(Qt6 REQUIRED COMPONENTS Widgets)


add_executable(Korvus
    src/main.cpp
    src/mainwindow.h
    src/mainwindow.cpp
    src/firewallmanager.h
    src/firewallmanager.cpp
    src/auditmanager.h
    src/auditmanager.cpp
    src/rule.h
    src/resources.qrc
)



target_link_libraries(Korvus
    PRIVATE
        Qt6::Widgets
)


install(
    TARGETS Korvus
    RUNTIME DESTINATION /usr/bin
)

install(
    FILES src/app_icon.png
    DESTINATION /usr/share/icons/hicolor/512x512/apps
    RENAME korvus.png
)

install(
    FILES korvus.desktop
    DESTINATION /usr/share/applications
)

install(
    FILES src/fastwall.policy
    DESTINATION /usr/share/polkit-1/actions
    RENAME com.korvus.policy
)



set(CPACK_PACKAGE_NAME
    "korvus"
)

set(CPACK_PACKAGE_VERSION
    "1.0.0"
)

set(CPACK_PACKAGE_CONTACT
    "Korvus Project"
)

set(CPACK_DEBIAN_PACKAGE_MAINTAINER
    "Korvus Project"
)

set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE
    "amd64"
)

set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Korvus Linux Host Firewall"
)

set(CPACK_PACKAGE_DESCRIPTION
    "Korvus is a graphical Linux host-based firewall."
)


set(CPACK_GENERATOR
    "DEB"
)

include(CPack)


file(
    READ
    "${CMAKE_BINARY_DIR}/CPackConfig.cmake"
    KORVUS_CPACK_BASE
)

file(
    WRITE
    "${CMAKE_BINARY_DIR}/CPackUbuntuConfig.cmake"
    "${KORVUS_CPACK_BASE}
set(CPACK_PACKAGE_FILE_NAME \"korvus_1.0.0_ubuntu_amd64\")
set(CPACK_DEBIAN_PACKAGE_DEPENDS \"libqt6widgets6, policykit-1\")
"
)



file(
    WRITE
    "${CMAKE_BINARY_DIR}/CPackDebianConfig.cmake"
    "${KORVUS_CPACK_BASE}
set(CPACK_PACKAGE_FILE_NAME \"korvus_1.0.0_debian_amd64\")
set(CPACK_DEBIAN_PACKAGE_DEPENDS \"libqt6widgets6, polkitd\")
"
)


message(STATUS "")
message(STATUS "==============================================")
message(STATUS "Korvus packaging configuration")
message(STATUS "==============================================")

message(STATUS "Ubuntu package:")
message(STATUS "  korvus_1.0.0_ubuntu_amd64.deb")
message(STATUS "  Dependencies:")
message(STATUS "    libqt6widgets6")
message(STATUS "    policykit-1")

message(STATUS "")

message(STATUS "Debian package:")
message(STATUS "  korvus_1.0.0_debian_amd64.deb")
message(STATUS "  Dependencies:")
message(STATUS "    libqt6widgets6")
message(STATUS "    polkitd")

message(STATUS "")

message(STATUS "Build:")
message(STATUS "  cmake --build .")

message(STATUS "")

message(STATUS "Ubuntu package:")
message(STATUS "  cpack --config CPackUbuntuConfig.cmake")

message(STATUS "")

message(STATUS "Debian package:")
message(STATUS "  cpack --config CPackDebianConfig.cmake")

message(STATUS "==============================================")