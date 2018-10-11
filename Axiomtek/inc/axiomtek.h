// Copyright(C) 2018 by Steven Adler
//
// This file is part of TwoCan, a plugin for OpenCPN.
//
// TwoCan is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// TwoCan is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with TwoCan. If not, see <https://www.gnu.org/licenses/>.
//
// NMEA2000® is a registered Trademark of the National Marine Electronics Association

#ifndef _TWOCAN_AXIOMTEK
#define _TWOCAN_AXIOMTEK

#include "..\..\common\inc\twocandriver.h"

#define WINDOWS_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>

#define DllExport __declspec( dllexport )

DllExport char *DriverName(void);
DllExport char *DriverVersion(void);
DllExport char *ManufacturerName(void);
DllExport int OpenAdapter(void);
DllExport int CloseAdapter(void);
DllExport int ReadAdapter(byte *frame);

DWORD WINAPI ReadThread(LPVOID lParam);
int ConfigureSerialPort(void);
int ConfigureAdapter(void);
int GetRegistrySettings(WCHAR *friendlyName, WCHAR *portName, int *baudRate, int *dataBits, int *stopBits, int *parity, int *isPresent);


// Axiomtek command strings to control the device
#define AXIOMTEK_COMMAND_MODE '+++'
#define AXIOMTEK_CLOSE_PORT '@C1'
#define AXIOMTEK_BITRATE_250 '@B9'
#define AXIOMTEK_REPORT_MODE '@S3'
#define AXIOMTEK_OPEN_PORT '@O100'

// Axiomtek characters used to indicate start of CAN Frame, identify CAN 2.0 extended frames etc.
#define AXIOMTEK_FRAME_START '@'
#define AXIOMTEK_FRAME_HEADER 'F'
#define	AXIOMTEK_CAN_V2 '1' 

#endif