#include <Unknwn.h> //for com port
#include <windows.h> //for external device and window/file creation
#include <iostream>
#include <fstream>
#include <string>
#include <stdlib.h>
#include "lotusReflash.h"
#include <stdint.h>
#include "ftd2xx.h"//included to directly drive the FTDI com port device
#include "OBDpolling.h"
using namespace std;



void createReflashPacket(string inputString, string reflashRanges, int encodeByteSet)
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
	//{3 unknown bytes
	//8 'flash sector clear' flag bytes} <- only in first packet
	//1 ID byte
	//1 number byte
	//3 address bytes
	//1 sum byte
	//{2 word sum bytes} <- only in final packet
	// = xx
	//this takes up xx*3/2 bytes in the main packet

	//255 - (19+27) = 209 bytes for data -> 2/3 -> 139 bytes -> round down to 138 to keep even
	//however the timing for the bytes dictates that only 116 bytes can get through per message
	//116 - (19) = 97 bytes for the sub packet -> decoded  this leaves 2/3*97 = 64 bytes
	uint8_t maxBytesInMessage = 64;//this limitation is set by the interrupt timer
	uint32_t maxBytesAllowedToSend = 511;//the decoded byte save point in the ECU only allows for 511 bytes to be indexed
	uint8_t maxMessagesAllowedToSend = 1;//somehow, when more than one message is sent, the ECU reports an error state

	//bring in a file which contains address ranges which we would like to reflash
	//string reflashRanges = "6-16-06 ECU read reflash ranges.txt";
	//getline(cin,reflashRanges);
	ifstream addressRanges(reflashRanges.c_str());

	//try to figure out the number of addresses and bytes are requested to be flashed
	uint32_t totalNumBytesFromSrec = 0;
	uint32_t totalNumAddressesFromSrec = 0;//this equals the number of messages too
	if(addressRanges.is_open())
	{
		string addressLine = "";
		bool notFinishedFirstCheck = true;
		bool needNextLine = true;
		while(notFinishedFirstCheck)
		{
			if(needNextLine)
			{
				if(!getline(addressRanges, addressLine))
				{
					notFinishedFirstCheck = false;
					break;
				}
			}

			//interpret the line to read a certain number of bytes from a given address
			uint8_t address[3] = {0,0,0};
			uint32_t numBytes = interpretAddressRange(addressLine, address);
			uint8_t extraBytes = 0;

			//then decide how many bytes we can send based on which message we're sending
			uint8_t bytesAllowedForMessage = maxBytesInMessage - 2;//subtract 2
			//because I'm not sure how to deal with identifying the final message
			if(totalNumAddressesFromSrec == 0)
			{
				//we're on the first message, so we have fewer bytes for the data part of the message
				//bytesAllowedForMessage -= 2;
				//numBytes += 11;
				extraBytes = 11+1;//+1 to make up for the odd number in the sector erase command section
			}


			//lets count the number of bytes in this message
			if(address[0] < 0x01 && address[1] < 0xB)
			{
				//we don't want to change anything close to the bootloader memory range (below 0xA8FA)
				continue;//skip to the next iteration of the while loop
			}
			if(address[2]%2 != 0)
			{
				//the low order byte of the address is not even
				//we cannot send bytes to an odd address
				numBytes += 1;
			}
			if(numBytes%2 != 0)
			{
				//an odd number of bytes to send from an even address
				//will end on an odd address
				//we cannot send bytes to an odd address
				numBytes += 1;
			}

			if(numBytes+extraBytes > bytesAllowedForMessage)
			{
				//we have too many bytes for a single packet

				//we should rewrite the 'line' string to contain
				//the new range we will poll next
				addressLine = "";
				//find the new starting address
				uint16_t lowOrderAddrByte = address[2] + bytesAllowedForMessage - extraBytes;
				uint8_t mod = lowOrderAddrByte%256;
				uint8_t divide = (lowOrderAddrByte - mod)/256;
				string temp = decToHex(address[0]);
				//then write to the 'line' string
				addressLine.append(temp);
				temp = decToHex(address[1] + divide);
				addressLine.append(temp);
				temp = decToHex(mod);
				addressLine.append(temp);
				addressLine.push_back('-');

				//now find the new ending address (same as before)
				long startAddr = address[2] + address[1]*256 + address[0]*256*256 + numBytes;
				uint8_t lowOrderByte = startAddr%256;
				int div = (startAddr-lowOrderByte)/256;
				uint8_t secondOrderByte = div%256;
				uint8_t thirdOrderByte = (div-secondOrderByte)/256;
				//and write this to the 'line' string
				temp = decToHex(thirdOrderByte);
				addressLine.append(temp);
				temp = decToHex(secondOrderByte);
				addressLine.append(temp);
				temp = decToHex(lowOrderByte);
				addressLine.append(temp);

				//raise a flag so that we keep working on this range
				needNextLine = false;
				//lower the number of bytes we're about to send
				numBytes = bytesAllowedForMessage - extraBytes;
			}
			else
			{
				needNextLine = true;
			}

			//lets increment counters for allowable bytes to send
			if(totalNumBytesFromSrec + numBytes + extraBytes <= maxBytesAllowedToSend)
			{
				//increment the total byte counter
				totalNumBytesFromSrec += (numBytes + extraBytes);
				//cout << "numbytes: " << (int) numBytes << '\n';
				/*if(numBytes > bytesAllowedForMessage)
				{
					//we want to send more bytes than can fit in this message
					totalNumAddressesFromSrec += 1;//so we'll have to extend into another message
				}*/
				totalNumAddressesFromSrec += 1;//add one message for the main requested bytes
			}
		}
	}
	addressRanges.close();
	addressRanges.open(reflashRanges.c_str());

	//The first step is anticipating the full packet 'index of decoded packet
	//word sum' number. This number includes all of the sub packet bytes

	//there are three bytes used for every two in the decoded packet:
	//thus, we know there will be bytes from the s-record, +11 bytes indicating
	//'sector clear state' (that is, 3+8, but it's included in the total num bytes from SREC),
	//6 bytes containing sum, address, and
	//number !for each packet! + 2 word sum bytes. All of these make up the decoded sub packet
	//where each word is worth 3 bytes in the final byte count

	//there will be 12 unknown function bytes in the main packet along with
	//4 'index of word sum' bytes (subtract 1 for index location)
	//and then the index has 5 added to it...

	int indexOfWordSum = ((totalNumBytesFromSrec+6*totalNumAddressesFromSrec+2)/2)*3 + totalNumAddressesFromSrec*(4+12)-1 + 5;

	//split this number into four bytes
	uint8_t lowOrderKeyByte = indexOfWordSum%256;
	int firstDivision = (indexOfWordSum-lowOrderKeyByte)/256;
	uint8_t secondOrderKeyByte = firstDivision%256;
	int secondDivision = (firstDivision - secondOrderKeyByte)/256;
	uint8_t thirdOrderKeyByte = secondDivision%256;
	int thirdDivision = (secondDivision - thirdOrderKeyByte)/256;
	uint8_t fourthOrderKeyByte = thirdDivision%256;
	//take the sum of those bytes and 9744
	uint16_t encryptingKeyWord = 9744 + lowOrderKeyByte + secondOrderKeyByte + thirdOrderKeyByte + fourthOrderKeyByte;
	//invert that sum
	encryptingKeyWord = ~encryptingKeyWord;

	//cout << totalNumBytesFromSrec << '\n';

	//finally, open a file to save the packets to
	ofstream outputPacket("encodedFlashBytes.txt", ios::trunc);
	//ofstream encoding("watchEncodingProcess.txt", ios::trunc);
	ofstream subPacketBytes("unencrypted sub packet.txt", ios::trunc);

	if(addressRanges.is_open())
	{
		bool isFinished = false;
		bool getNextRange = true;
		string line = "";
		uint16_t decodedByteSum = 0;
		bool firstMessage = true;
		uint8_t thisMessageNumber = 0;
		while(isFinished == false)
		{

			//perform special actions if we're on the first message
			uint8_t numBytesAllowableThisMessage = maxBytesInMessage - 2;
			uint8_t index = 0;//for populating the sub-packet
			uint8_t extraByteNumber = 0;//in order to accommodate the odd 11 bytes in the first message
			if(firstMessage)
			{
				numBytesAllowableThisMessage -= (11 + 2);//11 for the sector reflash, -2 for extra space;
				index = 11;
				extraByteNumber = 1;
				firstMessage = false;
			}

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

			thisMessageNumber += 1;//increment the count of which message we are on
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

			if(numBytes > numBytesAllowableThisMessage)
			{
				//we have too many bytes for a single packet

				//we should rewrite the 'line' string to contain
				//the new range we will poll next
				line = "";
				//find the new starting address
				uint16_t lowOrderAddrByte = startAddress[2] + numBytesAllowableThisMessage;
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
				numBytes = numBytesAllowableThisMessage;
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
			subPacket[index] = 85;
			subPacket[index+1] = numBytes + 5;//this count includes the ID, number and address bytes
			subPacket[index+2] = startAddress[0];
			subPacket[index+3] = startAddress[1];
			subPacket[index+4] = startAddress[2];
			//then populate the data bytes
			long startAddr = startAddress[2] + startAddress[1]*256 + startAddress[0]*256*256;
			long lastFilePosition = 0;
			for(uint32_t i = 0; i < numBytes; i++)
			{
				//retrieve the data as a string
				string decimalData = srecReader(inputString, startAddr+i, lastFilePosition);
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
				subPacket[index+5+i] = data;
			}
			//the calculate the sumByte and write data to the output file
			for(uint32_t k = 0; k < numBytes + 5; k++)
			{
				subPacket[index+5+numBytes] += subPacket[index+k];
				decodedByteSum += subPacket[index+k];
			}
			decodedByteSum += subPacket[index + 5 + numBytes];

			if(totalNumAddressesFromSrec == thisMessageNumber)
			{
				//put the word sum into the sub packet too- but as little endian
				uint8_t mod = decodedByteSum%256;
				subPacket[index + 5 + numBytes + extraByteNumber + 1] = mod;//low order byte
				subPacket[index + 5 + numBytes + extraByteNumber + 2] = (decodedByteSum - mod)/256;//high order byte
				numBytes += 2;
			}
			subPacketBytes << "SubPacket: ";
			for(uint32_t k = 0; k < numBytes + 6 + index + extraByteNumber;k++)
			{
				subPacketBytes<<(int) subPacket[k]<< ' ';
			}
			subPacketBytes << '\n';


			//Next, we must encrypt the sub-packet which will be used for reflashing.


			//over the length of the sub-packet, collect two words (reverse, or little endian, order) and encrypt them
			//gather up the correct encrypting byte set
			uint8_t numBytesMainPacket = 17;//count up the number of bytes in the packet as we prepare them
			uint8_t mainPacket[256];
			uint32_t decodeTable[16] = {0x7,0xF,0x17,0x2F,0x5D,0xBA,0x174,0x2E8,0x5D0,0xBA0,0x1740,0x2E80,0x5D00,0xBA00,0x17401,0x2E801};
			uint32_t decodeMod = 380951;
			uint32_t decodeMultiple = 3182;
			if(encodeByteSet == 1)
			{
				//switch to the byte set in the DEV ECU
				decodeMod = 119619;
				decodeMultiple = 20096;
				uint32_t decodeTableDevBytes[16] = {0x1,0x3,0x6,0xC,0x19,0x30,0x64,0xC8,0x18C,0x3EB,0x708,0xEA4,0x1CB6,0x3B6B,0x74D0,0xE9A1};
				for(int i = 0; i < 16; i++)
				{
					decodeTable[i] = decodeTableDevBytes[i];
				}
			}
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



			for(uint32_t z = 0; z < (numBytes+index+6+extraByteNumber); z+=2)
			{
				//collect the two sub-packet bytes into one word
				uint16_t flashWord = subPacket[z] + 256*subPacket[z+1];
				uint16_t counterWord = flashWord + encryptingKeyWord;
				//encoding << "pre-coded bytes: " << (int) subPacket[z] << ' ' << (int) subPacket[z+1] << ", add in key: " << (int) counterWord << '\n';
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
				//encoding << "coded word: " << (int) codedLong << '\n';

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

				//encoding << "pre-mod and divide long: " << (int) check << ", ";
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
				//encoding << "splits into bytes: " << (int) modLow << ' ' << (int) modMid << ' ' << (int) modHigh << '\n';
				numBytesMainPacket += 3;
			}
			//encoding << '\n';



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

	//now that we're finished with making the encrypted packets
	//add in the 'done with reflash mode' packet
	outputPacket << "1" << '\n' << "115" << '\n' << "116";

	//outputPacket << "finished" << '\n';
	subPacketBytes.close();
	addressRanges.close();
	outputPacket.close();
	//encoding.close();
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


void testReflashPacket(int encodeByteSet)
{
	/*
	 * setup serial (interrupt vector 64) baud rate for 31.205 kbit/s

look for serial data, correct packet with up to 20 interrupt counts per byte
	;[1,113,114] -> send back 0x8D and end routine
	;[1,129,129] -> set serial baud rate to 29.787 kBaud, send back 0x7E and end routine
	;any failure to get correct serial byte sequence results in jump to 0x10000
read in a packet up to 256 bytes (over 1000 interrupt timer counts), store on 0x80810
	packet structure: first byte: number of bytes in the packet (excluding number and sum bytes)
	Last Byte: sum of all bytes in packet (including first byte, excluding sum byte)
	ECU sends inverted checksum over serial in response
	check that the first data byte is 112
	store the data (past the 112) onto 0x80D42, and add that number of bytes to the value at 0x80004
		loop across that data as per the decoder/interpreter:
		0x80D2C_long - store a byte position index/counter, incremented after we process each byte
		0x80D28_long - store the first four read in bytes
				(byte0,byte1,byte2,byte3 as one single long word)
		0x80D3A_word - (store sum of the first four read in bytes + #9744)
				after adding 4th byte (position index = 3), the value stored here
				is inverted bitwise.
		For byte indices 4-15 these bytes
		apparently don't get copied into a sum or combined byte
		for byte index 16 and above, the following calculations occur:
		0x80D30_long - store here the read in byte (position greater or equal to 0x80D28 - 4) if remainder of
				(position counter - 16)/3 is 0
				add to this number the read in byte*256 (position greater or equal to 0x80D28 - 4) if remainder of
				(position counter - 16)/3 is 1
				add to this number the read in byte*256*256 (position greater or equal to 0x80D28 - 4) if remainder of
				(position counter - 16)/3 is 2
			each time $80D30_long is populated (remainder = 2), continue on as below
		0x80D34_word - compare remainder of [3182*(remainder of (long word at 0x80D30) / 380951)]/380951
				to the 16 long words starting at 0xA8BA

				0xA8BA = 00000007 0000000F 00000017 0000002F 0000005D
					 000000BA 00000174 000002E8 000005D0 00000BA0
				         00001740 00002E80 00005D00 0000BA00 00017401
					 0002E801

				starting at the 16th of these and stepping down, if the long word is larger than the remainder,
				subtract the long word from the remainder and set the bit place in a counter
				word to 1 -> this should result in a counter word being populated only if
				the remainder can be made up of some sum of the 16 long word values near 0xA8BA
				if failure, D0 = 0 - word at (0x80D3A).L, if success, D0 = counter word - word at (0x80D3A).L
				the D0 value is stored on 0x80D34 and on ($0,0x80B28,(word at 0x80D3C)) (but byte order inverted here,
				also, $80d3c_word is incremented at each byte passed through it (i.e. 2x per word stored), but kept below 512),
				$80D40_word is also incremented 2x for each word stored. This process populates the flashing byte packet
				at $80B28 (this is a decryption routine)
		0x80D36_word - if the combined long word (at 0x80D28) - 5 is not equal to the position counter
				add the two separate bytes of the word at 0x80D34 to the word at 0x80D36 (this is a checksum of sorts)
		0x80D38_word - if the combined long word (at 0x80D28) - 5 is equal to the position counter
				store the word at 0x80D34 at 0x80D38 (this is a checksum, of sorts)
	This process loops through until all of the bytes are checked and decoded

	Next, we perform actions based on the $80D40 counter set by the serial data -> this will clear flash memory
	sectors if certain bytes are set. The process checks the decoded bytes sequentially, decrementing the
	counter at $80D40 and incrementing the lookup index at $80D3E for each one. If $80D40 is 0 at any point,
	enter infinite loop.

	check byte flags:
	if the 3rd byte past (0x80B28) == '1', then command flash memory sector erase: 0x8000
	if the 4rd byte past (0x80B28) == '1', then command flash memory sector erase: 0x10000

	repeat this 'if' 'then' process for bytes 5 through 10
		;potentially clearing flash memory sectors at: 0x20000,0x30000,0x40000,0x50000,0x60000, and 0x70000
		(this requires $80D40 starts >= 11
		and increments $80D3E by 11 because the check routine is called 11x)

	then use the packet we assembled at $80B28 to reflash the rom:
		store this sub-sub packet at 0x80918
		loop until the word at 0x80D40 is zero (decrementing the value each time): this is a number of bytes
		load the bytes starting at (0x80B28+(word at 0x80D3E)) (incrementing the word value at 0x80D3E on each loop)
		compare these bytes to an allowable packet structure:
			first input byte must be '85'
			second input byte is number of bytes in (sub) packet (excluding sum byte)
			the last byte is the sum of all other bytes in the (sub) packet
		The third through fifth byte in the packet are the second highest to lowest bytes in a long word address
			(the highest order byte is zero -> i.e. [00 3rd 4th 5th] -> this creates the destination address
			for flashing data)
		Copy 'number of bytes in packet (excluding sum byte) - 5' bytes from 0x8091D to the address
		specified by the long word in the combined byte and reprogramming the flash memory

	compare the number of recieved bytes (at 0x80004.L) to the number on 0x80D28.L
	then return a serial message depending on if the numbers match:
		long word on $80D28 =< number of received bytes ->0x02, 0x72, 0xFF, 0x73 -> then we're done
		long word on $80D28 > number of received bytes -> 0x02, 0x72, 0x00, 0x74,
				prepare to read in another packet...
		both messages need a serial data response of: 0xFD (11111101) within 10 interrupt counts

	if we're reading in another packet- check for the first data byte to be '112' again
		re-pass data through the decoder and re-flashing routines and repeat the process
		until we get a new first data byte or until long word on $80D28 =< number of received bytes

	this routine also looks for packets where the first data byte is '115'
	if '115'
		check -> if the word at 0x80D36 != word at 0x80D38
			send serial: 0x02, 0x72, 0xFF, 0x73 -> then we're done
			if the words are equal:
			send serial: 0x02, 0x72, 0x00, 0x74 -> then we're done
		both messages need a response of: 0xFD (11111101) within 10 interrupt counts
	if the first data byte is not '112' '113' or '115', attempt to read in another serial packet
		and apply the above rules.

	a fault at many locations in this process results in an impassible infinite loop

	 */

	uint32_t decodeTable[16] = {0x7,0xF,0x17,0x2F,0x5D,0xBA,0x174,0x2E8,0x5D0,0xBA0,0x1740,0x2E80,0x5D00,0xBA00,0x17401,0x2E801};
	uint32_t decodeMod = 380951;
	uint32_t decodeMultiple = 3182;
	int byteIndex = -1;
	uint16_t wordSum = 0;
	uint16_t wordSumFromDecode = 1;
	bool firstPacket = true;

	if(encodeByteSet == 1)
	{
		//switch to the byte set in the DEV ECU
		decodeMod = 119619;
		decodeMultiple = 20096;
		uint32_t decodeTableDevBytes[16] = {0x1,0x3,0x6,0xC,0x19,0x30,0x64,0xC8,0x18C,0x3EB,0x708,0xEA4,0x1CB6,0x3B6B,0x74D0,0xE9A1};
		for(int i = 0; i < 16; i++)
		{
			decodeTable[i] = decodeTableDevBytes[i];
		}
	}

	//open a file and begin reading in the data bytes to send
	ifstream myfile("encodedFlashBytes.txt");//line by line are decimal values denoting bytes
	ofstream decodedByteFile("testDecodedFlashPacket.txt", ios::trunc);
	decodedByteFile.close();
	if (myfile.is_open())
	{
		string line = "";
		uint32_t decodedWordSumIndex = 0;
		int bytePlace = 0;
		uint8_t mainPacketSum = 0;
		uint8_t bytesInPacket = 0;
		uint8_t packetIndicatorByte = 0;
		uint8_t deconvolutedBytes[256] = {0};
		uint8_t numDeconvolutedBytes = 0;
		uint32_t tempByte = 0;
		uint16_t word80D3A = 9744;
		while(getline(myfile, line))
		{
			ofstream decodedByteFile("testDecodedFlashPacket.txt", ios::app);
			if(bytePlace < 16 || (bytePlace+1) < bytesInPacket)
			{
				mainPacketSum += getByteFromLine(line);
			}

			//keep track of our position within the packet
			if(bytePlace > 1 && bytePlace < bytesInPacket - 1)
			{
				//these bytes are counted in the position index
				byteIndex += 1;
				//cout << "byte index: " << (int) byteIndex << ", index to check" << (int) decodedWordSumIndex - 5 << '\n';
			}

			//interpret the bytes in the packet
			if(bytePlace == 0)
			{
				bytesInPacket = getByteFromLine(line);
				bytesInPacket += 2;
				decodedByteFile << "bytes in packet: " << (int) bytesInPacket << '\n';
			}
			else if(bytePlace == 1)
			{
				packetIndicatorByte = getByteFromLine(line);//112, 113 or 115
				if(packetIndicatorByte != 112 && packetIndicatorByte != 115)
				{
					cout << "unknown packet indicator" << '\n';
				}
				decodedByteFile << "packet ID: " << (int) packetIndicatorByte << '\n';
			}
			else if(bytePlace > 1 && bytePlace < 6)
			{
				uint8_t byte = getByteFromLine(line);
				decodedWordSumIndex = decodedWordSumIndex*256;
				decodedWordSumIndex += byte;
				word80D3A += byte;
				if(bytePlace == 5)//invert the bits
				{
					word80D3A = ~word80D3A;
					decodedByteFile << "word 80d3a: " << (int) word80D3A << ", index of decoded word sum: " << (int) (decodedWordSumIndex - 5) << '\n';
				}
			}
			else if(bytePlace > 17)
			{
				uint8_t byte = getByteFromLine(line);
				//decodedByteFile << "decode byte: " << (int) byte << ", ";
				if((bytePlace-18)%3 == 0)
				{
					tempByte += byte;
				}
				if((bytePlace-18)%3 == 1)
				{
					tempByte += 256*byte;
				}
				//every third byte, stop to deconvolute bytes
				if((bytePlace-18)%3 == 2)
				{
					tempByte += 256*256*byte;
					//decodedByteFile << "encoded long word: " << (int) tempByte;
					//now, decode the byte
					/*uint32_t decodeTable[16] = {0x7,0xF,0x17,0x2F,0x5D,0xBA,0x174,0x2E8,0x5D0,0xBA0,0x1740,0x2E80,0x5D00,0xBA00,0x17401,0x2E801};
					uint32_t decodeMod = 380951;
					uint32_t decodeMultiple = 3182;
					if(encodeByteSet == 1)
					{
						//switch to the byte set in the DEV ECU
						decodeMod = 119619;
						decodeMultiple = 20096;
						uint32_t decodeTableDevBytes[16] = {0x1,0x3,0x6,0xC,0x19,0x30,0x64,0xC8,0x18C,0x3EB,0x708,0xEA4,0x1CB6,0x3B6B,0x74D0,0xE9A1};
						for(int i = 0; i < 16; i++)
						{
							decodeTable[i] = decodeTableDevBytes[i];
						}
					}*/
					uint16_t decodedWord = 0;
					uint16_t decodingBit = 32768;
					uint32_t remainder = tempByte%decodeMod;
					uint32_t product = decodeMultiple*remainder;
					uint32_t comparison = product%decodeMod;
					//decodedByteFile << ", comparison: " << (int) comparison << '\n';

					for(int i = 15; i > -1; i--)
					{
						//decodedByteFile << ", decodeTable: " << (int) decodeTable[i];
						if(comparison >= decodeTable[i])
						{
							//decodedByteFile << ", bit set: " << (int) decodingBit << ' ';
							decodedWord += decodingBit;
							comparison -= decodeTable[i];
						}
						else
						{
							//decodedByteFile << ", no bit set: " <<(int) decodingBit << ' ';
						}
						decodingBit = decodingBit / 2;
						//decodedByteFile << '\n';
					}
					decodedWord = decodedWord - word80D3A;
					//decodedByteFile << ", decoded word: " << (int) decodedWord << '\n';

					//split this into two bytes and store in the array
					uint8_t decodedByteLow = decodedWord%256;
					uint8_t decodedByteHigh = (decodedWord - decodedByteLow)/256;
					//decodedByteFile << ", decoded bytes: " << (int) decodedByteHigh << ' ' << (int) decodedByteLow << '\n';
					deconvolutedBytes[numDeconvolutedBytes] = decodedByteLow;//remember that we're little endian
					deconvolutedBytes[numDeconvolutedBytes + 1] = decodedByteHigh;//so this is the low order byte in the decoded packet
					tempByte = 0;
					numDeconvolutedBytes += 2;

					//then, check our decoded packet sum
					if(byteIndex == decodedWordSumIndex-5)
					{
						wordSumFromDecode = decodedByteLow + 256*decodedByteHigh;
						cout << "Sum of decoded words: " << wordSum << ", same- as encoded: " << wordSumFromDecode << '\n';
					}
					else
					{
						wordSum = wordSum + decodedByteLow + decodedByteHigh;
					}
				}
			}
			bytePlace += 1;

			//if we've read in a whole packet, interpret the deconvoluted bytes
			//then reset the packetPlace counter and number of bytes in
			//packet to read in the next packet
			if(bytePlace == bytesInPacket)
			{
				if(packetIndicatorByte == 112)
				{
					//print the sum to check manually
					cout << "main packet checksum calc for manual check: " << (int) mainPacketSum << '\n';

					uint16_t subpacketIndex = 0;
					if(firstPacket)
					{
						//the 4th through 10th byte are byte flags for
						//sector erase functions, so we'll simply print these
						decodedByteFile << "Sector erase flags from subPacket: ";
						for(int i = 3; i < 11; i++)
						{
							decodedByteFile << (int) deconvolutedBytes[i] << ' ';
						}
						decodedByteFile << '\n';
						firstPacket = false;
						subpacketIndex += 11;
					}

					//byte index 11 is '85', 12 is '#bytes, excluding sumbyte'
					decodedByteFile << "SubPacket ID flag: " << (int) deconvolutedBytes[subpacketIndex] << '\n';
					decodedByteFile << "Number of data bytes in SubPacket: " << (int) deconvolutedBytes[subpacketIndex+1] << '\n';
					uint8_t subPacketByteNumber = deconvolutedBytes[subpacketIndex+1];//number of bytes in sub packet, excluding sum byte
					//bytes 13 through 15 are address bytes
					string addr1 = decToHex(deconvolutedBytes[subpacketIndex+2]);
					string addr2 = decToHex(deconvolutedBytes[subpacketIndex+3]);
					string addr3 = decToHex(deconvolutedBytes[subpacketIndex+4]);
					decodedByteFile << "SubPacket Address: 0x" << addr1 << addr2 << addr3 << '\n';
					uint8_t sum = deconvolutedBytes[subpacketIndex] + deconvolutedBytes[subpacketIndex+1]  + deconvolutedBytes[subpacketIndex+2]  + deconvolutedBytes[subpacketIndex+3]  + deconvolutedBytes[subpacketIndex+4];
					for(uint8_t i = 5+subpacketIndex; i < subPacketByteNumber+subpacketIndex; i++)
					{
						sum += deconvolutedBytes[i];
						string data = decToHex(deconvolutedBytes[i]);
						decodedByteFile << data;
						if((i-(5+10))%16 == 0)
						{
							decodedByteFile << '\n';
						}
					}
					decodedByteFile << '\n';
					if(deconvolutedBytes[subPacketByteNumber+subpacketIndex] != sum)
					{
						cout << "subPacketChecksum error: sum =" << (int) sum << ", packet byte =" << (int) deconvolutedBytes[subPacketByteNumber+11] << '\n';
					}

					//reset global variables
					subpacketIndex = 0;
					bytePlace = 0;
					bytesInPacket = 0;
					mainPacketSum = 0;
					numDeconvolutedBytes = 0;
					tempByte = 0;
					word80D3A = 9744;
				}
				else if(packetIndicatorByte == 115)
				{
					decodedByteFile << "Exit reflash mode" << '\n';
				}
			}
			decodedByteFile.close();
		}
	}

	myfile.close();
}


uint8_t getByteFromLine(string line)
{
	uint8_t lineByte = 0;
	//ofstream byteInterpreter("getByteFromLine.txt", ios::app);
	//byteInterpreter << "line: " << line << ", line length: " << (int) line.length() << ", build: ";
	for(uint8_t i = 0; i < line.length(); i++)
	{
		if(line[i] > 47 && line[i] < 58)//only take numbers
		{
			lineByte = lineByte*10;
			lineByte += line[i] - 48;
			//byteInterpreter << (int) lineByte << ' ';
		}
	}
	//byteInterpreter << ", line byte: " << (int) lineByte << '\n';
	//byteInterpreter.close();
	return lineByte;
}


HANDLE initializeReflashMode(HANDLE serialPortHandle, TCHAR* ListItem, bool &success)
{

	//read in and save the serial port settings
	DCB dcb;
	COMMTIMEOUTS timeouts = { 0 };
	GetCommState(serialPortHandle, &dcb);//read in the current com config settings in order to save them
	GetCommTimeouts(serialPortHandle,&timeouts);//read in the current com timeout settings to save them


	//close the serial port which we need to access with another
	//API
	CloseHandle(serialPortHandle);

	//first, we'll pull the k-line low using the FTDI chips bit bang
	//we do this using FTDI's D2XX API
	//so we have to initialize that chip in a different way
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

	ftStatus = FT_OpenEx(Buffer1,FT_OPEN_BY_DESCRIPTION,&ftdiHandle);//initialize the FTDI device
	ftStatus = FT_SetBitMode(ftdiHandle, 0xFF, FT_BITMODE_RESET);//reset all FTDI pins
	ftStatus = FT_SetBitMode(ftdiHandle, 0x1, FT_BITMODE_SYNC_BITBANG);//set TxD pin as output in the bit bang operating mode
	ftStatus = FT_SetBaudRate(ftdiHandle, FT_BAUD_115200);//set the baud rate at which the pins can be updated
	ftStatus = FT_SetDataCharacteristics(ftdiHandle,FT_BITS_8,FT_STOP_BITS_1,FT_PARITY_NONE);//set up the com port parameters
	ftStatus = FT_SetTimeouts(ftdiHandle,200,100);//read timeout of 200ms, write timeout of 100 ms


	//in order to pull the l-line low, we'll send a bunch of '0's -> this works because the
	//VAG com cable has the k and l lines attached to each other.
	uint8_t fakeBytes[10] = {0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x1};//array of bits to send
	uint8_t fakeByte[1];//a byte to send (only the 0th bit matters, though)
	DWORD Written = 0;
	cout << "pull k and l lines low" << '\n';
	for(int i = 0; i < 10; i++)//loop through and send the '0's manually bit-by-bit
	{
		//no extra delay to prolong the amount of time we pull the l-line low
		fakeByte[0] = fakeBytes[i];//update the bit to send from the array
		ftStatus = FT_Write(ftdiHandle,fakeByte,1,&Written);//send one bit at a time
	}
	LARGE_INTEGER startLoopTime,loopTime;
	LARGE_INTEGER systemFreq;        // ticks per second
	QueryPerformanceFrequency(&systemFreq);// get ticks per second
	QueryPerformanceCounter(&startLoopTime);// start timer

	cout << "switch to serial comms.." << '\n';

	ftStatus = FT_SetBitMode(ftdiHandle, 0xFF, FT_BITMODE_RESET);//reset all FTDI pins or else the device stays in bit bang mode
	FT_Close(ftdiHandle);//close the FTDI device

	//re-open the serial port which is used for commuincation
	serialPortHandle = CreateFile(ListItem,GENERIC_READ | GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
	SetCommState(serialPortHandle, &dcb);//restore the COM port config
	SetCommTimeouts(serialPortHandle,&timeouts);//restore the COM port timeouts

	QueryPerformanceCounter(&loopTime);
	double runTime = (loopTime.QuadPart - startLoopTime.QuadPart)*1000 / systemFreq.QuadPart;// compute the elapsed time in milliseconds
	double elapsedRunTime = runTime/1000;//this is an attempt to get post-decimal points
	std::cout << "switch time: " << elapsedRunTime << '\n';

	uint8_t toSend[3] = {1,113,114};//{1,129,129} -> if we want to switch to 28.416 kBaud
	uint8_t recvdBytes[4] = {0,0,0,0};//a buffer which can be used to store read in bytes
	success = false;
	for(int i = 0; i < 100; i++)
	{
		QueryPerformanceCounter(&loopTime);
		writeToSerialPort(serialPortHandle, toSend, 3);//send
		readFromSerialPort(serialPortHandle, recvdBytes,3,1000);//read in bytes in up to 1000 ms
		if(recvdBytes[0] == 0x8D || recvdBytes[1] == 0x8D || recvdBytes[2] == 0x8D || recvdBytes[3] == 0x8D)
		{
			readFromSerialPort(serialPortHandle, recvdBytes,2,1000);
			//the read-in byte should be 0x8D (or 0x7E at new baud rate)
			runTime = (loopTime.QuadPart - startLoopTime.QuadPart)*1000 / systemFreq.QuadPart;// compute the elapsed time in milliseconds
			elapsedRunTime = runTime/1000;//this is an attempt to get post-decimal points
			std::cout << "loop time: " << elapsedRunTime << '\n';
			success = true;
			break;
		}
	}

	if(success == false)
	{
		cout << "Timing between pushing 'reflash' and turning on ECU power was incorrect." << '\n' << "Try again." << '\n';
	}

	return serialPortHandle;
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

uint8_t readCodedMessageIntoBuffer(long &lastPosition, uint8_t* byteBuffer)
{
	ifstream myfile("encodedFlashBytes.txt");//line by line are decimal values denoting bytes
	uint8_t bytesInPacket = 0;
	if (myfile.is_open())
	{
		myfile.seekg(lastPosition);//return to the last opened position
		string line = "";
		int bytesToRead = 0;
		int bytePlace = 0;
		int lineSize = 0;
		while(getline(myfile, line))
		{
			lineSize = line.length() + 2 ;//include newline char and return
			lastPosition += lineSize;
			uint8_t currentByte = getByteFromLine(line);
			//cout << (int) currentByte << ' ' << line << ' ' << (int) lineSize << '\n';
			if(bytePlace == 0)
			{
				bytesInPacket = currentByte;
				bytesInPacket += 2;
			}

			byteBuffer[bytePlace] = currentByte;
			bytePlace += 1;

			if(bytePlace == bytesInPacket)
			{
				break;
			}
		}
	}
	myfile.close();
	return bytesInPacket;
}

void calculateCheckBytes(uint8_t *packet, uint16_t packetLength)
{
	//calculate the four check bytes for the T4 stage I bootloader
	//serial packet. Pass in the packet without the check bytes
	//and this will modify the packet to include the bytes. Be sure
	//that the packet length includes space for the check bytes

	uint32_t polynomial = 291;
	uint8_t lookupBytes[1024] = {0x00,0x00,0x00,0x00,0x77,0x07,0x30,0x96,0xEE,0x0E,0x61,0x2C,0x99,0x09,0x51,0xBA,0x07,0x6D,0xC4,0x19,0x70,0x6A,0xF4,0x8F,0xE9,0x63,0xA5,0x35,0x9E,0x64,0x95,0xA3,0x0E,0xDB,0x88,0x32,0x79,0xDC,0xB8,0xA4,0xE0,0xD5,0xE9,0x1E,0x97,0xD2,0xD9,0x88,0x09,0xB6,0x4C,0x2B,0x7E,0xB1,0x7C,0xBD,0xE7,0xB8,0x2D,0x07,0x90,0xBF,0x1D,0x91,0x1D,0xB7,0x10,0x64,0x6A,0xB0,0x20,0xF2,0xF3,0xB9,0x71,0x48,0x84,0xBE,0x41,0xDE,0x1A,0xDA,0xD4,0x7D,0x6D,0xDD,0xE4,0xEB,0xF4,0xD4,0xB5,0x51,0x83,0xD3,0x85,0xC7,0x13,0x6C,0x98,0x56,0x64,0x6B,0xA8,0xC0,0xFD,0x62,0xF9,0x7A,0x8A,0x65,0xC9,0xEC,0x14,0x01,0x5C,0x4F,0x63,0x06,0x6C,0xD9,0xFA,0x0F,0x3D,0x63,0x8D,0x08,0x0D,0xF5,0x3B,0x6E,0x20,0xC8,0x4C,0x69,0x10,0x5E,0xD5,0x60,0x41,0xE4,0xA2,0x67,0x71,0x72,0x3C,0x03,0xE4,0xD1,0x4B,0x04,0xD4,0x47,0xD2,0x0D,0x85,0xFD,0xA5,0x0A,0xB5,0x6B,0x35,0xB5,0xA8,0xFA,0x42,0xB2,0x98,0x6C,0xDB,0xBB,0xC9,0xD6,0xAC,0xBC,0xF9,0x40,0x32,0xD8,0x6C,0xE3,0x45,0xDF,0x5C,0x75,0xDC,0xD6,0x0D,0xCF,0xAB,0xD1,0x3D,0x59,0x26,0xD9,0x30,0xAC,0x51,0xDE,0x00,0x3A,0xC8,0xD7,0x51,0x80,0xBF,0xD0,0x61,0x16,0x21,0xB4,0xF4,0xB5,0x56,0xB3,0xC4,0x23,0xCF,0xBA,0x95,0x99,0xB8,0xBD,0xA5,0x0F,0x28,0x02,0xB8,0x9E,0x5F,0x05,0x88,0x08,0xC6,0x0C,0xD9,0xB2,0xB1,0x0B,0xE9,0x24,0x2F,0x6F,0x7C,0x87,0x58,0x68,0x4C,0x11,0xC1,0x61,0x1D,0xAB,0xB6,0x66,0x2D,0x3D,0x76,0xDC,0x41,0x90,0x01,0xDB,0x71,0x06,0x98,0xD2,0x20,0xBC,0xEF,0xD5,0x10,0x2A,0x71,0xB1,0x85,0x89,0x06,0xB6,0xB5,0x1F,0x9F,0xBF,0xE4,0xA5,0xE8,0xB8,0xD4,0x33,0x78,0x07,0xC9,0xA2,0x0F,0x00,0xF9,0x34,0x96,0x09,0xA8,0x8E,0xE1,0x0E,0x98,0x18,0x7F,0x6A,0x0D,0xBB,0x08,0x6D,0x3D,0x2D,0x91,0x64,0x6C,0x97,0xE6,0x63,0x5C,0x01,0x6B,0x6B,0x51,0xF4,0x1C,0x6C,0x61,0x62,0x85,0x65,0x30,0xD8,0xF2,0x62,0x00,0x4E,0x6C,0x06,0x95,0xED,0x1B,0x01,0xA5,0x7B,0x82,0x08,0xF4,0xC1,0xF5,0x0F,0xC4,0x57,0x65,0xB0,0xD9,0xC6,0x12,0xB7,0xE9,0x50,0x8B,0xBE,0xB8,0xEA,0xFC,0xB9,0x88,0x7C,0x62,0xDD,0x1D,0xDF,0x15,0xDA,0x2D,0x49,0x8C,0xD3,0x7C,0xF3,0xFB,0xD4,0x4C,0x65,0x4D,0xB2,0x61,0x58,0x3A,0xB5,0x51,0xCE,0xA3,0xBC,0x00,0x74,0xD4,0xBB,0x30,0xE2,0x4A,0xDF,0xA5,0x41,0x3D,0xD8,0x95,0xD7,0xA4,0xD1,0xC4,0x6D,0xD3,0xD6,0xF4,0xFB,0x43,0x69,0xE9,0x6A,0x34,0x6E,0xD9,0xFC,0xAD,0x67,0x88,0x46,0xDA,0x60,0xB8,0xD0,0x44,0x04,0x2D,0x73,0x33,0x03,0x1D,0xE5,0xAA,0x0A,0x4C,0x5F,0xDD,0x0D,0x7C,0xC9,0x50,0x05,0x71,0x3C,0x27,0x02,0x41,0xAA,0xBE,0x0B,0x10,0x10,0xC9,0x0C,0x20,0x86,0x57,0x68,0xB5,0x25,0x20,0x6F,0x85,0xB3,0xB9,0x66,0xD4,0x09,0xCE,0x61,0xE4,0x9F,0x5E,0xDE,0xF9,0x0E,0x29,0xD9,0xC9,0x98,0xB0,0xD0,0x98,0x22,0xC7,0xD7,0xA8,0xB4,0x59,0xB3,0x3D,0x17,0x2E,0xB4,0x0D,0x81,0xB7,0xBD,0x5C,0x3B,0xC0,0xBA,0x6C,0xAD,0xED,0xB8,0x83,0x20,0x9A,0xBF,0xB3,0xB6,0x03,0xB6,0xE2,0x0C,0x74,0xB1,0xD2,0x9A,0xEA,0xD5,0x47,0x39,0x9D,0xD2,0x77,0xAF,0x04,0xDB,0x26,0x15,0x73,0xDC,0x16,0x83,0xE3,0x63,0x0B,0x12,0x94,0x64,0x3B,0x84,0x0D,0x6D,0x6A,0x3E,0x7A,0x6A,0x5A,0xA8,0xE4,0x0E,0xCF,0x0B,0x93,0x09,0xFF,0x9D,0x0A,0x00,0xAE,0x27,0x7D,0x07,0x9E,0xB1,0xF0,0x0F,0x93,0x44,0x87,0x08,0xA3,0xD2,0x1E,0x01,0xF2,0x68,0x69,0x06,0xC2,0xFE,0xF7,0x62,0x57,0x5D,0x80,0x65,0x67,0xCB,0x19,0x6C,0x36,0x71,0x6E,0x6B,0x06,0xE7,0xFE,0xD4,0x1B,0x76,0x89,0xD3,0x2B,0xE0,0x10,0xDA,0x7A,0x5A,0x67,0xDD,0x4A,0xCC,0xF9,0xB9,0xDF,0x6F,0x8E,0xBE,0xEF,0xF9,0x17,0xB7,0xBE,0x43,0x60,0xB0,0x8E,0xD5,0xD6,0xD6,0xA3,0xE8,0xA1,0xD1,0x93,0x7E,0x38,0xD8,0xC2,0xC4,0x4F,0xDF,0xF2,0x52,0xD1,0xBB,0x67,0xF1,0xA6,0xBC,0x57,0x67,0x3F,0xB5,0x06,0xDD,0x48,0xB2,0x36,0x4B,0xD8,0x0D,0x2B,0xDA,0xAF,0x0A,0x1B,0x4C,0x36,0x03,0x4A,0xF6,0x41,0x04,0x7A,0x60,0xDF,0x60,0xEF,0xC3,0xA8,0x67,0xDF,0x55,0x31,0x6E,0x8E,0xEF,0x46,0x69,0xBE,0x79,0xCB,0x61,0xB3,0x8C,0xBC,0x66,0x83,0x1A,0x25,0x6F,0xD2,0xA0,0x52,0x68,0xE2,0x36,0xCC,0x0C,0x77,0x95,0xBB,0x0B,0x47,0x03,0x22,0x02,0x16,0xB9,0x55,0x05,0x26,0x2F,0xC5,0xBA,0x3B,0xBE,0xB2,0xBD,0x0B,0x28,0x2B,0xB4,0x5A,0x92,0x5C,0xB3,0x6A,0x04,0xC2,0xD7,0xFF,0xA7,0xB5,0xD0,0xCF,0x31,0x2C,0xD9,0x9E,0x8B,0x5B,0xDE,0xAE,0x1D,0x9B,0x64,0xC2,0xB0,0xEC,0x63,0xF2,0x26,0x75,0x6A,0xA3,0x9C,0x02,0x6D,0x93,0x0A,0x9C,0x09,0x06,0xA9,0xEB,0x0E,0x36,0x3F,0x72,0x07,0x67,0x85,0x05,0x00,0x57,0x13,0x95,0xBF,0x4A,0x82,0xE2,0xB8,0x7A,0x14,0x7B,0xB1,0x2B,0xAE,0x0C,0xB6,0x1B,0x38,0x92,0xD2,0x8E,0x9B,0xE5,0xD5,0xBE,0x0D,0x7C,0xDC,0xEF,0xB7,0x0B,0xDB,0xDF,0x21,0x86,0xD3,0xD2,0xD4,0xF1,0xD4,0xE2,0x42,0x68,0xDD,0xB3,0xF8,0x1F,0xDA,0x83,0x6E,0x81,0xBE,0x16,0xCD,0xF6,0xB9,0x26,0x5B,0x6F,0xB0,0x77,0xE1,0x18,0xB7,0x47,0x77,0x88,0x08,0x5A,0xE6,0xFF,0x0F,0x6A,0x70,0x66,0x06,0x3B,0xCA,0x11,0x01,0x0B,0x5C,0x8F,0x65,0x9E,0xFF,0xF8,0x62,0xAE,0x69,0x61,0x6B,0xFF,0xD3,0x16,0x6C,0xCF,0x45,0xA0,0x0A,0xE2,0x78,0xD7,0x0D,0xD2,0xEE,0x4E,0x04,0x83,0x54,0x39,0x03,0xB3,0xC2,0xA7,0x67,0x26,0x61,0xD0,0x60,0x16,0xF7,0x49,0x69,0x47,0x4D,0x3E,0x6E,0x77,0xDB,0xAE,0xD1,0x6A,0x4A,0xD9,0xD6,0x5A,0xDC,0x40,0xDF,0x0B,0x66,0x37,0xD8,0x3B,0xF0,0xA9,0xBC,0xAE,0x53,0xDE,0xBB,0x9E,0xC5,0x47,0xB2,0xCF,0x7F,0x30,0xB5,0xFF,0xE9,0xBD,0xBD,0xF2,0x1C,0xCA,0xBA,0xC2,0x8A,0x53,0xB3,0x93,0x30,0x24,0xB4,0xA3,0xA6,0xBA,0xD0,0x36,0x05,0xCD,0xD7,0x06,0x93,0x54,0xDE,0x57,0x29,0x23,0xD9,0x67,0xBF,0xB3,0x66,0x7A,0x2E,0xC4,0x61,0x4A,0xB8,0x5D,0x68,0x1B,0x02,0x2A,0x6F,0x2B,0x94,0xB4,0x0B,0xBE,0x37,0xC3,0x0C,0x8E,0xA1,0x5A,0x05,0xDF,0x1B,0x2D,0x02,0xEF,0x8D};//0x3878 to 0x3C74+4

	for(uint16_t i = 0; i < (packetLength - 4); i++ )
	{
		//loop through all packet bytes
		uint8_t packetByte = packet[i];//load the packet byte
		uint8_t keyShift = polynomial >> 8;//shift the polynomial 8 bits right
		uint32_t packetByteLong = packetByte + 170;//note the shift to long words
		packetByteLong = packetByteLong ^ polynomial;//exclusive OR
		packetByteLong = packetByteLong & 255;//byte mask as index for lookup long word
		uint32_t lookup = (lookupBytes[0+packetByteLong*4] << 24) + (lookupBytes[1+packetByteLong*4] << 16) + (lookupBytes[2+packetByteLong*4] << 8) + (lookupBytes[3+packetByteLong*4]);//collect the long word looup value
		polynomial = lookup ^ keyShift;//exclusive OR
	}

	//store the check bytes in the packet buffer in memory
	packet[packetLength-4] = (polynomial >> 24) & 255;
	packet[packetLength-3] = (polynomial >> 16) & 255;
	packet[packetLength-2] = (polynomial >> 8) & 255;
	packet[packetLength-1] = (polynomial) & 255;

	/*
	 * 0x001dac:	MOVE.L #291,D2	; source -> destination	7596 + 6	001001000011110000000000000000000000000100100011	243C00000123	$<#
0x001db2:	MOVE.W #0,D3	; source -> destination	7602 + 4	00110110001111000000000000000000	363C0000	6<
0x001db6:	BRA.B $32	; PC + 2 + dn(twos complement = 50 ) -> PC displacement: 52 points to: 0x001dea	7606 + 2	0110000000110010	6032	`2

0x001db8:	MOVE.W D3,D0	; source -> destination	7608 + 2	0011000000000011	3003	0
0x001dba:	ADDQ.W #1,D3	; immediate data + destination -> destination	7610 + 2	0101001001000011	5243	RC
	;clear the higher order word in D0
0x001dbc:	SWAP D0	; Register (31 thru 16) -> register (15 thru 0)	7612 + 2	0100100001000000	4840	H@
0x001dbe:	CLR.W D0	; 0 -> Destination	7614 + 2	0100001001000000	4240	B@
0x001dc0:	SWAP D0	; Register (31 thru 16) -> register (15 thru 0)	7616 + 2	0100100001000000	4840	H@
	;load the byte at (passed in address + counter) into D4 and mask to ensure only a byte is kept
0x001dc2:	MOVE.B ($0,A2,D0.L*1),D4	; source -> destination	7618 + 4	00011000001100100000100000000000	18320800	2
0x001dc6:	ANDI.L #255,D4	; immediate data && destination -> destination	7622 + 6	000000101000010000000000000000000000000011111111	0284000000FF	„ÿ
	;D2 starts as 291 = 00000001 00100011
0x001dcc:	MOVE.L D2,D0	; source -> destination	7628 + 2	0010000000000010	2002
0x001dce:	LSR.L #8,D0	; Destination shifted by Count -> Destination	7630 + 2	1110000010001000	E088	àˆ
0x001dd0:	MOVE.L D4,D1	; source -> destination	7632 + 2	0010001000000100	2204	"
0x001dd2:	ADDI.L #170,D1	; immediate data + destination -> destination	7634 + 6	000001101000000100000000000000000000000010101010	0681000000AA
0x001dd8:	MOVE.L D2,D6	; source -> destination	7640 + 2	0010110000000010	2C02	,
0x001dda:	EOR.L D6,D1	; source OR(exclusive) destination -> destination	7642 + 2	1011110110000001	BD81
	;D1 = low order byte of: ((passed in address + counter).B+170) EOR D2
0x001ddc:	ANDI.L #255,D1	; immediate data && destination -> destination	7644 + 6	000000101000000100000000000000000000000011111111	0281000000FF
	;load long word from 0x3878+4*D1 onto D6
0x001de2:	MOVE.L ($0,A3,D1.L*4),D6	; source -> destination	7650 + 4	00101100001100110001110000000000	2C331C00	,3
	;D0 starts as 0x01
0x001de6:	EOR.L D0,D6	; source OR(exclusive) destination -> destination	7654 + 2	1011000110000110	B186	±†
	;update D2 to be (0x3878+4*(((passed in address + counter).B+170) EOR D2).B) EOR (second lowest order byte of D2)
0x001de8:	MOVE.L D6,D2	; source -> destination	7656 + 2	0010010000000110	2406	$

0x001dea:	MOVEQ #0,D0	; Immediate data -> destination	7658 + 2	0111000000000000	7000	p
0x001dec:	MOVE.W D3,D0	; source -> destination	7660 + 2	0011000000000011	3003	0
0x001dee:	CMP.L D5,D0	; Destination - source -> cc	7662 + 2	1011000010000101	B085	°…
	;if D3.W < (loaded in long word), loop back
0x001df0:	BLT.B $c6	; if condition (Less than) true, then PC + 2 + dn -> PC, twos complement = -58  displacement: -56 points to: 0x001db8	7664 + 2	0110110111000110	6DC6	mÆ
	;D2 is our calculated number
0x001df2:	MOVE.L D2,D0	; source -> destination	7666 + 2	0010000000000010	2002
0x001df4:	MOVEQ #16,D1	; Immediate data -> destination	7668 + 2	0111001000010000	7210	r
	;look at the highest order byte in our calculated number
0x001df6:	LSR.L D1,D0	; Destination shifted by Count -> Destination	7670 + 2	1110001010101000	E2A8	â¨
0x001df8:	LSR.W #8,D0	; Destination shifted by Count -> Destination	7672 + 2	1110000001001000	E048	àH
	;at this point, D3 = the loaded in long word
0x001dfa:	MOVE.W D3,D6	; source -> destination	7674 + 2	0011110000000011	3C03	<
0x001dfc:	ADDQ.W #1,D3	; immediate data + destination -> destination	7676 + 2	0101001001000011	5243	RC
	;clear the higher order word in D6
0x001dfe:	SWAP D6	; Register (31 thru 16) -> register (15 thru 0)	7678 + 2	0100100001000110	4846	HF
0x001e00:	CLR.W D6	; 0 -> Destination	7680 + 2	0100001001000110	4246	BF
0x001e02:	SWAP D6	; Register (31 thru 16) -> register (15 thru 0)	7682 + 2	0100100001000110	4846	HF
	;store the highest order byte if our calculated number at (input address+loaded in long word)
0x001e04:	MOVE.B D0,($0,A2,D6.L*1)	; source -> destination	7684 + 4	00010101100000000110100000000000	15806800	€h
0x001e08:	MOVE.L D2,D0	; source -> destination	7688 + 2	0010000000000010	2002
0x001e0a:	MOVEQ #16,D1	; Immediate data -> destination	7690 + 2	0111001000010000	7210	r
0x001e0c:	LSR.L D1,D0	; Destination shifted by Count -> Destination	7692 + 2	1110001010101000	E2A8	â¨
0x001e0e:	ANDI.W #255,D0	; immediate data && destination -> destination	7694 + 4	00000010010000000000000011111111	024000FF	@ÿ
	;at this point, D3 = loaded in long word + 1
0x001e12:	MOVE.W D3,D6	; source -> destination	7698 + 2	0011110000000011	3C03	<
0x001e14:	ADDQ.W #1,D3	; immediate data + destination -> destination	7700 + 2	0101001001000011	5243	RC
0x001e16:	SWAP D6	; Register (31 thru 16) -> register (15 thru 0)	7702 + 2	0100100001000110	4846	HF
0x001e18:	CLR.W D6	; 0 -> Destination	7704 + 2	0100001001000110	4246	BF
0x001e1a:	SWAP D6	; Register (31 thru 16) -> register (15 thru 0)	7706 + 2	0100100001000110	4846	HF
	;store the second highest order byte if our calculated number at (input address+loaded in long word+1)
0x001e1c:	MOVE.B D0,($0,A2,D6.L*1)	; source -> destination	7708 + 4	00010101100000000110100000000000	15806800	€h
0x001e20:	MOVE.L D2,D0	; source -> destination	7712 + 2	0010000000000010	2002
0x001e22:	ANDI.L #65535,D0	; immediate data && destination -> destination	7714 + 6	000000101000000000000000000000001111111111111111	02800000FFFF	€ÿÿ
0x001e28:	LSR.W #8,D0	; Destination shifted by Count -> Destination	7720 + 2	1110000001001000	E048	àH
0x001e2a:	MOVE.W D3,D1	; source -> destination	7722 + 2	0011001000000011	3203	2
0x001e2c:	ADDQ.W #1,D3	; immediate data + destination -> destination	7724 + 2	0101001001000011	5243	RC
0x001e2e:	SWAP D1	; Register (31 thru 16) -> register (15 thru 0)	7726 + 2	0100100001000001	4841	HA
0x001e30:	CLR.W D1	; 0 -> Destination	7728 + 2	0100001001000001	4241	BA
0x001e32:	SWAP D1	; Register (31 thru 16) -> register (15 thru 0)	7730 + 2	0100100001000001	4841	HA
	;store the second lowest order byte if our calculated number at (input address+loaded in long word+2)
0x001e34:	MOVE.B D0,($0,A2,D1.L*1)	; source -> destination	7732 + 4	00010101100000000001100000000000	15801800	€
0x001e38:	MOVE.L D2,D0	; source -> destination	7736 + 2	0010000000000010	2002
0x001e3a:	ANDI.L #65535,D0	; immediate data && destination -> destination	7738 + 6	000000101000000000000000000000001111111111111111	02800000FFFF	€ÿÿ
0x001e40:	ANDI.W #255,D0	; immediate data && destination -> destination	7744 + 4	00000010010000000000000011111111	024000FF	@ÿ
0x001e44:	MOVE.W D3,D1	; source -> destination	7748 + 2	0011001000000011	3203	2
0x001e46:	ADDQ.W #1,D3	; immediate data + destination -> destination	7750 + 2	0101001001000011	5243	RC
0x001e48:	SWAP D1	; Register (31 thru 16) -> register (15 thru 0)	7752 + 2	0100100001000001	4841	HA
0x001e4a:	CLR.W D1	; 0 -> Destination	7754 + 2	0100001001000001	4241	BA
0x001e4c:	SWAP D1	; Register (31 thru 16) -> register (15 thru 0)	7756 + 2	0100100001000001	4841	HA
	;store the lowest order byte if our calculated number at (input address+loaded in long word+3)
0x001e4e:	MOVE.B D0,($0,A2,D1.L*1)	; source -> destination	7758 + 4	00010101100000000001100000000000	15801800	€
	 */

	/*
	 * test this code:
	uint8_t packet[12] = {2,48,48,48,56,48,6,3,0,0,0,0};//this is the 'acknowledge response' packet
	calculateCheckBytes(packet, 12);
	for(int i = 0; i < 12; i++)
	{
		cout << (int) packet[i] << ',';
	}
	cout << '\n';
	//output: 2,48,48,48,56,48,6,3,90,5,223,255
	 */
}

uint16_t calculateCRCduringReflash(uint16_t currentCRC, uint8_t *packet, uint16_t packetLength)
{
	//input the current version of the CRC word as well as the packet which is being reflashed
	//this calculation will update the CRC word based on the bytes being reflashed

	//0x3CB0 to 0x3EAE + 2 from ROM - maybe this needs to be a lookup function...
	uint8_t lookupBytes[512] = {0x00,0x00,0x10,0x21,0x20,0x42,0x30,0x63,0x40,0x84,0x50,0xA5,0x60,0xC6,0x70,0xE7,0x81,0x08,0x91,0x29,0xA1,0x4A,0xB1,0x6B,0xC1,0x8C,0xD1,0xAD,0xE1,0xCE,0xF1,0xEF,0x12,0x31,0x02,0x10,0x32,0x73,0x22,0x52,0x52,0xB5,0x42,0x94,0x72,0xF7,0x62,0xD6,0x93,0x39,0x83,0x18,0xB3,0x7B,0xA3,0x5A,0xD3,0xBD,0xC3,0x9C,0xF3,0xFF,0xE3,0xDE,0x24,0x62,0x34,0x43,0x04,0x20,0x14,0x01,0x64,0xE6,0x74,0xC7,0x44,0xA4,0x54,0x85,0xA5,0x6A,0xB5,0x4B,0x85,0x28,0x95,0x09,0xE5,0xEE,0xF5,0xCF,0xC5,0xAC,0xD5,0x8D,0x36,0x53,0x26,0x72,0x16,0x11,0x06,0x30,0x76,0xD7,0x66,0xF6,0x56,0x95,0x46,0xB4,0xB7,0x5B,0xA7,0x7A,0x97,0x19,0x87,0x38,0xF7,0xDF,0xE7,0xFE,0xD7,0x9D,0xC7,0xBC,0x48,0xC4,0x58,0xE5,0x68,0x86,0x78,0xA7,0x08,0x40,0x18,0x61,0x28,0x02,0x38,0x23,0xC9,0xCC,0xD9,0xED,0xE9,0x8E,0xF9,0xAF,0x89,0x48,0x99,0x69,0xA9,0x0A,0xB9,0x2B,0x5A,0xF5,0x4A,0xD4,0x7A,0xB7,0x6A,0x96,0x1A,0x71,0x0A,0x50,0x3A,0x33,0x2A,0x12,0xDB,0xFD,0xCB,0xDC,0xFB,0xBF,0xEB,0x9E,0x9B,0x79,0x8B,0x58,0xBB,0x3B,0xAB,0x1A,0x6C,0xA6,0x7C,0x87,0x4C,0xE4,0x5C,0xC5,0x2C,0x22,0x3C,0x03,0x0C,0x60,0x1C,0x41,0xED,0xAE,0xFD,0x8F,0xCD,0xEC,0xDD,0xCD,0xAD,0x2A,0xBD,0x0B,0x8D,0x68,0x9D,0x49,0x7E,0x97,0x6E,0xB6,0x5E,0xD5,0x4E,0xF4,0x3E,0x13,0x2E,0x32,0x1E,0x51,0x0E,0x70,0xFF,0x9F,0xEF,0xBE,0xDF,0xDD,0xCF,0xFC,0xBF,0x1B,0xAF,0x3A,0x9F,0x59,0x8F,0x78,0x91,0x88,0x81,0xA9,0xB1,0xCA,0xA1,0xEB,0xD1,0x0C,0xC1,0x2D,0xF1,0x4E,0xE1,0x6F,0x10,0x80,0x00,0xA1,0x30,0xC2,0x20,0xE3,0x50,0x04,0x40,0x25,0x70,0x46,0x60,0x67,0x83,0xB9,0x93,0x98,0xA3,0xFB,0xB3,0xDA,0xC3,0x3D,0xD3,0x1C,0xE3,0x7F,0xF3,0x5E,0x02,0xB1,0x12,0x90,0x22,0xF3,0x32,0xD2,0x42,0x35,0x52,0x14,0x62,0x77,0x72,0x56,0xB5,0xEA,0xA5,0xCB,0x95,0xA8,0x85,0x89,0xF5,0x6E,0xE5,0x4F,0xD5,0x2C,0xC5,0x0D,0x34,0xE2,0x24,0xC3,0x14,0xA0,0x04,0x81,0x74,0x66,0x64,0x47,0x54,0x24,0x44,0x05,0xA7,0xDB,0xB7,0xFA,0x87,0x99,0x97,0xB8,0xE7,0x5F,0xF7,0x7E,0xC7,0x1D,0xD7,0x3C,0x26,0xD3,0x36,0xF2,0x06,0x91,0x16,0xB0,0x66,0x57,0x76,0x76,0x46,0x15,0x56,0x34,0xD9,0x4C,0xC9,0x6D,0xF9,0x0E,0xE9,0x2F,0x99,0xC8,0x89,0xE9,0xB9,0x8A,0xA9,0xAB,0x58,0x44,0x48,0x65,0x78,0x06,0x68,0x27,0x18,0xC0,0x08,0xE1,0x38,0x82,0x28,0xA3,0xCB,0x7D,0xDB,0x5C,0xEB,0x3F,0xFB,0x1E,0x8B,0xF9,0x9B,0xD8,0xAB,0xBB,0xBB,0x9A,0x4A,0x75,0x5A,0x54,0x6A,0x37,0x7A,0x16,0x0A,0xF1,0x1A,0xD0,0x2A,0xB3,0x3A,0x92,0xFD,0x2E,0xED,0x0F,0xDD,0x6C,0xCD,0x4D,0xBD,0xAA,0xAD,0x8B,0x9D,0xE8,0x8D,0xC9,0x7C,0x26,0x6C,0x07,0x5C,0x64,0x4C,0x45,0x3C,0xA2,0x2C,0x83,0x1C,0xE0,0x0C,0xC1,0xEF,0x1F,0xFF,0x3E,0xCF,0x5D,0xDF,0x7C,0xAF,0x9B,0xBF,0xBA,0x8F,0xD9,0x9F,0xF8,0x6E,0x17,0x7E,0x36,0x4E,0x55,0x5E,0x74,0x2E,0x93,0x3E,0xB2,0x0E,0xD1,0x1E,0xF0};

	uint32_t newCRC = currentCRC;

	//packet structure: [2,#,#,#,#,#,206,#,#,#,#,#,#,data 1, data 2, ... data n, 3, C1, C2, C3, C4]
	//length = (n) + 14 + 4
	uint8_t offset = 13;//first index of data in the packet
	for(uint16_t i = 0; i < packetLength - 18; i++)
	{
		//loop through the packet data bytes
		uint8_t packetByte = packet[i + offset];
		uint16_t index = (newCRC >> 8) & 255;//take the upper byte from the CRC as an index
		uint32_t lookupWord = 256*lookupBytes[0 + index*2] + lookupBytes[1 + index*2];//look up the value based on the index
		uint32_t CRCshift = (newCRC << 8);//shift the current CRC left by a byte
		uint32_t XORd = CRCshift ^ lookupWord;//take the exclusive OR of the lookup word and the shifted CRC
		uint32_t dataByte = packetByte + 170;//add 170 to the packet bytes
		newCRC = XORd ^ dataByte;//then take the XOR of the incremented packet byte and the convoluted, shifted CRC and lookup word
		//uint32_t hexString = index*2 + 0x3cb0;
		//cout << std::hex << hexString << ' ';
	}
	//cout << '\n';
	newCRC = (uint16_t) newCRC;
	return newCRC;
	/*

	 ;subroutine: input byte and word (each on long word)
	;return D0 = {[word from (0x3cb0+2*high order byte from input word)] EOR [input word * 256]} EOR {input byte + 170}
0x0027a8:	LINK.W A6,$fff8	; SP - 4 -> SP; An ->(SP); SP-> An; SP+dn -> SP, dn => twos complement = -8 	10152 + 4	01001110010101101111111111111000	4E56FFF8	NVÿø
0x0027ac:	MOVEM.L A2/D2,(A7)	; Registers -> destination (OR) source -> registers; register list mask: A7,A6,A5,A4,A3,A2,A1,A0,D7,D6,D5,D4,D3,D2,D1,D0 is reversed for -(An)	10156 + 4	01001000110101110000010000000100	48D70404	H×
	;$874ba was at 0x3cb0
0x0027b0:	LEA ($874ba).L,A2	; load effective address into address register	10160 + 6	010001011111100100000000000010000111010010111010	45F9000874BA	Eùtº
	;load in number at 14 past stack
	;this is the input word, onto D2 then D0
0x0027b6:	MOVE.W ($e,A6),D2	; source -> destination	10166 + 4	00110100001011100000000000001110	342E000E	4.
0x0027ba:	MOVE.W D2,D0	; source -> destination	10170 + 2	0011000000000010	3002	0
	;divide by 2^8 = 256
	;D0 = high order byte from input word
0x0027bc:	LSR.W #8,D0	; Destination shifted by Count -> Destination	10172 + 2	1110000001001000	E048	àH
0x0027be:	ANDI.W #255,D0	; immediate data && destination -> destination	10174 + 4	00000010010000000000000011111111	024000FF	@ÿ
	;clear the higher word in D0
0x0027c2:	SWAP D0	; Register (31 thru 16) -> register (15 thru 0)	10178 + 2	0100100001000000	4840	H@
0x0027c4:	CLR.W D0	; 0 -> Destination	10180 + 2	0100001001000000	4240	B@
0x0027c6:	SWAP D0	; Register (31 thru 16) -> register (15 thru 0)	10182 + 2	0100100001000000	4840	H@
	;load in word value at (0x3cb0+2*D0 bytes) and clear the higher word
0x0027c8:	MOVE.W ($0,A2,D0.L*2),D1	; source -> destination	10184 + 4	00110010001100100000101000000000	32320A00	22
0x0027cc:	SWAP D1	; Register (31 thru 16) -> register (15 thru 0)	10188 + 2	0100100001000001	4841	HA
0x0027ce:	CLR.W D1	; 0 -> Destination	10190 + 2	0100001001000001	4241	BA
0x0027d0:	SWAP D1	; Register (31 thru 16) -> register (15 thru 0)	10192 + 2	0100100001000001	4841	HA
0x0027d2:	MOVEQ #0,D0	; Immediate data -> destination	10194 + 2	0111000000000000	7000	p
0x0027d4:	MOVE.W D2,D0	; source -> destination	10196 + 2	0011000000000010	3002	0
	;mult by 2^8
	;D0 = input word * 256
0x0027d6:	ASL.L #8,D0	; Destination shifted by Count -> Destination	10198 + 2	1110000110000000	E180	á€
	;D1 = [word from (0x3cb0+2*high order byte from input word)] EOR [input word * 256]
0x0027d8:	EOR.L D0,D1	; source OR(exclusive) destination -> destination	10200 + 2	1011000110000001	B181
0x0027da:	MOVEQ #0,D0	; Immediate data -> destination	10202 + 2	0111000000000000	7000	p
	;load input byte onto D0
0x0027dc:	MOVE.B ($b,A6),D0	; source -> destination	10204 + 4	00010000001011100000000000001011	102E000B	.
0x0027e0:	SWAP D0	; Register (31 thru 16) -> register (15 thru 0)	10208 + 2	0100100001000000	4840	H@
0x0027e2:	CLR.W D0	; 0 -> Destination	10210 + 2	0100001001000000	4240	B@
0x0027e4:	SWAP D0	; Register (31 thru 16) -> register (15 thru 0)	10212 + 2	0100100001000000	4840	H@
0x0027e6:	ADDI.L #170,D0	; immediate data + destination -> destination	10214 + 6	000001101000000000000000000000000000000010101010	0680000000AA	€ª
	;D1 = {[word from (0x3cb0+2*high order byte from input word)] EOR [input word * 256]} EOR {input byte + 170}
0x0027ec:	EOR.L D0,D1	; source OR(exclusive) destination -> destination	10220 + 2	1011000110000001	B181
0x0027ee:	MOVE.L D1,D0	; source -> destination	10222 + 2	0010000000000001	2001
0x0027f0:	MOVEM.L (A7),A2/D2	; Registers -> destination (OR) source -> registers; register list mask: A7,A6,A5,A4,A3,A2,A1,A0,D7,D6,D5,D4,D3,D2,D1,D0 is reversed for -(An)	10224 + 4	01001100110101110000010000000100	4CD70404	L×
0x0027f4:	UNLK A6	; unlink: An -> SP; (SP) -> An; SP + 4 -> SP	10228 + 2	0100111001011110	4E5E	N^
0x0027f6:	RTS	; return from subroutine (SP) -> PC; SP + 4 -> SP

	 */
}


HANDLE initializeStageIReflashMode(HANDLE serialPortHandle, TCHAR* ListItem, bool &success)
{
	//read in and save the serial port settings
	DCB dcb;
	COMMTIMEOUTS timeouts = { 0 };
	GetCommState(serialPortHandle, &dcb);//read in the current com config settings in order to save them
	GetCommTimeouts(serialPortHandle,&timeouts);//read in the current com timeout settings to save them


	//close the serial port which we need to access with another
	//API
	CloseHandle(serialPortHandle);

	//first, we'll pull the k-line low using the FTDI chips bit bang
	//we do this using FTDI's D2XX API
	//so we have to initialize that chip in a different way
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

	ftStatus = FT_OpenEx(Buffer1,FT_OPEN_BY_DESCRIPTION,&ftdiHandle);//initialize the FTDI device
	ftStatus = FT_SetBitMode(ftdiHandle, 0xFF, FT_BITMODE_RESET);//reset all FTDI pins
	ftStatus = FT_SetBitMode(ftdiHandle, 0x1, FT_BITMODE_SYNC_BITBANG);//set TxD pin as output in the bit bang operating mode
	ftStatus = FT_SetBaudRate(ftdiHandle, FT_BAUD_115200);//set the baud rate at which the pins can be updated
	ftStatus = FT_SetDataCharacteristics(ftdiHandle,FT_BITS_8,FT_STOP_BITS_1,FT_PARITY_NONE);//set up the com port parameters
	ftStatus = FT_SetTimeouts(ftdiHandle,200,100);//read timeout of 200ms, write timeout of 100 ms


	//in order to pull the l-line low, we'll send a bunch of '0's -> this works because the
	//VAG com cable has the k and l lines attached to each other.
	uint8_t fakeBytes[10] = {0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x1};//array of bits to send
	uint8_t fakeByte[1];//a byte to send (only the 0th bit matters, though)
	DWORD Written = 0;
	cout << "pull k and l lines low" << '\n';
	for(int i = 0; i < 10; i++)//loop through and send the '0's manually bit-by-bit
	{
		//no extra delay to prolong the amount of time we pull the l-line low
		fakeByte[0] = fakeBytes[i];//update the bit to send from the array
		ftStatus = FT_Write(ftdiHandle,fakeByte,1,&Written);//send one bit at a time
	}
	LARGE_INTEGER startLoopTime,loopTime;
	LARGE_INTEGER systemFreq;        // ticks per second
	QueryPerformanceFrequency(&systemFreq);// get ticks per second
	QueryPerformanceCounter(&startLoopTime);// start timer

	cout << "switch to serial comms.." << '\n';

	ftStatus = FT_SetBitMode(ftdiHandle, 0xFF, FT_BITMODE_RESET);//reset all FTDI pins or else the device stays in bit bang mode
	FT_Close(ftdiHandle);//close the FTDI device

	//re-open the serial port which is used for commuincation
	serialPortHandle = CreateFile(ListItem,GENERIC_READ | GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
	SetCommState(serialPortHandle, &dcb);//restore the COM port config
	SetCommTimeouts(serialPortHandle,&timeouts);//restore the COM port timeouts

	QueryPerformanceCounter(&loopTime);
	double runTime = (loopTime.QuadPart - startLoopTime.QuadPart)*1000 / systemFreq.QuadPart;// compute the elapsed time in milliseconds
	double elapsedRunTime = runTime/1000;//this is an attempt to get post-decimal points
	std::cout << "switch time: " << elapsedRunTime << '\n';

	uint8_t toSend[3] = {83,89,78};//ascii 'SYN'
	//we expect a response of 73, 79 and 6E => 'syn'
	uint8_t recvdBytes[4] = {0,0,0,0};//a buffer which can be used to store read in bytes
	uint8_t sendIndex = 0;
	success = false;
	for(int i = 0; i < 100; i++)
	{
		uint8_t sendByte[1] = {toSend[sendIndex]};
		QueryPerformanceCounter(&loopTime);
		writeToSerialPort(serialPortHandle, sendByte, 1);//send
		readFromSerialPort(serialPortHandle, recvdBytes,1,1000);//read in bytes in up to 1000 ms
		if(recvdBytes[0] == 0x73 && sendByte[0] == 83)
		{
			sendIndex += 1;
			readFromSerialPort(serialPortHandle, recvdBytes,1,1000);
			runTime = (loopTime.QuadPart - startLoopTime.QuadPart)*1000 / systemFreq.QuadPart;// compute the elapsed time in milliseconds
			elapsedRunTime = runTime/1000;//this is an attempt to get post-decimal points
			std::cout << "loop time: " << elapsedRunTime << '\n';
		}
		else if(recvdBytes[0] == 0x79 && sendByte[0] == 89)
		{
			sendIndex += 1;
			readFromSerialPort(serialPortHandle, recvdBytes,1,1000);
		}
		else if(recvdBytes[0] == 0x6E && sendByte[0] == 78)
		{
			readFromSerialPort(serialPortHandle, recvdBytes,1,1000);
			success = true;
			break;
		}
	}

	if(success == false)
	{
		cout << "Timing between pushing 'reflash' and turning on ECU power was incorrect." << '\n' << "Try again." << '\n';
	}

	return serialPortHandle;
}

uint16_t buildStageIbootloaderPacket(uint8_t *packetBuffer, uint16_t bufferLength, uint8_t numBytesToSend, uint8_t numPacketsSent, string filename, long address, long &lastFilePosition)
{
	uint16_t packetLength = 0;
	//packet structure = [2,#,#,#,#,#,ID,#,#,#,#,#,#,..data..,3,C1,C2,C3,C4]
	if(bufferLength >= (numBytesToSend + 8 + 6 + 4))
	{
		packetLength = 18+numBytesToSend;
		packetBuffer[0] = 2;
		uint16_t tempNumSplit = numBytesToSend + 8 + 6;
		char numToSendString[4] = {48,48,48,48};
		uint32_t factor = 1000;
		for(uint8_t i = 0; i < 4; i++)
		{
			uint8_t split = tempNumSplit/factor;
			numToSendString[i] = split + 48;
			tempNumSplit -= split*factor;
			factor = factor/10;
		}
		packetBuffer[1] = numToSendString[0];
		packetBuffer[2] = numToSendString[1];
		packetBuffer[3] = numToSendString[2];
		packetBuffer[4] = numToSendString[3];
		packetBuffer[5] = 48;
		packetBuffer[6] = 206;

		tempNumSplit = numPacketsSent;
		char numPacketsString[6] = {48,48,48,48,48,48};
		factor = 100000;
		for(uint8_t i = 0; i < 6; i++)
		{
			uint8_t split = tempNumSplit/factor;
			numPacketsString[i] = split + 48;
			tempNumSplit -= split*factor;
			factor = factor/10;
		}
		packetBuffer[7] = numPacketsString[0];
		packetBuffer[8] = numPacketsString[1];
		packetBuffer[9] = numPacketsString[2];
		packetBuffer[10] = numPacketsString[3];
		packetBuffer[11] = numPacketsString[4];
		packetBuffer[12] = numPacketsString[5];

		uint32_t dataAddress = address;
		for(uint8_t i = 0; i < numBytesToSend; i++)
		{
			string readByte = srecReader(filename, dataAddress, lastFilePosition);
			uint8_t data = stringDec_to_int(readByte);
			dataAddress += 1;
			packetBuffer[i+13] = data;
		}
		packetBuffer[13+numBytesToSend] = 3;
		/*
		cout << "[ ";
		for(uint16_t i = 0; i< packetLength; i++)
		{
			cout << (int) packetBuffer[i] << ' ';
		}
		cout << ']' << '\n';
		 */
		calculateCheckBytes(packetBuffer, packetLength);
	}

	/*
	cout << "[ ";
	for(uint16_t i = 0; i< packetLength; i++)
	{
		cout << (int) packetBuffer[i] << ' ';
	}
	cout << ']' << '\n';
	*/

	return packetLength;
}

uint16_t stageIbootloaderRomFlashPacketBuilder(uint32_t &addressToFlash, long &bytesLeftToSend, uint16_t CRC, uint32_t &packetsSent, string filename, long &filePos)
{
	//uint16_t CRC = 291;
	uint8_t packetBuffer[300];
	//uint32_t packetsSent = 0;
	//long bytesLeftToSend = numberBytesToFlash;
	//long filePos = 0;
	uint32_t nextAddress = addressToFlash;
	if(bytesLeftToSend > 0)
	{
		uint8_t bytesToSend = 0;
		if(bytesLeftToSend > 255)
		{
			bytesToSend = 255;
		}
		else
		{
			bytesToSend = bytesLeftToSend;
		}
		bytesLeftToSend -= bytesToSend;

		uint16_t packetLength = buildStageIbootloaderPacket(packetBuffer, 300, bytesToSend, packetsSent, filename, nextAddress, filePos);
		//cout << "packet length: " << (int) packetLength << '\n';

		/*
		cout << "[ ";
		for(uint16_t i = 0; i < packetLength; i++)
		{
			cout << (int) packetBuffer[i] << ' ';
		}
		cout << ']' << '\n';
		*/

		CRC = calculateCRCduringReflash(CRC, packetBuffer, packetLength);
		//cout << (int) CRC << '\n';
		packetsSent += 1;
		nextAddress += bytesToSend;
		ofstream outputPacket("encodedFlashBytes.txt", ios::app);
		for(uint16_t i = 0; i < packetLength; i++)
		{
			outputPacket << (int) packetBuffer[i] << ' ';
		}
		outputPacket << '\n';
		outputPacket.close();
		addressToFlash = nextAddress;
		cout << (int) bytesLeftToSend << " bytes remain" << '\n';
	}

	return CRC;
}

uint16_t readPacketLineToBuffer(uint8_t *buffer, uint16_t bufferLength, long &lastFilePosition)
{
	uint16_t packetLength = 0;
	ifstream inputPacket("encodedFlashBytes.txt");

	if (inputPacket.is_open())
	{
		inputPacket.seekg(lastFilePosition);
		string fileline = "";
		getline(inputPacket,fileline);
		lastFilePosition += fileline.length() + 2;//2 extra chars for the /r/n
		uint16_t packetIndex = 0;
		uint8_t tempString[3] = {0,0,0};
		for(uint16_t i = 0; i < fileline.length(); i++)
		{
			if(packetIndex < bufferLength)
			{
				if(fileline[i] == ' ')
				{
					buffer[packetIndex] = tempString[0]*100 + tempString[1]*10 + tempString[2];
					tempString[0] = 0;
					tempString[1] = 0;
					tempString[2] = 0;
					packetIndex += 1;
					packetLength += 1;
				}
				else
				{
					tempString[0] = tempString[1];
					tempString[1] = tempString[2];
					tempString[2] = fileline[i] - 48;
				}
			}
		}
	}

	return packetLength;
}
