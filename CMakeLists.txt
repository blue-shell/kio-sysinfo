project(kio_sysinfo)

# search packages used by KDE
find_package(KDE4 REQUIRED)
include (KDE4Defaults)
include (MacroLibrary)
include(MacroOptionalAddSubdirectory)
include(MacroOptionalFindPackage)

add_definitions(${QT_DEFINITIONS} ${KDE4_DEFINITIONS})
include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} ${KDE4_INCLUDES})

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules ${CMAKE_MODULE_PATH})

macro_optional_find_package(HD)

string(TOLOWER "${SYSINFO_DISTRO}" _distro)
set(ABOUT_SUBDIR ${_distro})
if (_distro STREQUAL "suse")
   set(WITH_SUSE TRUE)
elseif (_distro STREQUAL "fedora")
   set(WITH_FEDORA TRUE)
   set(ABOUT_SUBDIR "generic")
elseif (_distro STREQUAL "mandriva")
   set(WITH_MANDRIVA TRUE)
elseif (_distro STREQUAL "debian")
   set(WITH_DEBIAN TRUE)
else (_distro STREQUAL "suse")
   set(ABOUT_SUBDIR "generic")
endif (_distro STREQUAL "suse")

if (HD_FOUND)
   set(HAVE_HD TRUE)
endif (HD_FOUND)

if (X11_FOUND)
   macro_optional_find_package(OpenGL)
   if (OPENGL_FOUND)
      set(HAVE_GLXCHOOSEVISUAL TRUE)
   endif (OPENGL_FOUND)
endif (X11_FOUND)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/config-kiosysinfo.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/src/config-kiosysinfo.h)

add_subdirectory(src)

#process and install translations
if(FALSE)
find_package(Gettext REQUIRED)
file(GLOB po_files po/*.po)
set(_gmoFiles)
foreach (_current_PO_FILE ${po_files})
   get_filename_component(_basename ${_current_PO_FILE} NAME_WE)
   set(_lang ${_basename})
   set(_gmoFile ${CMAKE_CURRENT_BINARY_DIR}/${_basename}.gmo)
   add_custom_command(OUTPUT ${_gmoFile}
      COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} -o ${_gmoFile} ${_current_PO_FILE}
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      DEPENDS ${_current_PO_FILE}
   )
   list(APPEND _gmoFiles ${_gmoFile})
   install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${_basename}.gmo DESTINATION ${LOCALE_INSTALL_DIR}/${_lang}/LC_MESSAGES/ RENAME kio_sysinfo.mo)
endforeach (_current_PO_FILE ${po_files})
add_custom_target(pofiles ALL DEPENDS ${_gmoFiles})
endif(FALSE)
