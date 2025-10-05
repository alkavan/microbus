cmake_minimum_required(VERSION 3.21)
project(microbus VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(microbus INTERFACE)

target_include_directories(microbus INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(microbus INTERFACE cxx_std_17)

# Define installation directory for headers
set(INCLUDE_INSTALL_DIR include)

# Create examples executable and link it to microbus
add_executable(microbus_examples src/examples.cpp)
target_include_directories(microbus_examples PRIVATE include)
target_link_libraries(microbus_examples microbus)

### EXPORT #####################################################################

install(TARGETS microbus
        EXPORT microbusTargets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include
)

install(FILES include/microbus.hpp DESTINATION include)

install(EXPORT microbusTargets
        FILE microbusTargets.cmake
        NAMESPACE microbus::
        DESTINATION lib/cmake/microbus
)

# Create a package config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
        microbusConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/microbusConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/microbusConfig.cmake"
        INSTALL_DESTINATION lib/cmake/microbus
        PATH_VARS
        INCLUDE_INSTALL_DIR
)

install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/microbusConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/microbusConfigVersion.cmake"
        DESTINATION lib/cmake/microbus
)
