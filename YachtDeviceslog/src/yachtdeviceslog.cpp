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
// Unit: Driver for Yacht Devices Log File Reader
// Unit Description: Replays exisiting log file, created by Yacht Devices Voyage Data Recorder
// Date 29/12/2018
// Function: Read a line from the log file, convert from raw format into 
// TwoCan byte array and signals an event to the application
//

#include "..\inc\yachtdeviceslog.h"

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

int badLineCount = 0;

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
	return (char *)L"Yacht Devices Logfile Reader";
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

	// Check if the log file exists
	WCHAR fileName[MAX_PATH];
	HRESULT result;

	result = SHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, fileName);

	if (result == S_OK) {
		PathAppend(fileName, CONST_LOG_FILE);

		if (!PathFileExists((LPCWSTR)fileName)) {
			DebugPrintf(L"Log File Not found (%d)\n", GetLastError());
			return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER, TWOCAN_ERROR_FILE_NOT_FOUND);
		}
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
	DebugPrintf(L"Read thread failed: %d (%d)\n", threadId, GetLastError());
	return SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER, TWOCAN_ERROR_CREATE_THREAD_HANDLE);
}

//
// Read thread, reads previously saved NMEA 2000 data from the output of Candump (linux utility).
// If a valid frame is received parse the frame into the correct format and notify the caller
//

DWORD WINAPI ReadThread(LPVOID lParam)
{
	DWORD mutexResult;
	WCHAR fileName[MAX_PATH];
	HRESULT result;
	byte canFrame[12];

	result = SHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, fileName);

	if (result == S_OK) {
		PathAppend(fileName, CONST_LOG_FILE);

		DebugPrintf(L"Log File: %s\n\r", fileName);

		if (PathFileExists((LPCWSTR)fileName)) {

			std::ifstream inputFile(fileName);

			// read a line from the log file
			std::string inputLine;
			// specific regular expression for yacht devices log format
			std::regex yachtDevicesRegex("^[0-9]{2}:[0-9]{2}:[0-9]{2}.[0-9]{3}\\sR\\s([0-9A-F]{8})[\\s]([0-9A-F]{2})[\\s]([0-9A-F]{2})[\\s]([0-9A-F]{2})[\\s]([0-9A-F]{2})[\\s]([0-9A-F]{2})[\\s]([0-9A-F]{2})[\\s]([0-9A-F]{2})[\\s]([0-9A-F]{2})$");
			std::smatch matchGroups;

			while (isRunning)  {
				std::getline(inputFile, inputLine);
				if (inputFile.eof()) {
					// if at the end of the file, restart from the beginning
					inputFile.clear();
					inputFile.seekg(0);
					std::getline(inputFile, inputLine);
				}

				// BUG BUG Not sure if this trickles up to report the error
				if (!std::regex_match(inputLine, yachtDevicesRegex)) {
					DebugPrintf(L"Invalid Log file Format: %s\n", inputLine);
					badLineCount++;
					if (badLineCount == CONST_MAX_BAD_LINES) {
						isRunning = FALSE;
						inputFile.close();
						SetEvent(threadFinishedEvent);
						ExitThread(SET_ERROR(TWOCAN_RESULT_FATAL, TWOCAN_SOURCE_DRIVER, TWOCAN_ERROR_INVALID_LOGFILE_FORMAT));
					}
				}

				if (std::regex_match(inputLine, matchGroups, yachtDevicesRegex)) {
					// Copy 4 byte header
					unsigned long temp = std::strtoul(matchGroups[1].str().c_str(), NULL, 16);
					memcpy(&canFrame[0], &temp, 4);

					// Copy 8 byte payload
					// BUG BUG Should really check that there are 8 data bytes
					canFrame[4] = static_cast<byte>(std::strtoul(matchGroups[2].str().c_str(), NULL, 16));
					canFrame[5] = static_cast<byte>(std::strtoul(matchGroups[3].str().c_str(), NULL, 16));
					canFrame[6] = static_cast<byte>(std::strtoul(matchGroups[4].str().c_str(), NULL, 16));
					canFrame[7] = static_cast<byte>(std::strtoul(matchGroups[5].str().c_str(), NULL, 16));
					canFrame[8] = static_cast<byte>(std::strtoul(matchGroups[6].str().c_str(), NULL, 16));
					canFrame[9] = static_cast<byte>(std::strtoul(matchGroups[7].str().c_str(), NULL, 16));
					canFrame[10] = static_cast<byte>(std::strtoul(matchGroups[8].str().c_str(), NULL, 16));
					canFrame[11] = static_cast<byte>(std::strtoul(matchGroups[9].str().c_str(), NULL, 16));

					// make sure we can get a lock on the buffer
					mutexResult = WaitForSingleObject(frameReceivedMutex, 200);

					if (mutexResult == WAIT_OBJECT_0) {
						// copy the frame to the buffer
						memcpy(canFramePtr, &canFrame[0], 12);

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

				} // end if regex

			} // end while isRunning 

			DebugPrintf(L"Closing File\n");
			inputFile.close();

			SetEvent(threadFinishedEvent);
			ExitThread(TWOCAN_RESULT_SUCCESS);

		} // end if file "yachtdevices.log" exists
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

