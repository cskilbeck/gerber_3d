set(PROJECT gerber_lib)

file(GLOB_RECURSE PROJECT_SOURCES "source/*.cpp")
file(GLOB_RECURSE PROJECT_HEADERS "include/*.h")

add_library(${PROJECT}
    ${PROJECT_SOURCES}
    ${PROJECT_HEADERS}
)

target_include_directories(${PROJECT} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")       # multiprocessor build
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4201")   # allow anonymous structs in unions
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4100")   # unreferenced formal parameter
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4505")   # unreferenced function with internal linkage has been removed
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING")
    target_compile_options(${PROJECT} PRIVATE /W4 /WX)
else()
    target_compile_options(${PROJECT} PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

target_compile_features(${PROJECT} PUBLIC cxx_std_20)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${PROJECT_SOURCES} ${PROJECT_HEADERS})

target_link_libraries(${PROJECT} PRIVATE gerber_util)
