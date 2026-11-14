cmake_minimum_required(VERSION 3.21)
project(debrief LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# On Omarchy the microphone, the screen, and the transcript come from ffmpeg,
# grim, and Superwhisper. With this on they come from inside the app instead:
# Qt Multimedia, QScreen, and whisper.cpp with a model file beside the program.
# Windows has none of the three tools, so it is always on there.
option(DEBRIEF_NATIVE "Record, capture, and transcribe in-process" ${WIN32})

find_package(Qt6 REQUIRED COMPONENTS Core Gui Network Qml Quick QuickControls2)

# Everything but main.cpp, shared by the app and the tests, so each source
# compiles once and a new file in src/ needs no line here.
file(GLOB debrief_sources CONFIGURE_DEPENDS src/*.cpp src/*.h)
list(REMOVE_ITEM debrief_sources ${CMAKE_SOURCE_DIR}/src/main.cpp)
add_library(debrief_core STATIC ${debrief_sources})
target_include_directories(debrief_core PUBLIC src)
target_link_libraries(debrief_core PUBLIC
  Qt6::Core Qt6::Gui Qt6::Network Qt6::Qml Qt6::Quick Qt6::QuickControls2)
target_compile_options(debrief_core PUBLIC -Wall -Wextra)
if(NOT WIN32)
  find_package(Qt6 REQUIRED COMPONENTS DBus)
  target_link_libraries(debrief_core PUBLIC Qt6::DBus)
endif()

if(DEBRIEF_NATIVE)
  find_package(Qt6 REQUIRED COMPONENTS Multimedia Widgets)
  # whisper.cpp, compiled into the program. -DFETCHCONTENT_SOURCE_DIR_WHISPER=
  # points at a checkout that is already there.
  include(FetchContent)
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(WHISPER_BUILD_SERVER OFF CACHE BOOL "" FORCE)
  # Plain threads, so there is no OpenMP runtime to ship.
  set(GGML_OPENMP OFF CACHE BOOL "" FORCE)
  set(GGML_CCACHE OFF CACHE BOOL "" FORCE)
  if(CMAKE_CROSSCOMPILING)
    # Not this machine's processor, and whisper.cpp turns every vector
    # instruction off when it can't ask one. Any x86-64 that runs Windows 11
    # has these, and without them a sentence takes many times longer.
    set(GGML_NATIVE OFF CACHE BOOL "" FORCE)
    foreach(instructions SSE42 AVX AVX2 BMI2 FMA F16C)
      set(GGML_${instructions} ON CACHE BOOL "" FORCE)
    endforeach()
  endif()
  FetchContent_Declare(whisper
    GIT_REPOSITORY https://github.com/ggml-org/whisper.cpp
    GIT_TAG v1.9.5
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL)
  FetchContent_MakeAvailable(whisper)

  file(GLOB debrief_native CONFIGURE_DEPENDS src/native/*.cpp src/native/*.h)
  target_sources(debrief_core PRIVATE ${debrief_native})
  target_compile_definitions(debrief_core PUBLIC DEBRIEF_NATIVE)
  target_link_libraries(debrief_core PUBLIC Qt6::Multimedia Qt6::Widgets whisper)
endif()

add_executable(debrief src/main.cpp src/resources.qrc)
target_link_libraries(debrief PRIVATE debrief_core)

if(WIN32)
  # A windowed program called Debrief.exe, with its icon; packaging/windows
  # gathers what it needs to run into a folder and a zip.
  target_sources(debrief PRIVATE packaging/windows/debrief.rc)
  set_target_properties(debrief PROPERTIES WIN32_EXECUTABLE ON OUTPUT_NAME Debrief)
  return()
endif()

# The tests load the real Main.qml, so they carry the resources too.
find_package(Qt6 REQUIRED COMPONENTS Test)
enable_testing()
add_executable(debrief_tests tests/debrief_tests.cpp src/resources.qrc)
target_link_libraries(debrief_tests PRIVATE debrief_core Qt6::Test)
add_test(NAME debrief_tests COMMAND debrief_tests)
set_tests_properties(debrief_tests PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)

# `cmake --install build --prefix ~/.local` puts the app in the launcher
# before it is packaged; a package installs the same files under /usr.
include(GNUInstallDirs)
install(TARGETS debrief RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES share/debrief.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES share/debrief.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
# The Omarchy bar widget; `debrief bar install` links it into the shell's plugins.
install(DIRECTORY share/omarchy-plugin/debrief-bar DESTINATION ${CMAKE_INSTALL_DATADIR}/debrief/omarchy-plugin)
install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/debrief)
