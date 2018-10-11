TwoCan Plug-in Drivers
==========================

These are Plug-In drivers to be used with TwoCan.

TwoCan is an OpenCPN PlugIn for integrating OpenCPN with NMEA2000® networks. It enables some NMEA2000® data to be directly integrated with OpenCPN by converting some NMEA2000® messages to NMEA 183 sentences and inserting them into the OpenCPN data stream. The TwoCan PlugIn is available at https://github.com/twocanplugin/twocanplugin.git

NMEA2000® is a registered trademark of the National Marine Electronics Association.

CAN Hardware interfaces
-----------------------

Three hardware interfaces are supported.

Kvaser Leaflight HS v2 - https://www.kvaser.com/product/kvaser-leaf-light-hs-v2/
USB interface, well packaged, relatively expensive, uses Kvaser provided software libraries

Canable Cantact - http://canable.io/
USB interface, very small PCB board, inexpensive, uses serial communications.

Axiomtek AX92903 - http://www.axiomtek.com/Default.aspx?MenuId=Products&FunctionId=ProductView&ItemId=8270&upcat=318&C=AX92903
Mini PCI Express (fits notebooks & some mini-ATX form factor motherboards), relatively inexpensive, uses serial communications.

One software interface is supported
FileDevice, simply replays raw log files. A sample log file named "twocanraw.log" may be copied to your "My Documents" folder to be replayed by the FileDevice driver. At present the name & location of the logfile are hardcoded in the FileDevice source code to match the same hardcoded values in the TwoCan plugin.

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

