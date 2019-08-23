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

//
// Project: TwoCan
// Project Description: NMEA2000 Plugin for OpenCPN
// Unit: Driver for Rusoku Toucan Marine adapter
// Unit Description: Access Rusoku Toucan adapter via CAN Abstraction Layer (CANAL) library
// Date: 20/8/2019
// Function: On receipt of valid CAN frame, converts CANAL Msg format into 
// TwoCan format and signals an event to the application.
// Version History
// 1.1 20/8/2019 Initial Release
// Note, initial version 1.1 indicates that this driver supports the Write functionality

#include "..\inc\toucan.h"

#include "..\..\common\inc\twocanerror.h"

// Separate thread to read data from the CAN device
HANDLE threadHandle;

// The thread id.
DWORD threadId;

// Event signalled when valid CAN Frame is received
HANDLE frameReceivedEvent;

// Signal that the thread has terminated
HANDLE threadFinishedEvent;

// Mutex used to synchronize access to the CAN Frame buffer
HANDLE frameReceivedMutex;

// Pointer to the caller's CAN Frame buffer
byte *canFramePtr;

// Variable to indicate thread state
BOOL isRunning = FALSE;

// CANAL variables
long status;
long handle;

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
// returns the name of this driver
// BUG BUG could retrieve this value from the registry or from the CANAL library
//

DllExport char *DriverName(void)	{
	return (char *)L"Rusoku Toucan";
}

//
// Version
// return an arbitary version number for this driver
//

DllExport char *DriverVersion(void)	{
	return (char *)L"1.1";
}

//
// Manufacturer
// return the name of this driver's hardware manufacturer
//

DllExport char *ManufacturerName(void)	{
	return (char *)L"Rusoku";
}


//
// Open. Connect to the adapter and get ready to start reading
// returns TWOCAN_RESULT_SUCCESS if events, mutexes and Toucan adapter configured correctly
//

DllExport int OpenAdapter(void)	{
	// Create an event that is used to notify the caller of a received frame
	frameReceivedEvent = CreateEvent(NULL, FALSE, FALSE, CONST_DATARX_EVENT);

	if (frameReceivedEvent == NULL)
	{
		// Fatal Error
		DebugPrintf(L"Create FrameReceivedEvent failed (%d)\n", GetLastError());
		return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER, TWOCAN_ERROR_CREATE_FRAME_RECEIVED_EVENT);
	}

	// Create an event that is used to notify the close method that the thread has ended
	threadFinishedEvent = CreateEvent(NULL, FALSE, FALSE, CONST_EVENT_THREAD_ENDED);

	if (threadFinishedEvent == NULL)
	{
		// Fatal Error
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

	// CANAL initialization
	char deviceSerialNumber[9];

	if (!FindAdapter(deviceSerialNumber, sizeof(deviceSerialNumber))) {
		DebugPrintf(L"CANAL Adapter not found\n");
		return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER, TWOCAN_ERROR_ADAPTER_NOT_FOUND);
	}

	DebugPrintf(L"Found Toucan Device Serial Number: %s\n", deviceSerialNumber);
	// Rusoku Toucan initialization string is of the form:
	// device no;serial no;baud where device number is 0, serial number is 8 digits, 
	// and baud must be 250 for NMEA 2000 networks.
	char initString[15];
	int length = sprintf_s(initString, sizeof(initString), "0;%s;250", deviceSerialNumber);
	DebugPrintf(L"CANAL Initialization String: %s\n", initString);

	handle = CanalOpen(initString, 0);

	if (handle <= 0) {
		// Fatal error
		DebugPrintf(L"CANAL Open failed (%l)\n", handle);
		return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER, TWOCAN_ERROR_SET_BUS_SPEED);
	}

	// Get the vendor id name
	char *vendorId;
	vendorId = (char *)malloc(1024);

	status = CanalGetVendor(handle, 1024, vendorId);
	if (status != CANAL_ERROR_SUCCESS) {
		free(vendorId);
		DebugPrintf(L"CANAL Get Vendor Id failed (%d)\n",status);
		return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER, TWOCAN_ERROR_PRODUCT_INFO_FAILURE);
	}
	else {
		// BUG BUG should do something useful with the driver name !!
		free(vendorId);
	}

	status = CanalInterfaceStop(handle);
	if (status != CANAL_ERROR_SUCCESS) {
		// Non fatal error
		DebugPrintf(L"CANAL Interface Off failed: (%d)\n", status);
	}

	status = CanalInterfaceStart(handle);

	if (status != CANAL_ERROR_SUCCESS) {
		// Fatal Error
		DebugPrintf(L"CANAL Interface On failed (%d)\n", status);
		return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER, TWOCAN_ERROR_SET_BUS_ON);
	}

	return TWOCAN_RESULT_SUCCESS;
}

//
// Close, Stop reading & disconnect
// returns TWOCAN_RESULT_SUCCESS if reading thread terminated successfully
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

	// Close the Canal adapter
	status = CanalInterfaceStop(handle);
	if (status != CANAL_ERROR_SUCCESS) {
		DebugPrintf(L"CANAL Interface Off failed: (%d)", status);
	}
	status = CanalClose(handle);
	if (status != CANAL_ERROR_SUCCESS) {
		DebugPrintf(L"CANAL Close Adapter failed: (%d)", status);
	}
	return TWOCAN_RESULT_SUCCESS;
}


//
// Read, starts the read thread
// [in] frame, pointer to byte array for the CAN Frame buffer
// Returns TWOCAN_RESULT_SUCCESS if thread created successfully
//

DllExport int ReadAdapter(byte *frame)	{

	// Save the pointer to the Can Frame buffer
	canFramePtr = frame;

	// Indicate thread is in running state
	isRunning = TRUE;

	// Start the read thread
	threadHandle = CreateThread(NULL, 0, ReadThread, NULL, 0, &threadId);

	if (threadHandle != NULL) {
		return TWOCAN_RESULT_SUCCESS;
	}
	// Fatal Error
	isRunning = FALSE;
	DebugPrintf(L"Read thread failed: %d (%d)\n", threadId, GetLastError());
	return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER, TWOCAN_ERROR_CREATE_THREAD_HANDLE);
}

//
// Write, Transmit a frame onto the NMEA 2000 network
// [in] 29bit Can header (id), payload and payload length
// returns TWOCAN_RESULT_SUCCESS 
//

DllExport int WriteAdapter(const unsigned int id, const int dataLength, byte *data) {
	canalMsg msg;
	msg.id = id;
	msg.sizeData = dataLength;
	memcpy(msg.data, data, dataLength);
	msg.flags = CANAL_IDFLAG_EXTENDED | CANAL_IDFLAG_SEND;
	status = CanalSend(handle, &msg);
	if (status == CANAL_ERROR_SUCCESS) {
		return TWOCAN_RESULT_SUCCESS;
	}
	else {
		DebugPrintf(L"Transmit frame failed: (%d)\n", status);
		return SET_ERROR(TWOCAN_RESULT_ERROR, TWOCAN_SOURCE_DRIVER, TWOCAN_ERROR_TRANSMIT_FAILURE);
	}
}

//
// Read thread, reads CAN Frames from Rusoku Toucan device, if a valid frame is received,
// parse the frame into the correct format and notify the caller
// Upon exit, returns TWOCAN_RESULT_SUCCESS as Thread Exit Code
//

DWORD WINAPI ReadThread(LPVOID lParam)
{
	DWORD mutexResult;
	canalMsg msg;

	while (isRunning) {

		// BUG BUG Use Blocking or non-Blocking calls ??
		status = CanalBlockingReceive(handle, &msg, 20);

		if(status == CANAL_ERROR_SUCCESS) {

			// Only interested in CAN 2.0 extended frames with 29bit Id's
			if (msg.flags & CANAL_IDFLAG_EXTENDED) {
			
				// Make sure we can get a lock on the buffer
				mutexResult = WaitForSingleObject(frameReceivedMutex, 200);

				if (mutexResult == WAIT_OBJECT_0) {

					// Convert id (long) to TwoCan header format (byte array)
					canFramePtr[3] = (msg.id >> 24) & 0xFF;
					canFramePtr[2] = (msg.id >> 16) & 0xFF;
					canFramePtr[1] = (msg.id >> 8) & 0xFF;
					canFramePtr[0] = msg.id & 0xFF;

					// Copy the CAN data
					memcpy(&canFramePtr[4], msg.data, msg.sizeData);

					// Release the lock
					ReleaseMutex(frameReceivedMutex);

					// Notify the caller
					if (SetEvent(frameReceivedEvent)) {
						Sleep(10);
					}
					else {
						// Non fatal error
						DebugPrintf(L"Set Event Error: %d\n", GetLastError());
					}
				}

				else {
					// Non fatal error
					DebugPrintf(L"Adapter Mutex: %d -->%d\n", mutexResult, GetLastError());
				}
			}  // end Can Extended Frame handling

			if (msg.flags & CANAL_IDFLAG_STANDARD) {
				// Not interested in these standard frames
			} 
			
			if (msg.flags & CANAL_IDFLAG_RTR) {
				// Nor these remote frames
			} 
			
			if (msg.flags & CANAL_IDFLAG_STATUS) {
				// Nor these error frames
			}
			
		} // end if CANAL_ERROR_SUCCESS

	} // end while
	SetEvent(threadFinishedEvent);
	ExitThread(TWOCAN_RESULT_SUCCESS);
}

#define TOUCAN_KEY_UNICODE  L"{88BAE032-5A81-49F0-BC3D-A4FF138216D6}"
#define TOUCAN_KEY_ANSI "{88BAE032-5A81-49F0-BC3D-A4FF138216D6}"
#define TOUCAN_PNP_KEY L"SYSTEM\\CurrentControlSet\\enum\\USB\\VID_16D0&PID_0EAC"

BOOL FindAdapter(char *serialNumber, int serialNumberLength) {
	// From .inf file the guid {88BAE032-5A81-49F0-BC3D-A4FF138216D6} matches the key ClassGUID
	// found in HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Enum\USB\VID_16D0&PID_0EAC
	// under a sub key which appears to be the serial number of the device.
	// Could also check DeviceClassGUID, 

	DebugPrintf(L"Opening Registry\n");
	DebugPrintf(L"Key Name: %s\n", TOUCAN_KEY_UNICODE);
	
	BOOL foundKey = FALSE;
	HKEY registryKey;
	LONG result;
	
	WCHAR *subKeyName = (WCHAR *)malloc(1024);
	DWORD subKeyLength = 1024;

	WCHAR *keyValue = (WCHAR *)malloc(1024);
	DWORD keyLength = 1024;

	DWORD keyType;

	// Get a handle to the registry key for the Rusoku Toucan device
	result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, TOUCAN_PNP_KEY, 0, KEY_READ, &registryKey);

	// if the key isn't found, assume the Rusoku Toucan device has never been installed
	if (result != ERROR_SUCCESS) {
		free(keyValue);
		free(subKeyName);
		return foundKey;
	}

	// key is present so assume Rusoku Toucan device has at least been installed
	DebugPrintf(L"RegOpenKey: %d (%d)\n", result, GetLastError());

	// iterate the sub keys until we find the sub key that contains the matching classGUID value
	for (int i = 0;; i++) {

		result = RegEnumKeyEx(registryKey, i, subKeyName, &subKeyLength, NULL, NULL, NULL, NULL);
		if (result != ERROR_SUCCESS) {
			break;
		}

		// BUG BUG Debug
		DebugPrintf(L"Sub Key: %s (%d)\n", subKeyName, subKeyLength);

		// check if we have the correct registry key by comparing with the GUID from the .inf file
		result = RegGetValue(registryKey, subKeyName, L"ClassGUID", RRF_RT_ANY, &keyType, keyValue, &keyLength);

		// BUG BUG Debug
		DebugPrintf(L"Class GUID Result: %d  (%d)\n", result, keyLength);

		if (result == ERROR_SUCCESS) {
			// BUG BUG Debug
			DebugPrintf(L"Class GUID Key Value: %s  [%d] \n", keyValue, keyType);

			// handle both unicode or ANSI variants of the GUID
			WCHAR unicodeValue[1024];
			MultiByteToWideChar(CP_OEMCP, 0, (LPCCH)keyValue, -1, (LPWSTR)unicodeValue, (int)keyLength);
			if ((strcmp((const char *)keyValue, (const char *)TOUCAN_KEY_ANSI)) || (wcscmp(unicodeValue, TOUCAN_KEY_UNICODE))) {
				// BUG BUG Debug
				DebugPrintf(L"Values Match\n");

				// Save this subkey value as it is used later on
				// wcsncpy(parentKey, keyValue, keyLength);
				// DebugPrintf(L"Parent Key: %s\n", keyValue);

				// The device serial number is the subkey which contained the matching ClassGUID
				DebugPrintf(L"Device Serial Number: %s\n", subKeyName);
				
				// Convert to ASCII string for use in CANAL initialization string
				WideCharToMultiByte(CP_OEMCP, 0, subKeyName, -1, serialNumber, serialNumberLength, NULL, NULL);

				foundKey = TRUE;

			} // end found matching GUID

		} // end iterating subKeys for ClassGUID values

		free(keyValue);
		free(subKeyName);

	}// end iterating sub keys

	return foundKey;
}
