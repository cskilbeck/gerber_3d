file(GLOB_RECURSE UTIL_SOURCES "source/*.cpp")
file(GLOB_RECURSE UTIL_HEADERS "include/*.h")

find_package(OpenCASCADE REQUIRED)

file(GLOB OpenCASCADE_LIBRARIES  "${OpenCASCADE_LIBRARY_DIR}/*.lib")

add_executable(gerber_util
    ${UTIL_SOURCES}
    ${UTIL_HEADERS}
)

target_include_directories(gerber_util PRIVATE ${OpenCASCADE_INCLUDE_DIR})

target_include_directories(gerber_util PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(gerber_util PRIVATE gerber_lib ${OpenCASCADE_LIBRARIES})

target_compile_features(gerber_util PRIVATE cxx_std_20)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${UTIL_SOURCES} ${UTIL_HEADERS})

set_property(TARGET gerber_util PROPERTY VS_DEBUGGER_ENVIRONMENT "PATH=$<$<CONFIG:DEBUG>:${OpenCASCADE_BINARY_DIR}d>$<$<NOT:$<CONFIG:DEBUG>>:${OpenCASCADE_BINARY_DIR}>;%PATH%")

