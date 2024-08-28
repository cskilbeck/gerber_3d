file(GLOB_RECURSE LIB_SOURCES "source/*.cpp")
file(GLOB_RECURSE LIB_HEADERS "include/*.h")

add_library(gerber_lib
    ${LIB_SOURCES}
    ${LIB_HEADERS}
)

target_include_directories(gerber_lib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")       # multiprocessor build
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4201")   # allow anonymous structs in unions
endif()

if(MSVC)
  target_compile_options(gerber_lib PRIVATE /W4 /WX)
else()
  target_compile_options(gerber_lib PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

target_compile_features(gerber_lib PUBLIC cxx_std_20)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${LIB_SOURCES} ${LIB_HEADERS})
