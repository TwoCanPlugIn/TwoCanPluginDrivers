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

#ifndef _TWOCAN_CANDUMP
#define _TWOCAN_CANDUMP

#include "..\..\common\inc\twocandriver.h"

#define WINDOWS_LEAN_AND_MEAN
#include <windows.h>

// for "my documents" folder and path appending
// remember to also add Shlwapi.lib to linker
#include <ShlObj.h>
#include <ShlWapi.h>

// 'C++' STL
#include <fstream>
#include <regex>


// Hardcoded input LogFile
#define CONST_LOG_FILE L"candump.log"

#define CONST_MAX_BAD_LINES 100

#define DllExport extern "C" __declspec(dllexport)

DllExport char *DriverName(void);
DllExport char *DriverVersion(void);
DllExport char *ManufacturerName(void);
DllExport int OpenAdapter(void);
DllExport int CloseAdapter(void);
DllExport int ReadAdapter(byte *frame);

DWORD WINAPI ReadThread(LPVOID lParam);

#endif