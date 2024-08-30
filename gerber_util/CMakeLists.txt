file(GLOB_RECURSE UTIL_SOURCES "source/*.cpp")
file(GLOB_RECURSE UTIL_HEADERS "include/*.h")

add_library(gerber_util
    ${UTIL_SOURCES}
    ${UTIL_HEADERS}
)

target_include_directories(gerber_util PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")       # multiprocessor build
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4201")   # allow anonymous structs in unions
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4100")   # unreferenced formal parameter
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4505")   # unreferenced function with internal linkage has been removed
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING")
endif()

if(MSVC)
  target_compile_options(gerber_util PRIVATE /W4 /WX)
else()
  target_compile_options(gerber_util PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

target_compile_features(gerber_util PUBLIC cxx_std_20)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${UTIL_SOURCES} ${UTIL_HEADERS})
