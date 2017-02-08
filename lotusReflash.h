#include <iostream>
#include <fstream>
#include <string>
#include <stdlib.h>
#include <stdint.h>
using namespace std;


void createReflashPacket();

void convert_s_record(ifstream &srec);

string decToHex(uint8_t decimal);

uint32_t interpretAddressRange(string addressRange, uint8_t* addressArray);

string decimalVersionReader(long address, long &lastFilePosition);

string makeAddress(long addressNumber, int addressLength);

string hexString_to_decimal(string hex);

long long stringDec_to_int(string number);

