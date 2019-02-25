##---------------------------------------------------------------------------
## Author:      Steven Adler (based on standard OpenCPN Plug-In CMAKE commands)
## Copyright:   2018
## License:     GPL v3+
##---------------------------------------------------------------------------

# define minimum cmake version
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

PROJECT(yachtdeviceslog)

SET(PACKAGE_NAME yachtdeviceslog)
SET(VERBOSE_NAME yachtdeviceslog)
SET(TITLE_NAME yachtdeviceslog)

SET(VERSION_MAJOR "1")
SET(VERSION_MINOR "0")

SET(SRC_YACHTDEVICESLOG
        inc/yachtdeviceslog.h
        src/yachtdeviceslog.cpp
        )


ADD_DEFINITIONS(-D__MSVC__)
ADD_DEFINITIONS(-D_CRT_NONSTDC_NO_DEPRECATE)
ADD_DEFINITIONS(-D_CRT_SECURE_NO_DEPRECATE)
ADD_DEFINITIONS(-DUNICODE)
ADD_DEFINITIONS(-D_UNICODE)

LINK_DIRECTORIES("${CMAKE_SOURCE_DIR}/../Common/build/release")

ADD_LIBRARY(${PACKAGE_NAME} SHARED ${SRC_YACHTDEVICESLOG})

TARGET_LINK_LIBRARIES(${PACKAGE_NAME} twocanutil Shlwapi)
