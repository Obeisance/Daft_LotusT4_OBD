#include <iostream>
#include <fstream>
#include <string>
#include <stdlib.h>
#include "lotusReflash.h"
#include <stdint.h>
using namespace std;



void createReflashPacket()
{
	//first, we must assemble the sub-packet which will be used for the reflashing.

	//The maximum number of bytes in the main 'send-to ECU' packet is 255, and of the
	//encrypted sub-packet, each two bytes take up three bytes in the main packet.
	//If we add in the flag bytes, ID bytes, number bytes, sum bytes and blank bytes
	//we have a number for the maximum number of data bytes which can be sent in
	//a given frame:
	//main packet:
	//1 number byte
	//1 ID byte
	//4 'total bytes coming' bytes
	//12 unknown function bytes
	//1 sum byte
	// = 19 bytes

	//sub-packet:
	//3 unknown bytes
	//8 'flash sector clear' flag bytes
	//1 ID byte
	//1 number byte
	//3 address bytes
	//1 sum byte
	// = 17 bytes-> make it 18 to be safe
	//this takes up 27 bytes in the main packet

	//255 - (19+27) = 209 bytes for data -> 2/3 -> 139 bytes -> round down to 138 to keep even

	//let's bring in an s-record to extract data for flashing
	string inputString = "6-16-06 ECU Read";
	//getline(cin,inputString);
	ifstream s_record(inputString.c_str());
	convert_s_record(s_record);//convert the s-record to a decimal version file which we can easily
	//extract data from
	s_record.close();

	//then, bring in a file which contains address ranges which we would like to reflash
	string reflashRanges = "6-16-06 ECU read reflash ranges.txt";
	//getline(cin,reflashRanges);
	ifstream addressRanges(reflashRanges.c_str());
	uint32_t totalNumBytesFromSrec = 0;
	if(addressRanges.is_open())
	{
		string addressLine = "";
		while(getline(addressRanges, addressLine))
		{
			uint8_t address[3] = {0,0,0};
			uint32_t numBytes = interpretAddressRange(addressLine, address);
			if(address[2]%2 != 0)
			{
				//we cannot send bytes to an odd address
				totalNumBytesFromSrec += 1;
			}
			if(numBytes%2 != 0)
			{
				//we cannot send bytes to an odd address
				totalNumBytesFromSrec += 1;
			}
			totalNumBytesFromSrec += numBytes;
		}
	}
	addressRanges.close();
	addressRanges.open(reflashRanges.c_str());

	cout << totalNumBytesFromSrec << '\n';
	//finally, open a file to save the packets to
	ofstream outputPacket("testFlashBytes.txt", ios::trunc);
	ofstream encoding("watchEncodingProcess.txt", ios::trunc);
	ofstream subPacketBytes("unencrypted sub packet.txt", ios::trunc);

	if(addressRanges.is_open())
	{
		bool isFinished = false;
		bool getNextRange = true;
		string line = "";
		while(isFinished == false)
		{
			if(getNextRange == true)//if we need to read in a new range of data to write to the ECU, do so
			{
				if(!getline(addressRanges, line))
				{
					isFinished = true;
					break;
				}
			}
			uint8_t startAddress[3] = {0,0,0};
			uint32_t numBytes = interpretAddressRange(line, startAddress);
			//cout << (int) numBytes << '\n';

			//check to make sure we're trying to write to a
			//safe and valid position
			if(startAddress[0] < 0x01 && startAddress[1] < 0xB)
			{
				//we don't want to change anything close to the bootloader memory range (below 0xA8FA)
				totalNumBytesFromSrec -= numBytes;
				continue;//skip to the next iteration of the while loop
			}
			if(startAddress[2]%2 != 0)
			{
				//we cannot send bytes to an odd address
				startAddress[2] -= 1;//make sure to include the data originally requested
				numBytes += 1;
			}
			if(numBytes%2 != 0)
			{
				//we cannot send bytes to an odd address
				numBytes += 1;//make sure to include the data originally requested
			}

			if(numBytes > 138)
			{
				//we have too many bytes for a single packet

				//we should rewrite the 'line' string to contain
				//the new range we will poll next
				line = "";
				//find the new starting address
				uint16_t lowOrderAddrByte = startAddress[2] + 138;
				uint8_t mod = lowOrderAddrByte%256;
				uint8_t divide = (lowOrderAddrByte - mod)/256;
				string temp = decToHex(startAddress[0]);
				//then write to the 'line' string
				line.append(temp);
				temp = decToHex(startAddress[1] + divide);
				line.append(temp);
				temp = decToHex(mod);
				line.append(temp);
				line.push_back('-');

				//now find the new ending address
				long startAddr = startAddress[2] + startAddress[1]*256 + startAddress[0]*256*256 + numBytes;
				uint8_t lowOrderByte = startAddr%256;
				int div = (startAddr-lowOrderByte)/256;
				uint8_t secondOrderByte = div%256;
				uint8_t thirdOrderByte = (div-secondOrderByte)/256;
				//and write this to the 'line' string
				temp = decToHex(thirdOrderByte);
				line.append(temp);
				temp = decToHex(secondOrderByte);
				line.append(temp);
				temp = decToHex(lowOrderByte);
				line.append(temp);

				//raise a flag so that we keep working on this range
				getNextRange = false;
				//lower the number of bytes we're about to send
				numBytes = 138;
			}
			else
			{
				getNextRange = true;
			}

			subPacketBytes << "address: " << (int) startAddress[0] << ' ' << (int) startAddress[1] << ' ' << (int) startAddress[2]<< '\n';
			subPacketBytes << "number of bytes: " << (int) numBytes << '\n';

			//now, use the decimal version reader to bring in numBytes from the address
			uint8_t subPacket[256];
			for(int i = 0; i < 256; i++)//begin by setting all sector erase flags (and extra bytes) to zero
			{
				subPacket[i] = 0;
			}
			//then set the ID, numBytes and addr bytes
			subPacket[11] = 85;
			subPacket[12] = numBytes + 5;//this count includes the ID, number and address bytes
			subPacket[13] = startAddress[0];
			subPacket[14] = startAddress[1];
			subPacket[15] = startAddress[2];
			//then populate the data bytes
			long startAddr = startAddress[2] + startAddress[1]*256 + startAddress[0]*256*256;
			long lastFilePosition = 0;
			for(uint32_t i = 0; i < numBytes; i++)
			{
				//retrieve the data as a string
				string decimalData = decimalVersionReader(startAddr+i, lastFilePosition);
				//convert the string to decimal
				uint8_t data = 0;
				for(uint32_t j = 0; j < decimalData.length(); j ++)
				{
					if(decimalData[j] > 47 && decimalData[j] < 58)
					{
						data = data*10;
						data += decimalData[j] - 48;
					}
				}
				//place the decimal data in the sub packet
				subPacket[16+i] = data;
			}
			//the calculate the sumByte and write data to the output file
			for(uint32_t k = 0; k < numBytes + 5; k++)
			{
				subPacket[16+numBytes] += subPacket[11+k];

			}
			subPacketBytes << "SubPacket: ";
			for(uint32_t k = 0; k < numBytes + 17;k++)
			{
				subPacketBytes<<(int) subPacket[k]<< ' ';
			}
			subPacketBytes << '\n';


			//next, we must encrypt the sub-packet which will be used for reflashing
			//the first step is anticipating the full packet 'number of bytes coming
			//over k-line' number. This number includes all of the main packet bytes
			//beyond the number and ID bytes
			//there are three bytes used for every two in the decoded packet
			int totalBytesToSend = ((totalNumBytesFromSrec+11+6+1)/2)*3 + 12 + 1 + 4;
			totalNumBytesFromSrec -= numBytes;//decrement our byte counter before continuing the loop
			//split this number into four bytes
			uint8_t lowOrderKeyByte = totalBytesToSend%256;
			int firstDivision = (totalBytesToSend-lowOrderKeyByte)/256;
			uint8_t secondOrderKeyByte = firstDivision%256;
			int secondDivision = (firstDivision - secondOrderKeyByte)/256;
			uint8_t thirdOrderKeyByte = secondDivision%256;
			int thirdDivision = (secondDivision - thirdOrderKeyByte)/256;
			uint8_t fourthOrderKeyByte = thirdDivision%256;
			//take the sum of those bytes and 9744
			uint16_t encryptingKeyWord = 9744 + lowOrderKeyByte + secondOrderKeyByte + thirdOrderKeyByte + fourthOrderKeyByte;
			//invert that sum
			encryptingKeyWord = ~encryptingKeyWord;

			//over the length of the sub-packet, collect two words (reverse order) and encrypt them
			uint8_t numBytesMainPacket = 17;//count up the number of bytes in the packet as we prepare them
			uint8_t mainPacket[256];
			uint32_t decodeTable[16] = {0x7,0xF,0x17,0x2F,0x5D,0xBA,0x174,0x2E8,0x5D0,0xBA0,0x1740,0x2E80,0x5D00,0xBA00,0x17401,0x2E801};
			uint32_t decodeMod = 380951;
			uint32_t decodeMultiple = 3182;
			/*
			 * compare remainder of [3182*(remainder of (long word at 0x80D30) / 380951)]/380951
				to the 16 long words starting at 0xA8BA

				0xA8BA = 00000007 0000000F 00000017 0000002F 0000005D
					 	 000000BA 00000174 000002E8 000005D0 00000BA0
				         00001740 00002E80 00005D00 0000BA00 00017401
					 	 0002E801

				starting at the 16th of these and stepping down, if the long word is smaller than the remainder,
				subtract the long word from the remainder and set the bit place in a counter
				word to 1 -> this should result in a counter word being populated only if
				the remainder can be made up of some sum of the 16 long word values near 0xA8BA
				if failure, D0 = 0 - word at (0x80D3A).L, if success, D0 = counter word - word at (0x80D3A).L
				the D0 value is stored on 0x80D34 and on ($0,0x80B28,(word at 0x80D3C)) (but byte order inverted here,
				also, $80d3c_word is incremented at each byte passed through it (i.e. 2x per word stored), but kept below 512),
				$80D40_word is also incremented 2x for each word stored. This process populates the flashing byte packet
				at $80B28 (this is a decryption routine)


				there are other versions of the encrypting routine key bytes, though->
				00004E80 -> instead of 3182, use 20096 as the decodeMultiplier
				0001D343 -> instead of 380591, use 119619 as the decodeMod
				and for the threshold bytes use:
				00000001 00000003 00000006 0000000C
				00000019 00000030 00000064 000000C8
				0000018C 000003EB 00000708 00000EA4
				00001CB6 00003B6B 000074D0 0000E9A1
			 */



			for(uint32_t z = 0; z < (numBytes+11+6+1); z+=2)
			{
				//collect the two sub-packet bytes into one word
				uint16_t flashWord = subPacket[z] + 256*subPacket[z+1];
				uint16_t counterWord = flashWord + encryptingKeyWord;
				encoding << "pre-coded bytes: " << (int) subPacket[z] << ' ' << (int) subPacket[z+1] << ", add in key: " << (int) counterWord << '\n';
				//now, use the decode table to encrypt the counterWord
				uint32_t codedLong = 0;
				for(int y = 0; y < 16; y++)
				{
					//step through the bits in the counterWord and add the corresponding
					//decode table value to the codedLong value
					uint16_t bitOfInterest = 1;
					bitOfInterest <<= y;//leftshift to get the single binary flag
					if((counterWord & bitOfInterest) != 0)
					{
						codedLong += decodeTable[y];
					}
				}
				encoding << "coded word: " << (int) codedLong << '\n';

				//next, we want to recover the initial value from an operation
				//that returned the remainder from division by 380951
				//the max sum from the decode table is 380929, which is less than the total
				//so we can retain the original value through this operation.

				//next we want to recover the initial value from an operation
				//that multiplied by 3182
				uint64_t check = 0;
				for(uint32_t x = 0; x < 16777216; x++)
				{
					check = (decodeMultiple*x)%decodeMod;
					if(check == codedLong)
					{
						check = x;
						break;
					}
				}

				encoding << "pre-mod and divide long: " << (int) check << ", ";
				//then, we have found our encoded word, stored on check
				//lets move this to the main packet
				uint8_t modLow = check%256;
				uint32_t divLow = (check - modLow)/256;
				uint8_t modMid = divLow%256;
				uint32_t divMid = (divLow - modMid)/256;
				uint8_t modHigh = divMid%256;//hopefully this takes care of all of the bits in the check word
				mainPacket[numBytesMainPacket+1] = modLow;
				mainPacket[numBytesMainPacket+2] = modMid;
				mainPacket[numBytesMainPacket+3] = modHigh;
				encoding << "splits into bytes: " << (int) modLow << ' ' << (int) modMid << ' ' << (int) modHigh << '\n';
				numBytesMainPacket += 3;
			}
			encoding << '\n';



			//finally, build the rest of the packet which will be sent to the ECU
			mainPacket[0] = numBytesMainPacket;
			mainPacket[1] = 112;
			mainPacket[2] = fourthOrderKeyByte;
			mainPacket[3] = thirdOrderKeyByte;
			mainPacket[4] = secondOrderKeyByte;
			mainPacket[5] = lowOrderKeyByte;
			for(int l = 6; l < 18; l++)
			{
				mainPacket[l] = 0;//these bytes do not seem to be used by the packet interpreter
			}

			//then, compute the checksum and write the bytes to the file
			uint8_t mainSumByte = 0;
			for(int a = 0; a < numBytesMainPacket+1; a++)
			{
				outputPacket << (int) mainPacket[a] << '\n';
				mainSumByte += mainPacket[a];
			}
			mainPacket[numBytesMainPacket + 2] = mainSumByte;
			outputPacket <<(int) mainSumByte << '\n';
		}
	}

	//outputPacket << "finished" << '\n';
	subPacketBytes.close();
	addressRanges.close();
	outputPacket.close();
	encoding.close();
	remove("decimalVersion.txt");
}

void convert_s_record(ifstream &srec)
{
	//takes in a motorola s record file, and writes a file "decimal version" formatted
	//the way I first wrote the interpreter
	ofstream writedec("decimalVersion.txt", ios::trunc);
	string line;
	string addressHex = "";
	string s_record_type = "";
	bool firstLine = true;
	long address = 0;

	if (srec.is_open())
	{
	  while (getline (srec,line))
	  {
		  if((line[0] == 'S' || line[0] =='s') && line[1] != '0' && firstLine == true)
		  {
			  //the first line output to writedec should not be data
			  writedec << "Interpreted file";// << '\n';
			  firstLine = false;//we've put something in the first line of the file
		  }

		  if(line[1] != '0' && line[1] != '4' && line[1] != '5' && line[1] != '6' && line[1] != '7' && line[1] != '8' && line[1] != '9')
		  {
			  s_record_type = line[1];
			  //the s record type indicates the format of the instruction
			  //for instance,  the number of bits used to write the address of the line
			  //16 bit address - s1, s5, s9
			  //24 bit address - s2, s6, s8
			  //32 bit address - s3, s7
			  //address field = '0000' - s0
			  int addressLength = 0;
			  if(s_record_type == "1" || s_record_type == "5" || s_record_type == "9")
			  {
				  addressLength = 4;
			  }
			  else if(s_record_type == "2" || s_record_type == "6" || s_record_type == "8")
			  {
				  addressLength = 6;
			  }
			  else if(s_record_type == "3" || s_record_type == "7")
			  {
				  addressLength = 8;
			  }

			  //the next two bits are the byte count of the line
			  int instructionLength = 0;
			  string hexNumberOfBytes = "";
			  hexNumberOfBytes.push_back(line[2]);
			  hexNumberOfBytes.push_back(line[3]);
			  hexNumberOfBytes = hexString_to_decimal(hexNumberOfBytes); //convert to decimal string
			  instructionLength = stringDec_to_int(hexNumberOfBytes); //convert from string to int
			  //cout << "instruction length dec string: " << hexNumberOfBytes << ", instruction length dec: " << instructionLength << '\n';
			  instructionLength -= (addressLength/2 + 1);//the byte count includes the checksum and address, which we're not interested in
			  //cout << "addressLength: " << addressLength << ", instruction length dec: " << instructionLength << '\n';


			  //the next handful of bytes contain the address of the line
			  long addressPoint = 0;
			  int mult = 1;
			  for(int c = 1; c < addressLength/2; c++)
			  {
				  mult = mult*256;
			  }
			  for(int a = 0; a < addressLength; a = a + 2)
			  {
				  string tempAddressNumber = "";
				  tempAddressNumber.push_back(line[a+4]);
				  tempAddressNumber.push_back(line[a+5]);
				  tempAddressNumber = hexString_to_decimal(tempAddressNumber);
				  //writedec << tempAddressNumber << '\t';
				  int temp = stringDec_to_int(tempAddressNumber); //convert from string to int
				  addressPoint += temp*mult;
				  mult = mult/256;
			  }
			  //cout << "line address: " << addressPoint << ", address: " << address << '\n';
			  //make sure our accounting of address is correct
			  /*if(addressPoint < address)
			  {
				  //somehow some s records duplicate data from line to line
				  instructionLength = 0;
			  }*/
			  if(addressPoint != address)// && address == 0)
			  {
				  //adjust the first value of address to match that of the s record file
				  address = addressPoint;
			  }



			  //then loop through the data
			  int startPoint = addressLength + 4;//ex. S1 24 0000 data
			  for(int b = startPoint; b < 2*instructionLength + startPoint; b = b + 2)
			  {
				  //if four addresses have been written
				  if(address % 4 == 0)
				  {
					  //switch to a new line
					  writedec << '\n';
					  //write the address of this starting point
					  string newAddress = makeAddress(address, 4);
					  writedec << newAddress;
				  }
				  string data = "";
				  data.push_back(line[b]);
				  data.push_back(line[b+1]);
				  data = hexString_to_decimal(data);
				  writedec << data << '\t';

				  address += 1;//address of next data point
			  }
		  }
		  else
		  {
			  if(firstLine == true)
			  {
				  writedec << line;// << '\n';
				  firstLine = false;
			  }
		  }
	  }
	}
	writedec.close();

	//test code
	/*
	 *
	  //file name of the S record to be disassembled
	  ifstream s_record("tutorial1.S68");
	  convert_s_record(s_record);
	  s_record.close();
	 */
}

string decToHex(uint8_t decimal)
{
	string hex = "";
	char hexChar[16] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
	uint8_t mod = decimal%16;
	char second = hexChar[mod];
	uint8_t div = (decimal-mod)/16;
	uint8_t modupper = div%16;
	char first = hexChar[modupper];
	hex.push_back(first);
	hex.push_back(second);
	return hex;
}

uint32_t interpretAddressRange(string addressRange, uint8_t* addressArray)
{
	uint32_t numBytesInRange = 0;
	int addressRangeLength = addressRange.length();
	long address = 0;
	long address2 = 0;
	for(int i = 0; i < addressRangeLength; i++)
	{
		uint8_t nibble = 0;
		//the characters are hex values
		if(addressRange[i] > 47 && addressRange[i] < 58)
		{
			nibble = addressRange[i] - 48;//numbers 0-9
			address = address*16 + nibble;
		}
		else if(addressRange[i] > 64 && addressRange[i] < 71)
		{
			nibble = addressRange[i] - 55;//numbers 10-15
			address = address*16 + nibble;
		}
		else if(addressRange[i] > 96 && addressRange[i] < 103)
		{
			nibble = addressRange[i] - 87;//numbers 10-15
			address = address*16 + nibble;
		}
		else if(addressRange[i] == '-' || addressRange[i] == ' ')
		{
			//we've reached a point between addresses
			address2 = address;
			address = 0;
		}
		else
		{
			//invalid character, so reset the address
			address = 0;
		}
	}
	if(address < address2)//in case the address order was incorrect
	{
		long address3 = address;
		address = address2;
		address2 = address3;
	}

	//cout << (int) address2 <<  ' ' << (int) address << '\n';

	numBytesInRange = address - address2;
	int modLow = address2%256;
	int	divLow = (address2 - modLow)/256;
	int modMid = divLow%256;
	int divMid = (divLow - modMid)/256;
	int modHigh = divMid%256;
	addressArray[2] = modLow;
	addressArray[1] = modMid;
	addressArray[0] = modHigh;
	return numBytesInRange;
}

string decimalVersionReader(long address, long &lastFilePosition)
{
	//input address, return value at address or "NAN" if address not allowed
	ifstream myfile("decimalVersion.txt");
	//ofstream testFile("testFile.txt", ios::trunc);
	string value = "NAN";
	string line;
	//string lineAddressString = "";
	long lineAddress = 0;
	long localAddress = 0;
	char delimiter = '\t';
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

	    	//if we need to read in a new line, do so
	    	if(getNextLine == true)
	    	{
	    		//collect the position before reading in the line
	    		//this position is the beginning of the next line
	    		lastFilePosition += lineSize;
	    		//if no new line is read in, we are at the end of the file
	    		if(!getline(myfile,line))
	    		{
	    			//set to exit the loop
	    			finished = true;
	    		}
	    		getNextLine = false;
	    		lineSize = line.length();
	    		lineAddress = 0;
	    		//cout << "position:" << lastFilePosition << ", line size: " << lineSize << '\n';
	    	}

	    	//interpret the line
	    	//if the first character is not a number character, skip the line
	    	if(line[0] < 48 || line[0] > 57)
	    	{
	    		getNextLine = true;
	    		//testFile << "skip line" << '\n';
	    	}

	    	//if the first character is a number, extract the address of the line
	    	//from the first four bytes separated by the delimiter
	    	//the next bytes are the values through which to search, separated and
	    	//terminated by the delimiter
	    	if(getNextLine == false)
	    	{
	    		int delimiterCount = 0;
	    		string byteString = "";
	    		int byteValue = 0;
	    		long addressByteMultiplier = 16777216;
	    		//loop through the line of data
	    		for(int i = 0; i < lineSize; i++)
	    		{
	    			//convert a char to an int
	    			char a = line[i] - 48;
	    			if(line[i] != delimiter)
	    			{
	    				//extract the byteValue
	    	        	int fragment = a;
	    	        	byteValue = byteValue*10 + fragment;
	    			}
	    			else if(delimiterCount < 4 && line[i] == delimiter)
	    			{
	    				//if we reach a delimiter, that ends a byte
	    				//below 4 delimiters, we have address bytes
	    				delimiterCount += 1;
	    				lineAddress += addressByteMultiplier*byteValue;
	    				addressByteMultiplier = addressByteMultiplier/256;
	    				//testFile << " byte: " << byteValue;
	    				byteValue = 0;
	    			}
	    			else if(delimiterCount >= 4 && line[i] == delimiter)
	    			{
	    				//if we reach a delimiter, that ends a byte.
	    				//above 4 delimiters, we have data
	    				delimiterCount += 1;
	    				localAddress = lineAddress + delimiterCount - 5;
	    				//testFile << "addr: " << localAddress << " byte: " << byteValue << '\n';

	    				//if the current byte address matches the requested one, collect it and end
	    				if(localAddress == address)
	    				{
	    				    char stringByte [7];//buffer for the conversion
	    					value = itoa(byteValue,stringByte,10);//convert to string
	    					finished = true;//breaks the while loop
	    					break;//breaks the for loop
	    				}
	    				byteValue = 0;
	    			}
	    		}
    			getNextLine = true;
	    	}
	    }
	}

	myfile.close();
	//testFile.close();
	return value;

	/*
	long seekPosition = 0;
	string test = decimalVersionReader(4096, seekPosition);
	cout << "test: " << test << " position: " << seekPosition << '\n';

	test = decimalVersionReader(4097, seekPosition);
	cout << "test: " << test << " position: " << seekPosition << '\n';

	test = decimalVersionReader(4098, seekPosition);
	cout << "test: " << test << " position: " << seekPosition << '\n';

	test = decimalVersionReader(4099, seekPosition);
	cout << "test: " << test << " position: " << seekPosition << '\n';

	test = decimalVersionReader(4100, seekPosition);
	cout << "test: " << test << " position: " << seekPosition << '\n';

	test = decimalVersionReader(4101, seekPosition);
	cout << "test: " << test << " position: " << seekPosition << '\n';

	test = decimalVersionReader(4102, seekPosition);
	cout << "test: " << test << " position: " << seekPosition << '\n';

	test = decimalVersionReader(4148, seekPosition);
	cout << "test: " << test << " position: " << seekPosition << '\n';

	test = decimalVersionReader(4151, seekPosition);
	cout << "test: " << test << " position: " << seekPosition << '\n';


	long seekPosition = 0;
	for(int i = 4096; i < 4278; i++)
	{
		string test = decimalVersionReader(i, seekPosition);
		cout << "pos: " << i << ", test: " << test << " position: " << seekPosition << '\n';
		if(test == "NAN")
		{
			seekPosition = 0;
		}
	}
	 */
}

string makeAddress(long addressNumber, int addressLength)
{
	//input a decimal address, split it into strings of bytes separated by tab values
	//addressNumber is the address to be converted into addressLength number of bytes
	string addressString = "";
	int address[addressLength];
	unsigned long mod = addressNumber;
	unsigned long divide = addressNumber;
	unsigned long subtract = addressNumber;
	for(int i = addressLength-1; i >= 0; i--)
	{
		mod = divide % 256;
		address[i] = mod;
		subtract = divide - mod;
	    divide = subtract/256;
	}
	for(int j = 0; j < addressLength; j++)
	{
	    char stringByte [16];
	    string decimal = itoa(address[j],stringByte,10);
		addressString.append(decimal);
		addressString.push_back('\t');
	}

	return addressString;

	//test code
	/*	long number = 1024;
	string address = makeAddress(number,4);
	cout << address << '\n';
	 */
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
