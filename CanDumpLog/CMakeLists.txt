##---------------------------------------------------------------------------
## Author:      Steven Adler (based on standard OpenCPN Plug-In CMAKE commands)
## Copyright:   2018
## License:     GPL v3+
##---------------------------------------------------------------------------

# define minimum cmake version
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

PROJECT(candumplog)

SET(PACKAGE_NAME candumplog)
SET(VERBOSE_NAME candumplog)
SET(TITLE_NAME candumplog)

SET(VERSION_MAJOR "1")
SET(VERSION_MINOR "0")

SET(SRC_CANDUMPLOG
        inc/candumplog.h
        src/candumplog.cpp
        )


ADD_DEFINITIONS(-D__MSVC__)
ADD_DEFINITIONS(-D_CRT_NONSTDC_NO_DEPRECATE)
ADD_DEFINITIONS(-D_CRT_SECURE_NO_DEPRECATE)
ADD_DEFINITIONS(-DUNICODE)
ADD_DEFINITIONS(-D_UNICODE)

LINK_DIRECTORIES("${CMAKE_SOURCE_DIR}/../Common/build/release")

ADD_LIBRARY(${PACKAGE_NAME} SHARED ${SRC_CANDUMPLOG})

TARGET_LINK_LIBRARIES(${PACKAGE_NAME} twocanutil Shlwapi)
