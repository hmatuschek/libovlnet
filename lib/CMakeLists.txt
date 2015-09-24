set(vlf_SOURCES 
    NtpClient.cpp NtpReply.cpp dht.cpp )
set(vlf_MOC_HEADERS 
    NtpClient.h dht.h)
set(vlf_HEADERS ${vlf_MOC_HEADERS}
    NtpPacket.h NtpReply.h NtpReply_p.h NtpTimestamp.h QNtp.h)

qt5_wrap_cpp(vlf_MOC_SOURCES ${vlf_MOC_HEADERS})

add_library(vlf ${vlf_SOURCES} ${vlf_MOC_SOURCES} ${vlf_RCC_SOURCES})
target_link_libraries(vlf ${LIBS})
