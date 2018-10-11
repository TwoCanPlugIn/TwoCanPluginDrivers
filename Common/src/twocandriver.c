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
// Project: TwoCanD
// Project Description: NMEA2000 Plugin for OpenCPN
// Unit: TwoCanDriver
// Unit Description: Utility functions for TwCan PlugIn drivers
// Date: 6/8/2018
// Function: Various utility functions for byte conversions from TwoCanPlugin Drivers
//


#include "..\inc\twocandriver.h"

//
// Reverse the 4 byte header as Cantact device seems to present the header as Big Endian
// [in][out] buf, pointer to byte array
//

void ReverseHeader(byte* buf)
{
	byte tmp[CONST_HEADER_LENGTH];
	byte *val;
	val = buf;
	for (int i = CONST_HEADER_LENGTH - 1; i >= 0; i--, val++)
		tmp[i] = *val;
	memcpy(buf, tmp, CONST_HEADER_LENGTH);
}

//
// Convert hexadecimal string to byte array
// [in]hexstr, pointer to array of hexadecimal characters
// [in]len, number of hexadecimal characters
// [out] buf, pointer to byte array
// returns error code
//

int ConvertHexStringToByteArray(const byte *hexstr, const unsigned int len, byte *buf) {
	// Must be a even number of hexadecimal characters
	// BUG BUG Another check is to perform an isxdigit on each character
	if ((len % 2 == 0) && (hexstr != NULL) && (buf != NULL)) {
		byte val;
		byte pair[2];
		for (unsigned int i = 0; i < len; i++) {
			pair[0] = hexstr[i * 2];
			pair[1] = hexstr[(i * 2) + 1];
			val = (byte)strtoul((char *)pair, NULL, 16);
			buf[i] = val;
		}
		return TRUE;
	}
	else	{
		return FALSE;
	}
}

//
// Convert an unsigned integer to a byte array
// Kvaser library presents the CAN Frame header as an int
// [in] value, as unsigned integer
// [out] buf, pointer to 4 byte array
// returns error code
//

int ConvertIntegerToByteArray(const unsigned int value, byte *buf){
	if ((buf != NULL) && (COUNT(buf) == 4)) {
		buf[0] = (value >> 24) & 0xFF;
		buf[1] = (value >> 16) & 0xFF;
		buf[2] = (value >> 8) & 0xFF;
		buf[3] = value & 0xFF;
		return TRUE;
	}
	else {
		return FALSE;
	}
}
