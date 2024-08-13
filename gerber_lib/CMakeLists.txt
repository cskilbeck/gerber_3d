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

target_compile_features(gerber_lib PUBLIC cxx_std_20)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${LIB_SOURCES} ${LIB_HEADERS})
