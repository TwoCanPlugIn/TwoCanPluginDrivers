TwoCan Plug-in Drivers
==========================

These are Plug-In drivers to be used with TwoCan.

TwoCan is an OpenCPN PlugIn for integrating OpenCPN with NMEA2000® networks. It enables some NMEA2000® data to be directly integrated with OpenCPN by converting some NMEA2000® messages to NMEA 183 sentences and inserting them into the OpenCPN data stream. The TwoCan PlugIn is available at https://github.com/twocanplugin/twocanplugin.git

NMEA2000® is a registered trademark of the National Marine Electronics Association.

CAN Hardware interfaces
-----------------------

Three hardware interfaces are supported for Windows:

Kvaser Leaflight HS v2 - https://www.kvaser.com/product/kvaser-leaf-light-hs-v2/
USB interface, well packaged, relatively expensive, uses Kvaser provided software libraries

Canable Cantact - http://canable.io/
USB interface, very small PCB board, inexpensive, uses serial communications.

Axiomtek AX92903 - http://www.axiomtek.com/Default.aspx?MenuId=Products&FunctionId=ProductView&ItemId=8270&upcat=318&C=AX92903
Mini PCI Express (fits notebooks & some mini-ATX form factor motherboards), relatively inexpensive, uses serial communications.

Log File Software interfaces
-----------------------

Four software interfaces are supported:

FileDevice, replays log files created natively by the TwoCan plugin. The default TwoCan input logfile name is twocanraw.log
The log file format looks like: 0x01,0x01,0xF8,0x09,0x64,0xD9,0xDF,0x19,0xC7,0xB9,0x0A,0x04
The first four 4 bytes form an integer which represents the CAN header; priority, PGN, source, and destination and the eight data bytes (in hex)

YachtDevicesLog, replays log files in the format used by Yacht Devices' products. The default Yacht Devices input logfile name is yachtdevices.log
The log file looks like: 19:07:47.607 R 0DF80503 00 2B 2D 9E 44 5A A0 A1
consisting of a time stamp, a flag indicating receive (R) or transmit (R), an integer (in hex) representing the CAN header; priority, PGN, source, and destination and the eight data bytes (in hex)

KeesLog, replays log files in the format used by Kees Verruijt's Canboat software. The default Kees input logfile name is Kees.log
The log file format looks like: 2014-08-14T19:00:00.042,3,128267,1,255,8,0B,F5,0D,8D,24,01,00,00
consisting of a time stamp, priority, PGN, source, destination, data length and the data bytes (in hex)

Candumplog, replays log files created by the Linux candump utility. The default input Candump logfile name is candump.log
The log file format looks like: (1542794025.315691) can0 1DEFFF03#A00FE59856050404 representing a time stamp, can bus adapter id, an integer (in hex) representing priority, PGN, source and destination and eight data bytes (in hex)

On Windows, the default location for these log files is the user's "My Documents" folder.

Obtaining the source code
-------------------------

git clone https://github.com/twocanplugin/twocanplugindrivers.git


Build Environment
-----------------

Only Windows is currently supported.

The drivers build outside of the OpenCPN source tree and outside of the TwoCanPlugin source tree


Build Commands
--------------

In the top level directory in which you have downloaded the files using git clone:

  mkdir build

  cd build

  cmake ..

  Then

  cmake --build . --config release

    or

  cmake --build . --config debug


Installation
------------
I have yet to figure out the CMAKE or CPACK commands to create an installation package, so no installation utilities are provided.

Simply copy the resulting builds (axiomtek.dll, cantact.dll, filedevice.dll or kvaser.dll) into
C:\Program Files\OpenCPN\PlugIns\TwoCan_PI\Drivers (This assumes a default install of OpenCPN on Windows)

Problems
--------

Please send bug reports/questions to the opencpn forum or via email to twocanplugin@hotmail.com


License
-------
The TwoCanPlugIn drivers are licensed under the terms of the GPL v3 or, at your convenience, later version.

