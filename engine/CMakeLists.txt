cmake_minimum_required(VERSION 3.20)
project(lanternfall CXX C)
# Сборка для Linux (разработка и автотесты). iOS собирается скриптом ios/build.sh.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

file(GLOB GAME_SOURCES src/game/*.cc)
file(GLOB ENGINE_SOURCES src/engine/*.cc src/client/*.cc)
set(PLATFORM_SOURCES src/platform/sokol_impl.cc src/platform/platform_common.cc src/platform/platform_linux.cc)

option(LF_CLIENT "Собирать клиент (нужны X11/GL/ALSA)" ON)

add_library(lf_core STATIC ${GAME_SOURCES})
target_include_directories(lf_core PUBLIC src)
target_compile_options(lf_core PRIVATE -Wall -Wextra -Wno-unused-parameter)

# Ядро + C-интерфейс одной статической библиотекой для сервера на Go (cgo).
add_library(lf_server STATIC ${GAME_SOURCES} src/capi/lf_capi.cc)
target_include_directories(lf_server PUBLIC src)
set_target_properties(lf_server PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(LF_CLIENT)
  add_executable(lanternfall src/main.cc ${ENGINE_SOURCES} ${PLATFORM_SOURCES})
  target_include_directories(lanternfall PRIVATE src third_party/sokol third_party/stb)
  target_compile_definitions(lanternfall PRIVATE SOKOL_GLCORE)
  target_link_libraries(lanternfall PRIVATE lf_core X11 Xi Xcursor GL dl pthread m asound)
endif()

enable_testing()
file(GLOB TEST_SOURCES tests/*.cc)
if(TEST_SOURCES)
  add_executable(lf_tests ${TEST_SOURCES})
  target_include_directories(lf_tests PRIVATE src tests)
  target_link_libraries(lf_tests PRIVATE lf_core)
  target_compile_options(lf_tests PRIVATE -Wno-format-zero-length)
  add_test(NAME core COMMAND lf_tests)
endif()
