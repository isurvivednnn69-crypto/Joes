cmake_minimum_required(VERSION 3.5)
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Set VITASDK")
  endif()
endif()
project(dbtap_vita C)
include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)
set(VITA_APP_NAME "DBTapBattle (port WIP)")
set(VITA_TITLEID  "DBTP00001")
set(VITA_VERSION  "00.01")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -O2 -std=gnu99")
add_executable(${PROJECT_NAME} src/main.c src/pac.c)
# Link line follows the vitaGL README; adjust if your vitaGL build differs.
target_link_libraries(${PROJECT_NAME}
  vitaGL vitashark SceShaccCg_stub mathneon SceGxm_stub SceDisplay_stub SceCtrl_stub
  SceTouch_stub SceAudio_stub SceSysmodule_stub SceCommonDialog_stub SceAppMgr_stub
  SceLibKernel_stub SceKernelThreadMgr_stub m)
vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME})
vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} ${PROJECT_NAME}.self
  VERSION ${VITA_VERSION} NAME ${VITA_APP_NAME})
cmake_minimum_required(VERSION 3.5)
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Set VITASDK")
  endif()
endif()
project(dbtap_vita C)
include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)
set(VITA_APP_NAME "DBTapBattle (port WIP)")
set(VITA_TITLEID  "DBTP00001")
set(VITA_VERSION  "00.01")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -O2 -std=gnu99")
add_executable(${PROJECT_NAME} src/main.c src/pac.c)
target_link_libraries(${PROJECT_NAME}
  vitaGL
  vitashark
  SceShaccCg_stub
  SceShaccCgExt
  taihen_stub
  mathneon
  SceKernelDmacMgr_stub
  SceGxm_stub
  SceDisplay_stub
  SceCtrl_stub
  SceTouch_stub
  SceAudio_stub
  SceSysmodule_stub
  SceCommonDialog_stub
  SceAppMgr_stub
  SceAppUtil_stub
  SceLibKernel_stub
  SceKernelThreadMgr_stub
  stdc++
  m)
vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME})
vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} ${PROJECT_NAME}.self
  VERSION ${VITA_VERSION} NAME ${VITA_APP_NAME})
