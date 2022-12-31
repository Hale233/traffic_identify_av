cmake_minimum_required (VERSION 2.8)
set (CMAKE_CXX_STANDARD 11)
set(plugin_name traffic_identify_av)

project (${plugin_name})

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
#include(Version)

set(CMAKE_MACOSX_RPATH 0)

set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -Wall)

#include_directories(${PROJECT_SOURCE_DIR}/inc/)
include_directories(/opt/MESA/include/)
include_directories(/opt/MESA/include/MESA)
link_directories(/opt/MESA/lib)

file(GLOB SRC
    "src/*.c"
    "src/*.cpp"
)

set(DEPEND_DYN_LIB  MESA_handle_logger  MESA_prof_load rdkafka)


# Shared Library Output
add_library(${plugin_name} SHARED ${SRC})
target_link_libraries(${plugin_name} ${DEPEND_DYN_LIB})
set_target_properties(${plugin_name}  PROPERTIES PREFIX "")

install(TARGETS ${plugin_name} LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/plug/business/${plugin_name})
install(FILES bin/${plugin_name}.inf DESTINATION ${CMAKE_INSTALL_PREFIX}/plug/business/${plugin_name})

#include(Package)