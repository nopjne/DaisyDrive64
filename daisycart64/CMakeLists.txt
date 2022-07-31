cmake_minimum_required(VERSION 3.12)

set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_BUILD_TYPE Debug)
#include(../cmake/default_build.cmake)
include(../cmake/toolchains/stm32h750xx.cmake)


project(daisycart64)
#if (NOT TARGET daisycart64)
#pico_sdk_init()

add_executable(daisycart64
    MCUMain.cpp
    cic_c.cpp
)

include_directories(daisycart64 
    ../src
    ../src/dev
    ../src/per
    ../src/sys
    ../src/hid
    ../Middlewares/Third_Party/FatFs/src
    ../Drivers/CMSIS/Include
    ../Drivers/STM32H7xx_HAL_Driver/Inc
    ../Drivers/CMSIS/Device/ST/STM32H7xx/Include
    )



#add_compile_definitions(daisycart64 PRIVATE HAS_SDIO_CLASS USE_MULTI_SECTOR_IO)

link_directories(../build/src)
target_link_libraries(daisycart64 PRIVATE daisy_seed)
#endif()

