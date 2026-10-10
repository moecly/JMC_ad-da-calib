cmake_minimum_required(VERSION 3.13)

include(cmake/gcc.cmake)

project(AIO C CXX ASM)

set(BOARD "DIG_8IN_8OUT_NPN" CACHE STRING "Target board")
set(BUILD_TYPE "Debug" CACHE STRING "Build type (Debug / Release)")

set(MAJOR 1)
set(MINOR 9)

set(CALIB_ROLE "APP" CACHE STRING "Calibration role (APP / REF / DUT / MANUAL)")

# 采集链路定位开关（仅 CALIB_ROLE != APP 时有意义）
set(CALIB_LOG_SPAN "" CACHE STRING "1 = DUT 每点追加窗内极差日志")
set(CALIB_FIXED_IDEAL "" CACHE STRING "非空 = 67 点全钉在该 ideal 上驻留")
set(CALIB_SETTLE_MS "" CACHE STRING "覆盖 kSettleMs（建压等待 ms）")
set(CALIB_FIRST_SETTLE_MS "" CACHE STRING "覆盖 kFirstSettleMs（首点建压等待 ms）")
set(CALIB_AUTO_START "" CACHE STRING "非空 = DUT 上电自动按默认配置（4 通道，REF ch0）启动")
set(AD_FILTER_NUM "" CACHE STRING "覆盖 AD7689Drv::FILTER_NUM（盒式窗深度）")
set(AD_BURST_SAMPLE "" CACHE STRING "非空 = 单通道连采 3 帧诊断路径")

set(LIB at32)
set(LIB_ITF at32_itf)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(${LIB} STATIC "")
add_library(${LIB_ITF} INTERFACE)
add_library(app OBJECT "")
target_link_libraries(${LIB} PUBLIC ${LIB_ITF})
target_link_libraries(app PRIVATE ${LIB})
if (BUILD_TYPE STREQUAL "Release")
  set(VERSION_SUFFIX "v${MAJOR}.${MINOR}")
else()
  set(VERSION_SUFFIX "${BUILD_TYPE}-v${MAJOR}.${MINOR}")
endif()
if (CALIB_ROLE STREQUAL "APP")
  set(PROJECT_NAME "${BOARD}-${VERSION_SUFFIX}")
else()
  set(PROJECT_NAME "${BOARD}-${CALIB_ROLE}-${VERSION_SUFFIX}")
endif()
set(PROJECT_EXEC_NAME ${PROJECT_NAME}.elf)
add_executable(${PROJECT_EXEC_NAME} $<TARGET_OBJECTS:app>)
target_link_libraries(${PROJECT_EXEC_NAME} PRIVATE app)

file(GLOB_RECURSE AT32_SOURCE
    ${CMAKE_SOURCE_DIR}/firmware/*.c
    ${CMAKE_SOURCE_DIR}/firmware/*.cpp
)
target_sources(${LIB} PRIVATE ${AT32_SOURCE})

set(AT32_INCLUDE
    ${CMAKE_SOURCE_DIR}/include/
    ${CMAKE_SOURCE_DIR}/include/libraries/drivers/inc/
    ${CMAKE_SOURCE_DIR}/include/libraries/cmsis/cm4/core_support/
    ${CMAKE_SOURCE_DIR}/include/libraries/cmsis/cm4/device_support/
)
target_include_directories(${LIB_ITF} INTERFACE ${AT32_INCLUDE})

set(AT32_DEFINED
    CONFIG_MAJOR_VERSION=${MAJOR}
    CONFIG_MINOR_VERSION=${MINOR}
    CONFIG_DEVICE_${BOARD}
    AT_START_F425_V1
    AT32F425K8T7
    USE_STDPERIPH_DRIVER
)
target_compile_definitions(${LIB_ITF} INTERFACE ${AT32_DEFINED})
if (NOT CALIB_ROLE STREQUAL "APP")
  target_compile_definitions(${LIB_ITF} INTERFACE "CONFIG_CALIB_ROLE_${CALIB_ROLE}")
  if (NOT CALIB_LOG_SPAN STREQUAL "")
    target_compile_definitions(${LIB_ITF} INTERFACE "CALIB_LOG_SPAN=${CALIB_LOG_SPAN}")
  endif()
  if (NOT CALIB_FIXED_IDEAL STREQUAL "")
    target_compile_definitions(${LIB_ITF} INTERFACE "CALIB_FIXED_IDEAL=${CALIB_FIXED_IDEAL}")
  endif()
  if (NOT CALIB_SETTLE_MS STREQUAL "")
    target_compile_definitions(${LIB_ITF} INTERFACE "CALIB_SETTLE_MS=${CALIB_SETTLE_MS}")
  endif()
  if (NOT CALIB_FIRST_SETTLE_MS STREQUAL "")
    target_compile_definitions(${LIB_ITF} INTERFACE "CALIB_FIRST_SETTLE_MS=${CALIB_FIRST_SETTLE_MS}")
  endif()
  if (NOT CALIB_AUTO_START STREQUAL "")
    target_compile_definitions(${LIB_ITF} INTERFACE "CALIB_AUTO_START=${CALIB_AUTO_START}")
  endif()
endif()
if (NOT AD_FILTER_NUM STREQUAL "")
  target_compile_definitions(${LIB_ITF} INTERFACE "AD_FILTER_NUM=${AD_FILTER_NUM}")
endif()
if (NOT AD_BURST_SAMPLE STREQUAL "")
  target_compile_definitions(${LIB_ITF} INTERFACE "AD_BURST_SAMPLE=${AD_BURST_SAMPLE}")
endif()
if(BUILD_TYPE STREQUAL "Debug")
  target_compile_definitions(${LIB_ITF} INTERFACE "DEBUG")
elseif(BUILD_TYPE STREQUAL "Release")
  target_compile_definitions(${LIB_ITF} INTERFACE "RELEASE")
endif()

set(AT32_COMPILE_OPTIONS
    -mcpu=cortex-m4
    -mthumb
    -O2
    -ffunction-sections
    $<$<COMPILE_LANGUAGE:C>:-std=c99>
)
target_compile_options(${LIB_ITF} INTERFACE ${AT32_COMPILE_OPTIONS})

file(GLOB_RECURSE APP_SOURCE
    ${CMAKE_SOURCE_DIR}/cmsis/*.c
    ${CMAKE_SOURCE_DIR}/cmsis/*.cpp
    ${CMAKE_SOURCE_DIR}/cmsis/*.s
    ${CMAKE_SOURCE_DIR}/Board/${BOARD}/*.c
    ${CMAKE_SOURCE_DIR}/Board/${BOARD}/*.cpp
    ${CMAKE_SOURCE_DIR}/user/*.c
    ${CMAKE_SOURCE_DIR}/user/*.cpp
)
target_sources(app PRIVATE ${APP_SOURCE})

if (NOT CALIB_ROLE STREQUAL "APP")
  file(GLOB_RECURSE CALIB_SOURCE ${CMAKE_SOURCE_DIR}/calib/*.cpp)
  target_sources(app PRIVATE ${CALIB_SOURCE})
endif()

set(APP_INCLUDE
    ${CMAKE_SOURCE_DIR}/Board/${BOARD}/project/inc
    ${CMAKE_SOURCE_DIR}/Board/${BOARD}
    ${CMAKE_SOURCE_DIR}/user/
    ${CMAKE_SOURCE_DIR}

    # GCC INCLUDE
    E:/develop/AT32IDE/platform/tools/gcc-arm-none-eabi-10.3-2021.10/arm-none-eabi/include
    E:/develop/AT32IDE/platform/tools/gcc-arm-none-eabi-10.3-2021.10/arm-none-eabi/include/c++/10.3.1
    E:/develop/AT32IDE/platform/tools/gcc-arm-none-eabi-10.3-2021.10/arm-none-eabi/include/c++/10.3.1/arm-none-eabi
)
target_include_directories(app PRIVATE ${APP_INCLUDE})

function(remove_semicolon target_val origin_val)
  set(tmp_val ${${origin_val}})
  string(REPLACE ";" " " tmp_val "${tmp_val}")
  string(STRIP "${tmp_val}" tmp_val)
  set(${target_val} "${tmp_val}" PARENT_SCOPE)
endfunction()

get_target_property(LD_FLAG ${LIB_ITF} INTERFACE_COMPILE_OPTIONS)
list(REMOVE_ITEM LD_FLAG $<$<COMPILE_LANGUAGE:C>:-std=c99>)
remove_semicolon(LD_FLAG LD_FLAG)

set_target_properties(${PROJECT_EXEC_NAME} PROPERTIES
    LINK_FLAGS "${LD_FLAG} -s -T \"${CMAKE_SOURCE_DIR}/ldscripts/AT32F425x8_FLASH.ld\" -Xlinker --gc-sections  -Wl,-Map,\"${BOARD}.map\" --specs=nano.specs -u _printf_float --specs=nosys.specs"
)

add_custom_command(
    TARGET ${PROJECT_EXEC_NAME}
    POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex ${PROJECT_EXEC_NAME} ${PROJECT_NAME}.hex
    COMMENT "Build ${PROJECT_NAME}.hex"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    VERBATIM
)
