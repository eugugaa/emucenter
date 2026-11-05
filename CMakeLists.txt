cmake_minimum_required(VERSION 3.16)
project(EmuCenter VERSION 0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets Network)
add_executable(EmuCenter WIN32
  src/main.cpp src/MainWindow.cpp src/Catalog.cpp src/Library.cpp
  src/Emulators.cpp src/Metadata.cpp src/I18n.cpp src/Theme.cpp src/SettingsDialog.cpp src/Igdb.cpp src/RomInfo.cpp src/GameSettingsDialog.cpp
  res/resources.qrc)
target_link_libraries(EmuCenter PRIVATE Qt6::Widgets Qt6::Network)

add_custom_command(TARGET EmuCenter POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/themes $<TARGET_FILE_DIR:EmuCenter>/themes)
if(WIN32)
  find_program(WINDEPLOYQT windeployqt HINTS "${Qt6_DIR}/../../../bin")
  if(WINDEPLOYQT)
    add_custom_command(TARGET EmuCenter POST_BUILD
      COMMAND ${WINDEPLOYQT} --no-translations "$<TARGET_FILE:EmuCenter>")
  endif()
endif()
