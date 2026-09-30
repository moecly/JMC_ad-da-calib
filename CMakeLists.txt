cmake_minimum_required(VERSION 3.13)

include(cmake/gcc.cmake)

project(AIO C CXX ASM)

set(BOARD "DIG_8IN_8OUT_NPN" CACHE STRING "Target board")
set(BUILD_TYPE "Debug" CACHE STRING "Build type (Debug / Release)")

set(MAJOR 1)
set(MINOR 8)

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
  set(PROJECT_NAME "${BOARD}-v${MAJOR}.${MINOR}")
  set(PROJECT_EXEC_NAME ${PROJECT_NAME}.elf)
else()
  set(PROJECT_NAME "${BOARD}-${BUILD_TYPE}-v${MAJOR}.${MINOR}")
  set(PROJECT_EXEC_NAME ${PROJECT_NAME}.elf)
endif()
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

set(APP_INCLUDE
    ${CMAKE_SOURCE_DIR}/Board/${BOARD}/project/inc
    ${CMAKE_SOURCE_DIR}/Board/${BOARD}
    ${CMAKE_SOURCE_DIR}/user/

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
