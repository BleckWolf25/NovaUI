# ------------------------------------------------------------
# @file CMakeLists.txt
#
# @version 1.0.0
# @author BleckWolf25
# @license MIT
#
# @summary Root build configuration for NovaUI
#
# @description
# Configures the C++20 MSVC toolchain for the Ninja generator with a statically linked CRT,
# strict warnings, explicit source lists and native Win32/DirectX shell libraries.
#
# @since 17/11/2026
# @updated 17/11/2026
# ------------------------------------------------------------
# ---------- CONFIGURATION
cmake_minimum_required(VERSION 3.25)
project(NovaUI VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# ---------- TARGETS
add_executable(NovaUI WIN32
  src/main.cpp
  src/Core/Window.cpp
  src/Core/AppBar.cpp
  src/Core/ShellTaskbar.cpp
  src/Modules/TaskbarUI.cpp
  src/Modules/TaskTracker.cpp
  src/Modules/ClockWidget.cpp
  src/Render/Renderer.cpp
)

target_include_directories(NovaUI PRIVATE src)
target_compile_definitions(NovaUI PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(NovaUI PRIVATE /W4 /WX /permissive- /utf-8 /Zc:__cplusplus)
target_link_libraries(NovaUI PRIVATE
  d2d1
  dwrite
  d3d11
  dxgi
  dcomp
  dwmapi
  windowscodecs
  shell32
  user32
  ole32
)