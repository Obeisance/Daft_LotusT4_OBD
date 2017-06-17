//============================================================================
// Name        : insertIntoS_record.cpp
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
string hexString_to_decimal(string hex);
long long stringDec_to_int(string number);
bool stringSearch(string searchTerm, string searchString);
bool addressTypeSearch(string searchString);
string getDelimitedSubString(string inputString, int delimiterOffset, char delimiter);
string getDelimitedSubString(string inputString, int delimiterOffset);
string getHexInstruction(string line);
void buildSREC(uint32_t address, string HEXdata);
string decimal_to_hexString(uint32_t number);

int main() {
	//input file names for the s-record to modify, for the disassembled code
	//that is used as a reference and for the file that contains options
	//identifiers

	string inputSrecFileName = "";
	cout << "-----------------------------------------------" << '\n';
	cout << "Welcome to the Daft MC68k Binary Splitting Tool" << '\n';
	cout << "-----------------------------------------------" << '\n' << '\n';
	cout << " This program reads in a disassembly code file " << '\n';
	cout << "   and a file which indicates interpretation   " << '\n';
	cout << " options in order to produce a modified binary " << '\n';
	cout << "s record file. Jump and branch instructions are" << '\n';
	cout << " read in and their landing points are adjusted " << '\n';
	cout << " in the output file.                           " << '\n' << '\n';
	//cout << "Input name of original s record file:" << '\n';
	//cout << ">>:";
	//getline(cin,inputSrecFileName);
	cout << "Input name of disassembled code file:" << '\n';
	cout << ">>:";
	string disassemblyCodeFileName = "";
	getline(cin,disassemblyCodeFileName);
	cout << "Input name of file with options ID's:" << '\n';
	cout << ">>:";
	string optionsFileName = "";
	getline(cin,optionsFileName);
	//ifstream s_record(inputSrecFileName.c_str());
	ifstream assemblyCode(disassemblyCodeFileName.c_str());
	ifstream optionsFile(optionsFileName.c_str());
	ofstream newSREC("shifted assembly.txt",ios::trunc);
	newSREC.close();

	//first, lets read in the options file to find
	//the address range to write to the modified srec
	//and the address to insert bytes at as well as the
	//special displacement options line
	uint32_t startAddress = 0;
	uint32_t endAddress = 0;
	uint32_t insertAddress = 0;
	uint32_t numBytesToInsert = 0;
	uint32_t addressToShift = 0;
	uint32_t shiftBy = 0;
	if(optionsFile.is_open())
	{
		string fileline = "";
		while(getline(optionsFile,fileline))
		{
			//read in the options, line by line
			if(stringSearch("Address Range", fileline))
			{
				string addrString = getDelimitedSubString(fileline, 2);
				string hexAddr = addrString.substr(2);
				string decAddr = hexString_to_decimal(hexAddr);
				startAddress = stringDec_to_int(decAddr);

				addrString = getDelimitedSubString(fileline, 3);
				hexAddr = addrString.substr(2);
				decAddr = hexString_to_decimal(hexAddr);
				endAddress = stringDec_to_int(decAddr);
			}
			else if(stringSearch("Insert at", fileline))
			{
				string addrString = getDelimitedSubString(fileline, 2);
				string hexAddr = addrString.substr(2);
				string decAddr = hexString_to_decimal(hexAddr);
				insertAddress = stringDec_to_int(decAddr);
			}
			else if(stringSearch("Insert bytes", fileline))
			{
				string addrString = getDelimitedSubString(fileline, 2);
				string hexAddr = addrString.substr(2);
				string decAddr = hexString_to_decimal(hexAddr);
				numBytesToInsert = stringDec_to_int(decAddr);
			}
			else if(stringSearch("Address to shift", fileline))
			{
				string addrString = getDelimitedSubString(fileline, 3);
				string hexAddr = addrString.substr(2);
				string decAddr = hexString_to_decimal(hexAddr);
				addressToShift = stringDec_to_int(decAddr);
			}
			else if(stringSearch("Shift by", fileline))
			{
				string addrString = getDelimitedSubString(fileline, 2);
				string hexAddr = addrString.substr(2);
				string decAddr = hexString_to_decimal(hexAddr);
				shiftBy = stringDec_to_int(decAddr);
			}
		}
		//cout << "address: " <<(int) startAddress << " to " <<(int) endAddress << " insert " << (int) numBytesToInsert << " at " << (int) insertAddress << " shift " << (int) shiftBy << " after " << (int) addressToShift << '\n';
	}
	optionsFile.close();

	//now that we've read in the options, it is time to scan through the assembly file until
	//we find the address we are looking for
	long assemblyFilePosition = 0;
	uint32_t assemblyFileAddress = 0;
	if(assemblyCode.is_open())
	{
		string fileline = "";
		while(getline(assemblyCode,fileline))
		{
			if(fileline[0] == '0' && fileline.length() >= 8)
			{
				string lineHexAddress = fileline.substr(2,6);
				string decAddr = hexString_to_decimal(lineHexAddress);
				assemblyFileAddress = stringDec_to_int(decAddr);
			}

			if(assemblyFileAddress >= startAddress)
			{
				//cout << fileline << '\n';
				break;
			}
			else
			{
				assemblyFilePosition += fileline.length() + 2;
			}
		}
	}

	//now that we've scanned the assembly file until we reached the address
	//of interest, it's time to process data.
	//look for LEA, PEA, JSR and BSR commands while copying data from the
	//original srec file
	if(assemblyCode.is_open())
	{
		assemblyCode.seekg(assemblyFilePosition);
		string fileline = "";
		string dataForSrec = "";
		uint32_t SRECaddress = startAddress;
		while(getline(assemblyCode,fileline))
		{
			if(fileline.length() > 0)
			{
				if(fileline[0] == '0')
				{
					//cout << fileline << '\n';
					//keep track of the address we are on so as to stay within our desired limits
					string lineHexAddress = fileline.substr(2,6);
					string decAddr = hexString_to_decimal(lineHexAddress);
					assemblyFileAddress = stringDec_to_int(decAddr);
					if(assemblyFileAddress > endAddress)
					{
						break;
					}

					//we're on a code line so lets extract the instruction
					string instruction = getDelimitedSubString(fileline, 1, '\t');
					//then, if the instruction has an instruction that points
					//to another address, we want to modify that instructions
					//landing point. Otherwise, just copy the instruction to
					//the new assembly file
					string hexInstruction = getHexInstruction(fileline);

					if(stringSearch("BSR",fileline))
					{
						if(instruction[4] == 'B')
						{
							//byte length displacement
							string displacement = hexInstruction.substr(2,2);//1 byte
							//convert the displacement string to a decimal
							string decDisp = hexString_to_decimal(displacement);
							int8_t displacementDec = stringDec_to_int(decDisp);
							uint32_t targetAddress = assemblyFileAddress + 2 + displacementDec;
							//cout << "target addr: " << (int) targetAddress << " " << fileline << '\n';
							//if the assembly line address is higher than the inserted bytes, but the
							//target address is lower, subtract the shift byte count from the displacement
							if(((assemblyFileAddress >= insertAddress && targetAddress <= insertAddress) || (assemblyFileAddress < insertAddress && targetAddress >= insertAddress))&& targetAddress <= endAddress)
							{
								displacementDec -= numBytesToInsert;
								string firstPart = hexInstruction.substr(0,2);
								string secondPart = decimal_to_hexString(displacementDec);
								//cout << secondPart << '\n';
								if(secondPart.length() > 2)
								{
									secondPart = secondPart.substr(secondPart.length()-2);
								}
								firstPart.append(secondPart);
								hexInstruction = firstPart;
							}
						}
						else if(instruction[4] == 'W')
						{
							//word displacement
							string displacement = hexInstruction.substr(4,4);//2 bytes
							//convert the displacement string to a decimal
							string decDisp = hexString_to_decimal(displacement);
							int16_t displacementDec = stringDec_to_int(decDisp);
							uint32_t targetAddress = assemblyFileAddress + 2 + displacementDec;
							//cout << "target addr: " << (int) targetAddress << " " << fileline << '\n';
							//if the assembly line address is higher than the inserted bytes, but the
							//target address is lower, subtract the shift byte count from the displacement
							if(((assemblyFileAddress >= insertAddress && targetAddress <= insertAddress) || (assemblyFileAddress < insertAddress && targetAddress >= insertAddress))&& targetAddress <= endAddress)
							{
								displacementDec -= numBytesToInsert;
								string firstPart = hexInstruction.substr(0,4);
								string secondPart = decimal_to_hexString(displacementDec);

								if(secondPart.length() < 4)
								{
									string extra = "";
									for(uint8_t i = 0; i < (4 - secondPart.length()); i++)
									{
										extra.push_back('0');
									}
									extra.append(secondPart);
									secondPart = extra;
								}
								else if(secondPart.length() > 4)
								{
									secondPart = secondPart.substr(secondPart.length()-4);
								}
								firstPart.append(secondPart);
								hexInstruction = firstPart;
							}
						}
						else if(instruction[4] == 'L')
						{
							//word displacement
							string displacement = hexInstruction.substr(4,8);//4 bytes
							//convert the displacement string to a decimal
							string decDisp = hexString_to_decimal(displacement);
							int32_t displacementDec = stringDec_to_int(decDisp);
							uint32_t targetAddress = assemblyFileAddress + 2 + displacementDec;
							//cout << "target addr: " << (int) targetAddress << " " << fileline << '\n';
							//if the assembly line address is higher than the inserted bytes, but the
							//target address is lower, subtract the shift byte count from the displacement
							if(((assemblyFileAddress >= insertAddress && targetAddress <= insertAddress) || (assemblyFileAddress < insertAddress && targetAddress >= insertAddress))&& targetAddress <= endAddress)
							{
								displacementDec -= numBytesToInsert;
								string firstPart = hexInstruction.substr(0,4);
								string secondPart = decimal_to_hexString(displacementDec);
								if(secondPart.length() < 8)
								{
									string extra = "";
									for(uint8_t i = 0; i < (8 - secondPart.length()); i++)
									{
										extra.push_back('0');
									}
									extra.append(secondPart);
									secondPart = extra;
								}
								firstPart.append(secondPart);
								hexInstruction = firstPart;
							}
						}
					}
					else if(stringSearch("JSR",fileline) || stringSearch("JMP",fileline) || stringSearch("PEA",fileline) || stringSearch("LEA",fileline))
					{
						if(instruction[5] == '$' && !addressTypeSearch(instruction))
						{
							//we have an address
							string landingAddress = hexInstruction.substr(4);
							//convert to decimal
							string decDisp = hexString_to_decimal(landingAddress);
							uint32_t decLandingAddr = stringDec_to_int(decDisp);
							//if the address has been shifted, un-shift it
							uint32_t unshiftedAddress = decLandingAddr;
							if(assemblyFileAddress >= addressToShift && unshiftedAddress > shiftBy)
							{
								unshiftedAddress -= shiftBy;
								unshiftedAddress += addressToShift;
							}

							//cout << "landing: " << (int) decLandingAddr << " unshifted: " << (int) unshiftedAddress << " " << fileline << '\n';

							//check to see if we need to adjust the value
							if(unshiftedAddress >= insertAddress && unshiftedAddress < endAddress)
							{
								decLandingAddr += numBytesToInsert;
								string firstPart = hexInstruction.substr(0,4);
								string secondPart = decimal_to_hexString(decLandingAddr);
								if(secondPart.length() < 8)
								{
									string extra = "";
									for(uint8_t i = 0; i < (8 - secondPart.length()); i++)
									{
										extra.push_back('0');
									}
									extra.append(secondPart);
									secondPart = extra;
								}
								firstPart.append(secondPart);
								hexInstruction = firstPart;
							}
						}
					}


					//figure out if we should add blank data for the shift:
					if(assemblyFileAddress <= insertAddress && (assemblyFileAddress + (hexInstruction.length()/2)) > insertAddress)
					{
						//then build an 'FF' string to add which is the shifted bytes
						string addedBytes = "";
						for(uint16_t i = 0; i < numBytesToInsert; i++)
						{
							addedBytes.append("FF");
						}
						//then find the place in the hexInstruction to put the shifted bytes
						if(assemblyFileAddress == insertAddress)
						{
							string tempString = "";
							tempString.append(addedBytes);
							tempString.append(hexInstruction);
							hexInstruction = tempString;
						}
						else if(assemblyFileAddress < insertAddress)
						{
							uint8_t difference = insertAddress - assemblyFileAddress;
							string firstPart = hexInstruction.substr(0,difference);
							string secondPart = "";
							if(hexInstruction.length()/2 > difference)
							{
								secondPart = hexInstruction.substr(difference);
							}
							firstPart.append(addedBytes);
							firstPart.append(secondPart);
							hexInstruction = firstPart;
						}
					}

					if(dataForSrec.length() >= 32)
					{
						dataForSrec.append(hexInstruction);
						string dataSendToSREC = dataForSrec.substr(0,32);
						buildSREC(SRECaddress, dataSendToSREC);
						if(dataForSrec.length() > 32)
						{
							dataForSrec = dataForSrec.substr(32);
						}
						else
						{
							dataForSrec = "";
						}
						SRECaddress += 16;
					}
					else
					{
						dataForSrec.append(hexInstruction);
					}
				}
			}
		}
	}


	//s_record.close();
	assemblyCode.close();

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
	string decString = srecReader(filename, address, lastFilePosition);
	//string decString = hexString_to_decimal(hexString);
	uint8_t decimalByte = stringDec_to_int(decString);
	return decimalByte;
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


bool stringSearch(string searchTerm, string searchString)
{
	//looks for searchTerm in searchString- returns true if found
	int sizeOfSearchString = searchString.length();
	int sizeOfSearchTerm = searchTerm.length();
	bool isPresent = false;
	for(int i = 0; i < sizeOfSearchString - sizeOfSearchTerm; i++)
	{
		string tempString = searchString.substr(i,sizeOfSearchTerm);
		if(tempString == searchTerm) {
			isPresent = true;
			break;
		}
	}
	return isPresent;
}

string getDelimitedSubString(string inputString, int delimiterOffset)
{
	string outputString = "";
	int delimiterCount = 0;
	for(uint16_t i = 0; i < inputString.length(); i++)
	{
		if(inputString[i] == ' ' || inputString[i] == '-' || inputString[i] == '\t')
		{
			delimiterCount += 1;
		}
		else if(delimiterCount == delimiterOffset)
		{
			outputString.push_back(inputString[i]);
		}
		else if(delimiterCount > delimiterOffset)
		{
			break;
		}
	}
	return outputString;
}

string getDelimitedSubString(string inputString, int delimiterOffset, char delimiter)
{
	string outputString = "";
	int delimiterCount = 0;
	for(uint16_t i = 0; i < inputString.length(); i++)
	{
		if(inputString[i] == delimiter)
		{
			delimiterCount += 1;
		}
		else if(delimiterCount == delimiterOffset)
		{
			outputString.push_back(inputString[i]);
		}
		else if(delimiterCount > delimiterOffset)
		{
			break;
		}
	}
	return outputString;
}

string getHexInstruction(string line)
{
	string hexValues = "";
	if(line[0] == '0')
	{
		//we're on an instruction line
		//lets get the first delimited value
		string firstValue = getDelimitedSubString(line, 1, '\t');
		if(firstValue.length() > 0)
		{
			if(firstValue[0] == '0')
			{
				//we're on an exception table entry
				hexValues = firstValue;
			}
			else
			{
				//we're on a regular instruction
				hexValues = getDelimitedSubString(line, 5, '\t');
			}
		}
		else
		{
			//we are on a 'no instruction matched' line
			hexValues = getDelimitedSubString(line, 3, '\t');
		}
	}
	return hexValues;
}

void buildSREC(uint32_t address, string HEXdata)
{
	ofstream newSREC("shifted assembly.txt",ios::app);
	if(HEXdata.length() == 32)
	{
		//we have enough data, so proceed
		//convert the address to a hex string
		string addressString = decimal_to_hexString(address);
		if(addressString.length() < 8)
		{
			string zeros = "";
			for(uint8_t i = 0; i < (8 - addressString.length()); i++)
			{
				zeros.push_back('0');
			}
			zeros.append(addressString);
			addressString = zeros;
		}
		string allDataForSum = "";
		allDataForSum.append(addressString);
		for(uint8_t i = 0; i < HEXdata.length();i++)
		{
			if(HEXdata[i] >= 97 && HEXdata[i] <= 102)
			{
				allDataForSum.push_back(HEXdata[i] - 32);
			}
			else
			{
				allDataForSum.push_back(HEXdata[i]);
			}
		}

		//then loop through the data to get a sum
		uint8_t sum = 21;
		for(uint8_t i = 0; i < allDataForSum.length(); i+=2)
		{
			char zero = allDataForSum[i];
			char one = allDataForSum[i+1];
			if(zero >= 48 && zero <= 57)
			{
				sum += (zero-48) << 4;
			}
			else if(zero >= 65 && zero <= 70)
			{
				sum += (zero-55) << 4;
			}
			if(one >= 48 && one <= 57)
			{
				sum += (one-48);
			}
			else if(one >= 65 && one <= 70)
			{
				sum += (one-55);
			}
		}

		sum = ~sum;//invert the bits of the sum
		string sumString = decimal_to_hexString(sum);
		string line = "S315";
		line.append(allDataForSum);
		line.append(sumString);
		newSREC << line << '\n';
	}
	newSREC.close();
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

bool addressTypeSearch(string searchString)
{
	//returns true if the instruction has direct address reference mode(i.e. ($xxxxx).L
	bool directAddress = false;
	string effectiveAddress = "";
	bool start = false;
	for(uint8_t i = 0; i < searchString.length(); i++)
	{
		if(searchString[i] == '(' && start == false)
		{
			start = true;
		}
		else if(searchString[i] == ')' && start == true)
		{
			start = false;
			break;
		}
		else if(start == true)
		{
			effectiveAddress.push_back(searchString[i]);
		}
	}
	directAddress = stringSearch(",",effectiveAddress);
	return directAddress;
}
