set(VLF_CLIENT_SOURCES main.cpp application.cpp dhtstatus.cpp dhtstatusview.cpp)
set(VLF_CLIENT_MOC_HEADERS application.h dhtstatus.h dhtstatusview.h)
set(VLF_CLIENT_HEADERS ${VLF_CLIENT_MOC_HEADERS})

qt5_wrap_cpp(VLF_CLIENT_MOC_SOURCES ${VLF_CLIENT_MOC_HEADERS})

add_executable(vlfclient ${VLF_CLIENT_SOURCES} ${VLF_CLIENT_MOC_SOURCES} ${VLF_CLIENT_RCC_SOURCES})
target_link_libraries(vlfclient vlf ${LIBS})
