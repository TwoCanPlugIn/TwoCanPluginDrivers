##---------------------------------------------------------------------------
## Author:      Steven Adler (based on standard OpenCPN Plug-In CMAKE commands)
## Copyright:   2018
## License:     GPL v3+
##---------------------------------------------------------------------------

# define minimum cmake version
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

IF(CMAKE_BUILD_TYPE STREQUAL release)
    message(STATUS "**********  release mode")
ENDIF(CMAKE_BUILD_TYPE STREQUAL release)


PROJECT(TwoCanPlugInDrivers)

ADD_SUBDIRECTORY(Common)
ADD_SUBDIRECTORY(Axiomtek)
ADD_SUBDIRECTORY(Cantact)
ADD_SUBDIRECTORY(FileDevice)
ADD_SUBDIRECTORY(Kvaser)