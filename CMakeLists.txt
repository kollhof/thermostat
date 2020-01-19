# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)


# TODO: should these live in a .h or better be part of provisioning?
if(DEV_BOARD STREQUAL "recom-1")
  add_definitions(-DCNF_BOARD_PICO32)
  add_definitions(-DCONFIG_APP_PWM_GPIO=27)
  add_definitions(-DCONFIG_APP_TEMP_GPIO=25)
  add_definitions(-DCONFIG_APP_LED_GPIO=18)
  add_definitions(-DCONFIG_APP_DEVICE_ID="recom-1")

elseif(DEV_BOARD STREQUAL "recom-2")
  add_definitions(-DCONFIG_APP_PWM_GPIO=19)
  add_definitions(-DCONFIG_APP_TEMP_GPIO=21)
  add_definitions(-DCONFIG_APP_LED_GPIO=32)
  add_definitions(-DCONFIG_APP_DEVICE_ID="recom-2")

elseif(DEV_BOARD STREQUAL "recom-3")
  add_definitions(-DCONFIG_APP_PWM_GPIO=22)
  add_definitions(-DCONFIG_APP_TEMP_GPIO=23)
  add_definitions(-DCONFIG_APP_LED_GPIO=13)
  add_definitions(-DCONFIG_APP_DEVICE_ID="recom-3")

elseif(DEV_BOARD STREQUAL "recom-4")
  add_definitions(-DCONFIG_APP_PWM_GPIO=22)
  add_definitions(-DCONFIG_APP_TEMP_GPIO=23)
  add_definitions(-DCONFIG_APP_LED_GPIO=13)
  add_definitions(-DCONFIG_APP_DEVICE_ID="recom-4")

elseif(DEV_BOARD STREQUAL "pico32")
  add_definitions(-DCONFIG_APP_PWM_GPIO=25)
  add_definitions(-DCONFIG_APP_TEMP_GPIO=19)
  add_definitions(-DCONFIG_APP_LED_GPIO=27)
  add_definitions(-DCONFIG_APP_DEVICE_ID="pico32")

elseif(DEV_BOARD STREQUAL "hilink")
  add_definitions(-DCONFIG_APP_PWM_GPIO=5)
  add_definitions(-DCONFIG_APP_TEMP_GPIO=21)
  add_definitions(-DCONFIG_APP_LED_GPIO=4)
  add_definitions(-DCONFIG_APP_LED2_GPIO=2)
  add_definitions(-DCONFIG_APP_DEVICE_ID="recom-3")
endif()


include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(thermostat)


add_custom_target(flash-ota python3 "../ota/flash.py" ${DEV_BOARD})
