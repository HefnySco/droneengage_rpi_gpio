cmake_minimum_required(VERSION 3.10.0)

if(NOT WIN32)
  string(ASCII 27 Esc)
  SET(ColourReset "${Esc}[m")
  SET(ColourBold  "${Esc}[1m")
  SET(Red         "${Esc}[31m")
  SET(Green       "${Esc}[32m")
  SET(Yellow      "${Esc}[33m")
  SET(Blue        "${Esc}[34m")
  SET(Magenta     "${Esc}[35m")
  SET(Cyan        "${Esc}[36m")
  SET(White       "${Esc}[37m")
  SET(BoldRed     "${Esc}[1;31m")
  SET(BoldGreen   "${Esc}[1;32m")
  SET(BoldYellow  "${Esc}[1;33m")
  SET(BoldBlue    "${Esc}[1;34m")
  SET(BoldMagenta "${Esc}[1;35m")
  SET(BoldCyan    "${Esc}[1;36m")
  SET(BoldWhite   "${Esc}[1;37m")
endif()


if ( ${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR} )
message ("${Red} In-source builds not allowed! Create a build directory and run CMake from there.  ${ColourReset}")
message( FATAL_ERROR “In-source builds not allowed! Create a build directory and run CMake from there. ” )
exit(1)
endif()

SET (__APP__VERSION__ "1.3.5")
add_definitions( -D__APP__VERSION__="${__APP__VERSION__}")
project(DE_RPI_GPIO VERSION ${__APP__VERSION__})



SET (CMAKE_C_COMPILER /usr/bin/gcc)
SET (CMAKE_CXX_COMPILER /usr/bin/g++)

#C++ Version
SET(CMAKE_CXX_STANDARD 17)
SET(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(Threads REQUIRED)


#define default build
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
SET(CMAKE_BUILD_TYPE DEBUG)
ENDIF()

# Define an option for user control over DETAILED_DEBUG
option(DDEBUG "Detailed Debug" OFF) # Default is OFF
option(TEST_MODE_NO_WIRINGPI_LINK "Simulation mode" OFF) # Default is OFF


message ("${Yellow}=========================================================================${ColourReset}")
message ("CMAKE_C_COMPILER: "  ${CMAKE_C_COMPILER})
message ("CMAKE_CXX_COMPILER: " ${CMAKE_CXX_COMPILER})

#deceide debug or release
IF(CMAKE_BUILD_TYPE MATCHES DEBUG)
message("CMAKE_BUILD_TYPE: ${BoldYellow} DEBUG ${ColourReset}")
ELSEIF (CMAKE_BUILD_TYPE MATCHES RELEASE)
message("CMAKE_BUILD_TYPE: ${BoldGreen} RELEASE ${ColourReset}")
ENDIF()

# adding CMAKE_CXX_FLAGS properly
add_compile_options(
  -Wno-return-type 
  -Wno-address-of-packed-member
  -Werror=unused-variable
  -Werror=unused-result
  $<$<CONFIG:DEBUG>:-g3>
  $<$<CONFIG:DEBUG>:-Og>
  $<$<CONFIG:RELEASE>:-O2>
  $<$<CONFIG:RELEASE>:-Werror=parentheses>
)

# adding DEFINEs
add_compile_definitions(
    $<$<CONFIG:DEBUG>:DEBUG>
    $<$<CONFIG:RELEASE>:RELEASE>
)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--no-export-dynamic")


SET(OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
file(MAKE_DIRECTORY ${OUTPUT_DIRECTORY})
SET(EXECUTABLE_OUTPUT_PATH ${OUTPUT_DIRECTORY})


#add_subdirectory(src)

file(GLOB folder_src "./src/*.cpp")
file(GLOB folder_helpers "./src/helpers/*.cpp")
file(GLOB folder_common "./src/de_common/*.cpp")
file(GLOB folder_gpio "./src/gpio/*.cpp")



set(files ${folder_src} ${folder_common} ${folder_gpio} ${folder_helpers})

include_directories(${PROJECT_SOURCE_DIR}/3rdparty)

add_executable( OUTPUT_BINARY ${files})

# Link the required libraries
if(NOT TEST_MODE_NO_WIRINGPI_LINK)
  target_link_libraries(OUTPUT_BINARY 
    wiringPi
  )
else()
  # Define the preprocessor macro for TEST_MODE_NO_WIRINGPI_LINK
  target_compile_definitions(OUTPUT_BINARY PRIVATE TEST_MODE_NO_WIRINGPI_LINK)
endif()

# Conditionally define DDEBUG based on the DDEBUG option
target_compile_definitions(OUTPUT_BINARY PRIVATE
    $<$<BOOL:${DDEBUG}>:DDEBUG>
)
set_target_properties( OUTPUT_BINARY 
                PROPERTIES 
                    OUTPUT_NAME "de_rpi_gpio"
                    SUFFIX ".so"
                )


target_link_libraries(OUTPUT_BINARY  Threads::Threads)

target_compile_options(OUTPUT_BINARY
    PRIVATE
        -Wall
        -Wno-return-type-c-linkage
)

configure_file(de_rpi_gpio.config.module.json ${OUTPUT_DIRECTORY}/de_rpi_gpio.config.module.json COPYONLY)

# Highlight if DDEBUG or TEST_MODE_NO_HAILO_LINK are enabled
if (DDEBUG)
    message(STATUS "${Red}Option DDEBUG is ENABLED.${ColourReset}")
endif()


if (TEST_MODE_NO_WIRINGPI_LINK)
    message(STATUS "${Red}TEST_MODE_NO_WIRINGPI_LINK - SIMULATION MODE is ENABLED.${ColourReset}")
endif()

message ("${Yellow}=========================================================================${ColourReset}")

