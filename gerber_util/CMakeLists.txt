file(GLOB_RECURSE UTIL_SOURCES "source/*.cpp")
file(GLOB_RECURSE UTIL_HEADERS "include/*.h")

find_package(OpenCASCADE REQUIRED)

file(GLOB OpenCASCADE_LIBRARIES  "${OpenCASCADE_LIBRARY_DIR}/*.lib")

add_executable(gerber_util
    ${UTIL_SOURCES}
    ${UTIL_HEADERS}
)

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")       # multiprocessor build
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4201")   # allow anonymous structs in unions
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4100")   # unreferenced formal parameter
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4505")   # unreferenced function with internal linkage has been removed
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING")
endif()

target_include_directories(gerber_util PRIVATE ${OpenCASCADE_INCLUDE_DIR})

target_include_directories(gerber_util PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(gerber_util PRIVATE gerber_lib ${OpenCASCADE_LIBRARIES})

target_compile_features(gerber_util PRIVATE cxx_std_20)

if(MSVC)
  target_compile_options(gerber_util PRIVATE /W4 /WX)
else()
  target_compile_options(gerber_util PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${UTIL_SOURCES} ${UTIL_HEADERS})

set_property(TARGET gerber_util PROPERTY VS_DEBUGGER_ENVIRONMENT "PATH=$<$<CONFIG:DEBUG>:${OpenCASCADE_BINARY_DIR}d>$<$<NOT:$<CONFIG:DEBUG>>:${OpenCASCADE_BINARY_DIR}>;%PATH%")

