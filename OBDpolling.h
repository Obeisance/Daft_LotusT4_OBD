#include <string> //for string manipulation
#include <Unknwn.h> //for com port
#include <windows.h> //for external device and window/file creation
#include <fstream> //for text file read
#include <iostream> //for cout
#include <iomanip> //in order to set cout precision
#include <stdint.h> //allow uint9_t type
#include "ftd2xx.h"//included to directly drive the FTDI com port device

//input a handle to a serial port, a pointer to an integer array and
//the number of bytes to send over that serial port
void writeToSerialPort(HANDLE serialPortHandle, uint8_t* bytesToSend, int numberOfBytesToSend);

//input a handle to a serial port, a pointer to an integer array and
//the number of bytes to receive as well as a timeout and receive
//the number of bytes which were correctly received
int readFromSerialPort(HANDLE serialPortHandle, uint8_t* serialBytesRecvd, int sizeOfSerialByteBuffer, int delayMS);

//output is a boolean flag which indicates success of serial data transaction
//input a handle to the serial port over which to communicate
//input two file handles to save raw data bytes and interpreted data
//input the OBD mode and PID as well as a reference double which returns with
//the serial data
bool serialPollingCycle(HANDLE serialPortHandle,std::ofstream &dataFile, std::ofstream &byteFile, int mode, int pid, uint8_t* outGoingData, double &returnData);//perform the serial message exchange of a given mode and PID

//input two data bytes and receive in return
//a decoded Diagnostic Trouble Code
std::string decodeDTC(uint8_t firstByte, uint8_t secondByte);//decodes a set of bytes into a DTC

//input a pointer to a 5 char array
//return the integer representation of that number
int chars_to_int(char* number);//converts a char array to an integer

//input a pointer to 5 char array which contains
//the desired baud rate for serial communication
//as well as a TCHAR listing the COM port
//receive a handle to an initiated COM port for
//serial communication
HANDLE setupComPort(char* baudRateStore, TCHAR* ListItem);//setup the windows com port for communication with car

//input a serial port handle, a TCHAR with the COM port
//and a reference to a boolean success state flag and
//a reference to a failed init attempt counter
//receive the serial port handle in return
HANDLE sendOBDInit(HANDLE serialPortHandle, TCHAR* ListItem, bool &initStatus, int &failedInits);//sends 5 baud 0x33 and other bytes to initialize ISO-9141 comms

//calculate the checksum of a number of bytes
uint8_t checkSum(uint8_t* bytes, int number);
