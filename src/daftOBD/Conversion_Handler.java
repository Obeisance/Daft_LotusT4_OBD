package daftOBD;

import java.util.Arrays;

public class Conversion_Handler {

	public Conversion_Handler() {
		//nothing to do in the constructor
	}
	
	public int Hex_or_Dec_string_to_int(String inString) {
		boolean isHex = false;
		int sign = 1;//positive
		int toHex = 0;
		int toDec = 0;
		for(int i = 0; i < inString.length(); i++)
		{
			char thisChar = inString.charAt(i);
			if(thisChar == 'x')
			{
				toHex = 0;
				isHex = true;
			}
			
			if(thisChar == '-')
			{
				sign = -1;
			}
			
			
			if(thisChar == '.' | thisChar == ',')
			{
				//someone put in a decimal value
				//so let's stop processing the string here
				break;
			}
			
			if(thisChar >= 48 && thisChar <= 57)
			{
				//we have a decimal or hex number
				toDec = toDec*10 + thisChar - 48;
				toHex = toHex*16 + thisChar - 48;
			}
			else if(thisChar >= 97 && thisChar <= 102)
			{
				isHex = true;
				toHex = toHex*16 + thisChar - 97 + 10;
			}
			else if(thisChar >= 65 && thisChar <= 70)
			{
				isHex = true;
				toHex = toHex*16 + thisChar - 65 + 10;
			}
		}
		
		int retVal = 0;
		
		if(isHex)
		{
			retVal = toHex*sign;
		}
		else
		{
			retVal = toDec*sign;
		}
		
		return retVal;
	}
	
	//at least this function is redundant with other built in functions
	public double Dec_string_to_double(String inString)
	{
		//input a decimal value as a string and convert to a double
		int sign = 1;//positive
		boolean rightOfDecimal = false;
		double subMult = 0.1;
		double toDoub = 0.0;
		for(int i = 0; i < inString.length(); i++)
		{
			char thisChar = inString.charAt(i);

			if(thisChar == '-')
			{
				sign = -1;
			}
			
			
			if(thisChar == '.' | thisChar == ',')
			{
				//remaining characters are sub-integers
				rightOfDecimal = true;
			}

			if(thisChar >= 48 && thisChar <= 57)
			{
				//we have a decimal number
				int thisCharInt = thisChar - 48;
				if(rightOfDecimal)
				{
					toDoub = toDoub + thisCharInt*subMult;
					subMult = subMult*0.1;
				}
				else
				{
					toDoub = toDoub*10 + thisCharInt;
				}
			}

		}
		//System.out.println("Converting to double from string: " + toDoub);
		return toDoub*sign;
	}
	
	public String Double_to_dec_string(double inDouble)
	{
		String outString = String.valueOf(inDouble);//this outputs a shorthand exponential notation
		//just shift the decimal according to the value here
		if(outString.contains("E"))
		{
			//figure out how many digits the sample must be shifted by
			int indexOfE = outString.indexOf("E");
			String pastE = outString.substring(indexOfE + 1);
			int shiftDecimal = Hex_or_Dec_string_to_int(pastE);
			
			//then loop through the string and remove the decimal character
			String hold_outString = outString;
			outString = "";
			for(int i = 0; i < indexOfE; i++)
			{
				if(hold_outString.charAt(i) != '.')
				{
					outString = outString.concat(hold_outString.substring(i,i+1));
				}
			}
			
			//finally, loop to add the decimal point back in
			if(shiftDecimal < 0)
			{
				String zeros = "";
				for(int i = 0; i < -1*shiftDecimal - 1; i++)
				{
					zeros = zeros.concat("0");
				}
				outString = "0.".concat(zeros).concat(outString);
			}
			else
			{
				hold_outString = outString;
				outString = "";
				for(int i = 0; i < shiftDecimal; i++)
				{
					if(i >= hold_outString.length())
					{
						outString = hold_outString.concat("0");
					}
					else if(i < hold_outString.length())
					{
						outString = outString.concat(hold_outString.substring(i,i+1));
					}
				}
				outString = outString.concat(".");
				if(hold_outString.length() > shiftDecimal)
				{
					outString = outString.concat(hold_outString.substring(shiftDecimal));
				}
			}
		}
		return outString;
	}
	
	public String Int_to_dec_string(int inInteger)
	{
		//String outString = String.valueOf(inInteger);//I cannot handle the shorthand exponential notation that this puts out
		String outString = "";
		String sign = "";
		int divide_int = inInteger;
		if(inInteger < 0)
		{
			sign = "-";
			divide_int = -1*inInteger;
		}
		while(divide_int > 0)
		{
			int lastDigit = divide_int % 10;
			outString = String.valueOf(lastDigit).concat(outString);
			divide_int = divide_int / 10;
		}
		if(inInteger == 0)
		{
			//special case
			outString = "0";
		}
		return sign.concat(outString);
	}
	
	public String Int_to_hex_string(int inInteger)
	{
		String[] hexChars = {"0","1","2","3","4","5","6","7","8","9","A","B","C","D","E","F"};
		String outString = "";
		int divide_int = inInteger;
		
		//signed values mess this up... maybe I'll have to use the other version at a later point...
		if(divide_int == 0)
		{
			outString = hexChars[0];
		}
		while(divide_int > 0)
		{
			int lastDigit = Integer.remainderUnsigned(divide_int, 16);//divide_int % 16;
			outString = hexChars[lastDigit].concat(outString);
			divide_int = Integer.divideUnsigned(divide_int, 16);//divide_int / 16;
		}
		
		return outString;
	}
	
	public String Int_to_hex_string(int inInteger, int numChars)
	{
		String[] hexChars = {"0","1","2","3","4","5","6","7","8","9","A","B","C","D","E","F"};
		String outString = "";
		int divide_int = inInteger;
		for(int i = 0; i < numChars; i++) {
			int lastDigit = divide_int & 0xF;
			outString = hexChars[lastDigit].concat(outString);
			divide_int = divide_int >> 4;
		}
		
		return outString;
	}
	
	//this function extracts the string after the input string Tag in the line
	public String stringAfter(String line, String tag)
	{
		String data = line;
		if(line.contains(tag))
		{
			int start = line.indexOf(tag, 0);
			start += tag.length();
			data = line.substring(start);
		}
		return data;
	}

	public String stringBefore(String line, String tag)
	{
		String data = line;
		if(line.contains(tag))
		{
			int end = line.indexOf(tag,0);
			data = line.substring(0,end);
		}
		return data;
	}

	public String stringBetween(String line, String beginTag, String endTag)
	{
		String laterBit = stringAfter(line, beginTag);
		String data = stringBefore(laterBit, endTag);
		if(line.equals(data))//if we did not find the tag, we should not output a value
		{
			data = "";
		}
		return data;
	}

	public String[] getAllOccurrencesBetween(String line, String beginTag, String endTag)
	{
		String list[] = new String[0];
		String stepThrough = line;
		while(stepThrough.contains(beginTag) & stepThrough.contains(endTag))
		{
			//get the first occurrence of the value between the tags
			String tag = stringBetween(stepThrough, beginTag, endTag);
			//then add the value to the list
			list = Arrays.copyOf(list, list.length + 1);
			list[list.length - 1] = tag;

			//then shorten the string
			String pastThis = beginTag.concat(tag).concat(endTag);
			stepThrough = stepThrough.substring(stepThrough.indexOf(pastThis) + pastThis.length());
		}
		return list;
	}
}
