# main project

set(PROJECT gerber_explorer)

file(GLOB_RECURSE PROJECT_SOURCES "source/*.cpp")
file(GLOB_RECURSE PROJECT_HEADERS "include/*.h")

add_executable(${PROJECT}
    ${PROJECT_SOURCES}
    ${PROJECT_HEADERS}
)

target_include_directories(${PROJECT} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")       # multiprocessor build
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4201")   # allow anonymous structs in unions
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4100")   # unreferenced formal parameter
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4505")   # unreferenced function with internal linkage has been removed
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D_MBCS")
    target_compile_options(${PROJECT} PRIVATE /W4 /WX)
else()
    target_compile_options(${PROJECT} PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

target_compile_features(${PROJECT} PRIVATE cxx_std_20)

# OpenCascade

find_package(OpenCASCADE REQUIRED)

file(GLOB OpenCASCADE_LIBRARIES  "${OpenCASCADE_LIBRARY_DIR}/*.lib")

target_include_directories(${PROJECT} PRIVATE ${OpenCASCADE_INCLUDE_DIR})

# ImGui

set(IMGUI_PATH "" CACHE PATH "Path to ImGui")

if(NOT EXISTS "${IMGUI_PATH}")
    message(SEND_ERROR "The ImGui path '${IMGUI_PATH}' does not exist or is invalid")
endif()

file(GLOB IMGUI_SOURCES
    ${IMGUI_PATH}/*.cpp
    ${IMGUI_PATH}/backends/imgui_impl_opengl3.cpp
    ${IMGUI_PATH}/backends/imgui_impl_win32.cpp
)

add_library("ImGui" STATIC ${IMGUI_SOURCES})

target_include_directories("ImGui" PUBLIC
    ${IMGUI_PATH}
    ${IMGUI_PATH}/backends
)

# link

target_link_libraries(${PROJECT} PRIVATE
    gerber_lib
    gerber_util
    ImGui
    ${OpenCASCADE_LIBRARIES}
)

# IDE

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${PROJECT_SOURCES} ${PROJECT_HEADERS})

set_property(TARGET ${PROJECT} PROPERTY VS_DEBUGGER_ENVIRONMENT 
    "PATH=$<$<CONFIG:DEBUG>:${OpenCASCADE_BINARY_DIR}d>$<$<NOT:$<CONFIG:DEBUG>>:${OpenCASCADE_BINARY_DIR}>;%PATH%")

set_property(TARGET ${PROJECT} PROPERTY CHARACTER_SET MultiByte) 