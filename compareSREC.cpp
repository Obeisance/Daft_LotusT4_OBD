//============================================================================
// Name        : compareSREC.cpp
// Author      : Obeisance
// Version     :
// Copyright   : Free to use for all
// Description : Hello World in C++, Ansi-style
//============================================================================
#include <iostream>
#include <fstream>
#include <string>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
using namespace std;

string srecReader(string filename, long address, long &lastFilePosition);
uint8_t srecReader(string filename, long address, long &lastFilePosition, bool unused);
string decimal_to_hexString(uint32_t number);
string hexString_to_decimal(string hex);
long long stringDec_to_int(string number);

int main() {
	cout << "Compare S record files" << endl; // prints Convert to S record
	ofstream outputFile("mismatchedAddresses.txt",ios::trunc);
	outputFile.close();
	string fileName1 = "2017-06-06 Split 0x2E at 0x913e StageII bootloader srec.txt";
	string fileName2 = "2017-06-14 T4 StageII bootloader with read fcn.txt";
	long file1Pos = 0;
	long file2Pos = 0;

	long startAddress = 0x8000;
	long endAddress = 0x10000;
	while(startAddress < endAddress)
	{
		string file1Byte = srecReader(fileName1, startAddress, file1Pos);
		string file2Byte = srecReader(fileName2, startAddress, file2Pos);
		if(file1Byte != file2Byte)
		{
			string misAddr = decimal_to_hexString(startAddress);
			outputFile.open("mismatchedAddresses.txt",ios::app);
			outputFile << misAddr << " file 1: " << file1Byte << " file 2: " << file2Byte << '\n';
			outputFile.close();
		}
		startAddress += 1;
	}


	return 0;
}

string srecReader(string filename, long address, long &lastFilePosition)
{
	//input address, return value at address or "NAN" if address not allowed
	ifstream myfile(filename.c_str());
	string value = "NAN";
	string line;
	long lineAddress = 0;
	//long localAddress = 0;
	bool finished = false;
	bool getNextLine = true;
	int lineSize = 0;

	//continue if the file is open
	if (myfile.is_open())
	{
		myfile.seekg(lastFilePosition);
		//as long as we have lines to read in, continue
		while (finished == false)
		{
			int dataStartPoint = 0;
			int addressByteLength = 0;
			//if we need to read in a new line, do so
			if(getNextLine == true)
			{
				//collect the position before reading in the line
				//this position is the beginning of the next line
				lastFilePosition += lineSize;//update the position based on the previously read in line
				//if no new line is read in, we are at the end of the file
				if(!getline(myfile,line))
				{
					//set to exit the loop
					finished = true;
				}

				//cout << line << '\n';
				getNextLine = false;
				lineSize = line.length() + 2;//2 extra chars for the /r/n keeps us line aligned
				lineAddress = 0;
			}

			//interpret the line
			//if the first character is not S or s, skip the line
			if(line[0] != 's' && line[0] != 'S')
			{
				getNextLine = true;
			}
			else
			{
				//figure out which type of s record we have
				//each type dictates a different address length
				//so also extract the address
				string lineAddressHexString = "";
				if(line[1] == '1')//2 address bytes
				{
					addressByteLength = 2;
					lineAddressHexString = line.substr(4,4);
					dataStartPoint = 8;
				}
				else if(line[1] == '2')//3 byte address
				{
					addressByteLength = 3;
					lineAddressHexString = line.substr(4,6);
					dataStartPoint = 10;
				}
				else if(line[1] == '3')//4 byte address
				{
					addressByteLength = 4;
					lineAddressHexString = line.substr(4,8);
					dataStartPoint = 12;//string index
				}
				else
				{
					getNextLine = true;
				}

				if(!getNextLine)
				{
					//convert the hex address to a decimal address
					string lineAddressDecString = hexString_to_decimal(lineAddressHexString);
					lineAddress = stringDec_to_int(lineAddressDecString);

					string byteCountHexString = line.substr(2,2);
					string byteCountDecString = hexString_to_decimal(byteCountHexString);
					int bytesInLine = stringDec_to_int(byteCountDecString);
					bytesInLine -= (addressByteLength+1);

					if(lineAddress + bytesInLine <= address)
					{
						getNextLine = true;//the line we are on does not contain the data we want
					}
					else if(lineAddress > address)
					{
						//the data we want is at a lower address than the line we're on,
						//so begin a dangerous recursive process
						lastFilePosition = 0;//search the file from the beginning
						value = srecReader(filename, address, lastFilePosition);
						getNextLine = true;
						finished = true;
					}
					/*else
					{
						cout << lineAddressHexString << " which is: " << lineAddress << " which contains: " << bytesInLine << " bytes, but we're looking for: " << address << '\n';
					}*/
				}
			}

			//now that we believe that the data we want is on the line we're on
			//collect that data and return it as a decimal string
			if(getNextLine == false)
			{
				int dataPlace = address - lineAddress;
				//cout << line.size() << ' ' << dataStartPoint << ' ' << dataPlace << '\n';
				string hexData = line.substr(dataStartPoint+dataPlace*2,2);
				value = hexString_to_decimal(hexData);
				finished = true;
			}
		}
	}

	myfile.close();
	return value;

	/*
	 * long filePosition = 0;
	string data = srecReader("6-16-06 ECU Read", 0x1000F, filePosition);
	cout << "filePosition: " << filePosition << ", data: " << data << '\n';
	 */
}

uint8_t srecReader(string filename, long address, long &lastFilePosition, bool unused)
{
	string hexString = srecReader(filename, address, lastFilePosition);
	string decString = hexString_to_decimal(hexString);
	uint8_t decimalByte = stringDec_to_int(decString);
	return decimalByte;
}

string decimal_to_hexString(uint32_t number)
{
	//input a base10 number in a string, output a string with the equivalent hex number
	string hexNumber = "";
    char stringByte [12];//temporarily stores the output number

	//convert to hex
	hexNumber = itoa(number,stringByte,16);
	for(int i = 0; i < hexNumber.length(); i++)
	{
		if(hexNumber[i] == 'a' || hexNumber[i] == 'b' || hexNumber[i] == 'c' || hexNumber[i] == 'd' || hexNumber[i] == 'e' || hexNumber[i] == 'f')
		{
			hexNumber[i] -= 32;//switch to upper case
		}
	}
	if(hexNumber.length() == 1)
	{
		string tempString = "0";
		tempString.append(hexNumber);
		hexNumber = tempString;
	}

	return hexNumber;
}

string hexString_to_decimal(string hex)
{
	//input a string value hex value, convert to decimal string
	string decimal = "";
	int hexConvert[16] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
	int hexConvertlower[16] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};

	//take the hex number and get an int in decimal
	int stringLength = hex.length();
	long long number = 0;
	long long multiplier = 1;
	for(int a = stringLength - 1; a >= 0 ; a--)
	{
		for(int b = 0; b < 16; b++)
		{
			if(hex[a] == hexConvert[b] || hex[a] == hexConvertlower[b])
			{
				number += multiplier*b;
			}
		}
		multiplier = multiplier*16;
		//cout << "number: " << number << '\n';
	}

	//cout << "number: " << number << '\n';

	//take the decimal number and convert it to a string
	int temp = 0;
	long long divide = number;
	while(divide > 0)
	{
		long long preDivide = divide;
		divide = divide/10;
		temp = preDivide - divide*10;
		string tempString = decimal;
		decimal = "";
		decimal.push_back(hexConvert[temp]);
		decimal.append(tempString);
	}

	if(number == 0)
	{
		decimal = '0';
	}

	//cout << "hex: " << hex << ", number " << number << ", decimal "<< decimal << '\n';
	return decimal;
}


long long stringDec_to_int(string number)
{
	//input an integer in a string, convert to a number
	int size = number.length();
	long long integer = 0;
	long long multiple = 1;
	for(int i = size - 1; i >= 0; i--)
	{
		integer += multiple*(number[i] - 48);
		multiple = multiple*10;
	}
	return integer;
}
