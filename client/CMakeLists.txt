set(VLF_CLIENT_SOURCES main.cpp application.cpp dhtstatus.cpp dhtstatusview.cpp dhtnetgraph.cpp
    searchdialog.cpp buddylist.cpp buddylistview.cpp securechat.cpp chatwindow.cpp securecall.cpp
    callwindow.cpp filetransfer.cpp bootstrapnodelist.cpp filetransferdialog.cpp socksservice.cpp
    sockswindow.cpp logwindow.cpp)
set(VLF_CLIENT_MOC_HEADERS application.h dhtstatus.h dhtstatusview.h dhtnetgraph.h searchdialog.h
    buddylist.h buddylistview.h securechat.h chatwindow.h securecall.h callwindow.h filetransfer.h
    filetransferdialog.h socksservice.h sockswindow.h logwindow.h)
set(VLF_CLIENT_HEADERS ${VLF_CLIENT_MOC_HEADERS}
    bootstrapnodelist.h)

qt5_wrap_cpp(VLF_CLIENT_MOC_SOURCES ${VLF_CLIENT_MOC_HEADERS})
qt5_add_resources(VLF_CLIENT_RCC_SOURCES ../shared/resources.qrc)

add_executable(ovlclient ${VLF_CLIENT_SOURCES} ${VLF_CLIENT_MOC_SOURCES} ${VLF_CLIENT_RCC_SOURCES})
target_link_libraries(ovlclient ovl ${LIBS})

# Install binary
INSTALL(TARGETS ovlclient DESTINATION bin)

# Install logo and .desktop file under Linux:
IF(UNIX AND NOT APPLE)
 INSTALL(FILES ../shared/icons/ovlclient.png
         DESTINATION share/icons)
 INSTALL(FILES ../shared/ovlclient.desktop
         DESTINATION share/applications)
ENDIF(UNIX AND NOT APPLE)

