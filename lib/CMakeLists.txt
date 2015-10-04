set(vlf_SOURCES 
    ntp.cpp dht.cpp pcp.cpp natpmp.cpp crypto.cpp utils.cpp stream.cpp socks.cpp)
set(vlf_MOC_HEADERS 
    ntp.h dht.h pcp.h natpmp.h crypto.h stream.h socks.h)
set(vlf_HEADERS ${vlf_MOC_HEADERS}
    utils.h)

qt5_wrap_cpp(vlf_MOC_SOURCES ${vlf_MOC_HEADERS})

add_library(vlf SHARED ${vlf_SOURCES} ${vlf_MOC_SOURCES} ${vlf_RCC_SOURCES})
set_target_properties(vlf PROPERTIES MACOSX_RPATH "${CMAKE_INSTALL_RPATH}")
set_target_properties(vlf PROPERTIES INSTALL_NAME_DIR ${CMAKE_INSTALL_FULL_LIBDIR})
target_link_libraries(vlf ${LIBS})

install(TARGETS vlf DESTINATION ${CMAKE_INSTALL_LIBDIR})
INSTALL_HEADERS_WITH_DIRECTORY("${vlf_HEADERS}" "${CMAKE_INSTALL_INCLUDEDIR}/libvlf")
