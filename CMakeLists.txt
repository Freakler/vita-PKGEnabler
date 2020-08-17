cmake_minimum_required(VERSION 2.8)

# VitaSDK defines
if( NOT DEFINED CMAKE_TOOLCHAIN_FILE )
  if( DEFINED ENV{VITASDK} )
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
  message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()


# Project start
project(PKGEnabler)
include("${VITASDK}/share/vita.cmake" REQUIRED)
set(VITA_APP_NAME "PKG Enabler")
set(VITA_TITLEID  "PKGENABLE")
set(VITA_VERSION  "01.00")


# Flags and includes
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -O3 -Wno-unused-variable -Wno-unused-but-set-variable -Wno-format-truncation -fno-lto")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c+ + 11 -fno-rtti -fno-exceptions")
set(VITA_MKSFOEX_FLAGS "${VITA_MKSFOEX_FLAGS} -d PARENTAL_LEVEL=1")
set(VITA_MAKE_FSELF_FLAGS "${VITA_MAKE_FSELF_FLAGS} -a 0x2808000000000000")

include_directories(
)

link_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
)

add_executable(${PROJECT_NAME}
  src/main.c
  src/print/scr_printf.c
  src/print/font.c
  src/libsqlite/sqlite.c
)

target_link_libraries(${PROJECT_NAME}
  SceCtrl_stub
  SceVshBridge_stub
  SceDisplay_stub
  SceSqlite_stub
  SceSysmodule_stub
  ScePower_stub
)


vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME} UNSAFE)

### ### ### ### ### ### ### ### ### ### ### ### ### ### ### ### ### ### ### ### 

add_executable(${PROJECT_NAME}_user
  src/plugin/user.c
)
set_target_properties(${PROJECT_NAME}_user
  PROPERTIES LINK_FLAGS
  -nostartfiles
)
target_link_libraries(${PROJECT_NAME}_user
  taihen_stub
  SceLibKernel_stub
  SceIofilemgr_stub
)
vita_create_self(${PROJECT_NAME}.suprx ${PROJECT_NAME}_user
  CONFIG ${CMAKE_SOURCE_DIR}/src/plugin/user_exports.yml
  UNSAFE
)

### ### ### ### ### ### ### ### ### ### ### ### ### ### ### ### ### ### ### ### 

add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}.suprx)

vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} ${PROJECT_NAME}.self
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
	FILE PKGEnabler.suprx plugin/pkgenabler.suprx
	FILE livearea/icon0.png sce_sys/icon0.png
    FILE livearea/bg0.png sce_sys/livearea/contents/bg0.png
    FILE livearea/startup.png sce_sys/livearea/contents/startup.png
    FILE livearea/template.xml sce_sys/livearea/contents/template.xml
)


add_custom_target(vpksend
	COMMAND curl -T ${PROJECT_NAME}.vpk ftp://192.168.188.76:1337/ux0:/
	DEPENDS ${PROJECT_NAME}.vpk
)

add_custom_target(send
	COMMAND curl -T ${PROJECT_NAME}.self ftp://192.168.188.76:1337/ux0:/app/${VITA_TITLEID}/eboot.bin #$(PSVITAIP)
	DEPENDS ${PROJECT_NAME}.self
)

add_custom_target(copy
  COMMAND cp ${PROJECT_NAME}.self G:/app/${VITA_TITLEID}/eboot.bin
  DEPENDS ${PROJECT_NAME}.self
)