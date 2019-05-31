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
// Unit: Driver for Axiomtek AX9203 adapter
// Unit Description: Access Axiomtek adapter via Serial port
// Date: 6/8/2018
// Function: On receipt of valid CAN frame, converts raw Axiomtek format into 
// TwoCan byte array and signals an event to the application.
//

#include "..\inc\axiomtek.h"

#include "..\..\common\inc\twocanerror.h"

// Separate thread to read data from the serial port
HANDLE threadHandle;

// The thread id.
DWORD threadId;

// Event signalled when valid CAN Frame is received
HANDLE frameReceivedEvent;

// Event signalled when the thread has terminated
HANDLE threadFinishedEvent;

// Mutex used to synchronize access to the CAN Frame buffer
HANDLE frameReceivedMutex;

// Pointer to the caller's CAN Frame buffer
byte *canFramePtr;

// Variable to indicate thread state
BOOL isRunning = FALSE;

// Handle to the serial port
HANDLE serialPortHandle;

// Variable to receive the number of bytes written to the serial port
int bytesWritten;

// Serial Port stuff
// BUG BUG What about serial ports greater than COM9 which must be specified as "\\\\.\\COM10"
// How are they returned from the Ports registry entry ??
WCHAR friendlyName[1024];
WCHAR portName[5];
int baudRate;
int dataBits;
int stopBits;
int parity;
int adapterPresent;

//
// The DLL entry point
// 

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD  fdwReason, LPVOID    lpvReserved) {
	switch (fdwReason)	{
	case DLL_PROCESS_ATTACH:
		DebugPrintf(L"DLL Process Attach\n");
		break;
	case DLL_THREAD_ATTACH:
		DebugPrintf(L"DLL Thread Attach\n");
		break;
	case DLL_THREAD_DETACH:
		DebugPrintf(L"DLL Thread Detach\n");
		break;
	case DLL_PROCESS_DETACH:
		DebugPrintf(L"DLL Process Detach\n");
		break;
	}
	// As nothing to do, just return TRUE
	return TRUE;
}

//
// Drivername,
// returns the name of the driver
// BUG BUG could retrieve this value from the registry
//

DllExport char *DriverName(void)	{
	return (char *)L"Axiomtek AX9203";
}

//
// Version
// return an arbitary version number for this driver
//

DllExport char *DriverVersion(void)	{
	return (char *)L"1.0";
}

//
// Manufacturer
// return the name of the hardware manufacturer
//

DllExport char *ManufacturerName(void)	{
	return (char *)L"Axiomtek";
}

//
// Open, connect to the adapter and get ready to start reading
// returns TWOCAN_RESULT_SUCCESS if events and mutexes and adapter configured correctly
//

DllExport int OpenAdapter(void)	{
	DebugPrintf(L"Open Adapter called\n");

	// Create an event that is used to notify the caller of a received frame
	frameReceivedEvent = CreateEvent(NULL, FALSE, FALSE, CONST_DATARX_EVENT);

	if (frameReceivedEvent == NULL)
	{
		// Fatal error
		DebugPrintf(L"Create FrameReceivedEvent failed (%d)\n", GetLastError());
		return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER, TWOCAN_ERROR_CREATE_FRAME_RECEIVED_EVENT);
	}

	// Create an event that is used to notify the close method that the thread has ended
	threadFinishedEvent = CreateEvent(NULL, FALSE, FALSE, CONST_EVENT_THREAD_ENDED);

	if (threadFinishedEvent == NULL)
	{
		// Fatal error
		DebugPrintf(L"Create ThreadFinished Event failed (%d)\n", GetLastError());
		return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER, TWOCAN_ERROR_CREATE_THREAD_COMPLETE_EVENT);
	}

	// Open the mutex that is used to synchronize access to the Can Frame buffer
	// Initial state set to true, meaning we "own" the initial state of the mutex
	frameReceivedMutex = OpenMutex(SYNCHRONIZE, TRUE, CONST_MUTEX_NAME);

	if (frameReceivedMutex == NULL)
	{
		// Fatal error
		DebugPrintf(L"Open Mutex failed (%d)\n", GetLastError());
		return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER, TWOCAN_ERROR_CREATE_FRAME_RECEIVED_MUTEX);
	}

	// Retrieve COM Port Settings from Registry
	if (GetRegistrySettings(friendlyName, portName, &baudRate, &dataBits, &stopBits, &parity, &adapterPresent)) {
		DebugPrintf(L"Name: %s\nPort: %s\n", friendlyName, portName);
		DebugPrintf(L"Adapter Present: %d\n", adapterPresent);
	}
	else {
		// Fatal error
		DebugPrintf(L"Adapter not present");
		return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER, TWOCAN_ERROR_ADAPTER_NOT_FOUND);
	}

	// Configure serial port settings
	ConfigureSerialPort();

	// Axiomtek specific commands to set the bit rate, reporting mode, open the port.
	ConfigureAdapter();

	return TWOCAN_RESULT_SUCCESS;
}

//
// Close, Stop reading & disconnect
// returns TWOCAN_RESULT_SUCCESS if reading thread terminated correctly
//

DllExport int CloseAdapter(void)	{
	// Terminate the read thread
	isRunning = FALSE;

	// Wait for the thread to exit
	int waitResult;
	waitResult = WaitForSingleObject(threadFinishedEvent, 1000);
	if (waitResult == WAIT_TIMEOUT) {
		DebugPrintf(L"Wait for threadFinishedEvent timed out");
	}
	if (waitResult == WAIT_ABANDONED) {
		DebugPrintf(L"Wait for threadFinishedEvent abandoned");
	}
	if (waitResult == WAIT_FAILED) {
		DebugPrintf(L"Wait for threadFinishedEvent Error: %d", GetLastError());
	}

	// Close all the handles
	int closeResult;
	closeResult = CloseHandle(threadFinishedEvent);
	if (closeResult == 0) {
		DebugPrintf(L"Close threadFinsishedEvent Error: %d", GetLastError());
	}

	closeResult = CloseHandle(frameReceivedEvent);
	if (closeResult == 0) {
		DebugPrintf(L"Close frameReceivedEvent Error: %d", GetLastError());
	}
	closeResult = CloseHandle(threadHandle);
	if (closeResult == 0) {
		DebugPrintf(L"Close threadHandle Error: %d", GetLastError());
	}

	// Close the Axiomtek Adapter
	WriteFile(serialPortHandle, "+++\r\n", 5, &bytesWritten, NULL);
	
	WriteFile(serialPortHandle, "\r\n", 2, &bytesWritten, NULL);
	
	WriteFile(serialPortHandle, "@C1\r\n", 5, &bytesWritten, NULL);
	
	WriteFile(serialPortHandle, "\r\n", 2, &bytesWritten, NULL);
	
	// Close the serial port
	closeResult = CloseHandle(serialPortHandle);
	if (closeResult == 0) {
		DebugPrintf(L"Close Serial Port Error: %d", GetLastError());
	}

	return TWOCAN_RESULT_SUCCESS;
}


//
// Read, starts the read thread
// [in] frame, pointer to byte array for the CAN Frame
// Returns TWOCAN_RESULT_SUCCESS if thread created successfully
//

DllExport int ReadAdapter(byte *frame)	{
	// Save the pointer to the Can Frame
	canFramePtr = frame;
	
	// Start the read thread
	isRunning = TRUE;
	threadHandle = CreateThread(NULL, 0, ReadThread, NULL, 0, &threadId);
	if (threadHandle != NULL) {
		DebugPrintf(L"Axiomtek read thread started: %d\n", threadId);
		return TWOCAN_RESULT_SUCCESS;
	}
	isRunning = FALSE;
	DebugPrintf(L"Read thread failed: %d (%d)\n", threadId, GetLastError());
	return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER, TWOCAN_ERROR_CREATE_THREAD_HANDLE);
}

//
// Read thread, reads data from the serial port, if a valid Cantact Frame is received,
// process and notify the caller
// Upon exit, returns TWOCAN_RESULT_SUCCESS as the Thread Exit Code
//

DWORD WINAPI ReadThread(LPVOID lpParam)
{
	DWORD mutexResult;
	char *getPtr;
	char *putPtr;
	char serialBuffer[1024];
	char assemblyBuffer[4096];
	DWORD bytesRead;
	int bytesRemaining;

	BOOL start;
	BOOL end;
	BOOL partial;

	while (isRunning) {

		start = FALSE;
		end = FALSE;
		partial = FALSE;

		putPtr = assemblyBuffer;


		if (ReadFile(serialPortHandle, &serialBuffer, sizeof(serialBuffer), &bytesRead, NULL) != FALSE) {

			bytesRemaining = bytesRead;
			getPtr = serialBuffer;

			while (bytesRemaining != 0) {

				// start character
				if ((*getPtr == '@') || (*getPtr == '#')) {
					start = TRUE;
					*putPtr = *getPtr;
					putPtr++;
				}

				// normal character
				if ((*getPtr != '\n') && (*getPtr != '\r') && (*getPtr != '@') && (*getPtr != '#')){
					*putPtr = *getPtr;
					putPtr++;
				}

				// end character
				if (*getPtr == '\n') {
					end = TRUE;
				}

				// end character but no start or partial
				if ((end) && (!start) && (!partial)) {
					start = FALSE;
					end = FALSE;
					partial = FALSE;
					putPtr = assemblyBuffer;
				}

				//assert((putChar - assemblyBuffer) > sizeof(assemblyBuffer));

				// have a complete frame			
				if (((start) && (end)) || ((partial) && (end)))  {

					// ignore Axiomtek #OK & #ERROR messages
					if (assemblyBuffer[0] == '#') {
						start = FALSE;
						end = FALSE;
						partial = FALSE;
						putPtr = assemblyBuffer;
					}

					// Check we have the frame start character (@), CAN frame identifier (F) and extended frame identifier (1)
					if ((assemblyBuffer[0] == AXIOMTEK_FRAME_START) && (assemblyBuffer[1] == AXIOMTEK_FRAME_HEADER) && (assemblyBuffer[6] == '1')) {

						// 8 hexadecimal characters respresent the 4 byte CAN Header
						byte headerAsString[CONST_HEADER_LENGTH * 2];

						// copy the header bytes from the Axiomtek string
						memcpy(headerAsString, &assemblyBuffer[7], CONST_HEADER_LENGTH * 2);

						// convert hexadecimal string to byte array 
						byte *headerAsByte;
						headerAsByte = (byte *)malloc(CONST_HEADER_LENGTH * sizeof(byte));
						ConvertHexStringToByteArray(headerAsString, CONST_HEADER_LENGTH, headerAsByte);

						// reverse the integer bytes (I assume Endianess)
						ReverseHeader(headerAsByte);

						// now retrieve the payload
						int payload_len;
						byte *payload;

						// payload length is transmitted in byte 15 & 16
						payload_len = assemblyBuffer[16] - '0';
						payload = (byte *)malloc(payload_len * 2 * sizeof(byte));

						// copy payload from axiomtek string 
						memcpy(payload, &assemblyBuffer[17], payload_len * 2);

						// convert hex characters to byte array
						byte *data;
						data = (byte *)malloc(payload_len * sizeof(byte));

						ConvertHexStringToByteArray(payload, payload_len, data);

						// make sure we can get a lock on the buffer
						mutexResult = WaitForSingleObject(frameReceivedMutex, INFINITE);

						if (mutexResult == WAIT_OBJECT_0) {

							// copy the final data here
							memcpy(&canFramePtr[0], headerAsByte, CONST_HEADER_LENGTH);
							memcpy(&canFramePtr[4], data, payload_len);

							// release the lock
							ReleaseMutex(frameReceivedMutex);

							// notify the caller
							if (SetEvent(frameReceivedEvent)) {
								Sleep(5);
							}
							else {
								DebugPrintf(L"Set Event Error: %d\n", GetLastError());
							}

						}

						else {
							DebugPrintf(L"Adapter Mutex: %d -->%d\n", mutexResult, GetLastError());
						}

						// free the malloc's
						free(headerAsByte);
						free(payload);
						free(data);
					}

					// processed a valid frame so reset all 
					start = FALSE;
					end = FALSE;
					partial = FALSE;
					putPtr = assemblyBuffer;

				}

				getPtr++;
				bytesRemaining--;


			} // end while bytesRemaining != 0

			// had a start character but no end character, therefore a partial frame
			if ((start) && (!end)) {
				partial = TRUE;
			}

		} // end if ReadFile

	} // while isRunning

	SetEvent(threadFinishedEvent);
	ExitThread(TWOCAN_RESULT_SUCCESS);
}

//
// Configure Serial Port Settings, Port, Baud Rate, Start & Stop Bits, Parity etc.
//


int ConfigureSerialPort(void) {

	serialPortHandle = CreateFile(portName, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

	if (serialPortHandle == INVALID_HANDLE_VALUE) {
		DebugPrintf(L"Error opening %ls\n", portName);
		return 	SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER, TWOCAN_ERROR_CREATE_SERIALPORT);
	}

	DebugPrintf(L"Opened port %ls\n", portName);

	DCB dcbSettings = { 0 };
	dcbSettings.DCBlength = sizeof(dcbSettings);

	if (!GetCommState(serialPortHandle, &dcbSettings)) {
		DebugPrintf(L"Error retrieving GetCommState %d\n", GetLastError());
		// BUG BUG need error handling
	}

	// default settings for Cantact device
	// BUG BUG should retrieve these automagically from the registry
	dcbSettings.BaudRate = baudRate;
	dcbSettings.ByteSize = dataBits;
	dcbSettings.StopBits = stopBits;
	dcbSettings.Parity = parity;

	if (!SetCommState(serialPortHandle, &dcbSettings))  	{
		DebugPrintf(L"Error setting DCB Structure %d\n", GetLastError());
		// BUG BUG need error handling
	}
	else
	{
		DebugPrintf(L"Set DCB Structure\n");
		DebugPrintf(L"Baudrate = %d\n", dcbSettings.BaudRate);
		DebugPrintf(L"Data bits = %d\n", dcbSettings.ByteSize);
		DebugPrintf(L"Stop bits = %d\n", dcbSettings.StopBits);
		DebugPrintf(L"Parity = %d\n", dcbSettings.Parity);
	}

	COMMTIMEOUTS timeouts = { 0 };
	timeouts.ReadIntervalTimeout = 10;
	timeouts.ReadTotalTimeoutConstant = 0;
	timeouts.ReadTotalTimeoutMultiplier = 0;
	timeouts.WriteTotalTimeoutConstant = 10;
	timeouts.WriteTotalTimeoutMultiplier = 0;

	if (!SetCommTimeouts(serialPortHandle, &timeouts)) {
		DebugPrintf(L"Error setting Time Outs %ld\n", GetLastError());
		// BUG BUG need error handling
	}
	else {
		DebugPrintf(L"Setting Serial Port Timeouts Successful\n");
	}
	return TWOCAN_RESULT_SUCCESS;
}

//
// Configure the Axiomtek adapter
// Set the bus speed to 250K as used by NMEA 2000
// returns TWOCAN_RESULT_SUCCESS if no errors
//

int ConfigureAdapter(void) {
	
	if (serialPortHandle != NULL) {
		
		WriteFile(serialPortHandle, "+++\r\n", 5, &bytesWritten, NULL);
		DebugPrintf(L"Axiomtek Commnd Mode +++: %d\n", bytesWritten);
		
		WriteFile(serialPortHandle, "\r\n", 2, &bytesWritten, NULL);
		DebugPrintf(L"Axiomtek CrLf %d\n", bytesWritten);
		
		WriteFile(serialPortHandle, "@C1\r\n", 5, &bytesWritten, NULL);
		DebugPrintf(L"Axiomtek Close Port @C1: %d\n", bytesWritten);
		
		WriteFile(serialPortHandle, "\r\n", 2, &bytesWritten, NULL);
		DebugPrintf(L"Axiomtek CrLf %d\n", bytesWritten);
		
		WriteFile(serialPortHandle, "@B9\r\n", 5, &bytesWritten, NULL);
		DebugPrintf(L"Axiomtek Set Bitrate @B9: %d\n", bytesWritten);
		
		WriteFile(serialPortHandle, "\r\n", 2, &bytesWritten, NULL);
		DebugPrintf(L"Axiomtek CrLf %d\n", bytesWritten);

		WriteFile(serialPortHandle, "@O100\r\n", 7, &bytesWritten, NULL);
		DebugPrintf(L"Axiomtek Open Port 1 @O100: %d\n", bytesWritten);
		
		WriteFile(serialPortHandle, "\r\n", 2, &bytesWritten, NULL);
		DebugPrintf(L"Axiomtek CrLf %d\n", bytesWritten);
		
		WriteFile(serialPortHandle, "@S3\r\n", 5, &bytesWritten, NULL);
		DebugPrintf(L"Axiomtek  Report Mode @S3: %d\n", bytesWritten);

		WriteFile(serialPortHandle, "\r\n", 2, &bytesWritten, NULL);
		DebugPrintf(L"Axiomtek CrLf %d\n", bytesWritten);

		return TWOCAN_RESULT_SUCCESS;
	}
	else {
		DebugPrintf(L"Unable to Configure NMEA 2000 Adapter, invalid serial port");
		return SET_ERROR(TWOCAN_RESULT_ERROR, TWOCAN_SOURCE_DRIVER, TWOCAN_ERROR_CONFIGURE_ADAPTER);
	}
}

// Retrieve COM Port settings from the registry
//
// From the Axiomtek.inf installation file
// one of the values will be the Axiomtek/FTDI PnP Id USB\\VID_0403&PID_6001 appended with the matching subkey
#define CONST_FTDI_CONFIG_KEY L"SYSTEM\\CurrentControlSet\\services\\usbser\\enum"

#define CONST_FTDI_PNP_KEY L"SYSTEM\\CurrentControlSet\\Enum\\FTDIBUS\\VID_0403+PID_6001+"

//It appears as though the GUID is stored as ANSI ? Must be a ANSI .INF file or an ANSI version of the installation API's
#define CONST_FTDI_GUID_UNICODE  L"{4d36e978-e325-11ce-bfc1-08002be10318}"
#define CONST_FTDI_GUID_ANSI  "{4d36e978-e325-11ce-bfc1-08002be10318}"

#define CONST_SERIAL_PORT_CONFIG L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion"

int GetRegistrySettings(WCHAR *friendlyName, WCHAR *portName, int *baudRate, int *dataBits, int *stopBits, int *parity, int *isPresent) {

	DebugPrintf(L"Opening Registry\n");
	DebugPrintf(L"Key Name: %s\n", CONST_FTDI_PNP_KEY);

	HKEY registryKey;
	LONG result;
	WCHAR parentKey[1024];
	
	WCHAR *subKeyName = malloc(1024);
	DWORD subKeyLength = 1024 * (sizeof(subKeyName) / sizeof(*subKeyName));

	WCHAR *keyValue = malloc(1024);
	DWORD keyLength = 1024 * (sizeof(keyValue) / sizeof(*keyValue));

	DWORD tmpKeyLength = keyLength;

	DWORD keyType;

	// Get a handle to the registry key for the FTDI device
	result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, CONST_FTDI_PNP_KEY, 0, KEY_READ, &registryKey);

	// If the key isn't found, assume the Axiomtek device has never been installed
	if (result != ERROR_SUCCESS) {
		free(keyValue);
		free(subKeyName);
		return FALSE;
	}

	// Key is present assume Axiomtek device has at least been installed
	DebugPrintf(L"RegOpenKey: %d (%d)\n", result, GetLastError());

	// Iterate the sub keys until we find a sub key that contains the matching classGUID
	for (int i = 0;; i++) {

		result = RegEnumKeyEx(registryKey, i, subKeyName, &subKeyLength, NULL, NULL, NULL, NULL);
		if (result != ERROR_SUCCESS)  {
			break;
		}

		//BUG BUG Debug
		DebugPrintf(L"Sub Key: %s (%d)\n", subKeyName, subKeyLength);

		// Check if we have the correct registry key by comparing with the GUID from the FTDI .inf file
		result = RegGetValue(registryKey, subKeyName, L"ClassGUID", RRF_RT_ANY, &keyType, keyValue, &keyLength);

		//BUG BUG Debug
		DebugPrintf(L"Class GUID Result: %d  (%d)\n", result, keyLength);

		if (result == ERROR_SUCCESS) {
			//BUG BUG Debug
			DebugPrintf(L"Class GUID Key Value: %s  [%d] \n", keyValue, keyType);

			//handle both unicode or ANSI variants of the GUID
			WCHAR unicodeValue[1024];
			MultiByteToWideChar(CP_OEMCP, 0, (LPCCH)keyValue, -1, (LPWSTR)unicodeValue, (int)keyLength);
			if ((strcmp((const char *)keyValue, CONST_FTDI_GUID_ANSI)) || (wcscmp((const wchar_t *)unicodeValue, CONST_FTDI_GUID_UNICODE))) {
				//BUG BUG Debug
				DebugPrintf(L"Values Match\n");

				//Save this subkey value as it is used later on
				wcsncpy(parentKey, keyValue, keyLength);

				//Get the friendly name for the device
				keyLength = tmpKeyLength;
				result = RegGetValue(registryKey, subKeyName, L"FriendlyName", RRF_RT_ANY, &keyType, keyValue, &keyLength);
				if (result == ERROR_SUCCESS) {
					//BUG BUG Debug
					DebugPrintf(L"Friendly Name Result: %d  (%d)\n", result, keyLength);
					DebugPrintf(L"Friendly Name Key Value: %s  [%d] \n", keyValue, keyType);

					wcsncpy(friendlyName, keyValue, keyLength);
				} // end friendly name

				// Find the port name under the sub key Device Parameters
				wcscat(subKeyName, L"\\Device Parameters");
				// What serial port does the FTDI device use
				keyLength = tmpKeyLength;
				result = RegGetValue(registryKey, subKeyName, L"PortName", RRF_RT_ANY, &keyType, keyValue, &keyLength);
				if (result == ERROR_SUCCESS) {
					// BUG BUG Debug
					DebugPrintf(L"Port Name Result: %d  (%d)\n", result, keyLength);
					// Save the serial port name
					wcsncpy(portName, keyValue, keyLength);
					// BUG BUG Debug
					DebugPrintf(L"Port Name Key Value: %s  [%d] \n", portName, keyType);
					// Append the colon 
					result = _snwprintf(keyValue, tmpKeyLength, L"%s:", portName);

					DebugPrintf(L"Debug portname: %s (%d)\n", keyValue, result);
					wcsncpy(portName, keyValue, wcslen(keyValue));

				} // end port name

				// Now lookup the port details
				RegCloseKey(registryKey);
				result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, CONST_SERIAL_PORT_CONFIG, 0, KEY_READ, &registryKey);

				// BUG BUG Debug
				DebugPrintf(L"Port details: %d\n", result);

				if (result == ERROR_SUCCESS) {
					keyLength = tmpKeyLength;
					result = RegGetValue(registryKey, L"Ports", portName, RRF_RT_ANY, &keyType, keyValue, &keyLength);

					// BUG BUG Debug
					DebugPrintf(L"%s Result: %d  (%d)\n", portName, result, keyLength);
					DebugPrintf(L"%s\n", keyValue);

					WCHAR *buffer;

					baudRate = (int *)wcstol(wcstok_s(keyValue, L",", &buffer), NULL, 10);
					parity = (int *)wcstok_s(NULL, L",", &buffer)[0]; //Just want the first character
					dataBits = (int *)_wtoi(wcstok_s(NULL, L",", &buffer));
					stopBits = (int *)_wtoi(wcstok_s(NULL, L",", &buffer));

					// BUG BUG Debug
					DebugPrintf(L"Baud: %d, Data: %d, Stop: %d, Parity: %lc\n", baudRate, dataBits, stopBits, parity);

					RegCloseKey(registryKey);

				} // end port details

				// Now find if the device is inserted
				result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, CONST_FTDI_CONFIG_KEY, 0, KEY_READ, &registryKey);

				//BUG BUG Debug
				DebugPrintf(L"Is Inserted: %d\n", result);
				if (result == ERROR_SUCCESS) {
					keyLength = tmpKeyLength;
					result = RegGetValue(registryKey, NULL, L"Count", RRF_RT_ANY, &keyType, keyValue, &keyLength);

					// BUG BUG Debug
					DebugPrintf(L"Count Result: %d  (%d)\n", result, keyLength);
					int count = (int)*keyValue;
					WCHAR *countAsString = (WCHAR *)malloc((sizeof(int) * 8) + 1);

					DebugPrintf(L"Count Entries: %d\n", count);
					for (int i = 0; i < count; i++) {
						keyLength = tmpKeyLength;
						_itow(i, countAsString, 10);
						result = RegGetValue(registryKey, NULL, countAsString, RRF_RT_ANY, &keyType, keyValue, &keyLength);
						if (result == ERROR_SUCCESS) {
							// BUG BUG debugging
							DebugPrintf(L"Entry: %d, Value: %s, Length: %d\n", i, keyValue, keyLength);
							// Now compare the values
							WCHAR *buffer;
							DebugPrintf(L"%s\n", wcstok_s(keyValue, L"\\", &buffer)); //should match USB
							DebugPrintf(L"%s\n", wcstok_s(NULL, L"\\", &buffer)); //should match PnP ID
							DebugPrintf(L"%s\n", wcstok_s(NULL, L"\\", &buffer)); //should match subKey

						}
						else {
							DebugPrintf(L"Error %d (%d)\n", result, GetLastError());
						}

					}
					free(countAsString);
					RegCloseKey(registryKey);
				} // end is device present

			} // end found matching GUID

		} // end iterating subKeys for ClassGUID values

		free(keyValue);
		free(subKeyName);

	}// end iterating sub keys

	return TWOCAN_RESULT_SUCCESS;
}
