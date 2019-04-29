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
// Unit: Driver for Kvaser Lightleaf adapter
// Unit Description: Access Kvaser Lightleaf adapter via Kvaser library
// Date: 6/8/2018
// Function: On receipt of valid CAN frame, converts Kvaser format into 
// TwoCan format and signals an event to the application.
// Version History
// 1.0 Initial Release
// 1.1 - 2/4/2019 Added Write function

#include "..\inc\kvaser.h"

#include "..\..\common\inc\twocanerror.h"

// Separate thread to read data from the Kvaser Lightleaf device
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

// Kvaser variables
canStatus status;
canHandle handle;

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
// BUG BUG could retrieve this value from the registry or from KVaser library
//

DllExport char *DriverName(void)	{
	return (char *)L"Kvaser Leaflight";
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
	return (char *)L"Kvaser";
}


//
// Open. Connect to the adapter and get ready to start reading
// returns TWOCAN_RESULT_SUCCESS if events, mutexes and Kvaser adapter configured correctly
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

	// Kvaser Channel initialization
	canInitializeLibrary();

	// Get the driver name
	char *driverName;
	driverName = (char *)malloc(1024);

	status = canGetChannelData(0, canCHANNELDATA_DRIVER_NAME, driverName, sizeof(driverName));
	if (status != canOK) {
		free(driverName);
		DebugPrintf(L"Kvaser Get Channel Data failed (%d)\n",status);
		return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER, TWOCAN_ERROR_GET_SETTINGS);
	}
	else {
		// BUG BUG should do something useful with the driver name !!
		free(driverName);
	}


	// Open Channel 0
	handle = canOpenChannel(0, 0);

	// Set bitrate to 250k for NMEA2000
	status = canSetBusParams(handle, canBITRATE_250K, 0, 0, 0, 0, 0);
	
	if (status != canOK) {
		// Fatal Error
		DebugPrintf(L"Kvaser Set Bus speed failed (%d)\n", status);
		return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER, TWOCAN_ERROR_SET_BUS_SPEED);
	}

	if (canResetBus(handle) != canOK) {
		// Non fatal error
		DebugPrintf(L"Kvaser Reset Bus failed\n");
	}

	status = canBusOn(handle);

	if (status != canOK) {
		// Fatal Error
		DebugPrintf(L"Kvaser Set Bus On failed (%d)\n", status);
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

	// Close the Kvaser adapter
	status = canBusOff(handle);
	if (status != canOK) {
		DebugPrintf(L"Kvaser Set Bus Off Error: %d", status);
	}
	status = canClose(handle);
	if (status != canOK) {
		DebugPrintf(L"Kvaser Close Adapter Error: %d", status);
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
	status = canWrite(handle, id, data, dataLength, canMSG_EXT);
	if (status == canOK) {
		return TWOCAN_RESULT_SUCCESS;
	}
	else {
		DebugPrintf(L"Transmit frame failed: %d\n", status);
		return SET_ERROR(TWOCAN_RESULT_ERROR, TWOCAN_SOURCE_DRIVER, TWOCAN_ERROR_TRANSMIT_FAILURE);
	}
}

//
// Read thread, reads CAN Frames from Kvaser device, if a valid frame is received,
// parse the frame into the correct format and notify the caller
// Upon exit, returns TWOCAN_RESULT_SUCCESS as Thread Exit Code
//

DWORD WINAPI ReadThread(LPVOID lParam)
{
	DWORD mutexResult;
	byte data[8];
	long id;
	unsigned int dlc;
	unsigned int flags;
	unsigned long time;

	while (isRunning) {

		status = canReadWait(handle, &id, data, &dlc, &flags, &time, 100);
		if (status == canOK) {
			// Only interested in CAN 2.0 extended frames
			if (flags & canMSG_EXT) {
				// Make sure we can get a lock on the buffer
				mutexResult = WaitForSingleObject(frameReceivedMutex, 200);

				if (mutexResult == WAIT_OBJECT_0) {

					// Convert id (long) to TwoCan header format (byte array)
					canFramePtr[3] = (id >> 24) & 0xFF;
					canFramePtr[2] = (id >> 16) & 0xFF;
					canFramePtr[1] = (id >> 8) & 0xFF;
					canFramePtr[0] = id & 0xFF;

					// Copy the CAN data
					memcpy(&canFramePtr[4], data, dlc);

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

			if (flags & canMSG_STD) {
				// Not interested in these
			} 
			
			if (flags & canMSG_RTR) {
				// Nor these
			} 
			
			if (flags & canMSG_ERROR_FRAME) {
				// Nor these 
			}
			
		} // end if canStatus.OK

	} // end while
	SetEvent(threadFinishedEvent);
	ExitThread(TWOCAN_RESULT_SUCCESS);
}
