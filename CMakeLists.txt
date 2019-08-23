##---------------------------------------------------------------------------
## Author:      Steven Adler (based on standard OpenCPN Plug-In CMAKE commands)
## Copyright:   2018
## License:     GPL v3+
##---------------------------------------------------------------------------

# define minimum cmake version
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

PROJECT(TwoCanPlugInDrivers)

ADD_SUBDIRECTORY(Common)
ADD_SUBDIRECTORY(Axiomtek)
ADD_SUBDIRECTORY(Cantact)
ADD_SUBDIRECTORY(FileDevice)
ADD_SUBDIRECTORY(Kvaser)
ADD_SUBDIRECTORY(CandumpLog)
ADD_SUBDIRECTORY(KeesLog)
ADD_SUBDIRECTORY(YachtDevicesLog)
ADD_SUBDIRECTORY(Toucan)