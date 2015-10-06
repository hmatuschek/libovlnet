SET(VLF_DAEMON_SOURCES main.cpp echostream.cpp application.cpp qhal.cpp halchat.cpp
    socksconnection.cpp)
SET(VLF_DAEMON_MOC_HEADERS application.h halchat.h socksconnection.h)
SET(VLF_DAEMON_HEADERS echostream.h qhal.h)

qt5_wrap_cpp(VLF_DAEMON_MOC_SOURCES ${VLF_DAEMON_MOC_HEADERS})

add_executable(vlfdaemon ${VLF_DAEMON_SOURCES} ${VLF_DAEMON_MOC_SOURCES} ${VLF_DAEMON_RCC_SOURCES})
target_link_libraries(vlfdaemon vlf ${LIBS})

INSTALL(TARGETS vlfdaemon DESTINATION bin)

# Install service script
IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
INSTALL(
  FILES "${CMAKE_SOURCE_DIR}/shared/vlfdaemon.conf"
  DESTINATION "/etc/init")
ENDIF(CMAKE_SYSTEM_NAME MATCHES "Linux")
