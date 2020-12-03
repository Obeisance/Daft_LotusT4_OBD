package daftOBD;

import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.FocusEvent;
import java.awt.event.FocusListener;
import java.util.Arrays;

import javax.swing.JTextField;

public class Input implements ActionListener, FocusListener {
	//this class sets up the input objects that are used
	//as part of the serial packet
	JTextField inputBox;//the user interface to change the input value
	String display_input = "";//the text stored in the user interface text box
	String type = "";//indicates how to interpret the data: ex. int8, char8 -> also indicates bit length
	String equation = "";//indicates a mathematical transformation that is applied during conversion from display_input string to intData
	String variableName = "";//an alternate name used when the input is an input to another input/output

	Serial_Packet parentPacket;//a reference to the packet that has data that gets populated from this input
	int bit_start_position = 0;//the position within the serial message/packet of the data from this input
	int bitLength = 0;
	int intData = 0;//the integer representation of this input's data
	int dataMask = 0;//a mask to limit the intData to proper bit length
	
	//some inputs can have dependencies on other input/outputs in order to calculate a final value
	//these two objects hold references to the parameters that this input is dependent upon
	Input[] dependent_inputList = new Input[0];
	Output[] dependent_outputList = new Output[0];
	String[] dependent_names = new String[0];
	
	public Input(String disp, String typ, int bitPos, Serial_Packet pckt) {
		this.display_input = disp;
		this.inputBox = new JTextField(this.display_input);
		this.inputBox.addActionListener(this);
		this.inputBox.addFocusListener(this);
		
		this.parentPacket = pckt;
		this.type = typ;
		this.bit_start_position = bitPos;
		Conversion_Handler convert = new Conversion_Handler();
		
		//act depending on the type to set the bit length of the input
		if(this.type.contains("char"))
		{
			//we have a character string as the input
			String len = this.type.substring(4);//get the last characters from the type
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
		//convert_update_input();//update intData
	}
	
	public void set_variableName(String vn)
	{
		this.variableName = vn;
	}
	
	public void set_equation(String eq)
	{
		//System.out.println("update equation:" + eq);
		this.equation = eq;
		convert_update_input();//update intData
	}
	
	public void convert_update_input()
	{
		//this function updates the intData field based on the display_input String and the type/length parameters
		
		//act depending on the type
		if(this.type.contains("char"))
		{
			//now collect the correct number of bits from the input values
			int convertedChar = 0;
			for(int i = 0; i < this.display_input.length(); i++)//loop over characters in the string
			{
				convertedChar = convertedChar << i*8;//shift the bits to the left as we read in more characters
				int thisChar = (int) this.display_input.charAt(i);
				for(int j = 0; (j < 8) && (j + i*8 < this.bitLength); j++)
				{
					convertedChar += (thisChar & (0x80 >> j));//add in the bits of the character from left to right
				}
			}
			this.intData = convertedChar & this.dataMask;
		}
		else if(this.type.contains("int"))
		{
			//collect the string from the input field, convert it and evaluate 
			//an equation if it contains one, then update the integer input data
			//System.out.println("change an int type input...");
			String data = this.display_input;
			
			//convert the input value to a double and back to a string in
			//order to eliminate unusual characters
			Conversion_Handler convert = new Conversion_Handler();
			double convert_input = convert.Dec_string_to_double(data);
			String value = convert.Double_to_dec_string(convert_input);
			
			if(this.equation.length() > 0)
			{
				//System.out.println("There is an equation to evaluate: " + this.equation);
				
				//then put the value into the equation
				Equation_Parser parser = new Equation_Parser();
				String substitutedEquation = parser.replace_variable_with_value(value, this.equation);
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
				
				//and then evaluate the equation
				double evaluatedEqn = parser.evaluate(substitutedEquation, this.equation);
				//System.out.println("evaluate: " + substitutedEquation + " results in: " + evaluatedEqn);
				//then mask the output based on the length of the input
				
				//and finally update the data
				this.intData = ((int) evaluatedEqn) & this.dataMask;
			}
			else
			{
				this.intData = ((int) convert_input) & this.dataMask;
			}
		}
		//System.out.println(this.intData);

		//update the parent Serial_Packet
		this.parentPacket.update_append_bits(this.intData, this.bit_start_position, this.bitLength);
		this.parentPacket.updateChecksum();
	}
	
	public JTextField getJTextField()
	{
		//this function returns a reference to the JTextField that interfaces with this input
		return this.inputBox;
	}
	
	public void set_dependent_list(String[] newList) {
		//this function updates the list of dependent variable names
		this.dependent_names = newList;
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

	public void update_when_textBoxChanged()
	{
		//this function updates the input value intData parameter and the associated 
		//Serial_Packet when the input is changed
		
		//get the new value from the text box
		this.display_input = this.inputBox.getText();
		
		//if the new value is too short, re-draw the input to be longer
		if(this.display_input.length() == 0)
		{
			this.inputBox.setText("   ");
			this.display_input = "   ";
		}
		
		//then update the integer representation of the data
		convert_update_input();
	}
	
	@Override
	public void actionPerformed(ActionEvent arg0) {
		//The only action that can be performed is the input text is updated
		update_when_textBoxChanged();
	}

	@Override
	public void focusGained(FocusEvent arg0) {
		//we don't want to act here, but we will anyway
		update_when_textBoxChanged();
	}

	@Override
	public void focusLost(FocusEvent arg0) {
		//The only action that can be performed is the input text is updated
		update_when_textBoxChanged();
	}

}
