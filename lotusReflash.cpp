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
	//3 unknown bytes
	//8 'flash sector clear' flag bytes
	//1 ID byte
	//1 number byte
	//3 address bytes
	//1 sum byte
	// = 17 bytes-> make it 18 to be safe
	//this takes up 27 bytes in the main packet

	//255 - (19+27) = 209 bytes for data -> 2/3 -> 139 bytes -> round down to 138 to keep even

	//bring in a file which contains address ranges which we would like to reflash
	//string reflashRanges = "6-16-06 ECU read reflash ranges.txt";
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

	//cout << totalNumBytesFromSrec << '\n';

	//finally, open a file to save the packets to
	ofstream outputPacket("encodedFlashBytes.txt", ios::trunc);
	//ofstream encoding("watchEncodingProcess.txt", ios::trunc);
	//ofstream subPacketBytes("unencrypted sub packet.txt", ios::trunc);

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

			//subPacketBytes << "address: " << (int) startAddress[0] << ' ' << (int) startAddress[1] << ' ' << (int) startAddress[2]<< '\n';
			//subPacketBytes << "number of bytes: " << (int) numBytes << '\n';

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
				subPacket[16+i] = data;
			}
			//the calculate the sumByte and write data to the output file
			for(uint32_t k = 0; k < numBytes + 5; k++)
			{
				subPacket[16+numBytes] += subPacket[11+k];

			}
			/*subPacketBytes << "SubPacket: ";
			for(uint32_t k = 0; k < numBytes + 17;k++)
			{
				subPacketBytes<<(int) subPacket[k]<< ' ';
			}
			subPacketBytes << '\n';*/


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



			for(uint32_t z = 0; z < (numBytes+11+6+1); z+=2)
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

	//outputPacket << "finished" << '\n';
	//subPacketBytes.close();
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
		int bytesToRead = 0;
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
			if(bytePlace == 0)
			{
				bytesInPacket = getByteFromLine(line);
				bytesInPacket += 2;
				decodedByteFile << "bytes in packet: " << (int) bytesInPacket << '\n';
			}
			else if(bytePlace == 1)
			{
				packetIndicatorByte = getByteFromLine(line);//112, 113 or 115
				if(packetIndicatorByte != 112)
				{
					cout << "unknown packet indicator" << '\n';
				}
				decodedByteFile << "packet ID: " << (int) packetIndicatorByte << '\n';
			}
			else if(bytePlace > 1 && bytePlace < 6)
			{
				uint8_t byte = getByteFromLine(line);
				bytesToRead = bytesToRead*256;
				bytesToRead += byte;
				word80D3A += byte;
				if(bytePlace == 5)//invert the bits
				{
					word80D3A = ~word80D3A;
					decodedByteFile << "word 80d3a: " << (int) word80D3A << ", bytes to read: " << (int) bytesToRead << '\n';
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
					deconvolutedBytes[numDeconvolutedBytes] = decodedByteLow;
					deconvolutedBytes[numDeconvolutedBytes + 1] = decodedByteHigh;
					tempByte = 0;
					numDeconvolutedBytes += 2;
				}
			}
			bytePlace += 1;

			//if we've read in a whole packet, interpret the deconvoluted bytes
			//then reset the packetPlace counter and number of bytes in
			//packet to read in the next packet
			if(bytePlace == bytesInPacket)
			{
				//print the sum to check manually
				cout << "main packet checksum calc for manual check: " << (int) mainPacketSum << '\n';

				//the 4th through 10th byte are byte flags for
				//sector erase functions, so we'll simply print these
				decodedByteFile << "Sector erase flags from subPacket: ";
				for(int i = 3; i < 11; i++)
				{
					decodedByteFile << (int) deconvolutedBytes[i] << ' ';
				}
				decodedByteFile << '\n';

				//byte index 11 is '85', 12 is '#bytes, excluding sumbyte'
				decodedByteFile << "SubPacket ID flag: " << (int) deconvolutedBytes[11] << '\n';
				decodedByteFile << "Number of data bytes in SubPacket: " << (int) deconvolutedBytes[12] << '\n';
				uint8_t subPacketByteNumber = deconvolutedBytes[12];//number of bytes in sub packet, excluding sum byte
				//bytes 13 through 15 are address bytes
				string addr1 = decToHex(deconvolutedBytes[13]);
				string addr2 = decToHex(deconvolutedBytes[14]);
				string addr3 = decToHex(deconvolutedBytes[15]);
				decodedByteFile << "SubPacket Address: 0x" << addr1 << addr2 << addr3 << '\n';
				uint8_t sum = deconvolutedBytes[11] + deconvolutedBytes[12]  + deconvolutedBytes[13]  + deconvolutedBytes[14]  + deconvolutedBytes[15];
				for(uint8_t i = 5+11; i < subPacketByteNumber+11; i++)
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
				if(deconvolutedBytes[subPacketByteNumber+11] != sum)
				{
					cout << "subPacketChecksum error: sum =" << (int) sum << ", packet byte =" << (int) deconvolutedBytes[subPacketByteNumber+11] << '\n';
				}
				if(bytesToRead <= bytePlace)
				{
					break;
				}
				//reset global variables
				bytesToRead = 0;
				bytePlace = 0;
				bytesInPacket = 0;
				mainPacketSum = 0;
				numDeconvolutedBytes = 0;
				tempByte = 0;
				word80D3A = 9744;
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


HANDLE initializeReflashMode(HANDLE serialPortHandle, TCHAR* ListItem)
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
	uint8_t fakeBytes[10] = {0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0};//array of bits to send
	uint8_t fakeByte[1];//a byte to send (only the 0th bit matters, though)
	DWORD Written = 0;
	for(int i = 0; i < 10; i++)//loop through and send the '0's manually bit-by-bit
	{
		Sleep(100);//delay to prolong the amount of time we pull the l-line low
		fakeByte[0] = fakeBytes[i];//update the bit to send from the array
		ftStatus = FT_Write(ftdiHandle,fakeByte,1,&Written);//send one bit at a time
	}


	ftStatus = FT_SetBitMode(ftdiHandle, 0xFF, FT_BITMODE_RESET);//reset all FTDI pins or else the device stays in bit bang mode
	FT_Close(ftdiHandle);//close the FTDI device

	//re-open the serial port which is used for commuincation
	serialPortHandle = CreateFile(ListItem,GENERIC_READ | GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
	SetCommState(serialPortHandle, &dcb);//restore the COM port config
	SetCommTimeouts(serialPortHandle,&timeouts);//restore the COM port timeouts

	//invert third byte of serial response and send it back
	uint8_t toSend[3] = {1,113,114};//{1,129,129} -> if we want to switch to 29.787 kBaud
	Sleep(30);//delay
	writeToSerialPort(serialPortHandle, toSend, 3);//send

	uint8_t recvdBytes[3] = {0,0,0};//a buffer which can be used to store read in bytes
	readFromSerialPort(serialPortHandle, recvdBytes,1,500);//read in 1 bytes in up to 500 ms

	//the read-in byte should be 0x8D (or 0x7E at new baud rate)
	if(recvdBytes[0] != 0x8D)
	{
		cout << "init recv'd: " << (int) recvdBytes[0] << '\n';
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
					lineAddressHexString = line.substr(4,4);
					dataStartPoint = 8;
				}
				else if(line[1] == '2')//3 byte address
				{
					lineAddressHexString = line.substr(4,6);
					dataStartPoint = 10;
				}
				else if(line[1] == '3')//4 byte address
				{
					lineAddressHexString = line.substr(4,8);
					dataStartPoint = 12;
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
					bytesInLine -= dataStartPoint/2-1;

					if(lineAddress + bytesInLine < address)
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
