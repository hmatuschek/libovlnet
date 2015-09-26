set(vlf_SOURCES 
    ntp.cpp dht.cpp pcp.cpp natpmp.cpp crypto.cpp)
set(vlf_MOC_HEADERS 
    ntp.h dht.h pcp.h natpmp.h)
set(vlf_HEADERS ${vlf_MOC_HEADERS}
    crypto.h)

qt5_wrap_cpp(vlf_MOC_SOURCES ${vlf_MOC_HEADERS})

add_library(vlf SHARED ${vlf_SOURCES} ${vlf_MOC_SOURCES} ${vlf_RCC_SOURCES})
target_link_libraries(vlf ${LIBS})
