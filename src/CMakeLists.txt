# chord sources
set(vlf_chord_SOURCES
   chord/chord.cpp chord/transport.cpp)
set(vlf_chord_MOC_HEADERS
   chord/chord.h   chord/transport.h)
set(vlf_chord_HEADERS ${vlf_chord_MOC_HEADERS})

# libqntp sources
set(vlf_ntp_SOURCES
    qntp/NtpClient.cpp qntp/NtpReply.cpp)
set(vlf_ntp_MOC_HEADERS
    qntp/NtpClient.h)
set(vlf_ntp_HEADERS
    qntp/NtpPacket.h qntp/NtpReply.h qntp/NtpReply_p.h qntp/NtpTimestamp.h qntp/QNtp.h
    ${vlf_ntp_MOC_HEADERS})

# client sources
set(vlf_SOURCES main.cpp ${vlf_ntp_SOURCES} ${vlf_chord_SOURCES}}
    application.cpp )
set(vlf_MOC_HEADERS ${vlf_ntp_MOC_HEADERS} ${vlf_chord_MOC_HEADERS}
    application.h )
set(vlf_HEADERS ${vlf_ntp_HEADERS} ${vlf_chord_HEADERS} ${vlf_MOC_HEADERS})

qt5_wrap_cpp(vlf_MOC_SOURCES ${vlf_MOC_HEADERS})
set(vlf_HEADERS logger.hh ${vlf_MOC_HEADERS} )

add_executable(vlfclient ${vlf_SOURCES} ${vlf_MOC_SOURCES} ${vlf_RCC_SOURCES})
target_link_libraries(vlfclient ${LIBS})
