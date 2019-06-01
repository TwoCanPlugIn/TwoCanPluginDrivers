// Copyright(C) 2018 by Steven Adler
//
// This file is part of TwoCan, a plugin for OpenCPN.
//

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
// Unit: Driver for Cantact adapter
// Unit Description: Access Cantact adapter via Serial port
// Date: 6/8/2018
// Function: On receipt of valid CAN frame, converts raw Cantact format into 
// TwoCan byte array and signals an event to the application
//

#include "..\inc\cantact.h"

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

// Variable to receive thc number of bytes written to the serial port
int bytesWritten;

// Serial Port stuff
WCHAR friendlyName[1024];
WCHAR portName[5];
int baudRate;
int dataBits;
int stopBits;
int parity;
int adapterPresent;

//
// Standard DLL entry point
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
	return (char *)L"Cantact";
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
// BUG BUG Should retrieve from registry
//

DllExport char *ManufacturerName(void)	{
	return (char *)L"Canable";
}


//
// IsInstalled
// return whether the adapter is physically present
//

DllExport int IsInstalled(void) {
	
	// BUG BUG error handling
	if (GetRegistrySettings(friendlyName, portName, &baudRate, &dataBits, &stopBits, &parity, &adapterPresent)) {

		// BUG BUG remove for production
		DebugPrintf(L"Name: %s Port: %s\n", friendlyName, portName);
		DebugPrintf(L"Adapter Present: %d\n", adapterPresent?"TRUE":"FALSE");

		return TRUE;
	}
	else {
		return FALSE;
	}
}


//
// Open, configure events and mutexes, connect to the adapter, 
// configure the N2K bus and get ready to start reading
// returns TWOCAN_RESULT_SUCCESS if no errors
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
		// Fatal eror
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
		//BUG BUG remove for production
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

	// Configure the cantact adapter withe correct NMEA 2000 bus speed
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
		DebugPrintf(L"Wait for threadFinishedEVent timed out");
	}

	if (waitResult == WAIT_ABANDONED) {
		DebugPrintf(L"Wait for threadFinishedEVent abandoned");
	}

	if (waitResult == WAIT_FAILED) {
		DebugPrintf(L"Wait for threadFinishedEVent Error: %d", GetLastError());
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

	// Close the cantact adapter
	WriteFile(serialPortHandle, "C\r", 2, &bytesWritten, NULL);
	
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
// returns TWOCAN_RESULT_SUCCESS if thread successfully created
//

DllExport int ReadAdapter(byte *frame)	{
	// Save the pointer to the Can Frame
	canFramePtr = frame;
	
	// Start the read thread
	isRunning = TRUE;
	threadHandle = CreateThread(NULL,0,ReadThread,NULL,0,&threadId); 
	if (threadHandle != NULL) {
		DebugPrintf(L"Cantact Read thread started: %d\n", threadId);
		return TWOCAN_RESULT_SUCCESS;
	}
	isRunning = FALSE;
	DebugPrintf(L"Read thread failed: %d (%d)\n", threadId, GetLastError());
	return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER, TWOCAN_ERROR_CREATE_THREAD_HANDLE);
}

//
// Read thread, reads data from the serial port, 
// if a valid Cantact Frame is received, convert the Cantact frame
// to a TwoCan CAN Frame byte array and notify the caller
// Upon Exit, return TWOCAN_RESULT_SUCCESS as the Thread Exit Code
//

// BUG BUG Think about using overlapped IO
// BUG BUG performance issues about malloc's etc.

DWORD WINAPI ReadThread(LPVOID lpParam) {
	DWORD mutexResult;
	char *getPtr;
	char *putPtr;
	char serialBuffer[4096];
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
				if (*getPtr == CANTACT_EXTENDED_FRAME) {
					start = TRUE;
					*putPtr = *getPtr;
					putPtr++;
				}

				// normal character
				if ((*getPtr != '\n') && (*getPtr != '\r') && (*getPtr != CANTACT_EXTENDED_FRAME)){
					*putPtr = *getPtr;
					putPtr++;
				}

				// end character
				if (*getPtr == '\r') {
					end = TRUE;
				}

				// end character but no start or partial
				// must have caught a frame mid-stream
				if ((end) && (!start) && (!partial)) {
					start = FALSE;
					end = FALSE;
					partial = FALSE;
					putPtr = assemblyBuffer;
				}

				//assert((putChar - assemblyBuffer) > sizeof(assemblyBuffer));

				// if we now have a complete frame			
				if (((start) && (end)) || ((partial) && (end)))  {

						if (assemblyBuffer[0] == CANTACT_EXTENDED_FRAME) {

							// 8 hexadecimal characters respresent the 4 byte CAN Header
							byte headerAsString[CONST_HEADER_LENGTH * 2];


							// copy the header hexadecimal characters from the Cantact string
							// Start from character 1 (go past leading "T")
							// could alternatively use 	strncpy(headerAsString, &assemblyBuffer[1], 8);
							memcpy(headerAsString, &assemblyBuffer[1], CONST_HEADER_LENGTH * 2);

							// convert hexadecimal string to byte array 
							byte *headerAsByte;
							headerAsByte = (byte *)malloc(CONST_HEADER_LENGTH * sizeof(byte));
							ConvertHexStringToByteArray(headerAsString, CONST_HEADER_LENGTH, headerAsByte);

							// reverse the integer bytes for Cantact device (I assume Endianess)
							ReverseHeader(headerAsByte);
							
							// now retrieve the payload
							int payload_len;
							byte *payload;

							// payload length is transmitted in byte 9
							payload_len = assemblyBuffer[9] - '0';

							payload = (byte *)malloc(payload_len * 2 * sizeof(byte));
							
							// copy payload from cantact string 
							memcpy(payload, &assemblyBuffer[10], payload_len * 2);

							// convert hex characters to byte array
							byte *data;
							data = (byte *)malloc(payload_len * sizeof(byte));

							ConvertHexStringToByteArray(payload, payload_len, data);

							// make sure we can get a lock on the buffer
							mutexResult = WaitForSingleObject(frameReceivedMutex, INFINITE);

							if (mutexResult == WAIT_OBJECT_0) {

								memcpy(&canFramePtr[0], headerAsByte, 4);
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


			} // end while bytesRemaining !=0

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
// Configure the Cantact adapter
// Set the bus speed to 250K as used by NMEA 2000
// [in] handle, handle to an open serial port
// returns TWOCAN_RESULT_SUCCESS if no errors
//

int ConfigureAdapter(void) {
	if (serialPortHandle != NULL) {
		WriteFile(serialPortHandle, "C\r", 2, &bytesWritten, NULL);
		DebugPrintf(L"Cantact Close Port Bytes Writen: %d\n", bytesWritten);
		WriteFile(serialPortHandle, "S5\r",3, &bytesWritten, NULL);
		DebugPrintf(L"Cantact Port Speed Bytes Written: %d\n", bytesWritten);
		WriteFile(serialPortHandle, "O\r", 2, &bytesWritten, NULL);
		DebugPrintf(L"Cantact Open Port Bytes Written: %d\n", bytesWritten);
		return TWOCAN_RESULT_SUCCESS;
	}
	else {
		DebugPrintf(L"Unable to Configure NMEA 2000 Adapter, invalid serial port");
		return TWOCAN_RESULT_ERROR | TWOCAN_SOURCE_DRIVER | TWOCAN_ERROR_CONFIGURE_ADAPTER;
	}
}

//
// Configure the serial port
// opens the correct serial port and configures baud rate, data & stop bits, parity etc.
// returns TWOCAN_RESULT_SUCCESS if no error
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
		DebugPrintf(L"Error retrieving GetCommState %d\n",GetLastError());
		// BUG BUG need error handling
	}

	// default settings for Cantact device
	// BUG BUG should retrieve these automagically from the registry
	dcbSettings.BaudRate = baudRate;
	dcbSettings.ByteSize = dataBits;
	dcbSettings.StopBits = stopBits;
	dcbSettings.Parity = parity;

	if (!SetCommState(serialPortHandle, &dcbSettings))  	{
		DebugPrintf(L"Error setting DCB Structure %d\n",GetLastError());
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
// Retrieve COM Port Settings Baud rate, Parity, Start & Stop Bits etc. from the registry
// Returns TWOCAN_RESULT_SUCCESS if no errors ??
//
// From the CANtact .inf installation file
// If the device is plugged in, count = n and for (i=0,i< Count;i++) 
// one of the values will be the CANtact PnP Id USB\\VID_AD50&PID_60C4 appended with  the matching subkey
#define CONST_CANTACT_CONFIG_KEY L"SYSTEM\\CurrentControlSet\\services\\usbser\\enum"

#define CONST_CANTACT_PNP_KEY L"SYSTEM\\CurrentControlSet\\enum\\USB\\VID_AD50&PID_60C4"

// It appears as though the GUID is stored as ANSI ? Must be a ANSI .INF file or an ANSI version of the installation API's
#define CONST_CANTACT_GUID_UNICODE  L"{4d36e978-e325-11ce-bfc1-08002be10318}"
#define CONST_CANTACT_GUID_ANSI  "{4d36e978-e325-11ce-bfc1-08002be10318}"

// and the serial port config can be found under
// where the matching Port name (appended with ':') will have a value containing the folowing string value: baud,parity,data,stop
#define CONST_SERIAL_PORT_CONFIG L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion"

int GetRegistrySettings(WCHAR *friendlyName, WCHAR *portName, int *baudRate, int *dataBits, int *stopBits, int *parity, int *isPresent) {

	DebugPrintf(L"Opening Registry\n");
	DebugPrintf(L"Key Name: %s\n", CONST_CANTACT_PNP_KEY);

	HKEY registryKey;
	LONG result;
	WCHAR parentKey[1024];


	WCHAR *subKeyName = malloc(1024);
	DWORD subKeyLength = 1024 * (sizeof(subKeyName) / sizeof(*subKeyName));

	WCHAR *keyValue = malloc(1024);
	DWORD keyLength = 1024 * (sizeof(keyValue) / sizeof(*keyValue));

	DWORD tmpKeyLength = keyLength;

	DWORD keyType;

	// Get a handle to the registry key for the CANtact device
	result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, CONST_CANTACT_PNP_KEY, 0, KEY_READ, &registryKey);

	// if the key isn't found, assume the CANtact device has never been installed
	if (result != ERROR_SUCCESS) {
		free(keyValue);
		free(subKeyName);
		return FALSE;
	}

	// key is present assume CANtact device has at least been installed
	DebugPrintf(L"RegOpenKey: %d (%d)\n", result, GetLastError());

	// iterate the sub keys until we find a sub key that contains the matching classGUID
	for (int i = 0;; i++) {

		result = RegEnumKeyEx(registryKey, i, subKeyName, &subKeyLength, NULL, NULL, NULL, NULL);
		if (result != ERROR_SUCCESS)  {
			break;
		}

		// BUG BUG Debug
		DebugPrintf(L"Sub Key: %s (%d)\n", subKeyName, subKeyLength);

		// Note Service matches the SubKey 'usbser' for detecting if device is present
		// and Class matches the subkey 'Ports' for determining the serial port settings 

		// check if we have the correct registry key by comparing with the GUID from the CANtact .inf file
		result = RegGetValue(registryKey, subKeyName, L"ClassGUID", RRF_RT_ANY, &keyType, keyValue, &keyLength);

		// BUG BUG Debug
		DebugPrintf(L"Class GUID Result: %d  (%d)\n", result, keyLength);

		if (result == ERROR_SUCCESS) {
			// BUG BUG Debug
			DebugPrintf(L"Class GUID Key Value: %s  [%d] \n", keyValue, keyType);

			// handle both unicode or ANSI variants of the GUID
			WCHAR unicodeValue[1024];
			MultiByteToWideChar(CP_OEMCP, 0, (LPCCH)keyValue, -1, (LPWSTR)unicodeValue, (int)keyLength);
			if ((strcmp((const char *)keyValue, (const char *)CONST_CANTACT_GUID_ANSI)) || (wcscmp(unicodeValue, CONST_CANTACT_GUID_UNICODE))) {
				// BUG BUG Debug
				DebugPrintf(L"Values Match\n");

				// Save this subkey value as it is used later on
				wcsncpy(parentKey, keyValue, keyLength);

				// Get the friendly name for the CANtact device
				keyLength = tmpKeyLength;
				result = RegGetValue(registryKey, subKeyName, L"FriendlyName", RRF_RT_ANY, &keyType, keyValue, &keyLength);
				if (result == ERROR_SUCCESS) {
					// BUG BUG Debug
					DebugPrintf(L"Friendly Name Result: %d  (%d)\n", result, keyLength);
					DebugPrintf(L"Friendly Name Key Value: %s  [%d] \n", keyValue, keyType);

					wcsncpy(friendlyName, keyValue, keyLength);
				} // end friendly name

				// Find the port name under the sub key Device Parameters
				wcscat(subKeyName, L"\\Device Parameters");
				// What serial port does the CANtact device use
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
				result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, CONST_CANTACT_CONFIG_KEY, 0, KEY_READ, &registryKey);

				// BUG BUG Debug
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
							// BUG BUG Debug
							DebugPrintf(L"Entry: %d, Value: %s, Length: %d\n", i, keyValue, keyLength);
							// Now compare the values
							WCHAR *buffer;
							DebugPrintf(L"%s\n", wcstok_s(keyValue, L"\\",&buffer)); //should match USB
							DebugPrintf(L"%s\n", wcstok_s(NULL, L"\\",&buffer)); //should match PnP ID
							DebugPrintf(L"%s\n", wcstok_s(NULL, L"\\",&buffer)); //should match subKey

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
