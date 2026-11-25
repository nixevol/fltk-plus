#
# CMakeLists.txt to build fluid-plus, the new designer.
# Copied from fluid. The original fluid target is left unchanged.
#
# Copyright 1998-2026 by Bill Spitzak and others.
#
# This library is free software. Distribution and use rights are outlined in
# the file "COPYING" which should have been included with this file.  If this
# file is missing or damaged, see the license at:
#
#     https://www.fltk.org/COPYING.php
#
# Please see the following page on how to report bugs and issues:
#
#     https://www.fltk.org/bugs.php
#

# Targets that will be installed: fluid, fluid-shared, and fluid-cmd (Windows only)
set(TARGETS "")
# Targets that will be installed *and* "exported" as targets in CMake config file
set(EXPORTS "")

# Defaults to be used and potentially modified later

set(BACKEND_APPLE FALSE)        # FIXME: should be global, e.g. FLTK_BACKEND_APPLE
set(ICON_NAME "")
set(ICON_PATH "")
set(SUFFIX "UNIX")              # suffix for platform specific source files

# platform specific settings

if(APPLE AND NOT FLTK_BACKEND_X11)    # macOS "native"
  set(BACKEND_APPLE TRUE)
  set(ICON_NAME fluid.icns)
  set(ICON_PATH "${CMAKE_CURRENT_SOURCE_DIR}/icons/${ICON_NAME}")
elseif(WIN32)
  set(SUFFIX "WIN32")
endif()

# This macro is used to avoid duplicate code to create executable programs.
# This must be a macro because it changes at least one global variable: TARGETS.
# This macro also uses some (local) variables defined above.
# In the future this might be converted to a function to avoid side effects.
#
# Parameters:
#  - TARGET       target name, e.g. 'fluid', 'fluid-cmd', 'fluid-shared', ...
#  - GUI          TRUE (GUI) or FALSE (command-line)
#  - SOURCES      source files
#  - LIBS         link libraries
#  - EXPORT_NAME  should be the same as TARGET, unless there are reasons...
#  - EXPORT       TRUE if "exported" in config file, FALSE if not
#  - (others)     see local variables defined above !

macro(make_target TARGET GUI SOURCES LIBS EXPORT_NAME EXPORT)

  # message(STATUS "[fluid] make_target ${TARGET} GUI:${GUI} ${SOURCES} ${LIBS} NAME:${EXPORT_NAME} EXPORT:${EXPORT}")

  # Options WIN32 and MACOSX_BUNDLE build a Windows GUI program or macOS bundle,
  # respectively. Both options are ignored on other platforms.

  if(${GUI})
    add_executable(${TARGET} WIN32 MACOSX_BUNDLE ${SOURCES} ${ICON_PATH})
  else()
    add_executable(${TARGET} ${SOURCES})
    target_compile_definitions(${TARGET} PUBLIC FLUID_CMD)
  endif(${GUI})

  if(${EXPORT})
    list(APPEND EXPORTS ${TARGET})
  else()
    list(APPEND TARGETS ${TARGET})
  endif()

  if(BACKEND_APPLE)

    # set bundle properties
    set_target_properties(${TARGET} PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/${TARGET}.plist")
    set_target_properties(${TARGET} PROPERTIES MACOSX_BUNDLE_ICON_FILE ${ICON_NAME})
    set_target_properties(${TARGET} PROPERTIES XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "org.fltk.${TARGET}")
    set_target_properties(${TARGET} PROPERTIES RESOURCE ${ICON_PATH})

    # install command line tool
    install(PROGRAMS $<TARGET_FILE:${TARGET}>
            DESTINATION ${FLTK_BINDIR})

    # create macOS bundle wrapper script

    set(WRAPPER "${EXECUTABLE_OUTPUT_PATH}/${CMAKE_CFG_INTDIR}/${TARGET}")
    add_custom_command(
      TARGET ${TARGET} POST_BUILD
      COMMAND cp ${FLTK_SOURCE_DIR}/CMake/macOS-bundle-wrapper.in ${WRAPPER}
      COMMAND chmod u+x,g+x,o+x ${WRAPPER}
      BYPRODUCTS ${WRAPPER}
      VERBATIM
    )
    unset(WRAPPER)

  endif(BACKEND_APPLE)

  target_link_libraries(${TARGET} PRIVATE ${LIBS})
  set_target_properties(${TARGET} PROPERTIES EXPORT_NAME ${EXPORT_NAME})

endmacro(make_target TARGET GUI SOURCES LIBS EXPORT_NAME EXPORT)


# Main source and header files used for the executable because macOS (Xcode)
# needs at least one source file (main.cxx) to link the main program properly

set(MAIN_FILES main.cxx main.h fluid.rc)

# Source files for 'fluid-lib': all source files except ${MAIN_FILES}

set(CPPFILES
  Fluid.cxx
  Project.cxx
  message.cxx
  fluid_icon.cxx
  app/args.cxx
  app/Snap_Action.cxx
  app/history.cxx
  app/shell_command.cxx
  app/templates.cxx
  app/ui_locale.cxx
  app/designer_shell.cxx
  app/code_stage.cxx
  app/export_actions.cxx
  app/dock.cxx
  app/frame_skin.cxx
  app/toolbox.cxx
  app/property_sheet.cxx
  app/code_console.cxx
  app/agent_session.cxx
  io/Code_Writer.cxx
  io/file_chooser.cxx
  io/Project_Writer.cxx
  io/Project_Reader.cxx
  io/String_Writer.cxx
  nodes/Tree.cxx
  nodes/Button_Node.cxx
  nodes/Function_Node.cxx
  nodes/Grid_Node.cxx
  nodes/Group_Node.cxx
  nodes/iterators.cxx
  nodes/Menu_Node.cxx
  nodes/Node.cxx
  nodes/Node_debug.cxx
  nodes/Widget_Image.cxx
  nodes/Widget_Node.cxx
  nodes/Window_Node.cxx
  nodes/factory.cxx
  panels/about_panel.cxx
  panels/codeview_panel.cxx
  panels/function_panel.cxx
  panels/main_panel.cxx
  panels/settings_panel.cxx
  panels/template_panel.cxx
  panels/widget_panel.cxx
  panels/widget_panel_callbacks.cxx
  panels/widget_panel/Grid_Tab.cxx
  panels/widget_panel/Grid_Child_Tab.cxx
  proj/Node_Alignment.cxx
  proj/i18n.cxx
  proj/Image_Asset.cxx
  proj/Image_Asset_Map.cxx
  proj/mergeback.cxx
  proj/undo.cxx
  rsrcs/pixmaps.cxx
  tools/ExternalCodeEditor_${SUFFIX}.cxx
  tools/autodoc.cxx
  tools/filename.cxx
  widgets/App_Menu_Bar.cxx
  widgets/Code_Editor.cxx
  widgets/Code_Viewer.cxx
  widgets/Text_Viewer.cxx
  widgets/Formula_Input.cxx
  widgets/Bin_Button.cxx
  widgets/Style_Parser.cxx
  widgets/Node_Browser.cxx
)

if(MSVC)
  set_source_files_properties(app/ui_locale.cxx PROPERTIES COMPILE_OPTIONS "/utf-8")
endif()

# List header files in Apple's Xcode IDE

set(HEADERFILES
  Fluid.h
  Project.h
  message.h
  fluid_icon.h
  app/args.h
  app/Snap_Action.h
  app/history.h
  app/shell_command.h
  app/templates.h
  app/ui_locale.h
  app/designer_shell.h
  app/code_stage.h
  app/dock.h
  app/frame_skin.h
  app/owned_choice.h
  app/toolbox.h
  app/property_sheet.h
  app/code_console.h
  io/Code_Writer.h
  io/file_chooser.h
  io/Project_Writer.h
  io/Project_Reader.h
  io/String_Writer.h
  nodes/Tree.h
  nodes/Button_Node.h
  nodes/Function_Node.h
  nodes/Grid_Node.h
  nodes/Group_Node.h
  nodes/iterators.h
  nodes/Menu_Node.h
  nodes/Node.h
  nodes/Node_debug.h
  nodes/Widget_Image.h
  nodes/Widget_Node.h
  nodes/Window_Node.h
  nodes/factory.h
  panels/about_panel.h
  panels/codeview_panel.h
  panels/function_panel.h
  panels/main_panel.h
  panels/settings_panel.h
  panels/template_panel.h
  panels/widget_panel.h
  panels/widget_panel/Grid_Tab.h
  panels/widget_panel/Grid_Child_Tab.h
  proj/Node_Alignment.h
  proj/Image_Asset.h
  proj/Image_Asset_Map.h
  proj/mergeback.h
  proj/i18n.h
  proj/undo.h
  rsrcs/comments.h
  rsrcs/pixmaps.h
  tools/ExternalCodeEditor_${SUFFIX}.h
  tools/autodoc.h
  tools/filename.h
  widgets/App_Menu_Bar.h
  widgets/Code_Editor.h
  widgets/Code_Viewer.h
  widgets/Text_Viewer.h
  widgets/Formula_Input.h
  widgets/Bin_Button.h
  widgets/Style_Parser.h
  widgets/Node_Browser.h
)

source_group(
  TREE
    ${CMAKE_CURRENT_SOURCE_DIR}
  FILES
    ${CPPFILES}
    ${HEADERFILES}
    ${MAIN_FILES}
    CMakeLists.txt
)

# Build a local object library to avoid compiling all source files
# multiple times for all fluid targets on Windows (fluid + fluid-cmd).

add_library(fluid-plus-lib OBJECT EXCLUDE_FROM_ALL)
target_sources(fluid-plus-lib PRIVATE ${CPPFILES} ${HEADERFILES})
target_include_directories(fluid-plus-lib PUBLIC .)
target_link_libraries(fluid-plus-lib PUBLIC fltk::images)

# Build targets. Do not export these targets, and do not replace FLTK_FLUID_EXECUTABLE.

make_target(fluid-plus TRUE "${MAIN_FILES}" fluid-plus-lib fluid-plus FALSE)

add_custom_command(TARGET fluid-plus POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_CURRENT_SOURCE_DIR}/locale"
          "$<TARGET_FILE_DIR:fluid-plus>/locale"
  COMMENT "Copy fluid-plus language packs")

add_custom_command(TARGET fluid-plus POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_CURRENT_SOURCE_DIR}/fonts"
          "$<TARGET_FILE_DIR:fluid-plus>/fonts"
  COMMENT "Copy fluid-plus fonts")

# Build the console app on Windows
# This is done for all Windows targets, even if cross-compiling.

if(WIN32)
  make_target(fluid-plus-cmd FALSE "${MAIN_FILES}" fluid-plus-lib fluid-plus-cmd FALSE)
endif()

# Add the "shared" executable (linked against the shared FLTK libs).
# Note 1: only the GUI version is built as "shared" executable.
# Note 2: For MSVC we need the special object library 'call_main'.

if(FLTK_BUILD_SHARED_LIBS)

  add_library(fluid-plus-lib-shared OBJECT EXCLUDE_FROM_ALL)
  target_sources(fluid-plus-lib-shared PRIVATE ${CPPFILES} ${HEADERFILES})
  target_include_directories(fluid-plus-lib-shared PUBLIC .)
  if(MSVC)
    target_link_libraries(fluid-plus-lib-shared PUBLIC fltk::fltk-shared)
  else()
    target_link_libraries(fluid-plus-lib-shared PUBLIC fltk::images-shared)
  endif(MSVC)

  if(MSVC)
    make_target(fluid-plus-shared TRUE "${MAIN_FILES}" "fluid-plus-lib-shared;call_main" fluid-plus-shared FALSE)
  else()
    make_target(fluid-plus-shared TRUE "${MAIN_FILES}" fluid-plus-lib-shared fluid-plus-shared FALSE)
  endif()

  # experimental
  # if(NOT WIN32)
  #   set(FLTK_FLUID_EXECUTABLE fltk::fluid-shared)
  #   message(STATUS "** experimental ** FLTK_FLUID_EXECUTABLE = ${FLTK_FLUID_EXECUTABLE}")
  # endif()

endif(FLTK_BUILD_SHARED_LIBS)

# Create aliases for all targets

foreach(tgt ${TARGETS} ${EXPORTS})
  add_executable(fltk::${tgt} ALIAS ${tgt})
endforeach()

# Install the GUI and (on Windows only) the commandline tool 'fluid-cmd'
# message(STATUS "[fluid] INSTALL + EXPORT: ${EXPORTS}")
# message(STATUS "[fluid] INSTALL - only  : ${TARGETS}")

if(EXPORTS)
  install(TARGETS ${EXPORTS}
          EXPORT  FLTK-Targets
          RUNTIME DESTINATION ${FLTK_BINDIR}
          LIBRARY DESTINATION ${FLTK_LIBDIR}
          ARCHIVE DESTINATION ${FLTK_LIBDIR}
          BUNDLE  DESTINATION ${FLTK_BINDIR} # macOS: bundles
  )
endif()

if(TARGETS)
  install(TARGETS ${TARGETS}
          RUNTIME DESTINATION ${FLTK_BINDIR}
          LIBRARY DESTINATION ${FLTK_LIBDIR}
          ARCHIVE DESTINATION ${FLTK_LIBDIR}
          BUNDLE  DESTINATION ${FLTK_BINDIR} # macOS: bundles
  )
endif()

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/locale/"
  DESTINATION ${FLTK_DATADIR}/fltk-plus/fluid-plus/locale
  FILES_MATCHING
    PATTERN "*.json"
    PATTERN "README.txt"
)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/fonts/"
  DESTINATION ${FLTK_DATADIR}/fltk-plus/fluid-plus/fonts
  FILES_MATCHING
    PATTERN "*.ttf"
    PATTERN "*.otf"
    PATTERN "*.ttc"
    PATTERN "*.otc"
    PATTERN "OFL.txt"
    PATTERN "README.txt"
)
