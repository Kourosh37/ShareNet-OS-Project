cmake_minimum_required(VERSION 3.16)
project(ShareNetQt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Widgets Network Core QUIET)
if (Qt6_FOUND)
    set(QT_PACKAGE Qt6)
else()
    find_package(Qt5 REQUIRED COMPONENTS Widgets Network Core)
    set(QT_PACKAGE Qt5)
endif()

add_executable(sharenet_qt_client QtClient.cpp)
add_executable(sharenet_qt_server QtServer.cpp)

target_link_libraries(sharenet_qt_client PRIVATE ${QT_PACKAGE}::Widgets ${QT_PACKAGE}::Network ${QT_PACKAGE}::Core)
target_link_libraries(sharenet_qt_server PRIVATE ${QT_PACKAGE}::Widgets ${QT_PACKAGE}::Network ${QT_PACKAGE}::Core)

if (WIN32)
    set_target_properties(sharenet_qt_client sharenet_qt_server PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
