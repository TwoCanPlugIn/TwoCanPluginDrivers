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

//
// Project: TwoCan
// Project Description: NMEA2000 Plugin for OpenCPN
// Unit: TwoCanError
// Unit Description: Encode, Decode and Output error messages
// Date: 6/8/2018
//


#include "../../common/inc/twocanerror.h"

// Formatted debug output
void DebugPrintf(wchar_t *fmt, ...)
{
	va_list argp;
	va_start(argp, fmt);
	wchar_t dbg_out[4096];
	vswprintf_s(dbg_out, sizeof(dbg_out), fmt, argp);
	va_end(argp);
	OutputDebugString(dbg_out);
}

// Retrieve the system error message for the last-error code
char *GetErrorMessage(int win32ErrorCode) {

	LPVOID lpMsgBuf;
	
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		win32ErrorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf,
		0, NULL);
	return (char *)lpMsgBuf;
	
}