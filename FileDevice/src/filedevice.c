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
// Unit: Driver for Log File Reader
// Unit Description: Replays exisiting log file, created by TwoCan Device
// Date 6/8/2018
// Function: Read a line from the log file, convert from raw format into 
// TwoCan byte array and signals an event to the application
//

#include "..\inc\filedevice.h"

#include "..\..\common\inc\twocanerror.h"

// Separate thread to read data from the logfile
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
//

DllExport char *DriverName(void)	{
	return (char *)L"TwoCan Logfile Reader";
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
// return the name of this driver#s hardware manufacturer
//

DllExport char *ManufacturerName(void)	{
	return (char *)L"TwoCan";
}


//
// Open, configure events and mutexes 
// returns TWOCAN_RESULT_SUCCESS if no errors
//

DllExport int OpenAdapter(void)	{
	DebugPrintf(L"Open called\n");

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
	return TWOCAN_RESULT_SUCCESS;
}

//
// Close, Stop reading & disconnect
// returns TRUE if reading thread terminated
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
	
	return TWOCAN_RESULT_SUCCESS;
}


//
// Read, starts the read thread
// [in] frame, pointer to byte array for the CAN Frame buffer
//

DllExport int ReadAdapter(byte *frame)	{
	
	// Save the pointer to the Can Frame buffer
	canFramePtr = frame;

	// Running
	isRunning = TRUE;

	// Start the read thread
	threadHandle = CreateThread(NULL, 0, ReadThread, NULL, 0, &threadId);
	
	if (threadHandle != NULL) {
		DebugPrintf(L"Read thread started: %d\n", threadId);
		return TWOCAN_RESULT_SUCCESS;
	}
	
	// Fatal error
	isRunning = FALSE;
	DebugPrintf(L"Read thread failed: %d (%d)\n", threadId,GetLastError());
	return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER, TWOCAN_ERROR_CREATE_THREAD_HANDLE);
}

//
// Read thread, reads previously saved raw NMEA 2000 data from the log file.
// If a valid frame is received parse the frame into the correct format and notify the caller
//

DWORD WINAPI ReadThread(LPVOID lParam)
{
	DWORD mutexResult;
	FILE *fileHandle;
	WCHAR fileName[MAX_PATH];
	HRESULT result;
	byte canFrame[12];
	byte *framePtr = canFrame;
	char *token;
	char delimiter[2] = ",";
	char buffer[1024];

	result = SHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, fileName);

	if (result == S_OK) {
		PathAppend(fileName, CONST_LOG_FILE);
		
		DebugPrintf(L"Log File: %s\n\r", fileName);
		
		if (PathFileExists((LPCWSTR)fileName)) {
			
			fileHandle = _wfopen((wchar_t *)fileName, (wchar_t *)"r");
			
			if (fileHandle == NULL) {
				DebugPrintf(L"File Error\n");
				ExitThread(SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER, TWOCAN_ERROR_FILE_NOT_FOUND));
			}

			// read a line from the log file
			while (isRunning)  {

				if (!fgets(buffer, sizeof(buffer), fileHandle)) {
					// if at the end of the file, restart from the beginning
					rewind(fileHandle);
					fgets(buffer, sizeof(buffer), fileHandle);
				}

				// read each hex character into a byte value
				token = strtok(buffer, delimiter);
				
				while (token != NULL) {
					*framePtr = (byte)strtol(token, NULL, 16);
					// BUG BUG Debug, print out each NMEA 2000 value
					DebugPrintf(L"%d ", *framePtr);
					token = strtok(NULL, delimiter);
					framePtr++;
				} 

				// Terminate each NMEA 2000 frame with newline character
				DebugPrintf(L"\n");

				// make sure we can get a lock on the buffer
				mutexResult = WaitForSingleObject(frameReceivedMutex, 200);

				if (mutexResult == WAIT_OBJECT_0) {
					// copy the frame to the buffer
					memcpy(canFramePtr,canFrame, 12);
					
					// reset the pointer
					framePtr = canFrame;

					// release the lock
					ReleaseMutex(frameReceivedMutex);

					// Notify the caller
					if (SetEvent(frameReceivedEvent)) {
						Sleep(10);
					}
					else {
						
						DebugPrintf(L"Set Event Error: %d\n", GetLastError());
					}
				}

				else {
					DebugPrintf(L"Adapter Mutex: %d -->%d\n", mutexResult, GetLastError());
				}


			} // end while isRunning 
				
			DebugPrintf(L"Closing File\n");
			fclose(fileHandle);

			SetEvent(threadFinishedEvent);
			ExitThread(TWOCAN_RESULT_SUCCESS);

		} // end if file "twocanraw.log" exists
		else {
			DebugPrintf(L"LogFile Error: %d (%d)\n", result, GetLastError());
			isRunning = FALSE;
			ExitThread(SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER, TWOCAN_ERROR_FILE_NOT_FOUND));
		}

	} // end if My Documents folder exists
	else {
		DebugPrintf(L"My Documents Path Error: %d (%d)\n", result, GetLastError());
		isRunning = FALSE;
		ExitThread(SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER, TWOCAN_ERROR_PATH_NOT_FOUND));
	}

}

