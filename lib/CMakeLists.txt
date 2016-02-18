set(ovl_SOURCES crypto.cc utils.cc buckets.cc logger.cc
    ntp.cc node.cc pcp.cc natpmp.cc stream.cc socks.cc filetransfer.cc securechat.cc securecall.cc
    httpservice.cc secureshell.cc httpproxy.cc)
set(ovl_MOC_HEADERS 
    ntp.hh node.hh pcp.hh natpmp.hh stream.hh socks.hh filetransfer.hh securechat.hh securecall.hh
    httpservice.hh secureshell.hh httpproxy.hh)
set(ovl_HEADERS ${ovl_MOC_HEADERS}
    ovlnet.hh buckets.hh utils.hh crypto.hh logger.hh)

qt5_wrap_cpp(ovl_MOC_SOURCES ${ovl_MOC_HEADERS})

add_library(ovlnet SHARED ${ovl_SOURCES} ${ovl_MOC_SOURCES} ${ovl_RCC_SOURCES})
set_target_properties(ovlnet PROPERTIES MACOSX_RPATH "${CMAKE_INSTALL_RPATH}")
set_target_properties(ovlnet PROPERTIES INSTALL_NAME_DIR ${CMAKE_INSTALL_FULL_LIBDIR})
target_link_libraries(ovlnet ${LIBS})

install(TARGETS ovlnet DESTINATION ${CMAKE_INSTALL_LIBDIR})
INSTALL_HEADERS_WITH_DIRECTORY("${ovl_HEADERS}" "${CMAKE_INSTALL_INCLUDEDIR}/libovlnet")
