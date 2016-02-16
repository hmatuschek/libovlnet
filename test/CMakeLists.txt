SET(OVL_test_SOURCES main.cpp
    application.cc)
SET(OVL_test_MOC_HEADERS
    application.hh)
SET(OVL_test_HEADERS ${OVL_test_MOC_HEADERS} )

qt5_wrap_cpp(OVL_test_MOC_SOURCES ${OVL_test_MOC_HEADERS})

add_executable(ovltest ${OVL_test_SOURCES} ${OVL_test_MOC_SOURCES} ${OVL_test_RCC_SOURCES})
target_link_libraries(ovltest ovlnet ${LIBS})

INSTALL(TARGETS ovltest DESTINATION bin)

