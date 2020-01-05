# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)


# if(DEV_BOARD)
#   set(SDKCONFIG "sdkconfig-${DEV_BOARD}")
# endif()


include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(thermostat)


add_custom_target(flash-ota python3 "../ota/flash.py")

