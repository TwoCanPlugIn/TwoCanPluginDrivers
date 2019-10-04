##---------------------------------------------------------------------------
## Author:      Steven Adler (based on standard OpenCPN Plug-In CMAKE commands)
## Copyright:   2018
## License:     GPL v3+
##---------------------------------------------------------------------------

# define minimum cmake version
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

PROJECT(toucan)

SET(PACKAGE_NAME toucan)
SET(VERBOSE_NAME toucan)
SET(TITLE_NAME toucan)

SET(VERSION_MAJOR "1")
SET(VERSION_MINOR "1")

SET(SRC_TOUCAN
        inc/canal.h
        inc/canal_a.h
        inc/toucan.h
        src/toucan.c
        )


ADD_DEFINITIONS(-D__MSVC__)
ADD_DEFINITIONS(-D_CRT_NONSTDC_NO_DEPRECATE)
ADD_DEFINITIONS(-D_CRT_SECURE_NO_DEPRECATE)
ADD_DEFINITIONS(-DUNICODE)
ADD_DEFINITIONS(-D_UNICODE)

#for twocanutil static library
LINK_DIRECTORIES("${CMAKE_SOURCE_DIR}/../Common/build/release")

#for canal libraries
LINK_DIRECTORIES("${CMAKE_SOURCE_DIR}/toucan/lib")

ADD_LIBRARY(${PACKAGE_NAME} SHARED ${SRC_TOUCAN})

IF ("${CMAKE_SIZEOF_VOID_P}" STREQUAL "4")
  TARGET_LINK_LIBRARIES(${PACKAGE_NAME} twocanutil canal32)
ENDIF()

IF ("${CMAKE_SIZEOF_VOID_P}" STREQUAL "8")
  TARGET_LINK_LIBRARIES(${PACKAGE_NAME} twocanutil canal64)
ENDIF()
