set(VLF_CLIENT_SOURCES main.cpp bootstrapnodelist.cc
    application.cc dhtstatus.cc dhtstatusview.cc dhtnetgraph.cc searchdialog.cc buddylist.cc
    buddylistview.cc chatwindow.cc callwindow.cc filetransferdialog.cc sockswindow.cc logwindow.cc
    settings.cc settingsdialog.cc searchcompletion.cc)
set(VLF_CLIENT_MOC_HEADERS
    application.hh dhtstatus.hh dhtstatusview.hh dhtnetgraph.hh searchdialog.hh buddylist.hh
    buddylistview.hh chatwindow.hh callwindow.hh filetransferdialog.hh sockswindow.hh logwindow.hh
    settings.hh settingsdialog.hh searchcompletion.hh)
set(VLF_CLIENT_HEADERS ${VLF_CLIENT_MOC_HEADERS}
    bootstrapnodelist.hh)

qt5_wrap_cpp(VLF_CLIENT_MOC_SOURCES ${VLF_CLIENT_MOC_HEADERS})
qt5_add_resources(VLF_CLIENT_RCC_SOURCES ../shared/resources.qrc)

add_executable(ovlclient ${VLF_CLIENT_SOURCES} ${VLF_CLIENT_MOC_SOURCES} ${VLF_CLIENT_RCC_SOURCES})
target_link_libraries(ovlclient ovlnet ${LIBS})

# Install binary
INSTALL(TARGETS ovlclient DESTINATION bin)

# Install logo and .desktop file under Linux:
IF(UNIX AND NOT APPLE)
 INSTALL(FILES ../shared/ovlclient.png
         DESTINATION share/icons)
 INSTALL(FILES ../shared/ovlclient.desktop
         DESTINATION share/applications)
ENDIF(UNIX AND NOT APPLE)

# Create "Application Bundle" under MacOS X
if(UNIX AND APPLE)
 install(TARGETS ovlclient DESTINATION /Applications/OvlClient.app/Contents/MacOS)
 install(FILES ../shared/macos/ovlclient.icns DESTINATION /Applications/OvlClient.app/Contents)
 install(FILES ../shared/macos/Info.plist DESTINATION /Applications/OvlClient.app/Contents)
endif(UNIX AND APPLE)
