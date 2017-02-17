#include <Unknwn.h> //for com port
#include <windows.h> //for external device and window/file creation
#include <iostream>
#include <fstream>
#include <string>
#include <stdlib.h>
#include <stdint.h>
#include "ftd2xx.h"//included to directly drive the FTDI com port device
using namespace std;


void createReflashPacket(string inputString, string reflashRanges, int encodeByteSet);

string decToHex(uint8_t decimal);

uint32_t interpretAddressRange(string addressRange, uint8_t* addressArray);

string hexString_to_decimal(string hex);

long long stringDec_to_int(string number);

void testReflashPacket(int encodeByteSet);

uint8_t getByteFromLine(string line);

HANDLE initializeReflashMode(HANDLE serialPortHandle, TCHAR* ListItem, bool &success);

string srecReader(string filename, long address, long &lastFilePosition);

uint8_t readCodedMessageIntoBuffer(long &lastPosition, uint8_t* byteBuffer);
