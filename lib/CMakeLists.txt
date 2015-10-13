set(ovl_SOURCES 
    ntp.cpp dht.cpp pcp.cpp natpmp.cpp crypto.cpp utils.cpp stream.cpp socks.cpp buckets.cpp
    logger.cpp)
set(ovl_MOC_HEADERS 
    ntp.h dht.h pcp.h natpmp.h stream.h socks.h)
set(ovl_HEADERS ${ovl_MOC_HEADERS}
    buckets.h utils.h crypto.h logger.h)

qt5_wrap_cpp(ovl_MOC_SOURCES ${ovl_MOC_HEADERS})

add_library(ovl SHARED ${ovl_SOURCES} ${ovl_MOC_SOURCES} ${ovl_RCC_SOURCES})
set_target_properties(ovl PROPERTIES MACOSX_RPATH "${CMAKE_INSTALL_RPATH}")
set_target_properties(ovl PROPERTIES INSTALL_NAME_DIR ${CMAKE_INSTALL_FULL_LIBDIR})
target_link_libraries(ovl ${LIBS})

install(TARGETS ovl DESTINATION ${CMAKE_INSTALL_LIBDIR})
INSTALL_HEADERS_WITH_DIRECTORY("${ovl_HEADERS}" "${CMAKE_INSTALL_INCLUDEDIR}/libovlnet")
