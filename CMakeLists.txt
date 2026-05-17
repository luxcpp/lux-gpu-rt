cmake_minimum_required(VERSION 3.20)
project(lux-gpu-rt VERSION 0.1.1 LANGUAGES CXX)

# lux-gpu-rt builds ONLY a static core library (C++23 abstract GPU runtime).
# This is linked into lux-accel, not used directly by consumers.
#
# Renamed from `lux-gpu` (v0.1.0): the CMake project namespace `lux-gpu`
# is now reserved for the MLX-fork at ~/work/lux/gpu (Identity C).
# Header namespace `lux::gpu` is unchanged for API stability.

add_library(lux_gpu_rt STATIC
  src/context.cpp
  src/device.cpp
  src/buffer.cpp
  src/kernel.cpp
  src/registry.cpp
)

# Alias for internal use
add_library(lux::gpu-rt ALIAS lux_gpu_rt)

target_compile_features(lux_gpu_rt PUBLIC cxx_std_23)

target_include_directories(lux_gpu_rt
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# PIC required since this will be linked into shared library
set_target_properties(lux_gpu_rt PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON
)

# Platform-specific libraries for dlopen
if(UNIX AND NOT APPLE)
  target_link_libraries(lux_gpu_rt PRIVATE dl)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Install static library (for C++ consumers who want to build their own accel)
install(TARGETS lux_gpu_rt
  EXPORT lux-gpu-rt-targets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT lux-gpu-rt-targets
  NAMESPACE lux::
  FILE lux-gpu-rtTargets.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lux-gpu-rt
)

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/lux-gpu-rtConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/lux-gpu-rtConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lux-gpu-rt
)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/lux-gpu-rtConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY ExactVersion
)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/lux-gpu-rtConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/lux-gpu-rtConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lux-gpu-rt
)
