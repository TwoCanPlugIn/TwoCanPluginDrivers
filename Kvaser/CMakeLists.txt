##---------------------------------------------------------------------------
## Author:      Steven Adler (based on standard OpenCPN Plug-In CMAKE commands)
## Copyright:   2018
## License:     GPL v3+
##---------------------------------------------------------------------------

# define minimum cmake version
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

PROJECT(kvaser)

SET(PACKAGE_NAME kvaser)
SET(VERBOSE_NAME kvaser)
SET(TITLE_NAME kvaser)

SET(VERSION_MAJOR "1")
SET(VERSION_MINOR "0")

SET(SRC_KVASER
        inc/canlib.h
        inc/canstat.h
        inc/canevt.h
        inc/obsolete.h
        inc/predef.h
        inc/kvaser.h
        src/kvaser.c
        )


#ADD_DEFINITIONS(-D__MSVC__)
#ADD_DEFINITIONS(-D_CRT_NONSTDC_NO_DEPRECATE)
#ADD_DEFINITIONS(-D_CRT_SECURE_NO_DEPRECATE)
ADD_DEFINITIONS(-DUNICODE)
ADD_DEFINITIONS(-D_UNICODE)

#for twocanutil static library
LINK_DIRECTORIES("${CMAKE_SOURCE_DIR}/../Common/build/release")

#for kvaser canlib32.lib
LINK_DIRECTORIES("${CMAKE_SOURCE_DIR}/kvaser/lib")

ADD_LIBRARY(${PACKAGE_NAME} SHARED ${SRC_KVASER})

#ADD_DEPENDENCIES(${PACKAGE_NAME} twocanutil canlib32)

TARGET_LINK_LIBRARIES(${PACKAGE_NAME} twocanutil canlib32)
