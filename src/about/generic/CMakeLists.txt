install( FILES
    images/background.png
    images/bar.png
    images/battery.png
    images/cpu.png
    images/dirs.png
    images/display.png
    images/hdd.png
    images/hwinfo.png
    images/inner-bcg.png
    images/launch.png
    images/mem.png
    images/network.png
    images/sysinfo.png
    DESTINATION ${DATA_INSTALL_DIR}/sysinfo/about/images
)

install (FILES
   my-computer.html
   style.css
   DESTINATION ${DATA_INSTALL_DIR}/sysinfo/about
)
