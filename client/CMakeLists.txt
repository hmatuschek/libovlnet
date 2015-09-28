set(VLF_CLIENT_SOURCES main.cpp application.cpp dhtstatus.cpp dhtstatusview.cpp dhtnetgraph.cpp
    searchdialog.cpp buddylist.cpp buddylistview.cpp securechat.cpp chatwindow.cpp)
set(VLF_CLIENT_MOC_HEADERS application.h dhtstatus.h dhtstatusview.h dhtnetgraph.h searchdialog.h
    buddylist.h buddylistview.h securechat.h chatwindow.h)
set(VLF_CLIENT_HEADERS ${VLF_CLIENT_MOC_HEADERS})

qt5_wrap_cpp(VLF_CLIENT_MOC_SOURCES ${VLF_CLIENT_MOC_HEADERS})
qt5_add_resources(VLF_CLIENT_RCC_SOURCES ../shared/resources.qrc)

add_executable(vlfclient ${VLF_CLIENT_SOURCES} ${VLF_CLIENT_MOC_SOURCES} ${VLF_CLIENT_RCC_SOURCES})
target_link_libraries(vlfclient vlf ${LIBS})

INSTALL(TARGETS vlfclient DESTINATION bin)
