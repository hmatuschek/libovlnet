set(ovl_SOURCES 
    ntp.cpp dht.cpp pcp.cpp natpmp.cpp crypto.cpp utils.cpp stream.cpp socks.cpp buckets.cpp
    logger.cpp filetransfer.cpp securechat.cpp securecall.cpp httpservice.cpp secureshell.cpp
    httpproxy.cpp)
set(ovl_MOC_HEADERS 
    ntp.h dht.h pcp.h natpmp.h stream.h socks.h filetransfer.h securechat.h securecall.h
    httpservice.h secureshell.h httpproxy.h)
set(ovl_HEADERS ${ovl_MOC_HEADERS}
    ovlnet.h buckets.h utils.h crypto.h logger.h)

qt5_wrap_cpp(ovl_MOC_SOURCES ${ovl_MOC_HEADERS})

add_library(ovlnet SHARED ${ovl_SOURCES} ${ovl_MOC_SOURCES} ${ovl_RCC_SOURCES})
set_target_properties(ovlnet PROPERTIES MACOSX_RPATH "${CMAKE_INSTALL_RPATH}")
set_target_properties(ovlnet PROPERTIES INSTALL_NAME_DIR ${CMAKE_INSTALL_FULL_LIBDIR})
target_link_libraries(ovlnet ${LIBS})

install(TARGETS ovlnet DESTINATION ${CMAKE_INSTALL_LIBDIR})
INSTALL_HEADERS_WITH_DIRECTORY("${ovl_HEADERS}" "${CMAKE_INSTALL_INCLUDEDIR}/libovlnet")
