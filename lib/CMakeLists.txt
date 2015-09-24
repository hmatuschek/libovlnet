set(vlf_SOURCES 
    ntp.cpp dht.cpp pcp.cpp)
set(vlf_MOC_HEADERS 
    ntp.h dht.h pcp.h)
set(vlf_HEADERS ${vlf_MOC_HEADERS})

qt5_wrap_cpp(vlf_MOC_SOURCES ${vlf_MOC_HEADERS})

add_library(vlf SHARED ${vlf_SOURCES} ${vlf_MOC_SOURCES} ${vlf_RCC_SOURCES})
target_link_libraries(vlf ${LIBS})
