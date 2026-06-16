cmake_minimum_required(VERSION 3.16)
project(ShareNetFltk LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(FLTK CONFIG QUIET)
if (NOT FLTK_FOUND)
    find_package(FLTK REQUIRED)
endif()

set(COMMON_SRC
    ../common/protocol.c
    ../common/socket_utils.c
    ../common/file_utils.c
    ../common/chunk.c
)

add_executable(sharenet_fltk_client FltkClient.cpp ${COMMON_SRC})
add_executable(sharenet_fltk_server FltkServer.cpp ${COMMON_SRC})

target_include_directories(sharenet_fltk_client PRIVATE ../../include ${FLTK_INCLUDE_DIRS})
target_include_directories(sharenet_fltk_server PRIVATE ../../include ${FLTK_INCLUDE_DIRS})

foreach(target sharenet_fltk_client sharenet_fltk_server)
    set(SHARENET_FLTK_TARGETS "")
    foreach(candidate fltk fltk_forms fltk_images fltk::fltk FLTK::FLTK)
        if (TARGET ${candidate})
            list(APPEND SHARENET_FLTK_TARGETS ${candidate})
        endif()
    endforeach()

    if (SHARENET_FLTK_TARGETS)
        target_link_libraries(${target} PRIVATE ${SHARENET_FLTK_TARGETS})
    else()
        target_link_libraries(${target} PRIVATE ${FLTK_LIBRARIES})
    endif()
    if (WIN32)
        target_link_libraries(${target} PRIVATE ws2_32)
        set_target_properties(${target} PROPERTIES WIN32_EXECUTABLE TRUE)
    endif()
endforeach()
