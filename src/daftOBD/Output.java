package daftOBD;

import java.util.Arrays;

//inputs and outputs need lists of their parent Serial_Packets so that they can update their data or update the serial_packet data
//similarly, they need lists of their dependent input or output parameters for the same reason

public class Output {
	//this class sets up the output objects that are used
	//as part of the serial packet- data can be calculated
	//from serial packets- the transformed data is an output
	String equation = "";//indicates a mathematical transformation that is applied during conversion from binary data to the output type/representation
	String value = "";//after transformation - this is a string representation of the data
	String type = "";//indicates how to interpret the data: ex. int8, char8 -> also indicates bit length
	String name = "";//a title that represents the significance of this output ex. "vehicle speed"
	String representation = "";//the type of representation that is used in value, ex. "ASCII", "Int" etc.
	String variableName = "";//an alternate name used when the the output is an input to another input/output

	Serial_Packet parentPacket;//a reference to the packet that has data that dictates this output
	int bit_start_position = 0;//the position within the serial message/packet where the data is stored
	int bitLength = 0;
	int intData = 0;//the integer representation of the data used to calculate this output
	int dataMask = 0;//a mask to limit the intData to proper bit length
	
	String[] conversionTable_output = new String[0];//some outputs will be bit flags for boolean values, so we use a table to lookup the output
	int[] conversionTable_data = new int[0];//the index for looking up data is found based on data in this table

	//some outputs can have dependencies on other input/outputs in order to calculate a final value
	//these two objects hold references to the parameters that this output is dependent upon
	Input[] dependent_inputList = new Input[0];
	Output[] dependent_outputList = new Output[0];
	String[] dependent_names = new String[0];
	
	//there are cases where the output is combined with others
	boolean child_to_other_output = false;
	Output[] combined_sub_outputs = new Output[0];

	
	public Output(String nam, String rep, String typ, int bitPos, Serial_Packet pckt)
	{
		this.parentPacket = pckt;
		this.name = nam;
		this.representation = rep;
		this.type = typ;
		this.bit_start_position = bitPos;
		Conversion_Handler convert = new Conversion_Handler();

		//act depending on the type
		if(this.type.contains("char"))
		{
			//we have a character string as the input
			int pos = this.type.indexOf("char");
			String len = this.type.substring(pos + 4);//get the last characters from the type
			this.bitLength = convert.Hex_or_Dec_string_to_int(len);//convert those into a bit length and set the value for the input
		}
		else if(this.type.contains("int"))
		{
			int pos = this.type.indexOf("int");
			String len = this.type.substring(pos + 3);
			this.bitLength = convert.Hex_or_Dec_string_to_int(len);
		}
		
		//then create the bit-mask we'll use to limit the input length
		for(int i = 0; i < this.bitLength; i++)
		{
			this.dataMask = (this.dataMask << 0x1) | 0x1;
		}
	}

	public void set_equation(String e)
	{
		this.equation = e;
	}

	public void set_variableName(String vn)
	{
		this.variableName = vn;
	}

	public void append_data_table(int data, String newVal)
	{
		this.conversionTable_data = Arrays.copyOf(this.conversionTable_data, this.conversionTable_data.length + 1);
		this.conversionTable_data[this.conversionTable_data.length - 1] = data;

		this.conversionTable_output = Arrays.copyOf(this.conversionTable_output, this.conversionTable_output.length + 1);
		this.conversionTable_output[this.conversionTable_output.length - 1] = newVal;
	}

	public String update_output_value()
	{
		//collect the input data according to the bit length
		//assume that the input data has been shifted so bit0 is the end of data
		this.intData = this.parentPacket.getShiftedBitData(this.bit_start_position, this.bitLength);

		//use the mask in order to remove unnecessary bits, and pay attention to sign of data
		this.intData = this.intData & this.dataMask;
		if(!this.type.contains("uint"))//we have either signed or unsigned integers
		{
			//add back in the sign bits -> sign extend
			if((this.intData >> (this.bitLength-1) ) == 1)//check to see if the negative bit is set
			{
				for(int i = 31; i >= this.bitLength; i--)//assume that the intData is a 32 bit integer, at most
				{
					this.intData = this.intData | (0x1 << i);
				}
			}
		}
		
		
		//System.out.println("Time to update output: " + this.name + ", using data: " + this.intData);

		//then process the data according to the display type
		if(this.representation.equals("int") | this.representation.equals("double") | this.representation.equals("hex"))
		{
			Conversion_Handler convert = new Conversion_Handler();
			//we will use an equation to process the data if there is an equation stored
			double processedEquation = 0.0;
			if(this.equation.length() > 0)
			{
				//put the data into the equation where it belongs -> anywhere we find a '#'
				String dataString = convert.Int_to_dec_string(this.intData);
				Equation_Parser parser = new Equation_Parser();
				String substitutedEquation = parser.replace_variable_with_value(dataString, this.equation);
				//then put all variables that are necessary into the equation
				//first, the dependent inputs
				for(int i = 0; i < this.dependent_inputList.length; i++)
				{
					String varName = this.dependent_inputList[i].variableName;
					String varValue = convert.Int_to_dec_string(this.dependent_inputList[i].intData);
					substitutedEquation = parser.replace_variable_String_with_value(varValue, varName, substitutedEquation);
				}
				//then the dependent outputs
				for(int i = 0; i < this.dependent_outputList.length; i++)
				{
					String varName = this.dependent_outputList[i].variableName;
					String varValue = this.dependent_outputList[i].value;
					if(varValue.isEmpty())
					{
						varValue = "0";
					}
					substitutedEquation = parser.replace_variable_String_with_value(varValue, varName, substitutedEquation);
				}
				
				//then process the equation
				processedEquation = parser.evaluate(substitutedEquation,this.equation);
			}
			else
			{
				processedEquation = (double) this.intData;
			}

			//then we must convert the output string to something according to the desired representation
			if(this.representation.equals("double"))
			{
				this.value = convert.Double_to_dec_string(processedEquation);
			}
			else if(this.representation.equals("int"))
			{
				this.value = convert.Int_to_dec_string((int) processedEquation);
			}
			else if(this.representation.equals("hex"))
			{
				this.value = convert.Int_to_hex_string((int) processedEquation);
				//System.out.println("output is hex value, convert: " + processedEquation + " to hex: " + this.value);
			}

		}
		else if(this.representation.equals("lookup"))
		{
			//look up the value from the table
			this.value = "";
			for(int i = 0; i < this.conversionTable_data.length; i++)
			{
				//System.out.println("conversion table compare input: '" + this.intData + "' with '" + this.conversionTable_output[i] +"'");
				if(this.intData == this.conversionTable_data[i])
				{
					this.value = this.conversionTable_output[i];
					break;
				}
			}
		}
		else if(this.representation.equals("boolean"))
		{
			if(this.intData == 0)
			{
				this.value = "false";
			}
			else
			{
				this.value = "true";
			}
		}
		else if(this.representation.equals("ASCII"))
		{
			//we have a character
			this.value = Character.toString((char) this.intData);
		}
		
		//add on the updated values of the children outputs
		for(int i = 0; i < this.combined_sub_outputs.length; i++)
		{
			this.combined_sub_outputs[i].update_output_value();
			this.value = this.value.concat(this.combined_sub_outputs[i].value);
		}
		//System.out.println("updated output value: " + this.value);

		//finally return the value
		return this.value;
	}
	
	public void set_dependent_list(String[] newList) {
		//this function updates the list of dependent variable names
		this.dependent_names = newList;
	}
	
	public void add_combined_output(Output newChild)
	{
		//this function adds an output which will be appended to the value 
		//generated by this output. It also sets that output as a child so it does not report.
		newChild.child_to_other_output = true;
		this.combined_sub_outputs = Arrays.copyOf(this.combined_sub_outputs, this.combined_sub_outputs.length + 1);
		this.combined_sub_outputs[this.combined_sub_outputs.length - 1] = newChild;
	}
	
	public void update_dependents_from_list_of_candidates(Input[] listOfCandIn, Output[] listOfCandOut)
	{
		//this function scans through the list of input and output values, and compares them to the list of dependents
		//string array. if they match, the input or output is added to the list of dependents,
		//but only if the value is not already present
		for(int i = 0; i < listOfCandIn.length; i++)
		{
			for(int j = 0; j < this.dependent_names.length; j++)
			{
				if(listOfCandIn[i].variableName.equals(this.dependent_names[j]))
				{
					//we have a potential match- scan the currently stored list to make sure we are not duplicating
					boolean inList = false;
					for(int k = 0; k < this.dependent_inputList.length; k++)
					{
						if(this.dependent_inputList[k].variableName.equals(listOfCandIn[i].variableName))
						{
							//the value is already in the list, set a flag so we do not save it again
							inList = true;
							break;
						}
					}
					if(!inList)
					{
						//if the value is not in the list, add it to the list
						this.dependent_inputList = Arrays.copyOf(this.dependent_inputList, this.dependent_inputList.length + 1);
						this.dependent_inputList[this.dependent_inputList.length - 1] = listOfCandIn[i];
					}
					break;
				}
			}
		}
		
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
}
