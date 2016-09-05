#include <string>
#include <Unknwn.h>
#include <windows.h>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <stdint.h>
#include "ftd2xx.h"//included to directly drive the FTDI com port device
#include "OBDpolling.h"
using namespace std;


void writeToSerialPort(HANDLE serialPortHandle, uint8_t* bytesToSend, int numberOfBytesToSend)
{
	DWORD numberOfBytesSent = 0;     // No of bytes written to the port
	std::cout << "send: ";
	for(int i = 0; i < numberOfBytesToSend; i++)
	{
		std::cout << (int) bytesToSend[i] << ' ';
	}
	std::cout << '\n';
	WriteFile(serialPortHandle, bytesToSend, numberOfBytesToSend, &numberOfBytesSent, NULL);
	//std::cout << numberOfBytesSent << '\n';
}


int readFromSerialPort(HANDLE serialPortHandle, uint8_t* serialBytesRecvd, int sizeOfSerialByteBuffer, int delayMS)
{
	/*
	//event flag method
	SetCommMask(serialPortHandle, EV_RXCHAR);//set up event when serial bytes have been received
	DWORD dwEventMask;
	WaitCommEvent(serialPortHandle, &dwEventMask, NULL);//wait until the "data has been received" event occurs
	uint8_t recvdByte;
	//std::cout << (int) sizeOfSerialByteBuffer << '\n';
	DWORD numberOfBytesReceived = 1;
	int recvdBufferPosition = 0;
	while(numberOfBytesReceived > 0 && recvdBufferPosition < sizeOfSerialByteBuffer)
	{
		ReadFile(serialPortHandle, &recvdByte, sizeof(recvdByte), &numberOfBytesReceived, NULL);//read in one byte at a time
		serialBytesRecvd[recvdBufferPosition] = recvdByte;
		recvdBufferPosition += 1;
	}
	//std::cout << recvdBufferPosition << '\n';
	 *
	 */


	//continuous polling method: manual calculation of timeout
	//variables for tracking received bytes:
	uint8_t recvdByte;
	DWORD numberOfBytesReceived = 1;
	int recvdBufferPosition = 0;

	//helped from: http://stackoverflow.com/questions/2150291/how-do-i-measure-a-time-interval-in-c
	LARGE_INTEGER frequency;        // ticks per second
	LARGE_INTEGER t1, t2, lastByteTime;           // ticks
	double elapsedTime = 0;
	double elapsedTimeSinceLastByte = 0;

	// get ticks per second
	QueryPerformanceFrequency(&frequency);

	// start timer
	QueryPerformanceCounter(&t1);
	bool notFinished = true;
	bool haveReceivedBytes = false;

	std::cout << "read: ";

	while(notFinished)
	{
	    // stop timer
	    QueryPerformanceCounter(&t2);//get the loop position time
	    if(!haveReceivedBytes)
	    {
	    	elapsedTime = (t2.QuadPart - t1.QuadPart)*1000 / frequency.QuadPart;// compute the elapsed time in milliseconds
	    }
	    else if(haveReceivedBytes)
	    {
	    	elapsedTimeSinceLastByte = (t2.QuadPart - lastByteTime.QuadPart)*1000 / frequency.QuadPart;// compute the elapsed time in milliseconds
	    }
	    ReadFile(serialPortHandle, &recvdByte, sizeof(recvdByte), &numberOfBytesReceived, NULL);//read in one byte at a time
	    if(numberOfBytesReceived > 0 && recvdBufferPosition < sizeOfSerialByteBuffer)
	    {
			serialBytesRecvd[recvdBufferPosition] = recvdByte;
			recvdBufferPosition += 1;
			lastByteTime = t2;//the time that the previous byte was read in
			haveReceivedBytes = true;//raise a flag that we've begun receiving a response

			//update the starttimer in order to extend the timeout
			//for reading-in bytes relative to the last received byte
	       	//QueryPerformanceCounter(&t1);

			std::cout << (int) recvdByte << " ";//print the received bytes
	    }

	    //if we've overrun our receive buffer, over run the timeout, or have overrun 100 ms since
	    //we last received a serial byte, exit the loop
	    if(elapsedTime >= delayMS || recvdBufferPosition >= sizeOfSerialByteBuffer || elapsedTimeSinceLastByte > 100)
	    {
	    	notFinished = false;//breaks the loop if we time out or if we have received all the bytes we wanted
	    	if(recvdBufferPosition < sizeOfSerialByteBuffer)
	    	{
	    		std::cout << "timeout";// << '\n';
	    	}
	    }
	}
	std::cout << '\n';
	return recvdBufferPosition;//this is also the number of received bytes total
}


bool serialPollingCycle(HANDLE serialPortHandle,std::ofstream &dataFile, std::ofstream &byteFile, int mode, int pid, uint8_t* outGoingData, double &returnData)
{
	bool success = true;
	//for each logging parameter, send a message, recieve a message, interpret the message and save the data

	//initialize a read-in data buffer
	uint8_t recvdBytes[100];//a buffer which can be used to store read-in bytes
	for(int i = 0; i < 100; i++)
	{
		recvdBytes[i] = 0;
	}

/*
	//get the current timer value in order to diagnose message timing problems
	QueryPerformanceCounter(&currentTime);//get the loop position time
	double LoggerTimeMS = (currentTime.QuadPart - startTime.QuadPart)*1000 / systemFrequency.QuadPart;// compute the elapsed time in milliseconds
	elapsedLoggerTime = LoggerTimeMS/1000;//this is an attempt to get post-decimal points
	std::cout << "time: " << elapsedLoggerTime << '\n';
*/

	//scan through the different OBD PIDs, checking if they are to be polled
	//if they are to be polled, send a message, read a message and store the data
	returnData = 0.0;

	switch(mode)
	{
	case 1:
		//28 PIDs to check for mode 1
		switch(pid)
		{
		case 0:
			//PID 0
			//if(mode1checkboxstate[0] != BST_UNCHECKED && success == true && placeCounter == 0)
		{
			//mode 1 PID 0: list of valid PIDs 0-20
			byteFile << ',';
			dataFile << ',';
			//mode = 1;
			//pid = 0;


			uint8_t sendBytes[6] = {104,106,241,1,0,196};
			writeToSerialPort(serialPortHandle, sendBytes, 6);
			readFromSerialPort(serialPortHandle, recvdBytes,6,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,10,1000);
			if(numBytesRead < 10)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";
			}
			else
			{
				//this should return 4 bytes which are binary flags
				//for the next 32 PIDs indicating if they work with this car
				//the return packet looks like 72,107,16,65,0,x,x,x,x,sum
				int PIDplace = 0;
				for(int i = 5; i < 9; i++)//loop through bytes
				{
					for(int j = 7; j > -1; j--)
					{
						PIDplace += 1;
						if(((recvdBytes[i] >> j) & 0x1))//shift and mask to check bit flags
						{
							dataFile << PIDplace << ' ';
						}
					}
				}
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 1:
			//mode 1 PID 1
			//if(mode1checkboxstate[1] != BST_UNCHECKED && success == true && placeCounter == 1)
		{
			//mode 1 PID 1: Monitor status since DTCs cleared. (Includes malfunction indicator lamp (MIL) status and number of DTCs.)
			byteFile << ',';
			dataFile << ',';
			//mode = 1;
			//pid = 1;

			uint8_t sendBytes[6] = {104,106,241,1,1,197};
			writeToSerialPort(serialPortHandle, sendBytes, 6);
			readFromSerialPort(serialPortHandle, recvdBytes,6,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,10,1000);
			if(numBytesRead < 10)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";
			}
			else
			{
				//this should return 4 bytes which are binary/byte flags
				//for vehicle monitors of this car
				//the return packet looks like 72,107,16,65,1,x,x,x,x,sum

				if((recvdBytes[5] >> 7) & 0x1)
				{
					dataFile << "CEL on: ";
				}
				else
				{
					dataFile << "CEL off: ";
				}
				int numCELs = 0;
				int multiplier = 1;
				for(int i = 0; i < 7; i++)
				{
					numCELs += ((recvdBytes[5] >> i) & 0x1)*multiplier;
					multiplier = multiplier*2;
				}
				dataFile << numCELs << " codes indicated ";
				if((recvdBytes[6] >> 0) & 0x1)
				{
					dataFile << "misfire test available ";
				}
				else
				{
					dataFile << "misfire test N/A ";
				}
				if((recvdBytes[6] >> 4) & 0x1)
				{
					dataFile << "misfire test incomplete ";
				}
				else
				{
					dataFile << "misfire test complete ";
				}
				if((recvdBytes[6] >> 1) & 0x1)
				{
					dataFile << "fuel system test available ";
				}
				else
				{
					dataFile << "fuel system test N/A ";
				}
				if((recvdBytes[6] >> 5) & 0x1)
				{
					dataFile << "fuel system test incomplete ";
				}
				else
				{
					dataFile << "fuel system test complete ";
				}
				if((recvdBytes[6] >> 2) & 0x1)
				{
					dataFile << "components test available ";
				}
				else
				{
					dataFile << "components test N/A ";
				}
				if((recvdBytes[6] >> 6) & 0x1)
				{
					dataFile << "components test incomplete ";
				}
				else
				{
					dataFile << "components test complete ";
				}
				if((recvdBytes[6] >> 3) & 0x1)
				{
					dataFile << "Compression ignition monitors: ";
					if((recvdBytes[7] >> 0) & 0x1)
					{
						dataFile << "NMHC test available ";
					}
					else
					{
						dataFile << "NMHC test N/A ";
					}
					if((recvdBytes[8] >> 0) & 0x1)
					{
						dataFile << "NMHC test incomplete ";
					}
					else
					{
						dataFile << "NMHC test complete ";
					}
					if((recvdBytes[7] >> 1) & 0x1)
					{
						dataFile << "NOx/SCR monitor test available ";
					}
					else
					{
						dataFile << "NOx/SCR monitor test N/A ";
					}
					if((recvdBytes[8] >> 1) & 0x1)
					{
						dataFile << "NOx/SCR monitor test incomplete ";
					}
					else
					{
						dataFile << "NOx/SCR monitor test complete ";
					}
					if((recvdBytes[7] >> 2) & 0x1)
					{
						dataFile << "?? test available ";
					}
					else
					{
						dataFile << "?? test N/A ";
					}
					if((recvdBytes[8] >> 2) & 0x1)
					{
						dataFile << "?? test incomplete ";
					}
					else
					{
						dataFile << "?? test complete ";
					}
					if((recvdBytes[7] >> 3) & 0x1)
					{
						dataFile << "Boost pressure test available ";
					}
					else
					{
						dataFile << "Boost pressure test N/A ";
					}
					if((recvdBytes[8] >> 3) & 0x1)
					{
						dataFile << "Boost pressure test incomplete ";
					}
					else
					{
						dataFile << "Boost pressure test complete ";
					}
					if((recvdBytes[7] >> 4) & 0x1)
					{
						dataFile << "?? test available ";
					}
					else
					{
						dataFile << "?? test N/A ";
					}
					if((recvdBytes[8] >> 4) & 0x1)
					{
						dataFile << "?? test incomplete ";
					}
					else
					{
						dataFile << "?? test complete ";
					}
					if((recvdBytes[7] >> 5) & 0x1)
					{
						dataFile << "exhaust gas sensor test available ";
					}
					else
					{
						dataFile << "exhaust gas sensor test N/A ";
					}
					if((recvdBytes[8] >> 5) & 0x1)
					{
						dataFile << "exhaust gas sensor test incomplete ";
					}
					else
					{
						dataFile << "exhaust gas sensor test complete ";
					}
					if((recvdBytes[7] >> 6) & 0x1)
					{
						dataFile << "PM filter monitoring test available ";
					}
					else
					{
						dataFile << "PM filter monitoring test N/A ";
					}
					if((recvdBytes[8] >> 6) & 0x1)
					{
						dataFile << "PM filter monitoring test incomplete ";
					}
					else
					{
						dataFile << "PM filter monitoring test complete ";
					}
					if((recvdBytes[7] >> 7) & 0x1)
					{
						dataFile << "EGR/VVT system test available ";
					}
					else
					{
						dataFile << "EGR/VVT system test N/A ";
					}
					if((recvdBytes[8] >> 7) & 0x1)
					{
						dataFile << "EGR/VVT system test incomplete ";
					}
					else
					{
						dataFile << "EGR/VVT system test complete ";
					}
				}
				else
				{
					dataFile << "Spark ignition monitors: ";
					if((recvdBytes[7] >> 0) & 0x1)
					{
						dataFile << "Catalyst test available ";
					}
					else
					{
						dataFile << "Catalyst test N/A ";
					}
					if((recvdBytes[8] >> 0) & 0x1)
					{
						dataFile << "Catalyst test incomplete ";
					}
					else
					{
						dataFile << "Catalyst test complete ";
					}
					if((recvdBytes[7] >> 1) & 0x1)
					{
						dataFile << "Heated catalyst test available ";
					}
					else
					{
						dataFile << "Heated catalyst test N/A ";
					}
					if((recvdBytes[8] >> 1) & 0x1)
					{
						dataFile << "Heated catalyst test incomplete ";
					}
					else
					{
						dataFile << "Heated catalyst test complete ";
					}
					if((recvdBytes[7] >> 2) & 0x1)
					{
						dataFile << "Evaporative system test available ";
					}
					else
					{
						dataFile << "Evaporative system test N/A ";
					}
					if((recvdBytes[8] >> 2) & 0x1)
					{
						dataFile << "Evaporative system test incomplete ";
					}
					else
					{
						dataFile << "Evaporative system test complete ";
					}
					if((recvdBytes[7] >> 3) & 0x1)
					{
						dataFile << "Secondary air system test available ";
					}
					else
					{
						dataFile << "Secondary air system test N/A ";
					}
					if((recvdBytes[8] >> 3) & 0x1)
					{
						dataFile << "Secondary air system test incomplete ";
					}
					else
					{
						dataFile << "Secondary air system test complete ";
					}
					if((recvdBytes[7] >> 4) & 0x1)
					{
						dataFile << "A/C refrigerant test available ";
					}
					else
					{
						dataFile << "A/C refrigerant test N/A ";
					}
					if((recvdBytes[8] >> 4) & 0x1)
					{
						dataFile << "A/C refrigerant test incomplete ";
					}
					else
					{
						dataFile << "A/C refrigerant test complete ";
					}
					if((recvdBytes[7] >> 5) & 0x1)
					{
						dataFile << "Oxygen sensor test available ";
					}
					else
					{
						dataFile << "Oxygen sensor test N/A ";
					}
					if((recvdBytes[8] >> 5) & 0x1)
					{
						dataFile << "Oxygen sensor test incomplete ";
					}
					else
					{
						dataFile << "Oxygen sensor test complete ";
					}
					if((recvdBytes[7] >> 6) & 0x1)
					{
						dataFile << "Oxygen sensor heater test available ";
					}
					else
					{
						dataFile << "Oxygen sensor heater test N/A ";
					}
					if((recvdBytes[8] >> 6) & 0x1)
					{
						dataFile << "Oxygen sensor heater test incomplete ";
					}
					else
					{
						dataFile << "Oxygen sensor heater test complete ";
					}
					if((recvdBytes[7] >> 7) & 0x1)
					{
						dataFile << "EGR system test available ";
					}
					else
					{
						dataFile << "EGR system test N/A ";
					}
					if((recvdBytes[8] >> 7) & 0x1)
					{
						dataFile << "EGR system test incomplete ";
					}
					else
					{
						dataFile << "EGR system test complete ";
					}
				}
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 2:
			//MODE 1 PID 2
			//if(mode1checkboxstate[2] != BST_UNCHECKED && success == true && placeCounter == 2)
		{
			//mode 1 PID 2: freeze DTC
			byteFile << ',';
			dataFile << ',';
			//mode = 1;
			//pid = 2;

			uint8_t sendBytes[6] = {104,106,241,1,2,198};
			writeToSerialPort(serialPortHandle, sendBytes, 6);
			readFromSerialPort(serialPortHandle, recvdBytes,6,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 8)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";
			}
			else
			{
				//this should return 2 bytes which are binary flags
				//indicating which DTC triggered the freeze frame
				//the return packet looks like 72,107,16,65,2,x,x,sum


				if(recvdBytes[5] == 0 && recvdBytes[6] == 0)
				{
					dataFile << "no DTC";
				}
				else
				{
					int letterCode = ((recvdBytes[5] >> 7) & 0x1)*2 + ((recvdBytes[5] >> 6) & 0x1);//will be 0,1,2, or 3
					if(letterCode == 0)
					{
						dataFile << 'P';
					}
					else if(letterCode == 1)
					{
						dataFile << 'C';
					}
					else if(letterCode == 2)
					{
						dataFile << 'B';
					}
					else if(letterCode == 3)
					{
						dataFile << 'U';
					}
					int firstDecimal = ((recvdBytes[5] >> 5) & 0x1)*2 + ((recvdBytes[5] >> 4) & 0x1);
					int secondDecimal = ((recvdBytes[5] >> 3) & 0x1)*8 + ((recvdBytes[5] >> 2) & 0x1)*4 + ((recvdBytes[5] >> 1) & 0x1)*2 + ((recvdBytes[5] >> 0) & 0x1);
					int thirdDecimal = ((recvdBytes[6] >> 7) & 0x1)*8 + ((recvdBytes[6] >> 6) & 0x1)*4 + ((recvdBytes[6] >> 5) & 0x1)*2 + ((recvdBytes[6] >> 4) & 0x1);
					int fourthDecimal = ((recvdBytes[6] >> 3) & 0x1)*8 + ((recvdBytes[6] >> 2) & 0x1)*4 + ((recvdBytes[6] >> 1) & 0x1)*2 + ((recvdBytes[6] >> 0) & 0x1);
					dataFile << firstDecimal << secondDecimal << thirdDecimal << fourthDecimal;
				}
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 3:
			//mode 1 PID 3
			//if(mode1checkboxstate[3] != BST_UNCHECKED && success == true && placeCounter == 3)
		{
			//mode 1 PID 3: fuel system status
			byteFile << ',';
			dataFile << ',';
			//mode = 1;
			//pid = 3;

			uint8_t sendBytes[6] = {104,106,241,1,3,199};
			writeToSerialPort(serialPortHandle, sendBytes, 6);
			readFromSerialPort(serialPortHandle, recvdBytes,6,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 8)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";
			}
			else
			{

				//this should return 2 bytes which are byte flags
				//for the next 32 PIDs indicating if they work with this car
				//the return packet looks like 72,107,16,65,3,x,x,sum

				if(recvdBytes[5] != 0){
					dataFile << " Fueling system 1: ";
					if(((recvdBytes[5] >> 0) & 0x1))
					{
						dataFile << "Open loop due to insufficient engine temperature";
					}
					else if(((recvdBytes[5] >> 1) & 0x1))
					{
						dataFile << "Closed loop using oxygen sensor feedback to determine fuel mix";
					}
					else if(((recvdBytes[5] >> 2) & 0x1))
					{
						dataFile << "Open loop due to engine load OR fuel cut due to deceleration";
					}
					else if(((recvdBytes[5] >> 3) & 0x1))
					{
						dataFile << "Open loop due to system failure";
					}
					else if(((recvdBytes[5] >> 4) & 0x1))
					{
						dataFile << "Closed loop using at least one oxygen sensor but there is a fault in the feedback system";
					}
				}

				if(recvdBytes[6] != 0){
					dataFile << " Fueling system 2: ";
					if(((recvdBytes[6] >> 0) & 0x1))
					{
						dataFile << "Open loop due to insufficient engine temperature";
					}
					else if(((recvdBytes[6] >> 1) & 0x1))
					{
						dataFile << "Closed loop using oxygen sensor feedback to determine fuel mix";
					}
					else if(((recvdBytes[6] >> 2) & 0x1))
					{
						dataFile << "Open loop due to engine load OR fuel cut due to deceleration";
					}
					else if(((recvdBytes[6] >> 3) & 0x1))
					{
						dataFile << "Open loop due to system failure";
					}
					else if(((recvdBytes[6] >> 4) & 0x1))
					{
						dataFile << "Closed loop using at least one oxygen sensor but there is a fault in the feedback system";
					}
				}
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 4:
			//mode 1 PID 4
			//if(mode1checkboxstate[4] != BST_UNCHECKED && success == true && placeCounter == 4)
		{
			//mode 1 PID 4: calculated engine load
			byteFile << ',';
			dataFile << ',';
			//mode = 1;
			//pid = 4;

			uint8_t sendBytes[6] = {104,106,241,1,4,200};
			writeToSerialPort(serialPortHandle, sendBytes, 6);
			readFromSerialPort(serialPortHandle, recvdBytes,6,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,7,1000);
			if(numBytesRead < 7)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";
				//returnData = rand()%256;//as random plot data generator for troubleshooting
			}
			else
			{
				//this should return 1 byte which indicates engine load
				//the return packet looks like 72,107,16,65,4,x,sum

				double load = recvdBytes[5]/2.55;
				dataFile << std::setprecision(2) << std::fixed << load;
				returnData = load;
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 5:
			//mode 1 PID 5
			//if(mode1checkboxstate[5] != BST_UNCHECKED && success == true && placeCounter == 5)
		{
			//mode 1 PID 5: engine coolant temperature
			byteFile << ',';
			dataFile << ',';
			//mode = 1;
			//pid = 5;

			uint8_t sendBytes[6] = {104,106,241,1,5,201};
			writeToSerialPort(serialPortHandle, sendBytes, 6);
			readFromSerialPort(serialPortHandle, recvdBytes,6,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,7,1000);
			if(numBytesRead < 7)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 1 byte which indicates engine coolant temp
				//the return packet looks like 72,107,16,65,5,x,sum

				int temp = recvdBytes[5]-40;
				dataFile << temp;
				returnData = temp;
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 6:
			//mode 1 PID 6
			//if(mode1checkboxstate[6] != BST_UNCHECKED && success == true && placeCounter == 6)
		{
			//mode 1 PID 6: short term fuel trim: bank 1
			byteFile << ',';
			dataFile << ',';
			//mode = 1;
			//pid = 6;

			uint8_t sendBytes[6] = {104,106,241,1,6,202};
			writeToSerialPort(serialPortHandle, sendBytes, 6);
			readFromSerialPort(serialPortHandle, recvdBytes,6,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,7,1000);
			if(numBytesRead < 7)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 1 byte which indicates STFT
				//the return packet looks like 72,107,16,65,6,x,sum

				double STFT = recvdBytes[5]/1.28-100;
				dataFile << std::setprecision(2) << std::fixed << STFT;
				returnData = STFT;
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 7:
			//mode 1 PID 7
			//if(mode1checkboxstate[7] != BST_UNCHECKED && success == true && placeCounter == 7)
		{
			//mode 1 PID 7: long term fuel trim: bank 1
			byteFile << ',';
			dataFile << ',';
			//mode = 1;
			//pid = 7;

			uint8_t sendBytes[6] = {104,106,241,1,7,203};
			writeToSerialPort(serialPortHandle, sendBytes, 6);
			readFromSerialPort(serialPortHandle, recvdBytes,6,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,7,1000);
			if(numBytesRead < 7)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 1 byte which indicates LTFT
				//the return packet looks like 72,107,16,65,7,x,sum

				double LTFT = recvdBytes[5]/1.28-100;
				dataFile << std::setprecision(2) << std::fixed << LTFT;
				returnData = LTFT;
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 8:
			//mode 1 PID 0xC
			//if(mode1checkboxstate[8] != BST_UNCHECKED && success == true && placeCounter == 8)
		{
			//mode 1 PID 0xC: engine RPM
			byteFile << ',';
			dataFile << ',';
			//mode = 1;
			//pid = 8;

			uint8_t sendBytes[6] = {104,106,241,1,12,208};
			writeToSerialPort(serialPortHandle, sendBytes, 6);
			readFromSerialPort(serialPortHandle, recvdBytes,6,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 8)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 2 bytes which indicate engine RPM
				//the return packet looks like 72,107,16,65,12,x,x,sum

				double RPM = (recvdBytes[5]*256.00+recvdBytes[6])/4.00;
				dataFile << std::setprecision(2) << std::fixed << RPM;
				returnData = RPM;
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 9:
			//mode 1 PID 0xD
			//if(mode1checkboxstate[9] != BST_UNCHECKED && success == true && placeCounter == 9)
		{
			//mode 1 PID 0xD: Vehicle speed
			byteFile << ',';
			dataFile << ',';
			//mode = 1;
			//pid = 9;

			uint8_t sendBytes[6] = {104,106,241,1,13,209};
			writeToSerialPort(serialPortHandle, sendBytes, 6);
			readFromSerialPort(serialPortHandle, recvdBytes,6,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,7,1000);
			if(numBytesRead < 7)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 1 byte which indicates vehicle speed in km/h
				//the return packet looks like 72,107,16,65,13,x,sum

				int speed = recvdBytes[5];
				dataFile << speed;
				returnData = speed;
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 10:
			//mode 1 PID 0xE
			//if(mode1checkboxstate[10] != BST_UNCHECKED && success == true && placeCounter == 10)
		{
			//mode 1 PID 0xE: Timing advance
			byteFile << ',';
			dataFile << ',';
			//mode = 1;
			//pid = 10;

			uint8_t sendBytes[6] = {104,106,241,1,14,210};
			writeToSerialPort(serialPortHandle, sendBytes, 6);
			readFromSerialPort(serialPortHandle, recvdBytes,6,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,7,1000);
			if(numBytesRead < 7)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 1 byte which indicates timing advance
				//the return packet looks like 72,107,16,65,14,x,sum

				double timingAdvance = recvdBytes[5]/2.00 - 64.00;
				dataFile << std::setprecision(2) << std::fixed << timingAdvance;
				returnData = timingAdvance;
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 11:
			//mode 1 PID 0xF
			//if(mode1checkboxstate[11] != BST_UNCHECKED && success == true && placeCounter == 11)
		{
			//mode 1 PID 0xF: Intake air temperature
			byteFile << ',';
			dataFile << ',';
			//mode = 1;
			//pid = 11;

			uint8_t sendBytes[6] = {104,106,241,1,15,211};
			writeToSerialPort(serialPortHandle, sendBytes, 6);
			readFromSerialPort(serialPortHandle, recvdBytes,6,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,7,1000);
			if(numBytesRead < 7)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 1 byte which indicates intake air temperature
				//the return packet looks like 72,107,16,65,15,x,sum

				int IAT = recvdBytes[5] - 40;
				dataFile << IAT;
				returnData = IAT;
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 12:
			//mode 1 PID 0x10
			//if(mode1checkboxstate[12] != BST_UNCHECKED && success == true && placeCounter == 12)
		{
			//mode 1 PID 0x10: Mass air flow rate
			byteFile << ',';
			dataFile << ',';
			//mode = 1;
			//pid = 12;

			uint8_t sendBytes[6] = {104,106,241,1,16,212};
			writeToSerialPort(serialPortHandle, sendBytes, 6);
			readFromSerialPort(serialPortHandle, recvdBytes,6,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 8)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll file";

			}
			else
			{
				//this should return 2 bytes which indicate mass airflow rate
				//the return packet looks like 72,107,16,65,16,x,x,sum

				double MAF = (recvdBytes[5]*256.00 + recvdBytes[6])/100.00;
				dataFile << std::setprecision(2) << std::fixed << MAF;
				returnData = MAF;
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 13:
			//mode 1 PID 0x11
			//if(mode1checkboxstate[13] != BST_UNCHECKED && success == true && placeCounter == 13)
		{
			//mode 1 PID 0x11: Throttle position
			byteFile << ',';
			dataFile << ',';
			//mode = 1;
			//pid = 13;

			uint8_t sendBytes[6] = {104,106,241,1,17,213};
			writeToSerialPort(serialPortHandle, sendBytes, 6);
			readFromSerialPort(serialPortHandle, recvdBytes,6,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,7,1000);
			if(numBytesRead < 7)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 1 byte which indicates throttle position
				//the return packet looks like 72,107,16,65,17,x,sum

				double TPS = recvdBytes[5]/2.55;
				dataFile << std::setprecision(2) << std::fixed << TPS;
				returnData = TPS;
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 14:
			//mode 1 PID 0x13
			//if(mode1checkboxstate[14] != BST_UNCHECKED && success == true && placeCounter == 14)
		{
			//mode 1 PID 0x13: Oxygen sensors present
			byteFile << ',';
			dataFile << ',';
			//mode = 1;
			//pid = 14;

			uint8_t sendBytes[6] = {104,106,241,1,19,215};
			writeToSerialPort(serialPortHandle, sendBytes, 6);
			readFromSerialPort(serialPortHandle, recvdBytes,6,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,7,1000);
			if(numBytesRead < 7)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 1 byte which indicates how many O2 sensors are present
				//the return packet looks like 72,107,16,65,19,x,sum

				dataFile << "Bank1 O2 sensors: ";
				if(((recvdBytes[5] >> 0) & 0x1))
				{
					dataFile << "1 ";
				}
				if(((recvdBytes[5] >> 1) & 0x1))
				{
					dataFile << "2 ";
				}
				if(((recvdBytes[5] >> 2) & 0x1))
				{
					dataFile << "3 ";
				}
				if(((recvdBytes[5] >> 3) & 0x1))
				{
					dataFile << "4 ";
				}
				dataFile << "Bank2 O2 sensors: ";
				if(((recvdBytes[5] >> 4) & 0x1))
				{
					dataFile << "1 ";
				}
				if(((recvdBytes[5] >> 5) & 0x1))
				{
					dataFile << "2 ";
				}
				if(((recvdBytes[5] >> 6) & 0x1))
				{
					dataFile << "3 ";
				}
				if(((recvdBytes[5] >> 7) & 0x1))
				{
					dataFile << "4 ";
				}
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 15:
			//mode 1 PID 0x14
			//if(mode1checkboxstate[15] != BST_UNCHECKED && success == true && placeCounter == 15)
		{
			//mode 1 PID 0x14: Oxygen sensor 1 voltage/short term fuel trim
			byteFile << ',';
			dataFile << ',';
			//mode = 1;
			//pid = 15;

			uint8_t sendBytes[6] = {104,106,241,1,20,216};
			writeToSerialPort(serialPortHandle, sendBytes, 6);
			readFromSerialPort(serialPortHandle, recvdBytes,6,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 8)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 2 bytes which indicate oxygen sensor voltage and STFT
				//the return packet looks like 72,107,16,65,20,x,x,sum

				double voltage = recvdBytes[5]/200.00;
				double STFT = recvdBytes[6]/1.28 - 100;
				dataFile << std::setprecision(2) << std::fixed << voltage << ' ' << STFT;
				returnData = voltage;
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 16:
			//mode 1 PID 0x15
			//if(mode1checkboxstate[16] != BST_UNCHECKED && success == true && placeCounter == 16)
		{
			//mode 1 PID 0x15: Oxygen sensor 2 voltage/short term fuel trim
			byteFile << ',';
			dataFile << ',';
			//mode = 1;
			//pid = 16;

			uint8_t sendBytes[6] = {104,106,241,1,21,217};
			writeToSerialPort(serialPortHandle, sendBytes, 6);
			readFromSerialPort(serialPortHandle, recvdBytes,6,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 8)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 2 bytes which indicate oxygen sensor voltage and STFT
				//the return packet looks like 72,107,16,65,21,x,x,sum

				double voltage = recvdBytes[5]/200.00;
				double STFT = recvdBytes[6]/1.28 - 100;
				dataFile << std::setprecision(2) << std::fixed << voltage << ' ' << STFT;
				returnData = voltage;
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 17:
			//mode 1 PID 0x1C
			//if(mode1checkboxstate[17] != BST_UNCHECKED && success == true && placeCounter == 17)
		{
			//mode 1 PID 0x1C: OBD standards this vehicle conforms to
			byteFile << ',';
			dataFile << ',';
			//mode = 1;
			//pid = 17;

			uint8_t sendBytes[6] = {104,106,241,1,28,224};
			writeToSerialPort(serialPortHandle, sendBytes, 6);
			readFromSerialPort(serialPortHandle, recvdBytes,6,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,7,1000);
			if(numBytesRead < 7)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 1 byte which indicates which OBD standards the vehicle conforms to
				//the return packet looks like 72,107,16,65,28,x,sum

				if(recvdBytes[5] == 1)
				{
					dataFile << "OBDII as defined by the CARB";
				}
				else if(recvdBytes[5] == 2)
				{
					dataFile << "OBDII as defined by the EPA";
				}
				else if(recvdBytes[5] == 3)
				{
					dataFile << "OBD and OBDII";
				}
				else if(recvdBytes[5] == 4)
				{
					dataFile << "OBD-I";
				}
				else if(recvdBytes[5] == 5)
				{
					dataFile << "Not OBD compliant";
				}
				else if(recvdBytes[5] == 6)
				{
					dataFile << "EOBD (Europe)";
				}
				else if(recvdBytes[5] == 7)
				{
					dataFile << "EOBD and OBDII";
				}
				else if(recvdBytes[5] == 8)
				{
					dataFile << "EOBD and OBD";
				}
				else if(recvdBytes[5] == 9)
				{
					dataFile << "EOBD, OBD and OBDII";
				}
				else if(recvdBytes[5] == 10)
				{
					dataFile << "JOBD (Japan)";
				}
				else if(recvdBytes[5] == 11)
				{
					dataFile << "JOBD and OBDII";
				}
				else if(recvdBytes[5] == 12)
				{
					dataFile << "JOBD and EOBD";
				}
				else if(recvdBytes[5] == 13)
				{
					dataFile << "JOBD, EOBD, and OBDII";
				}
				else if(recvdBytes[5] == 17)
				{
					dataFile << "Engine Manufacturer Diagnostics (EMD)";
				}
				else if(recvdBytes[5] == 18)
				{
					dataFile << "Engine Manufacturer Diagnostics Enhanced (EMD+)";
				}
				else if(recvdBytes[5] == 19)
				{
					dataFile << "Heavy Duty On-Board Diagnostics (Child/Partial) (HD OBD-C)";
				}
				else if(recvdBytes[5] == 20)
				{
					dataFile << "Heavy Duty On-Board Diagnostics (HD OBD)";
				}
				else if(recvdBytes[5] == 21)
				{
					dataFile << "World Wide Harmonized OBD (WWH OBD)";
				}
				else if(recvdBytes[5] == 23)
				{
					dataFile << "Heavy Duty Euro OBD Stage I without NOx control (HD EOBD-I)";
				}
				else if(recvdBytes[5] == 24)
				{
					dataFile << "Heavy Duty Euro OBD Stage I with NOx control (HD EOBD-I N)";
				}
				else if(recvdBytes[5] == 25)
				{
					dataFile << "Heavy Duty Euro OBD Stage II without NOx control (HD EOBD-II)";
				}
				else if(recvdBytes[5] == 26)
				{
					dataFile << "Heavy Duty Euro OBD Stage II with NOx control (HD EOBD-II N)";
				}
				else if(recvdBytes[5] == 28)
				{
					dataFile << "Brazil OBD Phase 1 (OBDBr-1)";
				}
				else if(recvdBytes[5] == 29)
				{
					dataFile << "Brazil OBD Phase 2 (OBDBr-2)";
				}
				else if(recvdBytes[5] == 30)
				{
					dataFile << "Korean OBD (KOBD)";
				}
				else if(recvdBytes[5] == 31)
				{
					dataFile << "India OBD I (IOBD I)";
				}
				else if(recvdBytes[5] == 32)
				{
					dataFile << "India OBD II (IOBD II)";
				}
				else if(recvdBytes[5] == 33)
				{
					dataFile << "Heavy Duty Euro OBD Stage VI (HD EOBD-IV)";
				}
			}

			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 18:
			//mode 1 PID 0x1F
			//if(mode1checkboxstate[18] != BST_UNCHECKED && success == true && placeCounter == 18)
		{
			//mode 1 PID 0x1F: Run time since engine start
			byteFile << ',';
			dataFile << ',';
			//mode = 1;
			//pid = 18;

			uint8_t sendBytes[6] = {104,106,241,1,31,227};
			writeToSerialPort(serialPortHandle, sendBytes, 6);
			readFromSerialPort(serialPortHandle, recvdBytes,6,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 8)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 2 bytes which indicate engine run time
				//the return packet looks like 72,107,16,65,31,x,x,sum

				int runTime = recvdBytes[5]*256 + recvdBytes[6];
				dataFile << runTime;
				returnData = runTime;
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 19:
			//mode 1 PID 0x20
			//if(mode1checkboxstate[19] != BST_UNCHECKED && success == true && placeCounter == 19)
		{
			//mode 1 PID 0x20: PIDs supported 21-40
			byteFile << ',';
			dataFile << ',';
			//mode = 1;
			//pid = 19;

			uint8_t sendBytes[6] = {104,106,241,1,32,228};
			writeToSerialPort(serialPortHandle, sendBytes, 6);
			readFromSerialPort(serialPortHandle, recvdBytes,6,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,10,1000);
			if(numBytesRead < 10)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 4 bytes which indicate which
				//PIDs are supported from 21-40
				//the return packet looks like 72,107,16,65,32,x,x,x,x,sum

				int PIDplace = 32;
				for(int i = 5; i < 9; i++)//loop through bytes
				{
					for(int j = 7; j > -1; j--)
					{
						PIDplace += 1;
						if(((recvdBytes[i] >> j) & 0x1))//shift and mask to check bit flags
						{
							dataFile << PIDplace << ' ';
						}
					}
				}
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 20:
			//mode 1 PID 0x21
			//if(mode1checkboxstate[20] != BST_UNCHECKED && success == true && placeCounter == 20)
		{
			//mode 1 PID 0x21: Distance traveled with malfunction indicator lamp (MIL) on
			byteFile << ',';
			dataFile << ',';
			//mode = 1;
			//pid = 20;

			uint8_t sendBytes[6] = {104,106,241,1,33,229};
			writeToSerialPort(serialPortHandle, sendBytes, 6);
			readFromSerialPort(serialPortHandle, recvdBytes,6,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 8)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 2 bytes which indicate distance traveled
				//with the CEL on
				//the return packet looks like 72,107,16,65,33,x,x,sum

				int distance = recvdBytes[5]*256 + recvdBytes[6];
				dataFile << distance;
				returnData = distance;
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 21:
			//mode 1 PID 0x2E
			//if(mode1checkboxstate[21] != BST_UNCHECKED && success == true && placeCounter == 21)
		{
			//mode 1 PID 0x2E: commanded evaportative purge
			byteFile << ',';
			dataFile << ',';
			//mode = 1;
			//pid = 21;

			uint8_t sendBytes[6] = {104,106,241,1,46,242};
			writeToSerialPort(serialPortHandle, sendBytes, 6);
			readFromSerialPort(serialPortHandle, recvdBytes,6,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,7,1000);
			if(numBytesRead < 7)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 1 byte which indicates the commanded evap purge
				//the return packet looks like 72,107,16,65,46,x,sum

				double evap = recvdBytes[5]/2.55;
				dataFile << std::setprecision(2) << std::fixed << evap;
				returnData = evap;
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 22:
			//mode 1 PID 0x2F
			//if(mode1checkboxstate[22] != BST_UNCHECKED && success == true && placeCounter == 22)
		{
			//mode 1 PID 0x2F: Fuel tank level
			byteFile << ',';
			dataFile << ',';
			//mode = 1;
			//pid = 22;

			uint8_t sendBytes[6] = {104,106,241,1,47,243};
			writeToSerialPort(serialPortHandle, sendBytes, 6);
			readFromSerialPort(serialPortHandle, recvdBytes,6,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,7,1000);
			if(numBytesRead < 7)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 1 byte which indicates the fuel tank input level
				//the return packet looks like 72,107,16,65,47,x,sum

				double fuelLevel = recvdBytes[5]/2.55;
				dataFile << std::setprecision(2) << std::fixed << fuelLevel;
				returnData = fuelLevel;
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 23:
			//mode 1 PID 0x33
			//if(mode1checkboxstate[23] != BST_UNCHECKED && success == true && placeCounter == 23)
		{
			//mode 1 PID 0x33: Absolute barometric pressure
			byteFile << ',';
			dataFile << ',';
			//mode = 1;
			//pid = 23;

			uint8_t sendBytes[6] = {104,106,241,1,51,247};
			writeToSerialPort(serialPortHandle, sendBytes, 6);
			readFromSerialPort(serialPortHandle, recvdBytes,6,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,7,1000);
			if(numBytesRead < 7)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 1 byte which indicates the absolute barometric pressure
				//the return packet looks like 72,107,16,65,51,x,sum

				int pressure = recvdBytes[5];
				dataFile << pressure;
				returnData = pressure;
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 24:
			//mode 1 PID 0x40
			//if(mode1checkboxstate[24] != BST_UNCHECKED && success == true && placeCounter == 24)
		{
			//mode 1 PID 0x40: PIDs supported 0x41 to 0x60
			byteFile << ',';
			dataFile << ',';
			//mode = 1;
			//pid = 24;

			uint8_t sendBytes[6] = {104,106,241,1,64,4};
			writeToSerialPort(serialPortHandle, sendBytes, 6);
			readFromSerialPort(serialPortHandle, recvdBytes,6,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,10,1000);
			if(numBytesRead < 10)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 4 bytes which indicate available OBD PIDs from 0x41 to 0x60
				//the return packet looks like 72,107,16,65,64,x,x,x,x,sum

				int PIDplace = 64;
				for(int i = 5; i < 9; i++)//loop through bytes
				{
					for(int j = 7; j > -1; j--)
					{
						PIDplace += 1;
						if(((recvdBytes[i] >> j) & 0x1))//shift and mask to check bit flags
						{
							dataFile << PIDplace << ' ';
						}
					}
				}
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 25:
			//mode 1 PID 0x42
			//if(mode1checkboxstate[25] != BST_UNCHECKED && success == true && placeCounter == 25)
		{
			//mode 1 PID 0x42: Control module voltage
			byteFile << ',';
			dataFile << ',';
			//mode = 1;
			//pid = 25;

			uint8_t sendBytes[6] = {104,106,241,1,66,6};
			writeToSerialPort(serialPortHandle, sendBytes, 6);
			readFromSerialPort(serialPortHandle, recvdBytes,6,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 8)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 2 byte which indicate the control module voltage
				//the return packet looks like 72,107,16,65,66,x,x,sum

				double voltage = (recvdBytes[5]*256.00+recvdBytes[6])/1000.00;
				dataFile << std::setprecision(2) << std::fixed << voltage;
				returnData = voltage;
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 26:
			//mode 1 PID 0x43
			//if(mode1checkboxstate[26] != BST_UNCHECKED && success == true && placeCounter == 26)
		{
			//mode 1 PID 0x43: Absolute load value
			byteFile << ',';
			dataFile << ',';
			//mode = 1;
			//pid = 26;

			uint8_t sendBytes[6] = {104,106,241,1,67,7};
			writeToSerialPort(serialPortHandle, sendBytes, 6);
			readFromSerialPort(serialPortHandle, recvdBytes,6,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 8)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 2 byte which indicate the absolute load
				//the return packet looks like 72,107,16,65,67,x,x,sum

				double load = (recvdBytes[5]*256.00+recvdBytes[6])/2.55;
				dataFile << std::setprecision(2) << std::fixed << load;
				returnData = load;
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 27:
			//mode 1 PID 0x45
			//if(mode1checkboxstate[27] != BST_UNCHECKED && success == true && placeCounter == 27)
		{
			//mode 1 PID 0x45: Relative throttle position
			byteFile << ',';
			dataFile << ',';
			//mode = 1;
			//pid = 27;

			uint8_t sendBytes[6] = {104,106,241,1,69,9};
			writeToSerialPort(serialPortHandle, sendBytes, 6);
			readFromSerialPort(serialPortHandle, recvdBytes,6,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,7,1000);
			if(numBytesRead < 7)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 1 byte which indicates the relative throttle position
				//the return packet looks like 72,107,16,65,69,x,sum

				double throttlePos = recvdBytes[5]/2.55;
				dataFile << std::setprecision(2) << std::fixed << throttlePos;
				returnData = throttlePos;
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		//end pid switch statment
		}
		break;


		//******************************************************************************

	case 2:
		//11 PIDs to check for mode 2
		switch(pid)
		{
		case 0:
			//PID 0
			//if(mode2checkboxstate[0] != BST_UNCHECKED && success == true && placeCounter == 28)
		{
			//mode 2 PID 0: list of valid PIDs 0x1-0x20
			byteFile << ',';
			dataFile << ',';
			//mode = 2;
			//pid = 0;

			uint8_t sendBytes[7] = {104,106,241,2,0,0,197};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 4 bytes which are binary flags
				//for the next 32 PIDs indicating if they work with this car
				//the return packet looks like 72,107,16,66,0,0,x,x,x,x,sum

				int PIDplace = 0;
				for(int i = 6; i < 10; i++)//loop through bytes
				{
					for(int j = 7; j > -1; j--)
					{
						PIDplace += 1;
						if(((recvdBytes[i] >> j) & 0x1))//shift and mask to check bit flags
						{
							dataFile << PIDplace << ' ';
						}
					}
				}
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 1:
			//MODE 2 PID 2
			//if(mode2checkboxstate[1] != BST_UNCHECKED && success == true && placeCounter == 29)
		{
			//mode 2 PID 2: freeze DTC
			byteFile << ',';
			dataFile << ',';
			//mode = 2;
			//pid = 1;

			uint8_t sendBytes[7] = {104,106,241,2,2,0,199};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,9,1000);
			if(numBytesRead < 9)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 2 bytes which are binary flags
				//indicating which DTC triggered the freeze frame
				//the return packet looks like 72,107,16,66,2,0,x,x,sum


				if(recvdBytes[6] == 0 && recvdBytes[7] == 0)
				{
					dataFile << "no DTC";
				}
				else
				{
					int letterCode = ((recvdBytes[6] >> 7) & 0x1)*2 + ((recvdBytes[6] >> 6) & 0x1);//will be 0,1,2, or 3
					if(letterCode == 0)
					{
						dataFile << 'P';
					}
					else if(letterCode == 1)
					{
						dataFile << 'C';
					}
					else if(letterCode == 2)
					{
						dataFile << 'B';
					}
					else if(letterCode == 3)
					{
						dataFile << 'U';
					}
					int firstDecimal = ((recvdBytes[6] >> 5) & 0x1)*2 + ((recvdBytes[6] >> 4) & 0x1);
					int secondDecimal = ((recvdBytes[6] >> 3) & 0x1)*8 + ((recvdBytes[6] >> 2) & 0x1)*4 + ((recvdBytes[6] >> 1) & 0x1)*2 + ((recvdBytes[6] >> 0) & 0x1);
					int thirdDecimal = ((recvdBytes[7] >> 7) & 0x1)*8 + ((recvdBytes[7] >> 6) & 0x1)*4 + ((recvdBytes[7] >> 5) & 0x1)*2 + ((recvdBytes[7] >> 4) & 0x1);
					int fourthDecimal = ((recvdBytes[7] >> 3) & 0x1)*8 + ((recvdBytes[7] >> 2) & 0x1)*4 + ((recvdBytes[7] >> 1) & 0x1)*2 + ((recvdBytes[7] >> 0) & 0x1);
					dataFile << firstDecimal << secondDecimal << thirdDecimal << fourthDecimal;
				}
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 2:
			//mode 2 PID 3
			//if(mode2checkboxstate[2] != BST_UNCHECKED && success == true && placeCounter == 30)
		{
			//mode 2 PID 3: fuel system status
			byteFile << ',';
			dataFile << ',';
			//mode = 2;
			//pid = 2;

			uint8_t sendBytes[7] = {104,106,241,2,3,0,200};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,9,1000);
			if(numBytesRead < 9)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 2 bytes which are byte flags
				//for the next 32 PIDs indicating if they work with this car
				//the return packet looks like 72,107,16,66,3,0,x,x,sum

				if(recvdBytes[6] != 0){
					dataFile << " Fueling system 1: ";
					if(((recvdBytes[6] >> 0) & 0x1))
					{
						dataFile << "Open loop due to insufficient engine temperature";
					}
					else if(((recvdBytes[6] >> 1) & 0x1))
					{
						dataFile << "Closed loop using oxygen sensor feedback to determine fuel mix";
					}
					else if(((recvdBytes[6] >> 2) & 0x1))
					{
						dataFile << "Open loop due to engine load OR fuel cut due to deceleration";
					}
					else if(((recvdBytes[6] >> 3) & 0x1))
					{
						dataFile << "Open loop due to system failure";
					}
					else if(((recvdBytes[6] >> 4) & 0x1))
					{
						dataFile << "Closed loop using at least one oxygen sensor but there is a fault in the feedback system";
					}
				}

				if(recvdBytes[7] != 0){
					dataFile << " Fueling system 2: ";
					if(((recvdBytes[7] >> 0) & 0x1))
					{
						dataFile << "Open loop due to insufficient engine temperature";
					}
					else if(((recvdBytes[7] >> 1) & 0x1))
					{
						dataFile << "Closed loop using oxygen sensor feedback to determine fuel mix";
					}
					else if(((recvdBytes[7] >> 2) & 0x1))
					{
						dataFile << "Open loop due to engine load OR fuel cut due to deceleration";
					}
					else if(((recvdBytes[7] >> 3) & 0x1))
					{
						dataFile << "Open loop due to system failure";
					}
					else if(((recvdBytes[7] >> 4) & 0x1))
					{
						dataFile << "Closed loop using at least one oxygen sensor but there is a fault in the feedback system";
					}
				}
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 3:
			//mode 2 PID 4
			//if(mode2checkboxstate[3] != BST_UNCHECKED && success == true && placeCounter == 31)
		{
			//mode 2 PID 4: calculated engine load
			byteFile << ',';
			dataFile << ',';
			//mode = 2;
			//pid = 3;

			uint8_t sendBytes[7] = {104,106,241,2,4,0,201};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 8)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 1 byte which indicates engine load
				//the return packet looks like 72,107,16,66,4,0,x,sum

				double load = recvdBytes[6]/2.55;
				dataFile << std::setprecision(2) << std::fixed << load;
				returnData = load;
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 4:
			//mode 2 PID 5
			//if(mode2checkboxstate[4] != BST_UNCHECKED && success == true && placeCounter == 32)
		{
			//mode 2 PID 5: engine coolant temperature
			byteFile << ',';
			dataFile << ',';
			//mode = 2;
			//pid = 4;

			uint8_t sendBytes[7] = {104,106,241,2,5,0,202};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 8)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 1 byte which indicates engine coolant temp
				//the return packet looks like 72,107,16,66,5,0,x,sum

				int temp = recvdBytes[6]-40;
				dataFile << temp;
				returnData = temp;
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 5:
			//mode 2 PID 6
			//if(mode2checkboxstate[5] != BST_UNCHECKED && success == true && placeCounter == 33)
		{
			//mode 2 PID 6: short term fuel trim: bank 1
			byteFile << ',';
			dataFile << ',';
			//mode = 2;
			//pid = 5;

			uint8_t sendBytes[7] = {104,106,241,2,6,0,203};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 8)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 1 byte which indicates STFT
				//the return packet looks like 72,107,16,66,6,0,x,sum

				double STFT = recvdBytes[6]/1.28-100;
				dataFile << std::setprecision(2) << std::fixed << STFT;
				returnData = STFT;
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 6:
			//mode 2 PID 7
			//if(mode2checkboxstate[6] != BST_UNCHECKED && success == true && placeCounter == 34)
		{
			//mode 2 PID 7: long term fuel trim: bank 1
			byteFile << ',';
			dataFile << ',';
			//mode = 2;
			//pid = 6;

			uint8_t sendBytes[7] = {104,106,241,2,7,0,204};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 8)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 1 byte which indicates LTFT
				//the return packet looks like 72,107,16,66,7,0,x,sum

				double LTFT = recvdBytes[6]/1.28-100;
				dataFile << std::setprecision(2) << std::fixed << LTFT;
				returnData = LTFT;
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 7:
			//mode 2 PID 0xC
			//if(mode2checkboxstate[7] != BST_UNCHECKED && success == true && placeCounter == 35)
		{
			//mode 2 PID 0xC: engine RPM
			byteFile << ',';
			dataFile << ',';
			//mode = 2;
			//pid = 7;

			uint8_t sendBytes[7] = {104,106,241,2,12,0,209};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,9,1000);
			if(numBytesRead < 9)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 2 bytes which indicate engine RPM
				//the return packet looks like 72,107,16,66,12,0,x,x,sum

				double RPM = (recvdBytes[6]*256.00+recvdBytes[7])/4.00;
				dataFile << std::setprecision(2) << std::fixed << RPM;
				returnData = RPM;
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 8:
			//mode 2 PID 0xD
			//if(mode2checkboxstate[8] != BST_UNCHECKED && success == true && placeCounter == 36)
		{
			//mode 2 PID 0xD: Vehicle speed
			byteFile << ',';
			dataFile << ',';
			//mode = 2;
			//pid = 8;

			uint8_t sendBytes[7] = {104,106,241,2,13,0,210};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 8)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 1 byte which indicates vehicle speed in km/h
				//the return packet looks like 72,107,16,66,13,0,x,sum

				int speed = recvdBytes[6];
				dataFile << speed;
				returnData = speed;
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 9:
			//mode 2 PID 0x10
			//if(mode2checkboxstate[9] != BST_UNCHECKED && success == true && placeCounter == 37)
		{
			//mode 2 PID 0x10: Mass air flow rate
			byteFile << ',';
			dataFile << ',';
			//mode = 2;
			//pid = 9;

			uint8_t sendBytes[7] = {104,106,241,2,16,0,213};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,9,1000);
			if(numBytesRead < 9)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 2 bytes which indicate mass airflow rate
				//the return packet looks like 72,107,16,66,16,0,x,x,sum

				double MAF = (recvdBytes[6]*256.00 + recvdBytes[7])/100.00;
				dataFile << std::setprecision(2) << std::fixed << MAF;
				returnData = MAF;
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 10:
			//mode 2 PID 0x11
			//if(mode2checkboxstate[10] != BST_UNCHECKED && success == true && placeCounter == 38)
		{
			//mode 2 PID 0x11: Throttle position
			byteFile << ',';
			dataFile << ',';
			//mode = 2;
			//pid = 10;

			uint8_t sendBytes[7] = {104,106,241,2,17,0,214};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 8)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 1 byte which indicates throttle position
				//the return packet looks like 72,107,16,66,17,0,x,sum

				double TPS = recvdBytes[6]/2.55;
				dataFile << std::setprecision(2) << std::fixed << TPS;
				returnData = TPS;
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		//end pid switch
		}
		break;


		//****************************************************************************

	case 3:
		//mode 3
		//if(mode3checkboxstate != BST_UNCHECKED && success == true && placeCounter == 39)
		{
			//mode 3: Read DTCs
			byteFile << ',';
			dataFile << ',';
			//mode = 3;
			//pid = 0;

			uint8_t sendBytes[5] = {104,106,241,3,198};//message for MODE 0x3 PID 0
			writeToSerialPort(serialPortHandle, sendBytes, 5);
			readFromSerialPort(serialPortHandle, recvdBytes,5,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,100,300);
			//std::cout << "read in " << numBytesRead << " bytes" << '\n';
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				/*for(int k = 0; k < 11; k++)
        	{
        		std::cout << (int) recvdBytes[k] << ' ';
        	}
        	std::cout << '\n';*/

			}
			//this should return pairs of bytes which indicate DTCs
			//the return packet looks like 72,107,16,67,x,x,x,x,x,x,sum

			if(success == true)
			{
				if(recvdBytes[4] == 0 && recvdBytes[5] == 0)
				{
					dataFile << "no DTC";
					//std::cout << "no DTC";
				}
				else
				{
					int index = 4;
					for(int i = 0;i < 3; i++)//loop through the bytes to interpret DTCs
					{
						if(!(recvdBytes[i*2+index] == 0 && recvdBytes[i*2+1+index] == 0))
						{
							std::string DTC = decodeDTC(recvdBytes[i*2+index],recvdBytes[i*2+1+index]);
							dataFile << DTC << ' ';
							//std::cout << DTC << ' ';
						}
						if(numBytesRead > index+7 && i*2+4+index > index+7)
						{
							i = 0;
							index += 11;
						}
					}
				}
				//std::cout << '\n';
			}

			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		//********************************************************************************

	case 4:
		//mode 4
		//if(mode4checkboxstate != BST_UNCHECKED && success == true && placeCounter == 40)
		{
			//mode 4: Clear DTCs
			byteFile << ',';
			dataFile << ',';
			//mode = 4;
			//pid = 0;

			uint8_t sendBytes[5] = {104,106,241,4,199};//message for MODE 0x4 PID 0
			writeToSerialPort(serialPortHandle, sendBytes, 5);
			readFromSerialPort(serialPortHandle, recvdBytes,5,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,5,1000);
			if(numBytesRead < 5)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return a blank message
				//the return packet looks like 72,107,16,68,sum

				dataFile << "DTCs cleared";
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		//************************************************************************************
	case 5:
		//9 PIDs to check for mode 5
		switch(pid)
		{
		case 0:
			//PID 0x100
			//if(mode5checkboxstate[0] != BST_UNCHECKED && success == true && placeCounter == 41)
		{
			//mode 5 PID 0x100: list of valid PIDs 0x0-0x20
			byteFile << ',';
			dataFile << ',';
			//mode = 5;
			//pid = 0;

			uint8_t sendBytes[7] = {104,106,241,5,0,1,201};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 4 bytes which are binary flags
				//for the next 32 PIDs indicating if they work with this car
				//the return packet looks like 72,107,16,69,0,1,x,x,x,x,sum

				int PIDplace = 0;
				for(int i = 6; i < 10; i++)//loop through bytes
				{
					for(int j = 7; j > -1; j--)
					{
						PIDplace += 1;
						if(((recvdBytes[i] >> j) & 0x1))//shift and mask to check bit flags
						{
							dataFile << PIDplace << ' ';
						}
					}
				}
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 1:
			//mode 5 PID 0x101
			//if(mode5checkboxstate[1] != BST_UNCHECKED && success == true && placeCounter == 42)
		{
			//mode 5 PID 0x101: Oxygen sensor monitor bank 1 sensor 1 voltage
			byteFile << ',';
			dataFile << ',';
			//mode = 5;
			//pid = 1;

			uint8_t sendBytes[7] = {104,106,241,5,1,1,202};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 8)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 1 byte which indicates oxygen sensor voltage
				//the return packet looks like 72,107,16,69,1,1,x,sum

				double voltage = recvdBytes[6]/200.00;
				dataFile << std::setprecision(2) << std::fixed << voltage;
				returnData = voltage;
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 2:
			//mode 5 PID 0x102
			//if(mode5checkboxstate[2] != BST_UNCHECKED && success == true && placeCounter == 43)
		{
			//mode 5 PID 0x102: Oxygen sensor monitor bank 1 sensor 2 voltage
			byteFile << ',';
			dataFile << ',';
			//mode = 5;
			//pid = 2;

			uint8_t sendBytes[7] = {104,106,241,5,2,1,203};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 8)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 1 byte which indicates oxygen sensor voltage
				//the return packet looks like 72,107,16,69,2,1,x,sum

				double voltage = recvdBytes[6]/200.00;
				dataFile << std::setprecision(2) << std::fixed << voltage;
				returnData = voltage;
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 3:
			//mode 5 PID 0x103
			//if(mode5checkboxstate[3] != BST_UNCHECKED && success == true && placeCounter == 44)
		{
			//mode 5 PID 0x103: Oxygen sensor monitor bank 1 sensor 3 voltage
			byteFile << ',';
			dataFile << ',';
			//mode = 5;
			//pid = 3;

			uint8_t sendBytes[7] = {104,106,241,5,3,1,204};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 8)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 1 byte which indicates oxygen sensor voltage
				//the return packet looks like 72,107,16,69,3,1,x,sum

				double voltage = recvdBytes[6]/200.00;
				dataFile << std::setprecision(2) << std::fixed << voltage;
				returnData = voltage;
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 4:
			//mode 5 PID 0x104
			//if(mode5checkboxstate[4] != BST_UNCHECKED && success == true && placeCounter == 45)
			{
				//mode 5 PID 0x104: Oxygen sensor monitor bank 1 sensor 4 voltage
				byteFile << ',';
				dataFile << ',';
				//mode = 5;
				//pid = 4;

				uint8_t sendBytes[7] = {104,106,241,5,4,1,205};
				writeToSerialPort(serialPortHandle, sendBytes, 7);
				readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
				int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
				if(numBytesRead < 8)
				{
					//we've failed the read and should flag to quit the polling
					success = false;
					dataFile << "poll fail";

				}
				else
				{
					//this should return 1 byte which indicates oxygen sensor voltage
					//the return packet looks like 72,107,16,69,4,1,x,sum

					double voltage = recvdBytes[6]/200.00;
					dataFile << std::setprecision(2) << std::fixed << voltage;
					returnData = voltage;
				}
				for(int k = 0; k < numBytesRead; k++)
				{
					byteFile << (int) recvdBytes[k] << ' ';
					recvdBytes[k] = 0;//clear the read buffer for the next set of data
				}
			}
			break;

		case 5:
			//mode 5 PID 0x105
			//if(mode5checkboxstate[5] != BST_UNCHECKED && success == true && placeCounter == 46)
		{
			//mode 5 PID 0x105: Oxygen sensor monitor bank 2 sensor 1 voltage
			byteFile << ',';
			dataFile << ',';
			//mode = 5;
			//pid = 5;

			uint8_t sendBytes[7] = {104,106,241,5,5,1,206};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 8)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 1 byte which indicates oxygen sensor voltage
				//the return packet looks like 72,107,16,69,5,1,x,sum

				double voltage = recvdBytes[6]/200.00;
				dataFile << std::setprecision(2) << std::fixed << voltage;
				returnData = voltage;
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 6:
			//mode 5 PID 0x106
			//if(mode5checkboxstate[6] != BST_UNCHECKED && success == true && placeCounter == 47)
		{
			//mode 5 PID 0x106: Oxygen sensor monitor bank 2 sensor 2 voltage
			byteFile << ',';
			dataFile << ',';
			//mode = 5;
			//pid = 6;

			uint8_t sendBytes[7] = {104,106,241,5,6,1,207};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 8)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 1 byte which indicates oxygen sensor voltage
				//the return packet looks like 72,107,16,69,6,1,x,sum

				double voltage = recvdBytes[6]/200.00;
				dataFile << std::setprecision(2) << std::fixed << voltage;
				returnData = voltage;
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 7:
			//mode 5 PID 0x107
			//if(mode5checkboxstate[7] != BST_UNCHECKED && success == true && placeCounter == 48)
		{
			//mode 5 PID 0x107: Oxygen sensor monitor bank 2 sensor 3 voltage
			byteFile << ',';
			dataFile << ',';
			//mode = 5;
			//pid = 7;

			uint8_t sendBytes[7] = {104,106,241,5,7,1,208};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 8)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 1 byte which indicates oxygen sensor voltage
				//the return packet looks like 72,107,16,69,7,1,x,sum

				double voltage = recvdBytes[6]/200.00;
				dataFile << std::setprecision(2) << std::fixed << voltage;
				returnData = voltage;
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 8:
			//mode 5 PID 0x108
			//if(mode5checkboxstate[8] != BST_UNCHECKED && success == true && placeCounter == 49)
		{
			//mode 5 PID 0x108: Oxygen sensor monitor bank 2 sensor 4 voltage
			byteFile << ',';
			dataFile << ',';
			//mode = 5;
			//pid = 8;

			uint8_t sendBytes[7] = {104,106,241,5,8,1,209};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 8)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 1 byte which indicates oxygen sensor voltage
				//the return packet looks like 72,107,16,69,8,1,x,sum

				double voltage = recvdBytes[6]/200.00;
				dataFile << std::setprecision(2) << std::fixed << voltage;
				returnData = voltage;
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		//end pid switch
		}
		break;

		//******************************************************************************
	case 6:
		//6 PIDs to check for mode 6
		switch(pid)
		{
		case 0:
			//PID 0x0
			//if(mode6checkboxstate[0] != BST_UNCHECKED && success == true && placeCounter == 50)
		{
			//mode 6 PID 0x0: list of valid PIDs 0x0-0x20?
			byteFile << ',';
			dataFile << ',';
			//mode = 6;
			//pid = 0;

			uint8_t sendBytes[6] = {104,106,241,6,0,201};
			writeToSerialPort(serialPortHandle, sendBytes, 6);
			readFromSerialPort(serialPortHandle, recvdBytes,6,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 4 bytes which are (?) binary flags
				//for the next 32 PIDs indicating if they work with this car
				//the return packet looks like 72,107,16,70,0,255,x,x,x,x,sum

				int PIDplace = 0;
				for(int i = 6; i < 10; i++)//loop through bytes
				{
					for(int j = 7; j > -1; j--)
					{
						PIDplace += 1;
						if(((recvdBytes[i] >> j) & 0x1))//shift and mask to check bit flags
						{
							dataFile << PIDplace << ' ';
						}
					}
				}
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 1:
			//PID 0x1
			//if(mode6checkboxstate[1] != BST_UNCHECKED && success == true && placeCounter == 51)
		{
			//mode 6 PID 0x1: ?
			byteFile << ',';
			dataFile << ',';
			//mode = 6;
			//pid = 1;

			uint8_t sendBytes[6] = {104,106,241,6,1,202};
			writeToSerialPort(serialPortHandle, sendBytes, 6);
			readFromSerialPort(serialPortHandle, recvdBytes,6,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 4 bytes which are (?)
				//the return packet looks like 72,107,16,70,1,1,x,x,x,x,sum

				for(int i = 6; i < 10; i++)//loop through bytes
				{
					dataFile << recvdBytes[i] << ' ';
				}
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 2:
			//PID 0x2
			//if(mode6checkboxstate[2] != BST_UNCHECKED && success == true && placeCounter == 52)
		{
			//mode 6 PID 0x2: ?
			byteFile << ',';
			dataFile << ',';
			//mode = 6;
			//pid = 2;

			uint8_t sendBytes[6] = {104,106,241,6,2,203};
			writeToSerialPort(serialPortHandle, sendBytes, 6);
			readFromSerialPort(serialPortHandle, recvdBytes,6,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 4 bytes which are (?)
				//the return packet looks like 72,107,16,70,2,1,x,x,x,x,sum

				for(int i = 6; i < 10; i++)//loop through bytes
				{
					dataFile << recvdBytes[i] << ' ';
				}
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 3:
			//PID 0x3
			//if(mode6checkboxstate[3] != BST_UNCHECKED && success == true && placeCounter == 53)
		{
			//mode 6 PID 0x3: ?
			byteFile << ',';
			dataFile << ',';
			//mode = 6;
			//pid = 3;

			uint8_t sendBytes[6] = {104,106,241,6,3,204};
			writeToSerialPort(serialPortHandle, sendBytes, 6);
			readFromSerialPort(serialPortHandle, recvdBytes,6,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 4 bytes which are (?)
				//the return packet looks like 72,107,16,70,3,1,x,x,x,x,sum

				for(int i = 6; i < 10; i++)//loop through bytes
				{
					dataFile << recvdBytes[i] << ' ';
				}
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 4:
			//PID 0x4
			//if(mode6checkboxstate[4] != BST_UNCHECKED && success == true && placeCounter == 54)
		{
			//mode 6 PID 0x4: ?
			byteFile << ',';
			dataFile << ',';
			//mode = 6;
			//pid = 4;

			uint8_t sendBytes[6] = {104,106,241,6,4,205};
			writeToSerialPort(serialPortHandle, sendBytes, 6);
			readFromSerialPort(serialPortHandle, recvdBytes,6,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 4 bytes which are (?)
				//the return packet looks like 72,107,16,70,4,1,x,x,x,x,sum

				for(int i = 6; i < 10; i++)//loop through bytes
				{
					dataFile << recvdBytes[i] << ' ';
				}
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 5:
			//PID 0x5
			//if(mode6checkboxstate[5] != BST_UNCHECKED && success == true && placeCounter == 55)
		{
			//mode 6 PID 0x5: ?
			byteFile << ',';
			dataFile << ',';
			//mode = 6;
			//pid = 5;

			uint8_t sendBytes[6] = {104,106,241,6,5,206};
			writeToSerialPort(serialPortHandle, sendBytes, 6);
			readFromSerialPort(serialPortHandle, recvdBytes,6,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 4 bytes which are (?)
				//the return packet looks like 72,107,16,70,5,1,x,x,x,x,sum

				for(int i = 6; i < 10; i++)//loop through bytes
				{
					dataFile << recvdBytes[i] << ' ';
				}
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		//end pid switch
		}
		break;

		//*************************************************************************
	case 7:
		//mode 7
		//if(mode7checkboxstate != BST_UNCHECKED && success == true && placeCounter == 56)
	{
		//mode 7: Read pending DTCs
		byteFile << ',';
		dataFile << ',';
		//mode = 7;
		//pid = 0;

		uint8_t sendBytes[5] = {104,106,241,7,202};//message for MODE 0x7 PID 0
		writeToSerialPort(serialPortHandle, sendBytes, 5);
		readFromSerialPort(serialPortHandle, recvdBytes,5,500);//read back what we just sent
		int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,100,1000);
		if(numBytesRead < 11)
		{
			//we've failed the read and should flag to quit the polling
			success = false;
			dataFile << "poll fail";

		}
		else
		{
			//this should return pairs of bytes which indicate DTCs
			//the return packet looks like 72,107,16,71,x,x,x,x,x,x,sum


			if(recvdBytes[4] == 0 && recvdBytes[5] == 0)
			{
				dataFile << "no pending DTC";
			}
			else
			{
				int index = 4;
				for(int i = 0;i < 3; i++)//loop through the bytes to interpret DTCs
				{
					if(!(recvdBytes[i*2+index] == 0 && recvdBytes[i*2+1+index] == 0))
					{
						std::string DTC = decodeDTC(recvdBytes[i*2+index],recvdBytes[i*2+1+index]);
						dataFile << DTC << ' ';
					}
					if(numBytesRead > index+7 && i*2+4+index > index+7)
					{
						i = 0;
						index += 11;
					}
				}
			}
		}

		for(int k = 0; k < numBytesRead; k++)
		{
			byteFile << (int) recvdBytes[k] << ' ';
			recvdBytes[k] = 0;//clear the read buffer for the next set of data
		}
	}
	break;

		//********************************************************************************
	case 8:
		//2 PIDs to check for mode 8
		switch(pid)
		{
		case 0:
			//PID 0x0
			//if(mode8checkboxstate[0] != BST_UNCHECKED && success == true && placeCounter == 57)
		{
			//mode 8 PID 0x0: ?
			byteFile << ',';
			dataFile << ',';
			//mode = 8;
			//pid = 0;

			uint8_t sendBytes[6] = {104,106,241,8,0,203};
			writeToSerialPort(serialPortHandle, sendBytes, 6);
			readFromSerialPort(serialPortHandle, recvdBytes,6,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 4 bytes which are (?)
				//the return packet looks like 72,107,16,72,0,0,x,x,x,x,sum

				for(int i = 6; i < 10; i++)//loop through bytes
				{
					dataFile << recvdBytes[i] << ' ';
				}
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 1:
			//PID 0x1
			//if(mode8checkboxstate[1] != BST_UNCHECKED && success == true && placeCounter == 58)
		{
			//mode 8 PID 0x1: ?
			byteFile << ',';
			dataFile << ',';
			//mode = 8;
			//pid = 1;

			uint8_t sendBytes[6] = {104,106,241,8,1,204};
			writeToSerialPort(serialPortHandle, sendBytes, 6);
			readFromSerialPort(serialPortHandle, recvdBytes,6,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 7)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 4 bytes which are (?)
				//the return packet looks like 72,107,16,72,1,0,0,0,0,0,sum
				// or 72,107,16,127,8,34,sum

				dataFile << "see byte log";
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		//end PID switch
		}
		break;

		//********************************************************************************
	case 9:
		//Mode 0x9
		switch(pid)
		{
		case 0:
		//PID 0x0
		//if(mode9checkboxstate[0] != BST_UNCHECKED && success == true && placeCounter == 59)
		{
			//mode 9 PID 0x0: list of valid PIDs 0x0-0x20?
			byteFile << ',';
			dataFile << ',';
			//mode = 9;
			//pid = 0;

			uint8_t sendBytes[6] = {104,106,241,9,0,204};
			writeToSerialPort(serialPortHandle, sendBytes, 6);
			readFromSerialPort(serialPortHandle, recvdBytes,6,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 4 bytes which are (?) binary flags
				//for the next 32 PIDs indicating if they work with this car
				//the return packet looks like 72,107,16,73,0,1,x,x,x,x,sum

				int PIDplace = 0;
				for(int i = 6; i < 10; i++)//loop through bytes
				{
					for(int j = 7; j > -1; j--)
					{
						PIDplace += 1;
						if(((recvdBytes[i] >> j) & 0x1))//shift and mask to check bit flags
						{
							dataFile << PIDplace << ' ';
						}
					}
				}
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 1:
		//PID 0x1
		//if(mode9checkboxstate[1] != BST_UNCHECKED && success == true && placeCounter == 60)
		{
			//mode 9 PID 0x1: number of packets with VIN data
			byteFile << ',';
			dataFile << ',';
			//mode = 9;
			//pid = 1;

			uint8_t sendBytes[6] = {104,106,241,9,1,205};
			writeToSerialPort(serialPortHandle, sendBytes, 6);
			readFromSerialPort(serialPortHandle, recvdBytes,6,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,7,1000);
			if(numBytesRead < 7)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 1 byte which is the number of packets
				//used to transmit the VIN data
				//the return packet looks like 72,107,16,73,1,5,sum

				dataFile << (int) recvdBytes[5];
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 2:
		//PID 0x2
		//if(mode9checkboxstate[2] != BST_UNCHECKED && success == true && placeCounter == 61)
		{
			//mode 9 PID 0x2: VIN data
			byteFile << ',';
			dataFile << ',';
			//mode = 9;
			//pid = 2;

			uint8_t sendBytes[6] = {104,106,241,9,2,206};
			writeToSerialPort(serialPortHandle, sendBytes, 6);
			readFromSerialPort(serialPortHandle, recvdBytes,6,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,55,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return multiple packets with 4 bytes each
				//used to transmit the VIN data
				//the return packet looks like 72,107,16,73,2,packet no.,X,X,X,X,sum

				int packetPlace = 0;
				for(int i = 0; i < numBytesRead;i++)
				{
					if((i-packetPlace) > 5 && (i-packetPlace) < 10)
					{
						char data = (char) recvdBytes[i];
						if((data >= 48 && data <= 57) | (data >= 65 && data <= 90))//include ascii 0-9 and A-Z
						{
							dataFile << data;
						}
					}
					if((i-packetPlace) == 10)
					{
						packetPlace += 11;
					}
				}
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 3:
		//PID 0x3
		//if(mode9checkboxstate[3] != BST_UNCHECKED && success == true && placeCounter == 62)
		{
			//mode 9 PID 0x3: number of packets used to send calibration ID data
			byteFile << ',';
			dataFile << ',';
			//mode = 9;
			//pid = 3;

			uint8_t sendBytes[6] = {104,106,241,9,3,207};
			writeToSerialPort(serialPortHandle, sendBytes, 6);
			readFromSerialPort(serialPortHandle, recvdBytes,6,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,7,1000);
			if(numBytesRead < 7)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 1 byte which is the number of packets
				//used to transmit the calibration ID data
				//the return packet looks like 72,107,16,73,3,4,sum

				dataFile << (int) recvdBytes[5];
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 4:
		//PID 0x4
		//if(mode9checkboxstate[4] != BST_UNCHECKED && success == true && placeCounter == 63)
		{
			//mode 9 PID 0x4: calibration ID data
			byteFile << ',';
			dataFile << ',';
			//mode = 9;
			//pid = 4;

			uint8_t sendBytes[6] = {104,106,241,9,4,208};
			writeToSerialPort(serialPortHandle, sendBytes, 6);
			readFromSerialPort(serialPortHandle, recvdBytes,6,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,44,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return multiple packets with 4 bytes each
				//used to transmit the calibration ID data
				//the return packet looks like 72,107,16,73,4,packet no.,X,X,X,X,sum

				int packetPlace = 0;
				for(int i = 0; i < numBytesRead;i++)
				{
					if((i-packetPlace) > 5 && (i-packetPlace) < 10)
					{
						uint8_t data = recvdBytes[i];
						if(data >= 32 && data <= 122)//include ascii chars
						{
							dataFile << data;
						}
					}
					else if((i-packetPlace) == 10)
					{
						packetPlace += 11;
					}
				}
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 5:
		//PID 0x5
		//if(mode9checkboxstate[5] != BST_UNCHECKED && success == true && placeCounter == 64)
		{
			//mode 9 PID 0x5: number of packets used to send calibration validation number data
			byteFile << ',';
			dataFile << ',';
			//mode = 9;
			//pid = 5;

			uint8_t sendBytes[6] = {104,106,241,9,5,209};
			writeToSerialPort(serialPortHandle, sendBytes, 6);
			readFromSerialPort(serialPortHandle, recvdBytes,6,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,7,1000);
			if(numBytesRead < 7)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 1 byte which is the number of packets
				//used to transmit the calibration validation number data
				//the return packet looks like 72,107,16,73,5,1,sum

				dataFile << (int) recvdBytes[5];
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 6:
		//PID 0x6
		//if(mode9checkboxstate[6] != BST_UNCHECKED && success == true && placeCounter == 65)
		{
			//mode 9 PID 0x6: calibration validation number data
			byteFile << ',';
			dataFile << ',';
			//mode = 9;
			//pid = 6;

			uint8_t sendBytes[6] = {104,106,241,9,6,210};
			writeToSerialPort(serialPortHandle, sendBytes, 6);
			readFromSerialPort(serialPortHandle, recvdBytes,6,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;
				dataFile << "poll fail";

			}
			else
			{
				//this should return 2 bytes which is the calibration validation number data
				//the return packet looks like 72,107,16,73,6,1,0,0,x,x,sum

				dataFile << recvdBytes[8]*256 + recvdBytes[9];
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		//end pid switch
	}
		break;

		//**************************************************************************************
	case 34:
		//Mode 0x22
		switch(pid)
		{
		case 0:
			//PID 512
			//if(mode22checkboxstate[0] != BST_UNCHECKED && success == true && placeCounter == 66)
		{
			//mode 0x22 PID 512: list of valid PIDs 0x0-0x20?
			//mode = 34;
			//pid = 0;

			uint8_t sendBytes[7] = {104,106,241,34,2,0,231};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//this should return 4 bytes which are (?) binary flags
			//for the next 32 PIDs indicating if they work with this car
			//the return packet looks like 72,107,16,98,2,0,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			int PIDplace = 0;
			for(int i = 6; i < 10; i++)//loop through bytes
			{
				for(int j = 7; j > -1; j--)
				{
					PIDplace += 1;
					if(((recvdBytes[i] >> j) & 0x1))//shift and mask to check bit flags
					{
						dataFile << PIDplace << ' ';
					}
				}
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 1:
			//PID 513
			//if(mode22checkboxstate[1] != BST_UNCHECKED && success == true && placeCounter == 67)
		{
			//mode 0x22 PID 513: ?
			//mode = 34;
			//pid = 1;

			uint8_t sendBytes[7] = {104,106,241,34,2,1,232};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 8)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,1,x,sum
			byteFile << ',';
			dataFile << ',';
			dataFile << recvdBytes[6];
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 2:
			//PID 515
			//if(mode22checkboxstate[2] != BST_UNCHECKED && success == true && placeCounter == 68)
		{
			//mode 0x22 PID 515: ?
			//mode = 34;
			//pid = 2;

			uint8_t sendBytes[7] = {104,106,241,34,2,3,234};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,3,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			dataFile << 256*(256*((256*recvdBytes[6])+recvdBytes[7])+recvdBytes[8])+recvdBytes[9];
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 3:
			//PID 516
			//if(mode22checkboxstate[3] != BST_UNCHECKED && success == true && placeCounter == 69)
		{
			//mode 0x22 PID 516: ?
			//mode = 34;
			//pid = 3;

			uint8_t sendBytes[7] = {104,106,241,34,2,4,235};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 8)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,4,x,sum
			byteFile << ',';
			dataFile << ',';
			dataFile << recvdBytes[6];
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 4:
			//PID 517
			//if(mode22checkboxstate[4] != BST_UNCHECKED && success == true && placeCounter == 70)
		{
			//mode 0x22 PID 517: ?
			//mode = 34;
			//pid = 4;

			uint8_t sendBytes[7] = {104,106,241,34,2,5,236};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,9,1000);
			if(numBytesRead < 9)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,5,x,x,sum
			byteFile << ',';
			dataFile << ',';
			dataFile << 256*recvdBytes[6]+recvdBytes[7];
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 5:
			//PID 518
			//if(mode22checkboxstate[5] != BST_UNCHECKED && success == true && placeCounter == 71)
		{
			//mode 0x22 PID 518: ?
			//mode = 34;
			//pid = 5;

			uint8_t sendBytes[7] = {104,106,241,34,2,6,237};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 8)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,6,x,sum
			byteFile << ',';
			dataFile << ',';
			dataFile << recvdBytes[6];
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 6:
			//PID 519
			//if(mode22checkboxstate[6] != BST_UNCHECKED && success == true && placeCounter == 72)
		{
			//mode 0x22 PID 519: ?
			//mode = 34;
			//pid = 6;

			uint8_t sendBytes[7] = {104,106,241,34,2,7,238};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,9,1000);
			if(numBytesRead < 9)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,7,x,x,sum
			byteFile << ',';
			dataFile << ',';
			dataFile << 256*recvdBytes[6]+recvdBytes[7];
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 7:
			//PID 520
			//if(mode22checkboxstate[7] != BST_UNCHECKED && success == true && placeCounter == 73)
		{
			//mode 0x22 PID 520: ?
			//mode = 34;
			//pid = 7;

			uint8_t sendBytes[7] = {104,106,241,34,2,8,239};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,9,1000);
			if(numBytesRead < 9)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,8,x,x,sum
			byteFile << ',';
			dataFile << ',';
			dataFile << 256*recvdBytes[6]+recvdBytes[7];
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 8:
			//PID 521
			//if(mode22checkboxstate[8] != BST_UNCHECKED && success == true && placeCounter == 74)
		{
			//mode 0x22 PID 521: ?
			//mode = 34;
			//pid = 8;

			uint8_t sendBytes[7] = {104,106,241,34,2,9,240};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 8)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,9,x,sum
			byteFile << ',';
			dataFile << ',';
			dataFile << recvdBytes[6];
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 9:
			//PID 522
			//if(mode22checkboxstate[9] != BST_UNCHECKED && success == true && placeCounter == 75)
		{
			//mode 0x22 PID 522: ?
			//mode = 34;
			//pid = 9;

			uint8_t sendBytes[7] = {104,106,241,34,2,10,241};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,9,1000);
			if(numBytesRead < 9)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,10,x,x,sum
			byteFile << ',';
			dataFile << ',';
			dataFile << 256*recvdBytes[6]+recvdBytes[7];
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 10:
			//PID 523
			//if(mode22checkboxstate[10] != BST_UNCHECKED && success == true && placeCounter == 76)
		{
			//mode 0x22 PID 523: ?
			//mode = 34;
			//pid = 10;

			uint8_t sendBytes[7] = {104,106,241,34,2,11,242};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,9,1000);
			if(numBytesRead < 9)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,11,x,x,sum
			byteFile << ',';
			dataFile << ',';
			dataFile << 256*recvdBytes[6]+recvdBytes[7];
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 11:
			//PID 524
			//if(mode22checkboxstate[11] != BST_UNCHECKED && success == true && placeCounter == 77)
		{
			//mode 0x22 PID 524: ?
			//mode = 34;
			//pid = 11;

			uint8_t sendBytes[7] = {104,106,241,34,2,12,243};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 8)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,12,x,sum
			byteFile << ',';
			dataFile << ',';
			dataFile << recvdBytes[6];
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 12:
			//PID 525
			//if(mode22checkboxstate[12] != BST_UNCHECKED && success == true && placeCounter == 78)
		{
			//mode 0x22 PID 525: ?
			//mode = 34;
			//pid = 12;

			uint8_t sendBytes[7] = {104,106,241,34,2,13,244};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,9,1000);
			if(numBytesRead < 9)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,13,x,x,sum
			byteFile << ',';
			dataFile << ',';
			dataFile << recvdBytes[6]*256+recvdBytes[7];
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 13:
			//PID 526-529
			//if(mode22checkboxstate[13] != BST_UNCHECKED && success == true && placeCounter == 79)
		{
			//mode 0x22 PID 526-529: ECU part number in ASCII
			//mode = 34;
			//pid = 13;

			uint8_t sendBytes[7] = {104,106,241,34,2,14,245};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//return groups of 4 ASCII characters
			//the return packet looks like 72,107,16,98,2,14,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			int packetPlace = 0;
			for(int i = 0; i < numBytesRead;i++)
			{
				if((i-packetPlace) > 5 && (i-packetPlace) < 10)
				{
					uint8_t data = recvdBytes[i];
					if(data >= 32 && data <= 122)//include ascii chars
					{
						dataFile << data;
					}
				}
				else if((i-packetPlace) == 10)
				{
					packetPlace += 11;
				}
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}

			//request next set of bytes- 527
			uint8_t sendBytesSetTwo[7] = {104,106,241,34,2,15,246};
			writeToSerialPort(serialPortHandle, sendBytesSetTwo, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//return groups of 4 ASCII characters
			//the return packet looks like 72,107,16,98,2,15,x,x,x,x,sum
			packetPlace = 0;
			for(int i = 0; i < numBytesRead;i++)
			{
				if((i-packetPlace) > 5 && (i-packetPlace) < 10)
				{
					uint8_t data = recvdBytes[i];
					if(data >= 32 && data <= 122)//include ascii chars
					{
						dataFile << data;
					}
				}
				else if((i-packetPlace) == 10)
				{
					packetPlace += 11;
				}
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}

			//request next set of bytes- 528
			uint8_t sendBytesSetThree[7] = {104,106,241,34,2,16,247};
			writeToSerialPort(serialPortHandle, sendBytesSetThree, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//return groups of 4 ASCII characters
			//the return packet looks like 72,107,16,98,2,16,x,x,x,x,sum
			packetPlace = 0;
			for(int i = 0; i < numBytesRead;i++)
			{
				if((i-packetPlace) > 5 && (i-packetPlace) < 10)
				{
					uint8_t data = recvdBytes[i];
					if(data >= 32 && data <= 122)//include ascii chars
					{
						dataFile << data;
					}
				}
				else if((i-packetPlace) == 10)
				{
					packetPlace += 11;
				}
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}

			//request next set of bytes- 529
			uint8_t sendBytesSetFour[7] = {104,106,241,34,2,17,248};
			writeToSerialPort(serialPortHandle, sendBytesSetFour, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//return groups of 4 ASCII characters
			//the return packet looks like 72,107,16,98,2,17,x,x,x,x,sum
			packetPlace = 0;
			for(int i = 0; i < numBytesRead;i++)
			{
				if((i-packetPlace) > 5 && (i-packetPlace) < 10)
				{
					uint8_t data = recvdBytes[i];
					if(data >= 32 && data <= 122)//include ascii chars
					{
						dataFile << data;
					}
				}
				else if((i-packetPlace) == 10)
				{
					packetPlace += 11;
				}
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 14:
			//PID 530
			//if(mode22checkboxstate[14] != BST_UNCHECKED && success == true && placeCounter == 80)
		{
			//mode 0x22 PID 530: ?
			//mode = 34;
			//pid = 14;

			uint8_t sendBytes[7] = {104,106,241,34,2,18,249};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,9,1000);
			if(numBytesRead < 9)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,18,x,x,sum
			byteFile << ',';
			dataFile << ',';
			dataFile << recvdBytes[6]*256+recvdBytes[7];
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 15:
			//PID 531
			//if(mode22checkboxstate[15] != BST_UNCHECKED && success == true && placeCounter == 81)
		{
			//mode 0x22 PID 531: ?
			//mode = 34;
			//pid = 15;

			uint8_t sendBytes[7] = {104,106,241,34,2,19,250};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,9,1000);
			if(numBytesRead < 9)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,19,x,x,sum
			byteFile << ',';
			dataFile << ',';
			dataFile << recvdBytes[6]*256+recvdBytes[7];
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 16:
			//PID 532
			//if(mode22checkboxstate[16] != BST_UNCHECKED && success == true && placeCounter == 82)
		{
			//mode 0x22 PID 532: ?
			//mode = 34;
			//pid = 16;

			uint8_t sendBytes[7] = {104,106,241,34,2,20,251};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 8)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,20,x,sum
			byteFile << ',';
			dataFile << ',';
			dataFile << recvdBytes[6];
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 17:
			//PID 533
			//if(mode22checkboxstate[17] != BST_UNCHECKED && success == true && placeCounter == 83)
		{
			//mode 0x22 PID 533: ?
			//mode = 34;
			//pid = 17;

			uint8_t sendBytes[7] = {104,106,241,34,2,21,252};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 8)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,21,x,sum
			byteFile << ',';
			dataFile << ',';
			dataFile << recvdBytes[6];
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 18:
			//PID 534
			//if(mode22checkboxstate[18] != BST_UNCHECKED && success == true && placeCounter == 84)
		{
			//mode 0x22 PID 534: ?
			//mode = 34;
			//pid = 18;

			uint8_t sendBytes[7] = {104,106,241,34,2,22,253};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 8)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,22,x,sum
			byteFile << ',';
			dataFile << ',';
			dataFile << recvdBytes[6];
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 19:
			//PID 536
			//if(mode22checkboxstate[19] != BST_UNCHECKED && success == true && placeCounter == 85)
		{
			//mode 0x22 PID 536: ?
			//mode = 34;
			//pid = 19;

			uint8_t sendBytes[7] = {104,106,241,34,2,24,255};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,9,1000);
			if(numBytesRead < 9)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,24,x,x,sum
			byteFile << ',';
			dataFile << ',';
			dataFile << recvdBytes[6]*256+recvdBytes[7];
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 20:
			//PID 537
			//if(mode22checkboxstate[20] != BST_UNCHECKED && success == true && placeCounter == 86)
		{
			//mode 0x22 PID 537: ?
			//mode = 34;
			//pid = 20;

			uint8_t sendBytes[7] = {104,106,241,34,2,25,0};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,9,1000);
			if(numBytesRead < 9)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,25,x,x,sum
			byteFile << ',';
			dataFile << ',';
			dataFile << recvdBytes[6]*256+recvdBytes[7];
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 21:
			//PID 538
			//if(mode22checkboxstate[21] != BST_UNCHECKED && success == true && placeCounter == 87)
		{
			//mode 0x22 PID 538: ?
			//mode = 34;
			//pid = 21;

			uint8_t sendBytes[7] = {104,106,241,34,2,26,1};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,9,1000);
			if(numBytesRead < 9)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,26,x,x,sum
			byteFile << ',';
			dataFile << ',';
			dataFile << recvdBytes[6]*256+recvdBytes[7];
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 22:
			//PID 539
			//if(mode22checkboxstate[22] != BST_UNCHECKED && success == true && placeCounter == 88)
		{
			//mode 0x22 PID 539: ?
			//mode = 34;
			//pid = 22;

			uint8_t sendBytes[7] = {104,106,241,34,2,27,2};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,9,1000);
			if(numBytesRead < 9)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,26,x,x,sum
			byteFile << ',';
			dataFile << ',';
			dataFile << recvdBytes[6]*256+recvdBytes[7];
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 23:
			//PID 540-543
			//if(mode22checkboxstate[23] != BST_UNCHECKED && success == true && placeCounter == 89)
		{
			//mode 0x22 PID 540-543: Calibration ID in ASCII
			//mode = 34;
			//pid = 23;

			uint8_t sendBytes[7] = {104,106,241,34,2,28,3};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//return groups of 4 ASCII characters
			//the return packet looks like 72,107,16,98,2,28,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			int packetPlace = 0;
			for(int i = 0; i < numBytesRead;i++)
			{
				if((i-packetPlace) > 5 && (i-packetPlace) < 10)
				{
					uint8_t data = recvdBytes[i];
					if(data >= 32 && data <= 122)//include ascii chars
					{
						dataFile << data;
					}
				}
				else if((i-packetPlace) == 10)
				{
					packetPlace += 11;
				}
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}

			//request next set of bytes- 541
			uint8_t sendBytesSetTwo[7] = {104,106,241,34,2,29,4};
			writeToSerialPort(serialPortHandle, sendBytesSetTwo, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//return groups of 4 ASCII characters
			//the return packet looks like 72,107,16,98,2,29,x,x,x,x,sum
			packetPlace = 0;
			for(int i = 0; i < numBytesRead;i++)
			{
				if((i-packetPlace) > 5 && (i-packetPlace) < 10)
				{
					uint8_t data = recvdBytes[i];
					if(data >= 32 && data <= 122)//include ascii chars
					{
						dataFile << data;
					}
				}
				else if((i-packetPlace) == 10)
				{
					packetPlace += 11;
				}
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}

			//request next set of bytes- 542
			uint8_t sendBytesSetThree[7] = {104,106,241,34,2,30,5};
			writeToSerialPort(serialPortHandle, sendBytesSetThree, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//return groups of 4 ASCII characters
			//the return packet looks like 72,107,16,98,2,30,x,x,x,x,sum
			packetPlace = 0;
			for(int i = 0; i < numBytesRead;i++)
			{
				if((i-packetPlace) > 5 && (i-packetPlace) < 10)
				{
					uint8_t data = recvdBytes[i];
					if(data >= 32 && data <= 122)//include ascii chars
					{
						dataFile << data;
					}
				}
				else if((i-packetPlace) == 10)
				{
					packetPlace += 11;
				}
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}

			//request next set of bytes- 543
			uint8_t sendBytesSetFour[7] = {104,106,241,34,2,31,6};
			writeToSerialPort(serialPortHandle, sendBytesSetFour, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//return groups of 4 ASCII characters
			//the return packet looks like 72,107,16,98,2,31,x,x,x,x,sum
			packetPlace = 0;
			for(int i = 0; i < numBytesRead;i++)
			{
				if((i-packetPlace) > 5 && (i-packetPlace) < 10)
				{
					uint8_t data = recvdBytes[i];
					if(data >= 32 && data <= 122)//include ascii chars
					{
						dataFile << data;
					}
				}
				else if((i-packetPlace) == 10)
				{
					packetPlace += 11;
				}
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 24:
			//PID 544
			//if(mode22checkboxstate[24] != BST_UNCHECKED && success == true && placeCounter == 90)
		{
			//mode 0x22 PID 544: PIDs available 0x21 - 0x40 ?
			//mode = 34;
			//pid = 24;

			uint8_t sendBytes[7] = {104,106,241,34,2,32,7};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,32,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			int PIDplace = 32;
			for(int i = 6; i < 10; i++)//loop through bytes
			{
				for(int j = 7; j > -1; j--)
				{
					PIDplace += 1;
					if(((recvdBytes[i] >> j) & 0x1))//shift and mask to check bit flags
					{
						dataFile << PIDplace << ' ';
					}
				}
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 25:
			//PIDs 545-548
			//if(mode22checkboxstate[25] != BST_UNCHECKED && success == true && placeCounter == 91)
		{
			//mode 0x22 PID 545-548: Make, model and year in ASCII
			//mode = 34;
			//pid = 25;

			uint8_t sendBytes[7] = {104,106,241,34,2,33,8};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//return groups of 4 ASCII characters
			//the return packet looks like 72,107,16,98,2,33,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			int packetPlace = 0;
			for(int i = 0; i < numBytesRead;i++)
			{
				if((i-packetPlace) > 5 && (i-packetPlace) < 10)
				{
					uint8_t data = recvdBytes[i];
					if(data >= 32 && data <= 122)//include ascii chars
					{
						dataFile << data;
					}
				}
				else if((i-packetPlace) == 10)
				{
					packetPlace += 11;
				}
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}

			//request next set of bytes- 546
			uint8_t sendBytesSetTwo[7] = {104,106,241,34,2,34,9};
			writeToSerialPort(serialPortHandle, sendBytesSetTwo, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//return groups of 4 ASCII characters
			//the return packet looks like 72,107,16,98,2,34,x,x,x,x,sum
			packetPlace = 0;
			for(int i = 0; i < numBytesRead;i++)
			{
				if((i-packetPlace) > 5 && (i-packetPlace) < 10)
				{
					uint8_t data = recvdBytes[i];
					if(data >= 32 && data <= 122)//include ascii chars
					{
						dataFile << data;
					}
				}
				else if((i-packetPlace) == 10)
				{
					packetPlace += 11;
				}
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}

			//request next set of bytes- 547
			uint8_t sendBytesSetThree[7] = {104,106,241,34,2,35,10};
			writeToSerialPort(serialPortHandle, sendBytesSetThree, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//return groups of 4 ASCII characters
			//the return packet looks like 72,107,16,98,2,35,x,x,x,x,sum
			packetPlace = 0;
			for(int i = 0; i < numBytesRead;i++)
			{
				if((i-packetPlace) > 5 && (i-packetPlace) < 10)
				{
					uint8_t data = recvdBytes[i];
					if(data >= 32 && data <= 122)//include ascii chars
					{
						dataFile << data;
					}
				}
				else if((i-packetPlace) == 10)
				{
					packetPlace += 11;
				}
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}

			//request next set of bytes- 548
			uint8_t sendBytesSetFour[7] = {104,106,241,34,2,36,11};
			writeToSerialPort(serialPortHandle, sendBytesSetFour, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//return groups of 4 ASCII characters
			//the return packet looks like 72,107,16,98,2,36,x,x,x,x,sum
			packetPlace = 0;
			for(int i = 0; i < numBytesRead;i++)
			{
				if((i-packetPlace) > 5 && (i-packetPlace) < 10)
				{
					uint8_t data = recvdBytes[i];
					if(data >= 32 && data <= 122)//include ascii chars
					{
						dataFile << data;
					}
				}
				else if((i-packetPlace) == 10)
				{
					packetPlace += 11;
				}
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 26:
			//PID 549
			//if(mode22checkboxstate[26] != BST_UNCHECKED && success == true && placeCounter == 92)
		{
			//mode 0x22 PID 549:  ?
			//mode = 34;
			//pid = 26;

			uint8_t sendBytes[7] = {104,106,241,34,2,37,12};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,37,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 10; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 27:
			//PID 550
			//if(mode22checkboxstate[27] != BST_UNCHECKED && success == true && placeCounter == 93)
		{
			//mode 0x22 PID 550:  ?
			//mode = 34;
			//pid = 27;

			uint8_t sendBytes[7] = {104,106,241,34,2,38,13};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,38,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 10; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 28:
			//PID 551
			//if(mode22checkboxstate[28] != BST_UNCHECKED && success == true && placeCounter == 94)
		{
			//mode 0x22 PID 551:  ?
			//mode = 34;
			//pid = 28;

			uint8_t sendBytes[7] = {104,106,241,34,2,39,14};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,39,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 10; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 29:
			//PID 552
			//if(mode22checkboxstate[29] != BST_UNCHECKED && success == true && placeCounter == 95)
		{
			//mode 0x22 PID 552:  ?
			//mode = 34;
			//pid = 29;

			uint8_t sendBytes[7] = {104,106,241,34,2,40,15};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,40,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 10; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 30:
			//PID 553
			//if(mode22checkboxstate[30] != BST_UNCHECKED && success == true && placeCounter == 96)
		{
			//mode 0x22 PID 552:  ?
			//mode = 34;
			//pid = 30;

			uint8_t sendBytes[7] = {104,106,241,34,2,41,16};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,41,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 10; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 31:
			//PID 554
			//if(mode22checkboxstate[31] != BST_UNCHECKED && success == true && placeCounter == 97)
		{
			//mode 0x22 PID 553:  ?
			//mode = 34;
			//pid = 31;

			uint8_t sendBytes[7] = {104,106,241,34,2,42,17};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,42,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 10; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 32:
			//PID 555
			//if(mode22checkboxstate[32] != BST_UNCHECKED && success == true && placeCounter == 98)
		{
			//mode 0x22 PID 555:  ?
			//mode = 34;
			//pid = 32;

			uint8_t sendBytes[7] = {104,106,241,34,2,43,18};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,43,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 10; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 33:
			//PID 556
			//if(mode22checkboxstate[33] != BST_UNCHECKED && success == true && placeCounter == 99)
		{
			//mode 0x22 PID 556:  ?
			//mode = 34;
			//pid = 33;

			uint8_t sendBytes[7] = {104,106,241,34,2,44,19};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,44,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 10; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 34:
			//PID 557
			//if(mode22checkboxstate[34] != BST_UNCHECKED && success == true && placeCounter == 100)
		{
			//mode 0x22 PID 557:  ?
			//mode = 34;
			//pid = 34;

			uint8_t sendBytes[7] = {104,106,241,34,2,45,20};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,45,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 10; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 35:
			//PID 558
			//if(mode22checkboxstate[35] != BST_UNCHECKED && success == true && placeCounter == 101)
		{
			//mode 0x22 PID 558:  ?
			//mode = 34;
			//pid = 35;

			uint8_t sendBytes[7] = {104,106,241,34,2,46,21};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,46,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 10; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 36:
			//PID 559
			//if(mode22checkboxstate[36] != BST_UNCHECKED && success == true && placeCounter == 102)
		{
			//mode 0x22 PID 559:  ?
			//mode = 34;
			//pid = 36;

			uint8_t sendBytes[7] = {104,106,241,34,2,47,22};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,47,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 10; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 37:
		//PID 560
		//if(mode22checkboxstate[37] != BST_UNCHECKED && success == true && placeCounter == 103)
		{
			//mode 0x22 PID 560:  ?
			//mode = 34;
			//pid = 37;

			uint8_t sendBytes[7] = {104,106,241,34,2,48,23};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,48,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 10; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 38:
			//PID 561
			//if(mode22checkboxstate[38] != BST_UNCHECKED && success == true && placeCounter == 104)
		{
			//mode 0x22 PID 561:  ?
			//mode = 34;
			//pid = 38;

			uint8_t sendBytes[7] = {104,106,241,34,2,49,24};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,49,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 10; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 39:
			//PID 562
			//if(mode22checkboxstate[39] != BST_UNCHECKED && success == true && placeCounter == 105)
		{
			//mode 0x22 PID 562:  ?
			//mode = 34;
			//pid = 39;

			uint8_t sendBytes[7] = {104,106,241,34,2,50,25};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,50,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 10; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 40:
			//PID 563
			//if(mode22checkboxstate[40] != BST_UNCHECKED && success == true && placeCounter == 106)
		{
			//mode 0x22 PID 563:  ?
			//mode = 34;
			//pid = 40;

			uint8_t sendBytes[7] = {104,106,241,34,2,51,26};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,51,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 10; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 41:
			//PID 564
			//if(mode22checkboxstate[41] != BST_UNCHECKED && success == true && placeCounter == 107)
		{
			//mode 0x22 PID 564:  ?
			//mode = 34;
			//pid = 41;

			uint8_t sendBytes[7] = {104,106,241,34,2,52,27};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,52,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 10; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 42:
			//PID 565
			//if(mode22checkboxstate[42] != BST_UNCHECKED && success == true && placeCounter == 108)
		{
			//mode 0x22 PID 565:  ?
			//mode = 34;
			//pid = 42;

			uint8_t sendBytes[7] = {104,106,241,34,2,53,28};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,53,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 10; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 43:
			//PID 566
			//if(mode22checkboxstate[43] != BST_UNCHECKED && success == true && placeCounter == 109)
		{
			//mode 0x22 PID 566:  ?
			//mode = 34;
			//pid = 43;

			uint8_t sendBytes[7] = {104,106,241,34,2,54,29};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,54,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 10; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 44:
			//PID 567
			//if(mode22checkboxstate[44] != BST_UNCHECKED && success == true && placeCounter == 110)
		{
			//mode 0x22 PID 567:  ?
			//mode = 34;
			//pid = 44;

			uint8_t sendBytes[7] = {104,106,241,34,2,55,30};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,55,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 10; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 45:
			//PID 568
			//if(mode22checkboxstate[45] != BST_UNCHECKED && success == true && placeCounter == 111)
		{
			//mode 0x22 PID 568:  ?
			//mode = 34;
			//pid = 45;

			uint8_t sendBytes[7] = {104,106,241,34,2,56,31};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,56,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 10; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 46:
			//PID 569
			//if(mode22checkboxstate[46] != BST_UNCHECKED && success == true && placeCounter == 112)
		{
			//mode 0x22 PID 569:  ?
			//mode = 34;
			//pid = 46;

			uint8_t sendBytes[7] = {104,106,241,34,2,57,32};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,57,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 10; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 47:
			//PID 570
			//if(mode22checkboxstate[47] != BST_UNCHECKED && success == true && placeCounter == 113)
		{
			//mode 0x22 PID 570:  ?
			//mode = 34;
			//pid = 47;

			uint8_t sendBytes[7] = {104,106,241,34,2,58,33};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,58,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 10; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 48:
			//PID 571
			//if(mode22checkboxstate[48] != BST_UNCHECKED && success == true && placeCounter == 114)
		{
			//mode 0x22 PID 571:  ?
			//mode = 34;
			//pid = 48;

			uint8_t sendBytes[7] = {104,106,241,34,2,59,34};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,59,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 10; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 49:
			//PID 572
			//if(mode22checkboxstate[49] != BST_UNCHECKED && success == true && placeCounter == 115)
		{
			//mode 0x22 PID 572:  ?
			//mode = 34;
			//pid = 49;

			uint8_t sendBytes[7] = {104,106,241,34,2,60,35};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,60,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 10; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 50:
			//PID 573
			//if(mode22checkboxstate[50] != BST_UNCHECKED && success == true && placeCounter == 116)
		{
			//mode 0x22 PID 573: PIDs available 0x41 - 0x60 ?
			//mode = 34;
			//pid = 50;

			uint8_t sendBytes[7] = {104,106,241,34,2,61,36};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,61,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			int PIDplace = 64;
			for(int i = 6; i < 10; i++)//loop through bytes
			{
				for(int j = 7; j > -1; j--)
				{
					PIDplace += 1;
					if(((recvdBytes[i] >> j) & 0x1))//shift and mask to check bit flags
					{
						dataFile << PIDplace << ' ';
					}
				}
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 51:
			//PID 577
			//if(mode22checkboxstate[51] != BST_UNCHECKED && success == true && placeCounter == 117)
		{
			//mode 0x22 PID 577:  ?
			//mode = 34;
			//pid = 51;

			uint8_t sendBytes[7] = {104,106,241,34,2,65,40};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,65,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 10; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 52:
			//PID 583
			//if(mode22checkboxstate[52] != BST_UNCHECKED && success == true && placeCounter == 118)
		{
			//mode 0x22 PID 583:  ?
			//mode = 34;
			//pid = 52;

			uint8_t sendBytes[7] = {104,106,241,34,2,71,46};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,71,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 10; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 53:
			//PID 584
			//if(mode22checkboxstate[53] != BST_UNCHECKED && success == true && placeCounter == 119)
		{
			//mode 0x22 PID 584:  ?
			//mode = 34;
			//pid = 53;

			uint8_t sendBytes[7] = {104,106,241,34,2,72,47};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,72,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 10; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 54:
			//PID 585
			//if(mode22checkboxstate[54] != BST_UNCHECKED && success == true && placeCounter == 120)
		{
			//mode 0x22 PID 585:  ?
			//mode = 34;
			//pid = 54;

			uint8_t sendBytes[7] = {104,106,241,34,2,73,48};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,73,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 10; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 55:
			//PID 586
			//if(mode22checkboxstate[55] != BST_UNCHECKED && success == true && placeCounter == 121)
		{
			//mode 0x22 PID 586:  ?
			//mode = 34;
			//pid = 55;

			uint8_t sendBytes[7] = {104,106,241,34,2,74,49};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,9,1000);
			if(numBytesRead < 9)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,77,x,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 8; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 56:
			//PID 591
			//if(mode22checkboxstate[56] != BST_UNCHECKED && success == true && placeCounter == 122)
		{
			//mode 0x22 PID 591:  ?
			//mode = 34;
			//pid = 56;

			uint8_t sendBytes[7] = {104,106,241,34,2,79,54};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,9,1000);
			if(numBytesRead < 9)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,79,x,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 8; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 57:
			//PID 592
			//if(mode22checkboxstate[57] != BST_UNCHECKED && success == true && placeCounter == 123)
		{
			//mode 0x22 PID 592:  ?
			//mode = 34;
			//pid = 57;

			uint8_t sendBytes[7] = {104,106,241,34,2,80,55};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,9,1000);
			if(numBytesRead < 9)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,80,x,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 8; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 58:
			//PID 593
			//if(mode22checkboxstate[58] != BST_UNCHECKED && success == true && placeCounter == 124)
		{
			//mode 0x22 PID 593:  ?
			//mode = 34;
			//pid = 58;

			uint8_t sendBytes[7] = {104,106,241,34,2,81,56};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,9,1000);
			if(numBytesRead < 9)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,81,x,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 8; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 59:
			//PID 594
			//if(mode22checkboxstate[59] != BST_UNCHECKED && success == true && placeCounter == 125)
		{
			//mode 0x22 PID 594:  ?
			//mode = 34;
			//pid = 59;

			uint8_t sendBytes[7] = {104,106,241,34,2,82,57};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,9,1000);
			if(numBytesRead < 9)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,82,x,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 8; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 60:
			//PID 595
			//if(mode22checkboxstate[60] != BST_UNCHECKED && success == true && placeCounter == 126)
		{
			//mode 0x22 PID 595:  ?
			//mode = 34;
			//pid = 60;

			uint8_t sendBytes[7] = {104,106,241,34,2,83,58};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 8)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,83,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 7; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 61:
			//PID 596
			//if(mode22checkboxstate[61] != BST_UNCHECKED && success == true && placeCounter == 127)
		{
			//mode 0x22 PID 596:  ?
			//mode = 34;
			//pid = 61;

			uint8_t sendBytes[7] = {104,106,241,34,2,84,59};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,84,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 10; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 62:
			//PID 598
			//if(mode22checkboxstate[62] != BST_UNCHECKED && success == true && placeCounter == 128)
		{
			//mode 0x22 PID 598:  ?
			//mode = 34;
			//pid = 62;

			uint8_t sendBytes[7] = {104,106,241,34,2,86,61};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 8)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,86,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 7; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 63:
			//PID 599
			//if(mode22checkboxstate[63] != BST_UNCHECKED && success == true && placeCounter == 129)
		{
			//mode 0x22 PID 599:  ?
			//mode = 34;
			//pid = 63;

			uint8_t sendBytes[7] = {104,106,241,34,2,87,62};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,87,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 10; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 64:
			//PID 601
			//if(mode22checkboxstate[64] != BST_UNCHECKED && success == true && placeCounter == 130)
		{
			//mode 0x22 PID 601:  ?
			//mode = 34;
			//pid = 64;

			uint8_t sendBytes[7] = {104,106,241,34,2,89,64};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 8)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,89,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 7; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 65:
			//PID 602
			//if(mode22checkboxstate[65] != BST_UNCHECKED && success == true && placeCounter == 131)
		{
			//mode 0x22 PID 602:  ?
			//mode = 34;
			//pid = 65;

			uint8_t sendBytes[7] = {104,106,241,34,2,90,65};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,90,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 10; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 66:
			//PID 603
			//if(mode22checkboxstate[66] != BST_UNCHECKED && success == true && placeCounter == 132)
		{
			//mode 0x22 PID 603:  ?
			//mode = 34;
			//pid = 66;

			uint8_t sendBytes[7] = {104,106,241,34,2,91,66};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 8)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,91,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 7; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 67:
			//PID 605
			//if(mode22checkboxstate[67] != BST_UNCHECKED && success == true && placeCounter == 133)
		{
			//mode 0x22 PID 605:  ?
			//mode = 34;
			//pid = 67;

			uint8_t sendBytes[7] = {104,106,241,34,2,93,68};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,93,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 10; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 68:
			//PID 606
			//if(mode22checkboxstate[68] != BST_UNCHECKED && success == true && placeCounter == 134)
		{
			//mode 0x22 PID 606:  ?
			//mode = 34;
			//pid = 68;

			uint8_t sendBytes[7] = {104,106,241,34,2,94,69};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 8)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,94,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 7; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 69:
			//PID 608
			//if(mode22checkboxstate[69] != BST_UNCHECKED && success == true && placeCounter == 135)
		{
			//mode 0x22 PID 608:  ?
			//mode = 34;
			//pid = 69;

			uint8_t sendBytes[7] = {104,106,241,34,2,96,71};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,96,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			int PIDplace = 96;
			for(int i = 6; i < 10; i++)//loop through bytes
			{
				for(int j = 7; j > -1; j--)
				{
					PIDplace += 1;
					if(((recvdBytes[i] >> j) & 0x1))//shift and mask to check bit flags
					{
						dataFile << PIDplace << ' ';
					}
				}
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 70:
			//PID 609
			//if(mode22checkboxstate[70] != BST_UNCHECKED && success == true && placeCounter == 136)
		{
			//mode 0x22 PID 609:  ?
			//mode = 34;
			//pid = 70;

			uint8_t sendBytes[7] = {104,106,241,34,2,97,72};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,97,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 10; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 71:
			//PID 610
			//if(mode22checkboxstate[71] != BST_UNCHECKED && success == true && placeCounter == 137)
		{
			//mode 0x22 PID 610:  ?
			//mode = 34;
			//pid = 71;

			uint8_t sendBytes[7] = {104,106,241,34,2,98,73};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 8)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,98,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 7; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 72:
			//PID 612
			//if(mode22checkboxstate[72] != BST_UNCHECKED && success == true && placeCounter == 138)
		{
			//mode 0x22 PID 612:  ?
			//mode = 34;
			//pid = 72;

			uint8_t sendBytes[7] = {104,106,241,34,2,100,75};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 8)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,100,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 7; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 73:
			//PID 613
			//if(mode22checkboxstate[73] != BST_UNCHECKED && success == true && placeCounter == 139)
		{
			//mode 0x22 PID 613:  ?
			//mode = 34;
			//pid = 73;

			uint8_t sendBytes[7] = {104,106,241,34,2,101,76};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 8)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,101,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 7; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 74:
			//PID 614
			//if(mode22checkboxstate[74] != BST_UNCHECKED && success == true && placeCounter == 140)
		{
			//mode 0x22 PID 614:  ?
			//mode = 34;
			//pid = 74;

			uint8_t sendBytes[7] = {104,106,241,34,2,102,77};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 8)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,102,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 7; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 75:
			//PID 615
			//if(mode22checkboxstate[75] != BST_UNCHECKED && success == true && placeCounter == 141)
		{
			//mode 0x22 PID 615:  ?
			//mode = 34;
			//pid = 75;

			uint8_t sendBytes[7] = {104,106,241,34,2,103,78};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 8)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,103,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 7; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 76:
			//PID 616
			//if(mode22checkboxstate[76] != BST_UNCHECKED && success == true && placeCounter == 142)
		{
			//mode 0x22 PID 616:  ?
			//mode = 34;
			//pid = 76;

			uint8_t sendBytes[7] = {104,106,241,34,2,104,79};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 8)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,104,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 7; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 77:
			//PID 617
			//if(mode22checkboxstate[77] != BST_UNCHECKED && success == true && placeCounter == 143)
		{
			//mode 0x22 PID 617:  ?
			//mode = 34;
			//pid = 77;

			uint8_t sendBytes[7] = {104,106,241,34,2,105,80};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 8)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,105,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 7; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 78:
			//PID 618
			//if(mode22checkboxstate[78] != BST_UNCHECKED && success == true && placeCounter == 144)
		{
			//mode 0x22 PID 618:  ?
			//mode = 34;
			//pid = 78;

			uint8_t sendBytes[7] = {104,106,241,34,2,106,81};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 8)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,106,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 7; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 79:
			//PID 619
			//if(mode22checkboxstate[79] != BST_UNCHECKED && success == true && placeCounter == 145)
		{
			//mode 0x22 PID 619:  ?
			//mode = 34;
			//pid = 79;

			uint8_t sendBytes[7] = {104,106,241,34,2,107,82};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 8)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,107,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 7; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 80:
			//PID 620
			//if(mode22checkboxstate[80] != BST_UNCHECKED && success == true && placeCounter == 146)
		{
			//mode 0x22 PID 620:  ?
			//mode = 34;
			//pid = 80;

			uint8_t sendBytes[7] = {104,106,241,34,2,108,83};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,108,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 10; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 81:
			//PID 621
			//if(mode22checkboxstate[81] != BST_UNCHECKED && success == true && placeCounter == 147)
		{
			//mode 0x22 PID 621:  ?
			//mode = 34;
			//pid = 81;

			uint8_t sendBytes[7] = {104,106,241,34,2,109,84};
			writeToSerialPort(serialPortHandle, sendBytes, 7);
			readFromSerialPort(serialPortHandle, recvdBytes,7,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,9,1000);
			if(numBytesRead < 9)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//
			//the return packet looks like 72,107,16,98,2,109,x,x,sum
			byteFile << ',';
			dataFile << ',';
			for(int i = 6; i < 8; i++)//loop through bytes
			{
				dataFile << recvdBytes[i] << ' ';
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		//end pid switch
		}
		break;

		//**************************************************************************************
	case 47:
		//Mode 0x2F
		switch(pid)
		{
		case 0:
			//PID 0x100
			//if(mode2Fcheckboxstate[0] != BST_UNCHECKED && success == true && placeCounter == 148)
		{
			//mode 0x2F PID 256: list of valid PIDs 0x0-0x20?
			//mode = 47;
			//pid = 0;

			uint8_t sendBytes[8] = {104,106,241,47,1,0,0,243};
			writeToSerialPort(serialPortHandle, sendBytes, 8);
			readFromSerialPort(serialPortHandle, recvdBytes,8,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//this should return 4 bytes which are (?) binary flags
			//for the next 32 PIDs indicating if they work with this car
			//the return packet looks like 72,107,16,111,1,0,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			int PIDplace = 256;
			for(int i = 6; i < 10; i++)//loop through bytes
			{
				for(int j = 7; j > -1; j--)
				{
					PIDplace += 1;
					if(((recvdBytes[i] >> j) & 0x1))//shift and mask to check bit flags
					{
						dataFile << PIDplace << ' ';
					}
				}
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 1:
			//PID 0x101
			//if(mode2Fcheckboxstate[1] != BST_UNCHECKED && success == true && placeCounter == 149)
		{
			//mode 0x2F PID 257: control TPU function 6
			//mode = 47;
			//pid = 1;

			uint8_t sendBytes[8] = {104,106,241,47,1,1,0,244};
			writeToSerialPort(serialPortHandle, sendBytes, 8);
			readFromSerialPort(serialPortHandle, recvdBytes,8,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 6)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//the return packet looks like 72,107,16,111,1,1,x,sum
			byteFile << ',';
			dataFile << ',';
			dataFile << "TPU function 6";

			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 2:
			//PID 0x102
			//if(mode2Fcheckboxstate[2] != BST_UNCHECKED && success == true && placeCounter == 150)
		{
			//mode 0x2F PID 258: control TPU function 7
			//mode = 47;
			//pid = 2;

			uint8_t sendBytes[8] = {104,106,241,47,1,2,0,245};
			writeToSerialPort(serialPortHandle, sendBytes, 8);
			readFromSerialPort(serialPortHandle, recvdBytes,8,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 6)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//the return packet looks like 72,107,16,111,1,2,x,sum
			byteFile << ',';
			dataFile << ',';
			dataFile << "TPU function 7";

			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 3:
			//PID 0x103
			//if(mode2Fcheckboxstate[3] != BST_UNCHECKED && success == true && placeCounter == 151)
		{
			//mode 0x2F PID 259: control TPU function 8
			//mode = 47;
			//pid = 3;

			uint8_t sendBytes[8] = {104,106,241,47,1,3,0,246};
			writeToSerialPort(serialPortHandle, sendBytes, 8);
			readFromSerialPort(serialPortHandle, recvdBytes,8,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 6)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//the return packet looks like 72,107,16,111,1,3,x,sum
			byteFile << ',';
			dataFile << ',';
			dataFile << "TPU function 8";

			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 4:
			//PID 0x104
			//if(mode2Fcheckboxstate[4] != BST_UNCHECKED && success == true && placeCounter == 152)
		{
			//mode 0x2F PID 260: control TPU function 9
			//mode = 47;
			//pid = 4;

			uint8_t sendBytes[8] = {104,106,241,47,1,4,0,247};
			writeToSerialPort(serialPortHandle, sendBytes, 8);
			readFromSerialPort(serialPortHandle, recvdBytes,8,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 6)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//the return packet looks like 72,107,16,111,1,4,x,sum
			byteFile << ',';
			dataFile << ',';
			dataFile << "TPU function 8";

			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 5:
			//PID 0x120
			//if(mode2Fcheckboxstate[5] != BST_UNCHECKED && success == true && placeCounter == 153)
		{
			//mode 0x2F PID 288: list of valid PIDs 0x121-0x140?
			//mode = 47;
			//pid = 5;

			uint8_t sendBytes[8] = {104,106,241,47,1,32,0,19};
			writeToSerialPort(serialPortHandle, sendBytes, 8);
			readFromSerialPort(serialPortHandle, recvdBytes,8,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//this should return 4 bytes which are (?) binary flags
			//for the next 32 PIDs indicating if they work with this car
			//the return packet looks like 72,107,16,111,1,32,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			int PIDplace = 288;
			for(int i = 6; i < 10; i++)//loop through bytes
			{
				for(int j = 7; j > -1; j--)
				{
					PIDplace += 1;
					if(((recvdBytes[i] >> j) & 0x1))//shift and mask to check bit flags
					{
						dataFile << PIDplace << ' ';
					}
				}
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 6:
			//PID 0x121
			//if(mode2Fcheckboxstate[6] != BST_UNCHECKED && success == true && placeCounter == 154)
		{
			//mode 0x2F PID 289: ?
			//mode = 47;
			//pid = 6;

			uint8_t sendBytes[8] = {104,106,241,47,1,33,0,20};
			writeToSerialPort(serialPortHandle, sendBytes, 8);
			readFromSerialPort(serialPortHandle, recvdBytes,8,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 6)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//the return packet looks like 72,107,16,111,1,33,0,sum
			byteFile << ',';
			dataFile << ',';
			dataFile << "X";

			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 7:
			//PID 0x122
			//if(mode2Fcheckboxstate[7] != BST_UNCHECKED && success == true && placeCounter == 155)
		{
			//mode 0x2F PID 290: ?
			//mode = 47;
			//pid = 7;

			uint8_t sendBytes[8] = {104,106,241,47,1,34,0,21};
			writeToSerialPort(serialPortHandle, sendBytes, 8);
			readFromSerialPort(serialPortHandle, recvdBytes,8,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 6)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//the return packet looks like 72,107,16,111,1,34,0,sum
			byteFile << ',';
			dataFile << ',';
			dataFile << "X";

			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 8:
			//PID 0x125
			//if(mode2Fcheckboxstate[8] != BST_UNCHECKED && success == true && placeCounter == 156)
		{
			//mode 0x2F PID 292: ?
			//mode = 47;
			//pid = 8;

			uint8_t sendBytes[8] = {104,106,241,47,1,36,0,23};
			writeToSerialPort(serialPortHandle, sendBytes, 8);
			readFromSerialPort(serialPortHandle, recvdBytes,8,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 6)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//the return packet looks like 72,107,16,111,1,36,0,sum
			byteFile << ',';
			dataFile << ',';
			dataFile << "X";

			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 9:
			//PID 0x126
			//if(mode2Fcheckboxstate[9] != BST_UNCHECKED && success == true && placeCounter == 157)
		{
			//mode 0x2F PID 293: ?
			//mode = 47;
			//pid = 9;

			uint8_t sendBytes[8] = {104,106,241,47,1,37,0,24};
			writeToSerialPort(serialPortHandle, sendBytes, 8);
			readFromSerialPort(serialPortHandle, recvdBytes,8,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 6)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//the return packet looks like 72,107,16,111,1,37,0,sum
			byteFile << ',';
			dataFile << ',';
			dataFile << "X";

			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 10:
			//PID 0x127
			//if(mode2Fcheckboxstate[10] != BST_UNCHECKED && success == true && placeCounter == 158)
		{
			//mode 0x2F PID 294: ?
			//mode = 47;
			//pid = 10;

			uint8_t sendBytes[8] = {104,106,241,47,1,38,0,25};
			writeToSerialPort(serialPortHandle, sendBytes, 8);
			readFromSerialPort(serialPortHandle, recvdBytes,8,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 6)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//the return packet looks like 72,107,16,111,1,38,0,sum
			byteFile << ',';
			dataFile << ',';
			dataFile << "X";

			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 11:
			//PID 0x12A
			//if(mode2Fcheckboxstate[11] != BST_UNCHECKED && success == true && placeCounter == 159)
		{
			//mode 0x2F PID 297: ?
			//mode = 47;
			//pid = 11;

			uint8_t sendBytes[8] = {104,106,241,47,1,41,0,28};
			writeToSerialPort(serialPortHandle, sendBytes, 8);
			readFromSerialPort(serialPortHandle, recvdBytes,8,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 6)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//the return packet looks like 72,107,16,111,1,41,0,sum
			byteFile << ',';
			dataFile << ',';
			dataFile << "X";

			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 12:
			//PID 0x140
			//if(mode2Fcheckboxstate[12] != BST_UNCHECKED && success == true && placeCounter == 160)
		{
			//mode 0x2F PID 320: list of valid PIDs 0x141-0x160?
			//mode = 47;
			//pid = 12;

			uint8_t sendBytes[8] = {104,106,241,47,1,64,0,51};
			writeToSerialPort(serialPortHandle, sendBytes, 8);
			readFromSerialPort(serialPortHandle, recvdBytes,8,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//this should return 4 bytes which are (?) binary flags
			//for the next 32 PIDs indicating if they work with this car
			//the return packet looks like 72,107,16,111,1,32,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			int PIDplace = 320;
			for(int i = 6; i < 10; i++)//loop through bytes
			{
				for(int j = 7; j > -1; j--)
				{
					PIDplace += 1;
					if(((recvdBytes[i] >> j) & 0x1))//shift and mask to check bit flags
					{
						dataFile << PIDplace << ' ';
					}
				}
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 13:
			//PID 0x141
			//if(mode2Fcheckboxstate[13] != BST_UNCHECKED && success == true && placeCounter == 161)
		{
			//mode 0x2F PID 321: ?
			//mode = 47;
			//pid = 13;

			uint8_t sendBytes[8] = {104,106,241,47,1,65,0,52};
			writeToSerialPort(serialPortHandle, sendBytes, 8);
			readFromSerialPort(serialPortHandle, recvdBytes,8,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 6)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//the return packet looks like 72,107,16,111,1,65,0,sum
			byteFile << ',';
			dataFile << ',';
			dataFile << "X";

			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 14:
			//PID 0x142
			//if(mode2Fcheckboxstate[14] != BST_UNCHECKED && success == true && placeCounter == 162)
		{
			//mode 0x2F PID 322: ?
			//mode = 47;
			//pid = 14;

			uint8_t sendBytes[8] = {104,106,241,47,1,66,0,53};
			writeToSerialPort(serialPortHandle, sendBytes, 8);
			readFromSerialPort(serialPortHandle, recvdBytes,8,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 6)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//the return packet looks like 72,107,16,111,1,66,0,sum
			byteFile << ',';
			dataFile << ',';
			dataFile << "X";

			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 47:
		//PID 0x143
		//if(mode2Fcheckboxstate[15] != BST_UNCHECKED && success == true && placeCounter == 163)
		{
			//mode 0x2F PID 323: ?
			//mode = 47;
			//pid = 15;

			uint8_t sendBytes[8] = {104,106,241,47,1,67,0,54};
			writeToSerialPort(serialPortHandle, sendBytes, 8);
			readFromSerialPort(serialPortHandle, recvdBytes,8,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 6)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//the return packet looks like 72,107,16,111,1,67,0,sum
			byteFile << ',';
			dataFile << ',';
			dataFile << "X";

			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 16:
			//PID 0x144
			//if(mode2Fcheckboxstate[16] != BST_UNCHECKED && success == true && placeCounter == 164)
		{
			//mode 0x2F PID 324: ?
			//mode = 47;
			//pid = 16;

			uint8_t sendBytes[8] = {104,106,241,47,1,68,0,54};
			writeToSerialPort(serialPortHandle, sendBytes, 8);
			readFromSerialPort(serialPortHandle, recvdBytes,8,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 6)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//the return packet looks like 72,107,16,111,1,68,0,sum
			byteFile << ',';
			dataFile << ',';
			dataFile << "X";

			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 17:
			//PID 0x146
			//if(mode2Fcheckboxstate[17] != BST_UNCHECKED && success == true && placeCounter == 165)
		{
			//mode 0x2F PID 326: ?
			//mode = 47;
			//pid = 17;

			uint8_t sendBytes[8] = {104,106,241,47,1,70,0,56};
			writeToSerialPort(serialPortHandle, sendBytes, 8);
			readFromSerialPort(serialPortHandle, recvdBytes,8,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 6)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//the return packet looks like 72,107,16,111,1,70,0,sum
			byteFile << ',';
			dataFile << ',';
			dataFile << "X";

			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 18:
		//PID 0x147
		//if(mode2Fcheckboxstate[18] != BST_UNCHECKED && success == true && placeCounter == 166)
		{
			//mode 0x2F PID 327: ?
			//mode = 47;
			//pid = 18;

			uint8_t sendBytes[8] = {104,106,241,47,1,71,0,57};
			writeToSerialPort(serialPortHandle, sendBytes, 8);
			readFromSerialPort(serialPortHandle, recvdBytes,8,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 6)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//the return packet looks like 72,107,16,111,1,71,0,sum
			byteFile << ',';
			dataFile << ',';
			dataFile << "X";

			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 19:
			//PID 0x148
			//if(mode2Fcheckboxstate[19] != BST_UNCHECKED && success == true && placeCounter == 167)
		{
			//mode 0x2F PID 328: ?
			//mode = 47;
			//pid = 19;

			uint8_t sendBytes[8] = {104,106,241,47,1,72,0,58};
			writeToSerialPort(serialPortHandle, sendBytes, 8);
			readFromSerialPort(serialPortHandle, recvdBytes,8,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 6)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//the return packet looks like 72,107,16,111,1,72,0,sum
			byteFile << ',';
			dataFile << ',';
			dataFile << "X";

			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 20:
		//PID 0x149
		//if(mode2Fcheckboxstate[20] != BST_UNCHECKED && success == true && placeCounter == 168)
		{
			//mode 0x2F PID 329: ?
			//mode = 47;
			//pid = 20;

			uint8_t sendBytes[8] = {104,106,241,47,1,73,0,59};
			writeToSerialPort(serialPortHandle, sendBytes, 8);
			readFromSerialPort(serialPortHandle, recvdBytes,8,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 6)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//the return packet looks like 72,107,16,111,1,73,0,sum
			byteFile << ',';
			dataFile << ',';
			dataFile << "X";

			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 21:
			//PID 0x14A
			//if(mode2Fcheckboxstate[21] != BST_UNCHECKED && success == true && placeCounter == 169)
		{
			//mode 0x2F PID 330: ?
			//mode = 47;
			//pid = 21;

			uint8_t sendBytes[8] = {104,106,241,47,1,74,0,60};
			writeToSerialPort(serialPortHandle, sendBytes, 8);
			readFromSerialPort(serialPortHandle, recvdBytes,8,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 6)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//the return packet looks like 72,107,16,111,1,74,0,sum
			byteFile << ',';
			dataFile << ',';
			dataFile << "X";

			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 22:
			//PID 0x14B
			//if(mode2Fcheckboxstate[22] != BST_UNCHECKED && success == true && placeCounter == 170)
		{
			//mode 0x2F PID 331: ?
			//mode = 47;
			//pid = 22;

			uint8_t sendBytes[8] = {104,106,241,47,1,75,0,61};
			writeToSerialPort(serialPortHandle, sendBytes, 8);
			readFromSerialPort(serialPortHandle, recvdBytes,8,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 6)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//the return packet looks like 72,107,16,111,1,75,0,sum
			byteFile << ',';
			dataFile << ',';
			dataFile << "X";

			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 23:
			//PID 0x160
			//if(mode2Fcheckboxstate[23] != BST_UNCHECKED && success == true && placeCounter == 171)
		{
			//mode 0x2F PID 352: list of valid PIDs 0x161-0x180?
			//mode = 47;
			//pid = 23;

			uint8_t sendBytes[8] = {104,106,241,47,1,96,0,83};
			writeToSerialPort(serialPortHandle, sendBytes, 8);
			readFromSerialPort(serialPortHandle, recvdBytes,8,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,1000);
			if(numBytesRead < 11)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//this should return 4 bytes which are (?) binary flags
			//for the next 32 PIDs indicating if they work with this car
			//the return packet looks like 72,107,16,111,1,96,x,x,x,x,sum
			byteFile << ',';
			dataFile << ',';
			int PIDplace = 352;
			for(int i = 6; i < 10; i++)//loop through bytes
			{
				for(int j = 7; j > -1; j--)
				{
					PIDplace += 1;
					if(((recvdBytes[i] >> j) & 0x1))//shift and mask to check bit flags
					{
						dataFile << PIDplace << ' ';
					}
				}
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 24:
			//PID 0x161
			//if(mode2Fcheckboxstate[24] != BST_UNCHECKED && success == true && placeCounter == 172)
		{
			//mode 0x2F PID 353: control TPU function 1
			//mode = 47;
			//pid = 24;

			uint8_t sendBytes[8] = {104,106,241,47,1,97,0,84};
			writeToSerialPort(serialPortHandle, sendBytes, 8);
			readFromSerialPort(serialPortHandle, recvdBytes,8,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 6)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//the return packet looks like 72,107,16,111,1,97,x,sum
			byteFile << ',';
			dataFile << ',';
			dataFile << "TPU function 1";

			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 25:
			//PID 0x162
			//if(mode2Fcheckboxstate[25] != BST_UNCHECKED && success == true && placeCounter == 173)
		{
			//mode 0x2F PID 354: control TPU function 2
			//mode = 47;
			//pid = 25;

			uint8_t sendBytes[8] = {104,106,241,47,1,98,0,85};
			writeToSerialPort(serialPortHandle, sendBytes, 8);
			readFromSerialPort(serialPortHandle, recvdBytes,8,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 6)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//the return packet looks like 72,107,16,111,1,98,x,sum
			byteFile << ',';
			dataFile << ',';
			dataFile << "TPU function 2";

			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 26:
			//PID 0x163
			//if(mode2Fcheckboxstate[26] != BST_UNCHECKED && success == true && placeCounter == 174)
		{
			//mode 0x2F PID 355: control TPU function 3
			//mode = 47;
			//pid = 26;

			uint8_t sendBytes[8] = {104,106,241,47,1,99,0,86};
			writeToSerialPort(serialPortHandle, sendBytes, 8);
			readFromSerialPort(serialPortHandle, recvdBytes,8,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 6)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//the return packet looks like 72,107,16,111,1,99,x,sum
			byteFile << ',';
			dataFile << ',';
			dataFile << "TPU function 3";

			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		break;

		case 27:
			//PID 0x164
			//if(mode2Fcheckboxstate[27] != BST_UNCHECKED && success == true && placeCounter == 175)
		{
			//mode 0x2F PID 356: control TPU function 4
			//mode = 47;
			//pid = 27;

			uint8_t sendBytes[8] = {104,106,241,47,1,100,0,87};
			writeToSerialPort(serialPortHandle, sendBytes, 8);
			readFromSerialPort(serialPortHandle, recvdBytes,8,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,1000);
			if(numBytesRead < 6)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//the return packet looks like 72,107,16,111,1,100,x,sum
			byteFile << ',';
			dataFile << ',';
			dataFile << "TPU function 4";

			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		//end pid switch for mode 47
		}

		break;

		//**************************************************************************************
		case 59:
			//Mode 0x3B
			//if(mode3Bcheckboxstate != BST_UNCHECKED && success == true && placeCounter == 176)
		{
			//mode 3B: re-write VIN, only using the available characters: //SCCPC111-5H------
			//mode = 59;
			//pid = 0;

			uint8_t VIN_one = (uint8_t) outGoingData[0];
			uint8_t VIN_two = (uint8_t) outGoingData[1];
			uint8_t VIN_three = (uint8_t) outGoingData[2];
			uint8_t VIN_four = (uint8_t) outGoingData[3];
			uint8_t VIN_five = (uint8_t) outGoingData[4];
			uint8_t sendBytes[11] = {104,106,241,59,1,VIN_one,VIN_two,VIN_three,VIN_four,VIN_five,0};
			//then compute the checksum
			sendBytes[10] = checkSum(sendBytes,11);
			/*for(int a = 0; a < 11; a++)
		{
			std::cout << (int) sendBytes[a] << ' ';
		}
		std::cout << '\n';*/
			writeToSerialPort(serialPortHandle, sendBytes, 11);
			readFromSerialPort(serialPortHandle, recvdBytes,11,500);//read back what we just sent
			int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,6,1000);
			if(numBytesRead < 6)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//this should return an acknowledge packet
			//the return packet looks like 72,107,16,123,1,sum
			byteFile << ',';
			dataFile << ',';
			if(success)
			{
				dataFile << "recv'd first 5 chars ";
			}
			else
			{
				dataFile << "did not recv first 5 chars ";
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}

			//then send the last two chars
			uint8_t VIN_six = (uint8_t) outGoingData[5];
			uint8_t VIN_seven = (uint8_t) outGoingData[6];
			uint8_t sendBytesForVin[11] = {104,106,241,59,2,VIN_six,VIN_seven,0,0,0,0};
			//then compute the checksum
			sendBytesForVin[10] = checkSum(sendBytesForVin,11);
			/*for(int a = 0; a < 11; a++)
		{
			std::cout << (int) sendBytesForVin[a] << ' ';
		}
		std::cout << '\n';*/
			writeToSerialPort(serialPortHandle, sendBytesForVin, 11);
			readFromSerialPort(serialPortHandle, recvdBytes,11,500);//read back what we just sent
			numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,6,1000);
			if(numBytesRead < 6)
			{
				//we've failed the read and should flag to quit the polling
				success = false;


			}
			//this should return an acknowledge packet
			//the return packet looks like 72,107,16,123,1,sum

			if(success)
			{
				dataFile << "recv'd last 2 chars";
			}
			else
			{
				dataFile << "did not recv last 2 chars";
			}
			for(int k = 0; k < numBytesRead; k++)
			{
				byteFile << (int) recvdBytes[k] << ' ';
				recvdBytes[k] = 0;//clear the read buffer for the next set of data
			}
		}
		//end OBD mode switch statement
	}

    return success;//will be false if we had a timeout occur
}



std::string decodeDTC(uint8_t firstByte, uint8_t secondByte)
{
	std::string DTC = "";
	//loop through the pairs of bytes to interpret DTCs
	int letterCode = ((firstByte >> 7) & 0x1)*2 + ((firstByte >> 6) & 0x1);//will be 0,1,2, or 3
	if(letterCode == 0)
	{
		DTC.push_back('P');
	}
	else if(letterCode == 1)
	{
		DTC.push_back('C');
	}
	else if(letterCode == 2)
	{
		DTC.push_back('B');
	}
	else if(letterCode == 3)
	{
		DTC.push_back('U');
	}
	int firstDecimal = ((firstByte >> 5) & 0x1)*2 + ((firstByte >> 4) & 0x1);
	int secondDecimal = ((firstByte >> 3) & 0x1)*8 + ((firstByte >> 2) & 0x1)*4 + ((firstByte >> 1) & 0x1)*2 + ((firstByte >> 0) & 0x1);
	int thirdDecimal = ((secondByte >> 7) & 0x1)*8 + ((secondByte >> 6) & 0x1)*4 + ((secondByte >> 5) & 0x1)*2 + ((secondByte >> 4) & 0x1);
	int fourthDecimal = ((secondByte >> 3) & 0x1)*8 + ((secondByte >> 2) & 0x1)*4 + ((secondByte >> 1) & 0x1)*2 + ((secondByte >> 0) & 0x1);

	//itoa does not work with c++11
	//char stringByte [14];
	/*std::string convertedDecimal = itoa(firstDecimal,stringByte,10);
	DTC.append(convertedDecimal);
	convertedDecimal = itoa(secondDecimal,stringByte,10);
	DTC.append(convertedDecimal);
	convertedDecimal = itoa(thirdDecimal,stringByte,10);
	DTC.append(convertedDecimal);
	convertedDecimal = itoa(fourthDecimal,stringByte,10);
	DTC.append(convertedDecimal);*/

	//I only anticipate characters from 0-9
	char temp;
	temp = (char) (firstDecimal + 48);
	DTC.push_back(temp);
	temp = (char) (secondDecimal + 48);
	DTC.push_back(temp);
	temp = (char) (thirdDecimal + 48);
	DTC.push_back(temp);
	temp = (char) (fourthDecimal + 48);
	DTC.push_back(temp);
	return DTC;
}


int chars_to_int(char* number)
{
	//works only with an array of length 5
	int integerNumber = 0;
	int multiplier = 1;
	//int length = sizeof(number);
	//std::cout << length << ' ';
	for(int x = 4; x > -1; x--)
	{
		int charToNum = 0;
		//std::cout << number[x] -48 << ' ';
		if(number[x] >= 48 && number[x] <= 57)
		{
			charToNum = number[x] - 48;
			//std::cout << charToNum << ' ';
			integerNumber += multiplier*charToNum;
			multiplier = multiplier*10;
		}
		else
		{
			integerNumber = 0;
			multiplier = 1;
		}
	}
	//std::cout << '\n';
	return integerNumber;
}


HANDLE setupComPort(char* baudRateStore, TCHAR* ListItem)
{
	//a handle for the data type which holds COM port settings
	DCB dcb;

	//open the COM port
	HANDLE Port = CreateFile(ListItem,GENERIC_READ | GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);

	//set up the COM port settings
	int baudRateInteger =  chars_to_int(baudRateStore);//numbers below 190 do not work
	//std::cout << baudRateInteger << '\n';

	//prepare the dcb data to configure the COM port
	dcb.DCBlength = sizeof(DCB);//the size of the dbc file
	GetCommState(Port, &dcb);//read in the current settings in order to modify them
	dcb.BaudRate = baudRateInteger;//  baud rate
	dcb.ByteSize = 8;              //  data size, xmit and rcv
	dcb.Parity   = NOPARITY;       //  parity bit
	dcb.StopBits = ONESTOPBIT;     //  stop bit
	SetCommState(Port, &dcb);//set the COM port with these options
	//one start bit, 8 data bits, one stop bit gives 10 bits per frame, just as the lotus is configured

	//set com port timeouts too
	COMMTIMEOUTS timeouts = { 0 };
	timeouts.ReadIntervalTimeout         = 50; // in milliseconds
	timeouts.ReadTotalTimeoutConstant    = 50; // in milliseconds
	timeouts.ReadTotalTimeoutMultiplier  = 10; // in milliseconds
	timeouts.WriteTotalTimeoutConstant   = 50; // in milliseconds
	timeouts.WriteTotalTimeoutMultiplier = 10; // in milliseconds
	SetCommTimeouts(Port,&timeouts);

	return Port;
}


HANDLE sendOBDInit(HANDLE serialPortHandle, TCHAR* ListItem, bool &initStatus, int &failedInits)
{
    //read in and save the serial port settings
    DCB dcb;
    COMMTIMEOUTS timeouts = { 0 };
    GetCommState(serialPortHandle, &dcb);//read in the current com config settings in order to save them
    GetCommTimeouts(serialPortHandle,&timeouts);//read in the current com timeout settings to save them

    //close the serial port which we need to access with another
    //API
    CloseHandle(serialPortHandle);

    //first, we'll send the '0x33' byte at an emulated 5 baud rate
    //we do this using FTDI's D2XX API
    FT_HANDLE ftdiHandle;//handle for the FTDI device
    FT_STATUS ftStatus;//store the status of each D2XX command
    DWORD numDevs;

    //first, read in the list of FTDI devices connected
    //into a set of arrays.
    char *BufPtrs[3];   // pointer to array of 3 pointers
    char Buffer1[64];      // buffer for description of first device
    char Buffer2[64];      // buffer for description of second device

    // initialize the array of pointers to hold the device names
    BufPtrs[0] = Buffer1;
    BufPtrs[1] = Buffer2;
    BufPtrs[2] = NULL;      // last entry should be NULL

    ftStatus = FT_ListDevices(BufPtrs,&numDevs,FT_LIST_ALL|FT_OPEN_BY_DESCRIPTION);//poll for the device names

    //print out the read in device names
    /*for(int i = 0; i < 64; i++)
    {
    	std::cout<< Buffer1[i];
    }
    std::cout << '\n';
    for(int i = 0; i < 64; i++)
    {
    	std::cout<< Buffer2[i];
    }
    std::cout << '\n';
    std::cout <<numDevs<< '\n';*/

    ftStatus = FT_OpenEx(Buffer1,FT_OPEN_BY_DESCRIPTION,&ftdiHandle);//initialize the FTDI device
    ftStatus = FT_SetBitMode(ftdiHandle, 0xFF, FT_BITMODE_RESET);//reset all FTDI pins
    ftStatus = FT_SetBitMode(ftdiHandle, 0x1, FT_BITMODE_SYNC_BITBANG);//set TxD pin as output in the bit bang operating mode
    ftStatus = FT_SetBaudRate(ftdiHandle, FT_BAUD_115200);//set the baud rate at which the pins can be updated
    ftStatus = FT_SetDataCharacteristics(ftdiHandle,FT_BITS_8,FT_STOP_BITS_1,FT_PARITY_NONE);//set up the com port parameters
    ftStatus = FT_SetTimeouts(ftdiHandle,200,100);//read timeout of 200ms, write timeout of 100 ms


    //0x33 is 0b00110011, include a stop and start bit
    //include a stop and start bit and we have our emulated byte
    uint8_t fakeBytes[10] = {0x0,0x1,0x1,0x0,0x0,0x1,0x1,0x0,0x0,0x1};//array of bits to send
    uint8_t fakeByte[1];//a byte to send (only the 0th bit matters, though)
    DWORD Written = 0;
    for(int i = 0; i < 10; i++)//loop through and send the 5 baud init '0x33' byte manually bit-by-bit
    {
    	Sleep(200);//delay to emulate a 5 bit/s rate
    	fakeByte[0] = fakeBytes[i];//update the bit to send from the array
    	ftStatus = FT_Write(ftdiHandle,fakeByte,1,&Written);//send one bit at a time
    }


    ftStatus = FT_SetBitMode(ftdiHandle, 0xFF, FT_BITMODE_RESET);//reset all FTDI pins or else the device stays in bit bang mode
    FT_Close(ftdiHandle);//close the FTDI device

    //re-open the serial port which is used for commuincation
    serialPortHandle = CreateFile(ListItem,GENERIC_READ | GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    SetCommState(serialPortHandle, &dcb);//restore the COM port config
	SetCommTimeouts(serialPortHandle,&timeouts);//restore the COM port timeouts

	uint8_t recvdBytes[3] = {0,0,0};//a buffer which can be used to store read in bytes
	readFromSerialPort(serialPortHandle, recvdBytes,3,500);//read in 3 bytes in up to 300 ms

	//invert third byte of serial response and send it back
	uint8_t toSend[1];
	toSend[0]= recvdBytes[2]^255;//invert the 2nd key byte
	//toSend[0]= 0x33;//this is a byte to send for troubleshooting
	Sleep(30);//according to the protocol, wait 25-50 ms
	writeToSerialPort(serialPortHandle, toSend, 1);//ack the received bytes

	//read in one more byte, which should be the inverse of 0x33 (51) => 0xCC (204)
	uint8_t lastByte[2] = {0,0};
	//lastByte[0] = 0;//a buffer which can be used to store read in bytes
	readFromSerialPort(serialPortHandle, lastByte,2,500);//read in 2 bytes

	bool initFailed = false;
	if(recvdBytes[0] != 0x55 || lastByte[1] != 0xCC)//the first response should be 0x55 (dec 85), the last response should be 0xCC (dec 204)
	{
		initFailed = true;
		failedInits += 1;//increment our 'attempts at init' counter only when we fail
		if(failedInits < 3)
		{
			serialPortHandle = sendOBDInit(serialPortHandle, ListItem, initStatus, failedInits);//retry the init
		}
		else
		{
			//std::cout << "init failed" << '\n';
			initStatus = false;//pass out an indication that the init attempts all failed
		}
	}


	if(ftStatus != FT_OK)
	{
		//std::cout << "FTDI error resetting" << '\n';
	}


	//print out what we read in
	if(initFailed == false)
	{

		std::cout << "read: ";
		for(int i = 0; i < 3; i++)
		{
			std::cout << (int) recvdBytes[i] << ",";
		}
		std::cout << (int) lastByte[1] << '\n';
	}

	return serialPortHandle;
}


uint8_t checkSum(uint8_t* bytes, int number)
{
	uint8_t sum = 0;
	for(int i = 0; i < number; i++)
	{
		sum += bytes[i];
	}
	return sum;
}
