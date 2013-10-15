if (ABOUT_SUBDIR)
   add_subdirectory(about/${ABOUT_SUBDIR})
endif (ABOUT_SUBDIR)
add_subdirectory(about/shared)

if (HD_FOUND)
   include_directories(${HD_INCLUDE_DIR})
endif (HD_FOUND)
if (OPENGL_FOUND)
   include_directories(${OPENGL_INCLUDE_DIR})
endif (OPENGL_FOUND)

set(kio_sysinfo_SRCS
   sysinfo.cpp
)
set_source_files_properties(sysinfo.cpp COMPILE_FLAGS -DQT_NO_KEYWORDS)
kde4_add_plugin(kio_sysinfo ${kio_sysinfo_SRCS})
target_link_libraries(kio_sysinfo ${KDE4_KIO_LIBS} ${KDE4_SOLID_LIBS})
if (HD_FOUND)
   target_link_libraries(kio_sysinfo ${HD_LIBRARY})
endif (HD_FOUND)
if (OPENGL_FOUND)
   target_link_libraries(kio_sysinfo ${OPENGL_gl_LIBRARY} ${X11_LIBRARIES})
endif (OPENGL_FOUND)
install(TARGETS kio_sysinfo DESTINATION ${PLUGIN_INSTALL_DIR})

set(libksysinfopart_SRCS
   ksysinfopart.cpp
)
kde4_add_plugin(libksysinfopart ${libksysinfopart_SRCS})
target_link_libraries(libksysinfopart ${KDE4_KHTML_LIBS} ${KDE4_KPART_LIBS} ${KDE4_SOLID_LIBRARY})
install(TARGETS libksysinfopart DESTINATION ${PLUGIN_INSTALL_DIR})
install( FILES sysinfo.protocol ksysinfopart.desktop DESTINATION  ${SERVICES_INSTALL_DIR} )
install( FILES kfmclient_sysinfo.desktop DESTINATION ${XDG_APPS_INSTALL_DIR} )

find_package(SharedMimeInfo REQUIRED)
install(FILES x-sysinfo.xml DESTINATION ${XDG_MIME_INSTALL_DIR})
update_xdg_mimetypes(${XDG_MIME_INSTALL_DIR})
