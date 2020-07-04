#include <Unknwn.h> //for com port
#include <windows.h> //for external device and window/file creation
#include <iostream>
#include <fstream>
#include <string>
#include <stdlib.h>
#include <stdint.h>
#include "ftd2xx.h"//included to directly drive the FTDI com port device
using namespace std;


void createReflashPacket(string inputString, string reflashRanges, int encodeByteSet, bool clearMemorySectors);

string decToHex(uint8_t decimal);

uint32_t interpretAddressRange(string addressRange, uint8_t* addressArray);

string hexString_to_decimal(string hex);

long long stringDec_to_int(string number);

void testReflashPacket(int encodeByteSet);

uint8_t getByteFromLine(string line);

HANDLE initializeReflashMode(HANDLE serialPortHandle, TCHAR* ListItem, bool &success);

string srecReader(string filename, long address, long &lastFilePosition);

uint8_t readCodedMessageIntoBuffer(long &lastPosition, uint8_t* byteBuffer);

void calculateCheckBytes(uint8_t *packet, uint16_t packetLength);

uint16_t calculateCRCduringReflash(uint16_t currentCRC, uint8_t *packet, uint16_t packetLength);

HANDLE initializeStageIReflashMode(HANDLE serialPortHandle, TCHAR* ListItem, bool &success);

uint16_t buildStageIbootloaderPacket(uint8_t *packetBuffer, uint16_t bufferLength, uint8_t numBytesToSend, uint8_t numPacketsSent, string filename, long address, long &lastFilePosition);

uint16_t stageIbootloaderRomFlashPacketBuilder(uint32_t &addressToFlash, long &bytesLeftToSend, uint16_t CRC, uint32_t &packetsSent, string filename, long &filePos);

uint16_t readPacketLineToBuffer(uint8_t *buffer, uint16_t bufferLength, long &lastFilePosition);

void buildSREC(uint32_t address, string HEXdata);

string decimal_to_hexString(uint32_t number);
