cmake_minimum_required(VERSION 3.20)
project(lux-gpu VERSION 0.1.0 LANGUAGES CXX)

# lux-gpu builds ONLY a static core library
# This is linked into lux-accel, not used directly by consumers

add_library(lux_gpu_core STATIC
  src/context.cpp
  src/device.cpp
  src/buffer.cpp
  src/kernel.cpp
  src/registry.cpp
)

# Alias for internal use
add_library(lux::gpu_core ALIAS lux_gpu_core)

target_compile_features(lux_gpu_core PUBLIC cxx_std_23)

target_include_directories(lux_gpu_core
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# PIC required since this will be linked into shared library
set_target_properties(lux_gpu_core PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON
)

# Platform-specific libraries for dlopen
if(UNIX AND NOT APPLE)
  target_link_libraries(lux_gpu_core PRIVATE dl)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Install static library (for C++ consumers who want to build their own accel)
install(TARGETS lux_gpu_core
  EXPORT lux-gpu-targets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT lux-gpu-targets
  NAMESPACE lux::
  FILE lux-gpuTargets.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lux-gpu
)

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/lux-gpuConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/lux-gpuConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lux-gpu
)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/lux-gpuConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY ExactVersion
)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/lux-gpuConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/lux-gpuConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lux-gpu
)
