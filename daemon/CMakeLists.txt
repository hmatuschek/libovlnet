SET(VLF_DAEMON_SOURCES main.cpp echostream.cpp application.cpp)
SET(VLF_DAEMON_MOC_HEADERS application.h)
SET(VLF_DAEMON_HEADERS echostream.h)

qt5_wrap_cpp(VLF_DAEMON_MOC_SOURCES ${VLF_DAEMON_MOC_HEADERS})

add_executable(vlfdaemon ${VLF_DAEMON_SOURCES} ${VLF_DAEMON_MOC_SOURCES} ${VLF_DAEMON_RCC_SOURCES})
target_link_libraries(vlfdaemon vlf ${LIBS})
