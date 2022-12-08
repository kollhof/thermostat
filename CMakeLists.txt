# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)
set(PROJECT_VER "2.7.1")


if(DEFINED ENV{HOMEKIT_PATH})
  set(HOMEKIT_PATH $ENV{HOMEKIT_PATH})
else()
  set(HOMEKIT_PATH $ENV{IDF_PATH}/../esp-homekit-sdk)
endif(DEFINED ENV{HOMEKIT_PATH})

set(
  EXTRA_COMPONENT_DIRS
    ${HOMEKIT_PATH}/components
    ${HOMEKIT_PATH}/components/homekit
)
message(STATUS "EXTRA_COMPONENT_DIRS: ${EXTRA_COMPONENT_DIRS}")


include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(thermostat)


add_custom_target(flash-ota python3 "../ota/flash.py" ${OTA_DEVICE_ID})
