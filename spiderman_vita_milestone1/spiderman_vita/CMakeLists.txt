cmake_minimum_required(VERSION 3.5)
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Set VITASDK")
  endif()
endif()
project(spiderman_vita C)
include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)
set(VITA_APP_NAME "Spider-Man (loader WIP)")
set(VITA_TITLEID  "SPDM00001")
set(VITA_VERSION  "00.01")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -O2 -std=gnu99 -D_GNU_SOURCE")
add_executable(${PROJECT_NAME}
  src/main.c src/so_util.c src/compat.c src/jni_fake.c)
# Milestone 1 does not touch GL yet; vitaGL/shacccg get added in milestone 2.
target_link_libraries(${PROJECT_NAME}
  kubridge_stub
  ScePower_stub
  SceLibKernel_stub
  SceKernelThreadMgr_stub
  SceKernelModulemgr_stub
  SceProcessmgr_stub
  m)
vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME} UNSAFE)
vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} ${PROJECT_NAME}.self
  VERSION ${VITA_VERSION} NAME ${VITA_APP_NAME})
