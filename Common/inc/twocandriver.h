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

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef _TWOCAN_DRIVER
#define _TWOCAN_DRIVER

#define WINDOWS_LEAN_AND_MEAN
#include <windows.h>

// Events and Mutexes used to notify callers & control access to data
#define CONST_DATARX_EVENT L"Global\\DataReceived"
#define CONST_DATATX_EVENT L"Global\\DataTransmit"
#define CONST_MUTEX_NAME L"Global\\DataMutex"
#define CONST_RW_MUTEX L"Globa\\ReadWriteMutex"
#define CONST_EVENT_THREAD_ENDED L"Local\\ThreadEnded"

// Treat all received NMEA 2000 bytes as unsigned char
typedef unsigned char byte;

// Length of a CAN v2.0 header
#define CONST_HEADER_LENGTH 4

// Length of an array 
#define COUNT(x)  (sizeof(x) / sizeof((x)[0]))

// CAN v2.0 29 bit header as used by NMEA 2000
typedef struct CanHeader {
	byte priority;
	byte source;
	byte destination;
	unsigned int pgn;
} CanHeader;

// A few functions used to convert the raw data from the different Windows devices (Axiomtek, Kvaser, Cantact) into a consistent CAN Frame byte array

// Reverse the byte order of a 4 byte header
// Cantact device seems to present the header as a BigEndian value
void ReverseHeader(byte* buf);

// Convert a hexadecimal string to a byte array
// Cantact & Axiomtek as serial devices present all of their data as hex strings so we convert the hex string to a byte array


int ConvertHexStringToByteArray(const byte *hexstr, const unsigned int len, byte *buf);

#ifdef __cplusplus
}
#endif

// Convert an unsigned integer to a 4 byte array
// Kvaser presents the CAN header as an integer
int ConvertIntegerToByteArray(const unsigned int value, byte *buf);

#endif