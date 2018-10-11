##---------------------------------------------------------------------------
## Author:      Steven Adler (based on standard OpenCPN Plug-In CMAKE commands)
## Copyright:   2018
## License:     GPL v3+
##---------------------------------------------------------------------------

# define minimum cmake version
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

PROJECT(twocanutil)

SET(PACKAGE_NAME twocanutil)
SET(VERBOSE_NAME twocanutil)
SET(TITLE_NAME twocanutil)


SET(VERSION_MAJOR "1")
SET(VERSION_MINOR "0")

ADD_DEFINITIONS(-D__MSVC__)
ADD_DEFINITIONS(-D_CRT_NONSTDC_NO_DEPRECATE)
ADD_DEFINITIONS(-D_CRT_SECURE_NO_DEPRECATE)
ADD_DEFINITIONS(-DUNICODE)
ADD_DEFINITIONS(-D_UNICODE)


SET(SRC_TWOCANUTIL
        inc/twocandriver.h
        src/twocandriver.c
	inc/twocanerror.h
	src/twocanerror.c
        )

ADD_LIBRARY(${PACKAGE_NAME} STATIC ${SRC_TWOCANUTIL})
