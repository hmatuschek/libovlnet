set(ovl_SOURCES http.cc utils.cc buckets.cc logger.cc optionparser.cc
    ntp.cc node.cc pcp.cc natpmp.cc stream.cc socks.cc filetransfer.cc securechat.cc securecall.cc
    httpservice.cc secureshell.cc httpproxy.cc upnp.cc httpclient.cc subnetwork.cc dht.cc plugin.cc
    network.cc crypto.cc mailservice.cc)
set(ovl_MOC_HEADERS 
    ntp.hh node.hh pcp.hh natpmp.hh stream.hh socks.hh filetransfer.hh securechat.hh securecall.hh
    httpservice.hh secureshell.hh httpproxy.hh upnp.hh httpclient.hh subnetwork.hh dht.hh plugin.hh
    network.hh crypto.hh mailservice.hh)
set(ovl_HEADERS ${ovl_MOC_HEADERS}
    dht_config.hh ovlnet.hh buckets.hh utils.hh logger.hh optionparser.hh http.hh)

qt5_wrap_cpp(ovl_MOC_SOURCES ${ovl_MOC_HEADERS})

add_library(ovlnet SHARED ${ovl_SOURCES} ${ovl_MOC_SOURCES} ${ovl_RCC_SOURCES})
set_target_properties(ovlnet PROPERTIES SOVERSION
  "${OVLNET_VERSION_MAJOR}.${OVLNET_VERSION_MINOR}.${OVLNET_VERSION_PATCH}")
set_target_properties(ovlnet PROPERTIES MACOSX_RPATH "${CMAKE_INSTALL_RPATH}")
set_target_properties(ovlnet PROPERTIES INSTALL_NAME_DIR ${CMAKE_INSTALL_FULL_LIBDIR})
target_link_libraries(ovlnet ${LIBS})

add_custom_target(ovl_headers SOURCES ${ovl_HEADERS})

install(TARGETS ovlnet DESTINATION ${CMAKE_INSTALL_LIBDIR})
INSTALL_HEADERS_WITH_DIRECTORY("${ovl_HEADERS}" "${CMAKE_INSTALL_INCLUDEDIR}/ovlnet")

install(FILES "${CMAKE_SOURCE_DIR}/cmake/Findovlnet.cmake" DESTINATION "share/ovlnet")
