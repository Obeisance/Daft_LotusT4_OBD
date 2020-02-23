package daftOBD;

import java.util.Arrays;

public class Serial_Packet {
	//every serial packet that can be sent is ultimately made up of 
	//8-bit bytes of data
	int packetLength = 0;
	int packetBitLength = 0;
	byte[] packetData = new byte[0];
	int baudRate = 0;
	boolean lastByteChecksum = false;
	
	Input[] inputList = new Input[0];
	Output[] outputList = new Output[0];
	
	//some serial packets are not to be read depending on other parameters
	//the evaluation is performed based on comparison between
	//a condition string, and expression string which has dependents
	String[] dependent_names = new String[0];
	Output[] dependent_outputList = new Output[0];
	String expression = "";
	String condition = "";
	
	public Serial_Packet() {
		//we don't really need the constructor to do anything
	}
	
	public void setPacket(byte[] data) {
		//change the packet data completely
		this.packetData = data;
		this.packetLength = this.packetData.length;
		this.packetBitLength = this.packetData.length*8;
		refreshPacket();
	}
	
	public byte[] getPacket() {
		//return the packet
		if(shouldBeRead())
		{
			//The packet is automatically refreshed elsewhere, so this is the most up to date version
			return this.packetData;
		}
		else
		{
			//System.out.println("Skip special read");
			return new byte[0];
		}
	}
	
	public void appendByte(int data) {
		//add 8 bits of input data to the end of the current packet 
		
		//figure out if the byte needs to be split in order to be added
		int bitSplit = this.packetBitLength % 8;//the byte must be split if this is not zero
		int nextByte = data;
		int lastByte = 0;
		if(bitSplit != 0)
		{
			nextByte = (data << (8 - bitSplit)) & 0xFF;
			lastByte = (data & 0xFF) >> bitSplit;
			//if we are truly appending data, this masking action is not really necessary
			//since subsequent bits should all start as zero anyway
			int mask = 0;
			for(int i = 0; i < bitSplit; i++)
			{
				mask = mask | (0x1 << (8 - i));
			}
			lastByte = (convertToInt(this.packetData[this.packetLength - 1]) & mask) | lastByte; 
			changeByte(lastByte,this.packetLength - 1);
		}
		//append to the packet
		byte[] newPacket = Arrays.copyOf(this.packetData, this.packetLength + 1);
		newPacket[newPacket.length - 1] = convertFromInt(nextByte);
		this.packetData = newPacket;
		this.packetLength += 1;
		this.packetBitLength += 8;
	}
	
	public void changeByte(int data, int index) {
		//change a byte of data in the packet based on an integer input
		if(index >= 0 && index < this.packetData.length)
		{
			this.packetData[index] = convertFromInt(data);
		}
	}
	
	public void updateChecksum() {
		//call this function to update the last byte in the packet as a checksum byte
		//loop through the data and sum it all up
		if(this.lastByteChecksum)
		{
			int sum = calcChecksum(this.packetData);
			//System.out.println("checksum: " + sum);
			changeByte(sum,this.packetLength-1);
		}
	}
	
	public void appendChecksum() {
		//call this function to add a checksum byte to the end of the packet
		appendByte(0);
		this.lastByteChecksum = true;
		updateChecksum();
	}
	
	public int calcChecksum(byte[] data) {
		//calculate the checksum for the packet
		//assume that the sum is for all bytes except the last one
		int sum = 0;
		for(int i = 0; i < data.length - 1; i++) {
			sum += (int) data[i];
		}
		sum = sum & 0xFF;
		return sum;
	}
	
	private byte convertFromInt(int data) {
		//convert an integer input to a byte
		byte output = 0;
		data = data % 256;//The integer could be up to 64 bits: use only the lowest order byte
		if(data >= 0 && data <= 255)//this is some goofy number handling... i've lost the implication of this by now 
		{
			if(data > 127)
			{
				output = (byte) (data - 256);
			}
			else
			{
				output = (byte) data;
			}
		}
		else if(data < 0 && data >= -128){
			output = (byte) (data & 0xFF);//maybe we were handed data that already fit the 'byte' type behavior.. this line may actually be all that is needed in this function..
		}
		return output;
	}
	
	private int convertToInt(byte data) {
		//convert byte data to an integer
		int output = data & 0xFF;//the ff mask may not be necessary.
		return output;
	}
	
	public int getBaudrate() {
		return this.baudRate;
	}
	
	public void setBaudrate(int newBaud) {
		this.baudRate = newBaud;
	}
	
	public void appendInput(String dispString, String type)
	{
		//append a new input to the list of inputs
		inputList = Arrays.copyOf(inputList, inputList.length + 1);
		Input input_to_add = new Input(dispString, type, this.packetBitLength, this);//be careful to distinguish the bit position and bit index
		inputList[inputList.length - 1] = input_to_add;
		
		//then add the input data to the serial packet byte set
		int dataByte = input_to_add.intData;
		update_append_bits(dataByte, input_to_add.bit_start_position, input_to_add.bitLength);
	}
	
	public void updateOutputs()
	{
		//this function loops through all outputs associated with this packet
		//and updates their string value based on the current data in the serial
		//packet
		for(int i = 0; i < outputList.length; i++)
		{
			//System.out.print("Old output: " + outputList[i].value);
			outputList[i].update_output_value();
			//System.out.println(", new output: " + outputList[i].value);
		}
	}
	
	public void updateInputs()
	{
		//this function loops through all inputs associated with this packet
		//and updates their intData value based on current data in all
		//associated serial_packets and inputs/outputs
		for(int i = 0; i < inputList.length; i++)
		{
			//System.out.print("Old input: " + inputList[i].display_input);
			inputList[i].convert_update_input();
			//System.out.println(", new input: " + inputList[i].display_input);
		}
	}
	
	public int getShiftedBitData(int bitPosition, int bitLength)
	{
		//this function returns 'bitLength' data starting at 'bitPosition' within the serial packet
		//the data is shifted so that the lowest order bit is in position 0 of the returned value
		int data = 0;
		
		//figure out which data to get
		int byteIndex = bitPosition / 8;
		int bitsIndex = bitPosition % 8;//and also find the bit position where we will change data
		
		//then get the sufficient number of bits from the packet
		int collectedFromPacket = 0;//we can deal with up to 32 bits at a time
		int bitsCollectedFromPacket = 0;
		while(bitsCollectedFromPacket < bitLength)
		{
			if(byteIndex < this.packetData.length)
			{
				byte pcktByte = this.packetData[byteIndex];
				//System.out.println("Get byte: " + pcktByte + ", from index: " + byteIndex);
				collectedFromPacket = collectedFromPacket << 8;
				collectedFromPacket = collectedFromPacket | (((int) pcktByte) & 0xFF);
				bitsCollectedFromPacket += 8;
				byteIndex += 1;
			}
			else
			{
				break;
			}
		}
		
		//then mask the data to remove unwanted bits
		int mask = 0;
		for(int i = 0 ; i < bitLength; i ++)
		{
			mask = mask | (0x1 << (bitsCollectedFromPacket - 1 - bitsIndex - i));
		}
		
		collectedFromPacket = collectedFromPacket & mask;
		
		//then shift the data so the lowest bit is at position 0
		data = collectedFromPacket >> (bitsCollectedFromPacket - (bitsIndex + bitLength));
		
		return data;
	}
	
	public void update_append_bits(int data, int bitPosition, int bitLength)
	{
		//this function will change the 'bitLength' amount of bit-data at 'bitPosition' in the packet
		//by either directly adjusting the data and by appending on a byte if necessary
		
		//we'll want to mask the input data to be sure we don't change any bytes outside
		//of our intention
		int bitmask = 0;
		for(int i = 0; i < bitLength; i++)
		{
			bitmask = bitmask | (0x1 << i);
		}
		data = data & bitmask;
		
		//find the index of the byte within the packet that we'll change
		int byteIndex = bitPosition / 8;
		int bitsIndex = bitPosition % 8;//and also find the bit position where we will change data
		if(bitPosition > this.packetBitLength && bitsIndex > 0)
		{
			byteIndex += 1;
		}
		
		//split the input data into 8-bit bytes in order to update it in the packet
		//in order to get the data, we need to first align the bits on a byte edge
		//before shifting them to the bit start position within the byte where we're
		//placing it -> assume that input data is aligned to bit0 instead of the packet
		//position, big endian
		int bitShiftToByteAlign = bitLength % 8;//the number of non-byte even bits
		int firstSplit = data << (8 - bitShiftToByteAlign);//shift so that the new data is byte aligned
		while(firstSplit > 255)
		{
			firstSplit = firstSplit >> 8;//then collect only the highest order byte
		}
		firstSplit = (firstSplit >> bitsIndex) & 0xFF;//take only the top (8 - bitsIndex) bits
		int firstSplitBitLength = bitLength;
		if(firstSplitBitLength > 8 - bitsIndex)
		{
			firstSplitBitLength = 8 - bitsIndex;
		}

		
		//save the remaining bits to replace too: I need a mask to remove the bits I just pulled out
		int secondSplitBitLength = bitLength - firstSplitBitLength;
		int secondSplitBitPosition = bitPosition + firstSplitBitLength;
		int secondMask = 0;
		for(int i = 0; i < secondSplitBitLength; i++)
		{
			secondMask = secondMask | (0x1 << i);
		}
		int secondSplit = (data & secondMask);
		
		/*
		System.out.println("Packet Bitlen: " + this.packetBitLength);
		System.out.println("Insert BitPos: " + bitPosition);
		System.out.println("new data Bitoffset: " + (8 - bitsIndex) + " and bitlen: " + firstSplitBitLength);
		System.out.println("Packet length: " + this.packetLength);
		System.out.println("Insert pos in Packet: " + byteIndex);
		System.out.println("extra data bitLen: " + secondSplitBitLength);
		System.out.println(" ");
		*/
		
		//now change data in the packet, or append if necessary
		if(this.packetLength*8 > bitPosition)
		{
			//we are changing data within the packet, rather than appending
			//collect data from the packet
			int thirdMask = 0;
			for(int i = 0; i < 8; i++)
			{
				if(i < bitsIndex | i >= bitsIndex + firstSplitBitLength)
				{
					thirdMask = thirdMask | (0x80 >> i);
				}
			}
			byte byteFromPacket = (byte) (this.packetData[byteIndex] & thirdMask);
			this.packetData[byteIndex] = (byte) (byteFromPacket | firstSplit);
			
			//if we're in a byte that is added to the packet, but the data is new, be sure
			//to update the packet bit length
			if(this.packetBitLength < bitPosition + firstSplitBitLength)
			{
				this.packetBitLength += firstSplitBitLength;
			}
			//System.out.print("changing data: ");
			//System.out.println(this.packetData[byteIndex]);
		}
		else
		{
			//we're appending the first set of new data to the packet
			//appendByte(firstSplit);//this always adds 8 bits, which is inappropriate
			
			//add the first byte to the packet, and only add the correct number of bits to the bit length
			this.packetData = Arrays.copyOf(this.packetData, this.packetData.length + 1);
			this.packetData[this.packetData.length - 1] = (byte) firstSplit;
			this.packetLength += 1;
			this.packetBitLength += firstSplitBitLength;
			//System.out.print("appending data: ");
			//System.out.println((byte) firstSplit);
		}
		
		
		//then recursively add the remaining bytes
		if(secondSplitBitLength > 0)
		{
			update_append_bits(secondSplit, secondSplitBitPosition, secondSplitBitLength);
		}
	}
	
	public void appendOutput(String name, String displayType, String typeAndSize)
	{
		//append a new output to the list of outputs
		outputList = Arrays.copyOf(outputList, outputList.length + 1);

		Output output_to_add = new Output(name, displayType, typeAndSize, this.packetBitLength, this);//be careful to distinguish the bit position and bit index
		outputList[outputList.length - 1] = output_to_add;

		//then add the output data to the serial packet byte set
		//System.out.println("adding output of bit length: " + output_to_add.bitLength);
		int dataByte = output_to_add.intData;
		update_append_bits(dataByte, output_to_add.bit_start_position, output_to_add.bitLength);
		updateOutputs();//update the stored values
	}
	
	public String getOutputNameList() {
		//this function returns the names of all 'outputs' from this Serial_Packet
		String names = "";
		//System.out.print("Check to see if the outputs in this packet should be read...");
		if(shouldBeRead())
		{
			//System.out.println("Collect the names of " + this.outputList.length + " outputs");
			for(int i = 0; i < this.outputList.length; i++)
			{
				if(!this.outputList[i].child_to_other_output)
				{
					names = names.concat(this.outputList[i].name).concat(",");
				}
			}
		}
		else
		{
			//System.out.println("nope");
		}
		return names;
	}
	
	public String getOutputValues() {
		//this function returns a comma separated string list containing
		//the values of the outputs associated with this serial packet
		String outputValues = "";
		if(shouldBeRead())
		{
			for(int i = 0; i < this.outputList.length; i++)
			{
				//loop over all outputs, append their values to a string list
				//and separate by commas
				this.outputList[i].update_output_value();//first we update the value

				//if the current output is not a child to other outputs, add it to the list
				if(!this.outputList[i].child_to_other_output)
				{
					outputValues = outputValues.concat(this.outputList[i].value).concat(",");
				}
			}
		}
		return outputValues;
	}
	
	public Input[] getInputList() {
		//this function returns the list of inputs associated with this serial packet
		return this.inputList;
	}
	
	public Output[] getOutputList() {
		//this function returns the list of outputs associated with this Serial_Packet
		return this.outputList;
	}
	
	public void refreshPacket() {
		//this function updates all output then input parameters
		//associated with this packet
		updateOutputs();
		updateInputs();
		updateChecksum();
	}
	
	public void print_packet() {
		Conversion_Handler convert = new Conversion_Handler();
		for(int i = 0; i < this.packetLength; i++)
		{
			System.out.print(convert.Int_to_hex_string(this.packetData[i],2)+" ");
		}
		System.out.println();
	}
	
	//special functions for conditional read serial packets
	public void setExpression(String expr) {
		//this function updates the expression used to decide if the packet should be read
		this.expression = expr;
	}
	
	public void setCondition(String cond) {
		//this function updates the condition used to decide if the packet should be read
		this.condition = cond;
	}
	
	public void setDependentStringList(String[] list) {
		//this function updates the list of dependent names used in the expression when
		//evaluating whether to read the serial packet
		this.dependent_names = list;
	}
	
	public boolean shouldBeRead() {
		//this function evaluates the condition and expression to see
		//if the receive packet should be read
		boolean shouldRead = true;
		Equation_Parser parser = new Equation_Parser();
		
		//calculate the equation value
		String substitutedExpression = "0";
		if(this.expression.length() > 0)
		{
			substitutedExpression = expression;
			for(int i = 0; i < this.dependent_outputList.length; i++)
			{
				//update the dependent outputs
				this.dependent_outputList[i].update_output_value();
				String varName = this.dependent_outputList[i].variableName;
				String varValue = this.dependent_outputList[i].value;
				if(varValue.isEmpty())
				{
					varValue = "0";
				}
				substitutedExpression = parser.replace_variable_String_with_value(varValue, varName, substitutedExpression);
			}
			//System.out.println("Uses an expression of length: " + this.expression.length() + " which is: " + substitutedExpression);
		}
		
		//System.out.println("Compare: " + substitutedExpression + " and " + this.condition);
		
		//first, we should check for the type of condition
		//System.out.println("Compare the conditional reporting string: " + this.condition);
		Conversion_Handler convert = new Conversion_Handler();
		if(this.condition.contains("<"))
		{
			String filteredCondition = convert.stringAfter(this.condition,"<");
			boolean equals = false;
			if(filteredCondition.contains("="))
			{
				equals = true;
				filteredCondition = convert.stringAfter(filteredCondition, "=");
			}
			//let's assume that the expression and condition contain a number and convert it
			double condNum = convert.Dec_string_to_double(filteredCondition);
			double exprNum = parser.evaluate(substitutedExpression);
			
			//then less/equal or less evaluation by comparing the numbers
			if(equals && exprNum <= condNum)
			{
				shouldRead = true;
			}
			else if(!equals && exprNum < condNum)
			{
				shouldRead = true;
			}
			else
			{
				shouldRead = false;
			}
		}
		else if(this.condition.contains(">"))
		{
			String filteredCondition = convert.stringAfter(this.condition,">");
			//System.out.println("Try to parse the condition, remove the '>': " + filteredCondition);
			boolean equals = false;
			if(filteredCondition.contains("="))
			{
				equals = true;
				filteredCondition = convert.stringAfter(filteredCondition, "=");
			}
			//let's assume that the expression and condition contain a number and convert it
			double condNum = convert.Dec_string_to_double(filteredCondition);
			double exprNum = parser.evaluate(substitutedExpression);
			
			//then less/equal or less evaluation by comparing the numbers
			if(equals && exprNum >= condNum)
			{
				shouldRead = true;
			}
			else if(!equals && exprNum > condNum)
			{
				shouldRead = true;
			}
			else
			{
				shouldRead = false;
			}
		}
		else if(this.condition.contains("!"))
		{
			String filteredCondition = convert.stringAfter(this.condition,"!");
			if(!substitutedExpression.equals(filteredCondition))
			{
				shouldRead = true;
			}
			else
			{
				shouldRead = false;
			}
		}
		else if(this.condition.contains("="))
		{
			String filteredCondition = convert.stringAfter(this.condition,"=");
			if(substitutedExpression.equals(filteredCondition))
			{
				shouldRead = true;
			}
			else
			{
				shouldRead = false;
			}
		}
		
		return shouldRead;
	}
	
	public void updateDependentsFromCandidates(Output[] listOfCandOut)
	{
		//this function updates the associated list of outputs that
		//the expression depends upon - look through the input list and add
		//it to the dependent list if the name matches our desired list.
		//then do the same for the output dependency candidates
		for(int i = 0; i < listOfCandOut.length; i++)
		{
			for(int j = 0; j < this.dependent_names.length; j++)
			{
				if(listOfCandOut[i].variableName.equals(this.dependent_names[j]))
				{
					//we have a potential match- scan the currently stored list to make sure we are not duplicating
					boolean inList = false;
					for(int k = 0; k < this.dependent_outputList.length; k++)
					{
						if(this.dependent_outputList[k].variableName.equals(listOfCandOut[i].variableName))
						{
							//the value is already in the list, set a flag so we do not save it again
							inList = true;
							break;
						}
					}
					if(!inList)
					{
						//if the value is not in the list, add it to the list
						this.dependent_outputList = Arrays.copyOf(this.dependent_outputList, this.dependent_outputList.length + 1);
						this.dependent_outputList[this.dependent_outputList.length - 1] = listOfCandOut[i];
					}
					break;
				}
			}
		}
	}
	
	public boolean compare_pckt_and_update_outputs(byte[] readPacket)
	{
		//this function compares the input packet to this object - if the structure matches
		//then the outputs are updated to the new values and a 'true' flag is returned
		
		//loop over the input packet and compare to this packet. if there is a 
		//mismatch, check to see if this bitposition is in an input or output
		//if it is, we're still okay
		boolean match = true;
		if(readPacket.length == this.packetLength)
		{
			for(int i = 0; i < this.packetLength; i++)
			{
				if(readPacket[i] != this.packetData[i])
				{
					match = false;//we have a mismatch
					//System.out.print("data mismatch: ");
					
					//check the bit position relative to inputs and outputs
					int currentBitPos = 8*i;
					for(int j = 0; j < this.inputList.length; j++)
					{
						
						int bitStart = this.inputList[j].bit_start_position;
						int bitEnd = bitStart + this.inputList[j].bitLength;
						if(currentBitPos >= bitStart && currentBitPos < bitEnd)
						{
							//the current byte is in an input
							match = true;
							//System.out.println(".. nevermind, its an input");
							break;
						}
					}
					
					
					for(int j = 0; j < this.outputList.length; j++)
					{
						
						int bitStart = this.outputList[j].bit_start_position;
						int bitEnd = bitStart + this.outputList[j].bitLength;
						if(currentBitPos >= bitStart && currentBitPos < bitEnd)
						{
							//the current byte is in an output
							match = true;
							//System.out.println(".. nevermind, its an output");
							break;
						}
					}
					
					//check to see if we are on the checksum byte
					if(this.lastByteChecksum && i == this.packetLength - 1)
					{
						//does the checksum match?
						int sum = calcChecksum(readPacket);
						//System.out.println("Compare calc'd sum: " + sum + " with recv'd sum: " + (readPacket[readPacket.length - 1] & 0xFF) );
						if(sum == (readPacket[this.packetLength - 1] & 0xFF) )
						{
							match = true;
							break;
						}
						//System.out.println("checksum mismatch");
					}
				}
			}
		}
		else
		{
			match = false;
		}
		
		
		if(match)
		{
			//System.out.println("Update stored packet");
			setPacket(readPacket);//update the packet bytes
		}
		return match;
	}
}