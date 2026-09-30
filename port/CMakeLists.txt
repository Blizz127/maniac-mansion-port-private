cmake_minimum_required(VERSION 3.16)
project(mm_port C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()
add_compile_options(-Wall -Wextra -Wno-unused-parameter)

add_library(mmcore STATIC
  src/core/cpu.c src/core/bus.c src/core/ppu.c src/core/apu.c
  src/core/mmc1.c src/core/nes.c src/core/cart.c src/core/sha256.c
  src/core/trace.c)

add_library(mmhost_common STATIC src/host/png.c src/host/palette.c)

add_executable(mm-headless src/headless/main.c)
target_link_libraries(mm-headless mmcore mmhost_common)

find_package(SDL2 REQUIRED)
set(MM_VERSION "dev" CACHE STRING "Version string baked into the port")
add_library(mmhost STATIC src/host/config.c src/host/pads.c src/host/chord.c src/host/devmenu.c src/host/presentation.c src/host/qol_input.c src/host/mode.c src/host/enhanced_save.c src/host/party.c src/host/restoration.c)
target_include_directories(mmhost PUBLIC ${SDL2_INCLUDE_DIRS})

add_executable(maniac-mansion-port src/host/main.c)
target_compile_definitions(maniac-mansion-port PRIVATE MM_VERSION="${MM_VERSION}")
target_include_directories(maniac-mansion-port PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(maniac-mansion-port mmhost mmhost_common mmcore ${SDL2_LIBRARIES})

enable_testing()
add_executable(test-mode tests/test_mode.c)
target_link_libraries(test-mode mmhost mmcore ${SDL2_LIBRARIES})
target_compile_options(test-mode PRIVATE -UNDEBUG)
add_test(NAME boot_mode COMMAND test-mode)
add_executable(test-pads tests/test_pads.c)
target_include_directories(test-pads PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(test-pads mmhost ${SDL2_LIBRARIES})
add_test(NAME pads COMMAND test-pads)

add_executable(tracegen tools/tracegen.c)
target_link_libraries(tracegen mmcore)

add_executable(test-config tests/test_config.c src/host/config.c)
target_compile_options(test-config PRIVATE -UNDEBUG)
add_test(NAME config_persistence COMMAND test-config)

add_executable(test-enhancement-menu tests/test_enhancement_menu.c)
target_link_libraries(test-enhancement-menu mmhost ${SDL2_LIBRARIES})
target_compile_options(test-enhancement-menu PRIVATE -UNDEBUG)
add_test(NAME enhancement_menu COMMAND test-enhancement-menu)

add_executable(test-presentation tests/test_presentation.c src/host/presentation.c src/host/palette.c)
target_compile_options(test-presentation PRIVATE -UNDEBUG)
add_test(NAME presentation COMMAND test-presentation)

add_executable(test-qol-input tests/test_qol_input.c src/host/qol_input.c)
target_link_libraries(test-qol-input mmcore)
target_compile_options(test-qol-input PRIVATE -UNDEBUG)
add_test(NAME qol_input COMMAND test-qol-input)

# Actual-ROM diagnostic executables require explicit owner-local fixtures.
option(MM_BUILD_ROM_DIAGNOSTICS "Build owner-local ROM/SDL diagnostic executables" OFF)
if(MM_BUILD_ROM_DIAGNOSTICS)
  # They are deliberately not automatic CTest dependencies or release assets.
  add_executable(test-restoration-native tests/test_restoration_native.c src/host/restoration.c)
  target_link_libraries(test-restoration-native mmcore mmhost_common)
  add_executable(test-qol-native tests/test_qol_native.c src/host/qol_input.c)
  target_link_libraries(test-qol-native mmcore)
  add_executable(test-party-native tests/test_party_native.c src/host/party.c)
  target_link_libraries(test-party-native mmcore)
  add_executable(test-party-fresh tests/test_party_fresh.c src/host/party.c)
  target_link_libraries(test-party-fresh mmcore)
  add_executable(test-party-saveload tests/test_party_saveload.c src/host/party.c)
  target_link_libraries(test-party-saveload mmcore)
  add_executable(test-party-death tests/test_party_death.c src/host/party.c)
  target_link_libraries(test-party-death mmcore)
  add_executable(test-qol-chase tests/test_qol_chase.c src/host/qol_input.c)
  target_link_libraries(test-qol-chase mmcore)
  add_executable(presentation-timing tests/presentation_timing.c)
  target_link_libraries(presentation-timing mmhost mmcore mmhost_common ${SDL2_LIBRARIES})
  find_package(X11 QUIET)
  if(X11_FOUND)
    add_executable(test-qol-host tests/test_qol_host.c)
    target_compile_definitions(test-qol-host PRIVATE MM_VERSION="test")
    target_include_directories(test-qol-host PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(test-qol-host mmhost mmhost_common mmcore ${SDL2_LIBRARIES} ${X11_LIBRARIES} ${CMAKE_DL_LIBS})
  endif()

endif() # MM_BUILD_ROM_DIAGNOSTICS

add_executable(test-timed-chord tests/test_timed_chord.c src/host/chord.c)
target_compile_options(test-timed-chord PRIVATE -UNDEBUG)
add_test(NAME timed_chord COMMAND test-timed-chord)

add_executable(test-enhanced-save tests/test_enhanced_save.c)
target_link_libraries(test-enhanced-save mmhost mmcore ${SDL2_LIBRARIES})
target_compile_options(test-enhanced-save PRIVATE -UNDEBUG)
add_test(NAME enhanced_save COMMAND test-enhanced-save)

add_executable(test-party tests/test_party.c src/host/party.c)
target_link_libraries(test-party mmcore)
target_compile_options(test-party PRIVATE -UNDEBUG)
add_test(NAME party_policy COMMAND test-party)

add_executable(test-restoration tests/test_restoration.c src/host/restoration.c)
target_link_libraries(test-restoration mmcore)
add_test(NAME restoration_policy COMMAND test-restoration)
