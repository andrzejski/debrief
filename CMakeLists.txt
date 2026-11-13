cmake_minimum_required(VERSION 3.21)
project(debrief LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Network Qml Quick QuickControls2 DBus Test)

# Everything but main.cpp, shared by the app and the tests, so each source
# compiles once and a new file in src/ needs no line here.
file(GLOB debrief_sources CONFIGURE_DEPENDS src/*.cpp src/*.h)
list(REMOVE_ITEM debrief_sources ${CMAKE_SOURCE_DIR}/src/main.cpp)
add_library(debrief_core STATIC ${debrief_sources})
target_include_directories(debrief_core PUBLIC src)
target_link_libraries(debrief_core PUBLIC
  Qt6::Core Qt6::Gui Qt6::Network Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::DBus)
target_compile_options(debrief_core PUBLIC -Wall -Wextra)

add_executable(debrief src/main.cpp src/resources.qrc)
target_link_libraries(debrief PRIVATE debrief_core)

# The tests load the real Main.qml, so they carry the resources too.
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
