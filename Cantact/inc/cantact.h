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

#ifndef _TWOCAN_CANTACT
#define _TWOCAN_CANTACT

#include "..\..\common\inc\twocandriver.h"

#define WINDOWS_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>

#define DllExport __declspec(dllexport)

DllExport char *DriverName(void);
DllExport char *DriverVersion(void);
DllExport char *ManufacturerName(void);
DllExport int IsInstalled(void);
DllExport int OpenAdapter(void);
DllExport int CloseAdapter(void);
DllExport int ReadAdapter(byte *frame);

DWORD WINAPI ReadThread(LPVOID lParam);
int ConfigureSerialPort(void);
int ConfigureAdapter(void);
int GetRegistrySettings(WCHAR *friendlyName, WCHAR *portName, int *baudRate, int *dataBits, int *stopBits, int *parity, int *isPresent);

// Cantact constants for opening & closing the bus, setting the bus speed, line endings
#define CANTACT_OPEN 'O'
#define CANTACT_250K 'S5'
#define CANTACT_CLOSE 'C'
#define CANTACT_LINE_TERMINATOR '\r'

// Cantact character used to indicate a CAN 2.0 extended frame
#define CANTACT_EXTENDED_FRAME 'T'
// Just for completeness
#define CANTACT_STANDARD_FRAME 't'
#define CANTACT_REMOTE_FRAME 'r'

#endif