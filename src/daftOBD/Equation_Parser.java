package daftOBD;

import java.util.Arrays;

public class Equation_Parser {
	//this function allows an input string to be turned into a calculated
	//value while respecting P E MD AS order of operations 
	//the operators I want to include are:
	// paren 	()
	// divide	/
	// multip	*
	// add		+
	// subtra	-
	// expone	^
	// expone	exp
	// log10	log
	// natlog	ln
	//and some integer operators
	// and		&
	// or		|
	// xor		xor
	// shiftL	<<
	// shiftR	>>
	// bitcomp  ~
	private String[] operators = {"(",")","^","exp","ln","log","*","/","+","-","&","|","XOR","xor","<<",">>","~"};
	Conversion_Handler convert = new Conversion_Handler();
	
	public Equation_Parser() {
		//we don't need to do anything in the constructor
	}
	
	public double evaluate(String equation) {
		return evaluate(equation, equation);
	}
	
	public double evaluate(String equation, String orig_eqn)
	{
		//reduce the equation and convert to a double
		String reduced_equation = reduce(equation);
		double outValue = convert.Dec_string_to_double(reduced_equation);
		//and only perform the significant figure evaluation at the end!
		int orig_eqn_sig_figs = sig_figs_eqn(orig_eqn);
		outValue = round_off(outValue,orig_eqn_sig_figs);
		//System.out.println("Evaluate: " + equation + " results in string: " + reduced_equation + " which is converted to double: " + outValue);
		return outValue;
	}
	
	public String reduce(String equation) {
		//this function takes in an equation string, and outputs a version of the string
		//with reduced complexity as it follows order of operations in order to evaluate the 
		//equation
		
		//we really want to reduce the number of parentheses in the equation
		//so we should split the equation up based on the presence of parenthesis
		//System.out.println("Reduce this: [" + equation + "]");
		String[] paren = collectParen(equation);
		String shortEquation = paren[1];
		
		//all that remains does not have parenthesis: next evaluate the exponential operators
		String removedExponents = exponentialOperators(shortEquation);
		//System.out.println("remove exponential operators: " + removedExponents);
		
		//then the multiply and divide operators
		String removedMultiply = multiplicationOperators(removedExponents);
		//System.out.println("remove multiply operators: " + removedMultiply);
		
		//then the add or subtract operators
		String removedAddition = additionOperators(removedMultiply);
		//System.out.println("remove addition operators: " + removedAddition);
		
		//then the integer operators - these will convert the input from a double value
		//to an integer by truncation if they are present
		String removedInteger = integerOperators(removedAddition);
		
		//finally, re-assemble the equation with the parenthesis segment replaced after operation
		//and check to see that we are done.
		String reducedEquation = paren[0].concat(removedInteger).concat(paren[2]);
		if(containsAnyOperator(reducedEquation))
		{
			reducedEquation = reduce(reducedEquation);
		}
		return reducedEquation;
	}
	
	private String[] collectParen(String equation)
	{
		//this function collects the part of the equation within the innermost parentheses
		String[] eqns = {"",equation,""};//equation before (), equation within () or eqn to act on, equation after ();
		int[] indices_open_paren = all_operator_indices(equation,"(");
		int[] indices_close_paren = all_operator_indices(equation,")");
		if(indices_open_paren.length > 0)
		{
			while(indices_open_paren.length < indices_close_paren.length)
			{
				equation = equation.concat(")");//make sure we close the paren sets
				indices_open_paren = all_operator_indices(equation,"(");
				indices_close_paren = all_operator_indices(equation,")");
			}
			//now we want to collect what is between the final set of parenthesis
			int index_paren_begin = indices_open_paren[indices_open_paren.length - 1];
			//System.out.println("index of begin paren: " + index_paren_begin);
			
			//find the corresponding index for the end of the parenthesis
			int index_paren_end = indices_close_paren[indices_close_paren.length - 1];//a starting guess
			//we want to find the first closing paren which has a larger index than the final starting paren
			for(int i = 0; i < indices_close_paren.length; i++)
			{
				if(indices_close_paren[i] > index_paren_begin)
				{
					index_paren_end = indices_close_paren[i];
					break;
				}
			}
			//System.out.println("index of end paren: " + index_paren_end);
			
			String stringBeforeParen = "";
			if(index_paren_begin > 0)
			{
				stringBeforeParen = equation.substring(0,index_paren_begin);;
			}
			String shortEquation = equation.substring(index_paren_begin + 1,index_paren_end);
			String stringAfterParen = "";
			if(index_paren_end < equation.length() - 1)
			{
				stringAfterParen = equation.substring(index_paren_end + 1);
			}
			
			//System.out.println("Split paren as:" + stringBeforeParen + ";" + shortEquation + ";" + stringAfterParen + ";");
			eqns[0] = stringBeforeParen;
			eqns[1] = shortEquation;
			eqns[2] = stringAfterParen;		
		}
		return eqns;
	}
	
	private String[] split_equation_around_operator(String combinedEquation, String operator) {
		//this function returns two strings- those separated by the input operator
		//find the first operator character and collect the values around it
		String[] returnVal = {combinedEquation,"0","",""};
		int[] special_minus_indices = special_minus_case(combinedEquation);//there are some special cases for the '-' operator
		
		String trimmedString = remove_special_minus_case(combinedEquation);//so we only want to proceed if there are operators apart from this
		//System.out.println("look at string without special '-' operators: " + trimmedString);
		if(trimmedString.contains(operator))
		{
			int[] all_indices = all_operator_indices(combinedEquation, operator);//get the list of all instances of the operator of interest
			int index_of_operator_of_interest = combinedEquation.length();//then compare the operators in this list to the special '-' cases
			//we want the first operator in the string that does not coincide with the special cases of the '-'
			for(int i = 0; i < all_indices.length; i++)
			{
				index_of_operator_of_interest = all_indices[i];
				boolean loop_finished = true;
				for(int j = 0; j < special_minus_indices.length; j++)
				{
					//System.out.println("Compare special '-': " + special_minus_indices[j] + " with eqn index: " + index_of_operator_of_interest);
					if(special_minus_indices[j] == index_of_operator_of_interest)
					{
						//the index of the current
						//System.out.println("skip this index_of_interest");
						loop_finished = false;
						break;
					}
				}
				if(loop_finished)
				{
					break;
				}
			}
			
			//now that we have the index of the operator of interest, collect the substring components
			String shortEqnRight = "";
			if(index_of_operator_of_interest < combinedEquation.length() - operator.length())
			{
				shortEqnRight = combinedEquation.substring(index_of_operator_of_interest + operator.length());
			}
			shortEqnRight = stringBeforeOperator(shortEqnRight);//we have the right part of the equation
			
			String shortEqnLeft = combinedEquation.substring(0,index_of_operator_of_interest);
			shortEqnLeft = stringAfterOperator(shortEqnLeft);//and the left part of the equation
			
			//we also need the bits preceding and succeeding the equation
			String leftOfSEL = "";
			if(index_of_operator_of_interest - shortEqnLeft.length() >= 0)
			{
				leftOfSEL = combinedEquation.substring(0,index_of_operator_of_interest - shortEqnLeft.length());
			}
			String rightOfSER = "";
			if(index_of_operator_of_interest + shortEqnRight.length() + operator.length() <= combinedEquation.length() - 1)
			{
				rightOfSER = combinedEquation.substring(index_of_operator_of_interest + shortEqnRight.length() + operator.length());
			}
			
			returnVal[0] = shortEqnLeft;
			returnVal[1] = shortEqnRight;
			returnVal[2] = leftOfSEL;
			returnVal[3] = rightOfSER;
			//System.out.println("index of operator: " + index_of_operator_of_interest);
			//System.out.println("split equation as such: " + leftOfSEL + ";" + shortEqnLeft + ";" + operator + ";" + shortEqnRight + ";" + rightOfSER + ";");
		}
		return returnVal;
	}
	
	private int[] all_operator_indices(String equation, String operator)
	{
		//this function returns the indices of all occurrences of the
		//string 'operator' within the string 'equation'
		int[] indices = new int[0];
		int index = 0;
		int counter = 0;
		while(index >= 0)
		{
			index = equation.indexOf(operator, counter);
			counter = index + 1;
			if(index >= 0)
			{
				indices = Arrays.copyOf(indices, indices.length + 1);
				//System.out.println("operator \"" + operator + "\" found at: " + index);
				indices[indices.length - 1] = index;
			}
		}
		return indices;
	}
	
	private String exponentialOperators(String equation)
	{
		//this function splits an equation (that does not have parentheses) and evaluates the exponential terms
		String outString = equation;
		//the exponential/log/ln functions will be outlined by parenthesis, but after evaluation of what is inside
		//they may look something like: exp(2.3+1.1)^3 -> exp3.4^3 -> evaluate the exp first, etc.
		//the "^" should be evaluated first when there are no other functions
		if(equation.contains("^") & !equation.contains("exp") & !equation.contains("ln") & !equation.contains("log"))
		{
			//System.out.println("power operator present");
			//find the final "^" character and collect the values around it
			String[] eqns = split_equation_around_operator(equation,"^");
			
			//get the numbers and evaluate the operation
			double leftNum = convert.Dec_string_to_double(eqns[0]);
			double rightNum = convert.Dec_string_to_double(eqns[1]);
			double eval = Math.pow(leftNum, rightNum);
			String evalString = convert.Double_to_dec_string(eval);
			
			//then replace the operation in the original equation string
			outString = eqns[2].concat(evalString).concat(eqns[3]);
			
		}
		else if(equation.contains("exp"))
		{
			//System.out.println("exp operator present");
			//collect the part of the equation after the final exp
			String[] splitExp = split_for_function(equation, "exp");
					
			//then evaluate the exp
			double insideExp = convert.Dec_string_to_double(splitExp[0]);
			double evalDoub = Math.exp(insideExp);
			String evalStri = convert.Double_to_dec_string(evalDoub);
			
			//the replace the function in the equation with the evaluated result
			outString = splitExp[1].concat(evalStri).concat(splitExp[2]);
		}
		else if(equation.contains("log"))
		{
			//System.out.println("log operator present");
			//collect the part of the equation after the final log
			String[] splitLog = split_for_function(equation, "log");

			//then evaluate the log
			double insideLog = convert.Dec_string_to_double(splitLog[0]);
			double evalDoub = Math.log10(insideLog);
			String evalStri = convert.Double_to_dec_string(evalDoub);

			//the replace the function in the equation with the evaluated result
			outString = splitLog[1].concat(evalStri).concat(splitLog[2]);
		}
		else if(equation.contains("ln"))
		{
			//System.out.println("ln operator present");
			//collect the part of the equation after the final ln
			String[] splitLn = split_for_function(equation, "ln");

			//then evaluate the ln
			double insideLn = convert.Dec_string_to_double(splitLn[0]);
			double evalDoub = Math.log(insideLn);
			String evalStri = convert.Double_to_dec_string(evalDoub);

			//the replace the function in the equation with the evaluated result
			outString = splitLn[1].concat(evalStri).concat(splitLn[2]);
		}
		
		//are we done with exponential operators?
		if(outString.contains("^") | outString.contains("exp") | outString.contains("log") | outString.contains("ln"))
		{
			//System.out.println("more exponential operators present");
			//we're not done, so we should iterate recursively
			outString = reduce(outString);
		}
		
		return outString;
	}
	
	private String[] split_for_function(String line, String fcn)
	{
		//trouble!
		//this function will split the line string at the fcn string
		//assuming that the data immediately following the fcn is what it acts upon
		String insideFcn = stringAfter(line,fcn);
		int indexInsideFcn = line.indexOf(insideFcn);
		while(insideFcn.contains(fcn))
		{
			//System.out.println("find last instance of: " + fcn + "leaving: " + insideFcn);
			indexInsideFcn += insideFcn.indexOf(fcn) + fcn.length();
			insideFcn = stringAfter(insideFcn,fcn);
		}
		int indexOfFcn = indexInsideFcn - fcn.length();
		insideFcn = stringBeforeOperator(insideFcn);//exclude subsequent operators
		String stringBeforeFcn = "";
		if(indexOfFcn - 1 > 0)
		{
			stringBeforeFcn = line.substring(0,indexOfFcn - 1);
		}
		String stringAfterFcn = "";
		if(indexInsideFcn + insideFcn.length() <= line.length())
		{
			stringAfterFcn = line.substring(indexInsideFcn + insideFcn.length());
		}
		//System.out.println("split around equation: " + stringBeforeFcn + ";" + fcn + ";" + insideFcn + ";" + stringAfterFcn + ";");
		String[] stringsOut = {insideFcn, stringBeforeFcn, stringAfterFcn};
		return stringsOut;
	}

	private String multiplicationOperators(String equation)
	{
		//act to operate on any multiply or divide operators
		String outString = equation;
		if(equation.contains("/"))
		{
			//System.out.println("divide operator present");
			String[] eqns = split_equation_around_operator(equation, "/");
			
			//get the numbers and evaluate the operation
			double leftNum = convert.Dec_string_to_double(eqns[0]);
			double rightNum = convert.Dec_string_to_double(eqns[1]);
			double eval = leftNum / rightNum;
			//eval = round_for_significant_figures(eval, leftNum, rightNum);
			String evalString = convert.Double_to_dec_string(eval);
			
			//then replace the operation in the original equation string
			outString = eqns[2].concat(evalString).concat(eqns[3]);
		}
		else if(equation.contains("*"))
		{
			//System.out.println("multiply operator present");
			String[] eqns = split_equation_around_operator(equation, "*");
			
			//get the numbers and evaluate the operation
			double leftNum = convert.Dec_string_to_double(eqns[0]);
			double rightNum = convert.Dec_string_to_double(eqns[1]);
			double eval = leftNum * rightNum;
			//eval = round_for_significant_figures(eval, leftNum, rightNum);
			String evalString = convert.Double_to_dec_string(eval);
			
			//then replace the operation in the original equation string
			outString = eqns[2].concat(evalString).concat(eqns[3]);
		}
		
		//are we done with multiplication and division operators?
		if(outString.contains("*") | outString.contains("/"))
		{
			//System.out.println("more divide or multiply operators present");
			//we're not done, so we should iterate recursively
			outString = reduce(outString);
		}

		return outString;
	}
	
	private String additionOperators(String equation)
	{
		//act to operate on any addition or subtraction operators
		String outString = equation;
		if(equation.contains("+"))
		{
			//System.out.println("addition operator present");
			String[] eqns = split_equation_around_operator(equation, "+");
			
			//get the numbers and evaluate the operation
			double leftNum = convert.Dec_string_to_double(eqns[0]);
			double rightNum = convert.Dec_string_to_double(eqns[1]);
			double eval = leftNum + rightNum;
			//eval = round_for_significant_figures(eval, leftNum, rightNum);
			String evalString = convert.Double_to_dec_string(eval);
			
			//then replace the operation in the original equation string
			outString = eqns[2].concat(evalString).concat(eqns[3]);
		}
		else if(equation.contains("-"))
		{
			//System.out.println("subtraction operator present");
			String[] eqns = split_equation_around_operator(equation, "-");
			
			//get the numbers and evaluate the operation
			double leftNum = convert.Dec_string_to_double(eqns[0]);
			double rightNum = convert.Dec_string_to_double(eqns[1]);
			double eval = leftNum - rightNum;
			//eval = round_for_significant_figures(eval, leftNum, rightNum);
			String evalString = convert.Double_to_dec_string(eval);
			
			//then replace the operation in the original equation string
			outString = eqns[2].concat(evalString).concat(eqns[3]);
		}
		
		//are we done with addition and subtraction operators?
		String shortenedString = remove_special_minus_case(outString);
		//System.out.println("looking at eqn without special '-' case: " + shortenedString);
		if(shortenedString.contains("+") | shortenedString.contains("-"))
		{
			//System.out.println("more addition or subtraction operators present");
			//we're not done, so we should iterate recursively
			outString = reduce(outString);
		}

		return outString;
	}
	
	private String integerOperators(String equation)
	{
		//act to operate on any integer operators
		String outString = equation;
		if(equation.contains("&"))
		{
			//get the numbers and evaluate the operation
			String[] eqns = split_equation_around_operator(equation, "&");
			
			//I assume that anyone using the integer operators is not abusing them
			//and putting in values with decimal places
			int leftNum = convert.Hex_or_Dec_string_to_int(eqns[0]);
			int rightNum = convert.Hex_or_Dec_string_to_int(eqns[1]);
			int eval = leftNum & rightNum;
			String evalString = convert.Int_to_dec_string(eval);
			
			//then replace the operation in the original equation string
			outString = eqns[2].concat(evalString).concat(eqns[3]);
			
		}
		else if(equation.contains("|"))
		{
			//get the numbers and evaluate the operation
			String[] eqns = split_equation_around_operator(equation, "|");
			
			//I assume that anyone using the integer operators is not abusing them
			//and putting in values with decimal places
			int leftNum = convert.Hex_or_Dec_string_to_int(eqns[0]);
			int rightNum = convert.Hex_or_Dec_string_to_int(eqns[1]);
			int eval = leftNum | rightNum;
			String evalString = convert.Int_to_dec_string(eval);
			
			//then replace the operation in the original equation string
			outString = eqns[2].concat(evalString).concat(eqns[3]);
		}
		else if(equation.contains("XOR") || equation.contains("xor"))
		{
			//get the numbers and evaluate the operation
			String[] eqns = new String[4];
			if(equation.contains("XOR"))
			{
				eqns = split_equation_around_operator(equation, "XOR");
			}
			else
			{
				eqns = split_equation_around_operator(equation, "xor");
			}
			
			//I assume that anyone using the integer operators is not abusing them
			//and putting in values with decimal places
			int leftNum = convert.Hex_or_Dec_string_to_int(eqns[0]);
			int rightNum = convert.Hex_or_Dec_string_to_int(eqns[1]);
			int eval = leftNum ^ rightNum;
			String evalString = convert.Int_to_dec_string(eval);
			
			//then replace the operation in the original equation string
			outString = eqns[2].concat(evalString).concat(eqns[3]);
		}
		else if(equation.contains("<<"))
		{
			//get the numbers and evaluate the operation
			String[] eqns = split_equation_around_operator(equation, "<<");
			
			//I assume that anyone using the integer operators is not abusing them
			//and putting in values with decimal places
			int leftNum = convert.Hex_or_Dec_string_to_int(eqns[0]);
			int rightNum = convert.Hex_or_Dec_string_to_int(eqns[1]);
			int eval = leftNum << rightNum;
			String evalString = convert.Int_to_dec_string(eval);
			
			//then replace the operation in the original equation string
			outString = eqns[2].concat(evalString).concat(eqns[3]);
		}
		else if(equation.contains(">>"))
		{
			//get the numbers and evaluate the operation
			String[] eqns = split_equation_around_operator(equation, ">>");
			
			//I assume that anyone using the integer operators is not abusing them
			//and putting in values with decimal places
			int leftNum = convert.Hex_or_Dec_string_to_int(eqns[0]);
			int rightNum = convert.Hex_or_Dec_string_to_int(eqns[1]);
			int eval = leftNum >> rightNum;
			String evalString = convert.Int_to_dec_string(eval);
			
			//then replace the operation in the original equation string
			outString = eqns[2].concat(evalString).concat(eqns[3]);
		}
		else if(equation.contains("~"))
		{
			//get the numbers around the twiddle
			String[] eqns = split_equation_around_operator(equation, "~");
			//System.out.println("evaluate '" + equation + "' for '~', left: " + eqns[2] + ", mid: " + eqns[0] + ", ~, right mid: " + eqns [1] + ", right: " + eqns[3]);
			//we only want to evaluate what is to the right of the operator
			int rightNum = convert.Hex_or_Dec_string_to_int(eqns[1]);
			int eval = ~rightNum;
			String evalString = convert.Int_to_dec_string(eval);
			//System.out.println("evaluates to int: " + eval + ", and str: " + evalString);
			
			//then replace the operation in the original equation string
			outString = eqns[2].concat(eqns[0]).concat(evalString).concat(eqns[3]);
		}
		
		//are we done with integer operators?
		if(outString.contains("&") | outString.contains("|") | outString.contains("XOR") | outString.contains("xor") | outString.contains("<<") | outString.contains(">>") | outString.contains("~"))
		{
			//we're not done, so we should iterate recursively
			outString = reduce(outString);
		}

		return outString;
	}
	
	//this function extracts the string after the input string Tag in the line
	private String stringAfter(String line, String tag)
	{
		String data = line;
		if(line.contains(tag))
		{
			int start = line.indexOf(tag);
			start += tag.length();
			data = line.substring(start);
		}
		return data;
	}

	private String stringBefore(String line, String tag)
	{
		String data = line;
		if(line.contains(tag))
		{
			int end = line.indexOf(tag);
			data = line.substring(0,end);
		}
		return data;
	}
	
	private String stringBeforeOperator(String line)
	{
		//extract the string before any operator
		//be careful that we do not return a string with no length-
		//there are cases where a "-" sign will be the first character
		//and we don't want to exclude this
		String data = line;
		int[] special_minus_indices = special_minus_case(line);//the list of all special case '-' operators
		
		//create the list of all other operators
		int[] indices_of_all_operators = new int[0];
		
		//String[] operators = {"(",")","^","exp","ln","log","*","/","+","-","&","|","XOR","xor","<<",">>"};
		for(int i = 0; i < operators.length; i++)
		{
			int[] indices_of_this_operator = all_operator_indices(line,operators[i]);
			for(int j = 0; j < indices_of_this_operator.length; j++)
			{
				indices_of_all_operators = Arrays.copyOf(indices_of_all_operators, indices_of_all_operators.length + 1);
				indices_of_all_operators[indices_of_all_operators.length - 1] = indices_of_this_operator[j];
			}
		}
		
		Arrays.sort(indices_of_all_operators);
		
		//then work forward through the sorted list of all operators to find the lowest index that is not shared in the 
		//special case list
		int index_of_first_operator = line.length();//set to index one past the end of the array in case we want to recreate the entire array
		for(int i = 0; i < indices_of_all_operators.length; i++)
		{
			int test_index_of_first_operator = indices_of_all_operators[i];
			boolean no_match_with_special_case = true;
			for(int j = 0; j < special_minus_indices.length; j++)
			{
				if(special_minus_indices[j] == test_index_of_first_operator)
				{
					//we cannot use this index
					no_match_with_special_case = false;
					break;
				}
			}
			if(no_match_with_special_case)
			{
				index_of_first_operator = test_index_of_first_operator;
				break;
			}
		}
		
		//then take the string before the index of the final operator
		if(index_of_first_operator <= line.length())
		{
			data = line.substring(0,index_of_first_operator);
		}
		
		//System.out.println("original: [" + line + "] after removing succeeding operators: [" + data +"]");
		return data;//until we finally return a string with no operators in it
	}
	
	private String stringAfterOperator(String line)
	{
		//extract the string after any operator
		String data = line;
		int[] special_minus_indices = special_minus_case(line);//the list of all special case '-' operators
		
		//create the list of all other operators
		int[] indices_of_all_operators = new int[0];
		
		//String[] operators = {"(",")","^","exp","ln","log","*","/","+","-","&","|","XOR","xor","<<",">>"};
		for(int i = 0; i < operators.length; i++)
		{
			int[] indices_of_this_operator = all_operator_indices(line,operators[i]);
			for(int j = 0; j < indices_of_this_operator.length; j++)
			{
				indices_of_all_operators = Arrays.copyOf(indices_of_all_operators, indices_of_all_operators.length + 1);
				indices_of_all_operators[indices_of_all_operators.length - 1] = indices_of_this_operator[j];
			}
		}
		
		Arrays.sort(indices_of_all_operators);
		
		//then work backward through the sorted list of all operators to find the highest index that is not shared in the 
		//special case list
		int index_of_final_operator = 0;
		for(int i = indices_of_all_operators.length - 1; i >= 0; i--)
		{
			int test_index_of_final_operator = indices_of_all_operators[i];
			boolean no_match_with_special_case = true;
			for(int j = special_minus_indices.length - 1; j >= 0; j--)
			{
				if(special_minus_indices[j] == test_index_of_final_operator)
				{
					//we cannot use this index
					no_match_with_special_case = false;
					break;
				}
			}
			if(no_match_with_special_case)
			{
				index_of_final_operator = test_index_of_final_operator;
				index_of_final_operator += 1;
				break;
			}
		}
		
		//then take the string after the index of the final operator
		if(index_of_final_operator < line.length())
		{
			data = line.substring(index_of_final_operator);
		}
		
		//System.out.println("original: [" + line + "] after removing preceding operators: [" + data +"]");
		return data;//until we finally return a string with no operators in it
	}
	
	private boolean containsAnyOperator(String line)
	{
		//String[] operators = {"(",")","^","exp","ln","log","*","/","+","-","&","|","XOR","xor","<<",">>"};
		for(int i = 0; i < operators.length; i++)
		{
			if(line.contains(operators[i]))
			{
				//again, be careful of the '-' sign, since this could be the sign or an operator
				if(i == 9)
				{
					//we're dealing with the "-" sign, so be careful about excluding it in some places
					//the two cases where the "-" sign is not an operator is where it is immediately
					//preceded by another operator or when it is the first non-space character
					int[] listOfIndices = special_minus_case(line);
					//make a version of the input string that does not contain these special
					//case '-' signs
					String recheck = "";
					if(listOfIndices.length > 0)
					{
						recheck = line.substring(0,listOfIndices[0]);
						for(int j = 1; j < listOfIndices.length; j++)
						{
							recheck = recheck.concat(line.substring(listOfIndices[j-1]+1,listOfIndices[j]));
						}
						return containsAnyOperator(recheck);
					}
					else
					{
						return true;
					}
				}
				else
				{
					return true;
				}
			}
		}
		return false;
	}
	
	private int[] special_minus_case(String input)
	{
		//this function scans through the input string and checks for
		//the case that the '-' character is either immediately preceded by
		//another operator or is the first character in the input string.
		//a list of the indices of these special cases is output
		//String[] operators = {"(",")","^","exp","ln","log","*","/","+","-","&","|","XOR","xor","<<",">>"};
		int[] indices = new int[0];
		String prev_char = "+";
		for(int i = 0; i < input.length(); i++)
		{
			char consider_char = input.charAt(i);
			if(consider_char != ' ')//only do things if we're not on a space
			{
				if(consider_char == '-')//special action may be taken on a '-' character
				{
					for(int j = 0; j < operators.length; j++)//look through all possible operators
					{
						if(operators[j].equals(prev_char))//if the previous character set in the input string is an operator
						{
							indices = Arrays.copyOf(indices, indices.length + 1);
							indices[indices.length - 1] = i;//then save the current index as a special case
							//System.out.println("Special '-' in: [" + input + "] at " + i);
							break;
						}
					}
					prev_char = "-";
				}
				else//when not on a '-' character, we want to update the prev_char string
				{
					boolean operatorMatch = false;
					for(int j = 0; j < operators.length; j++)//loop through all operators
					{
						String current_operator = operators[j];
						for(int k = 0; k < current_operator.length(); k++)//within the operator, see if the current character is plausible
						{
							char op_char = current_operator.charAt(k);
							if(op_char == consider_char)
							{
								//we have a possible match with an operator!
								if(k == 0)
								{
									operatorMatch = true;
									prev_char = current_operator.substring(0,1);
									break;
								}
								else if(prev_char.charAt(k-1) == current_operator.charAt(k-1))//if we're still tracking the same operator, append the next character
								{
									operatorMatch = true;
									prev_char = prev_char.concat(current_operator.substring(k,k+1));
									break;
								}
							}
						}
						if(operatorMatch)
						{
							break;
						}
					}
					if(!operatorMatch)
					{
						prev_char = "0";//not an operator, any string will work
					}
				}
			}
		}
		return indices;
	}
	
	private String remove_special_minus_case(String line)
	{
		int[] listOfIndices = special_minus_case(line);
		//make a version of the input string that does not contain these special
		//case '-' signs
		String removedLine = line;
		if(listOfIndices.length > 0)
		{
			removedLine = line.substring(0,listOfIndices[0]);
			for(int j = 1; j < listOfIndices.length; j++)
			{
				removedLine = removedLine.concat(line.substring(listOfIndices[j-1]+1,listOfIndices[j]));
			}
			if(listOfIndices[listOfIndices.length - 1] + 1 < line.length())
			{
				removedLine = removedLine.concat(line.substring(listOfIndices[listOfIndices.length - 1] + 1));
			}
		}
		return removedLine;
	}
	
	public String replace_variable_with_value(String value, String equation)
	{
		//this function substitutes all occurrences of "#" with the input value
		return replace_variable_String_with_value(value, "#", equation);
	}
	
	public String replace_variable_String_with_value(String value, String variable, String equation)
	{
		// this function substitutes all occurrences of the input variable string
		//in equation with value
		//be careful not to use variable names that overlap with operators!
		
		String substituted_equation = equation;
		while(substituted_equation.contains(variable))
		{
			int index = substituted_equation.indexOf(variable);
			String firstPart = "";
			if(index > 0 & index < substituted_equation.length())
			{
				firstPart = substituted_equation.substring(0,index);
			}
			String lastPart = "";
			if(index + variable.length() < substituted_equation.length())
			{
				lastPart = substituted_equation.substring(index + variable.length());
			}
			substituted_equation = firstPart.concat(value).concat(lastPart);
		}
		//System.out.println("replace: " + variable + " with " + value + " in " + equation + " to get: " + substituted_equation);
		return substituted_equation;
	}
	
	private double round_for_significant_figures(double result, double inOne, double inTwo)
	{
		//round a number based on the number of digits in the two input values
		double sigFigs = result;
		
		//we'll use a string representation of the numbers in order to do this
		Conversion_Handler convert = new Conversion_Handler();
		String inOneString = convert.Double_to_dec_string(inOne);
		String inTwoString = convert.Double_to_dec_string(inTwo);
		
		//count the number of digits in each input number to figure out how many the output should have
		int reqd_result_sig_figs = num_sig_figs(inOneString);
		int inTwo_sig_figs = num_sig_figs(inTwoString);
		if(inTwo_sig_figs < reqd_result_sig_figs) {
			reqd_result_sig_figs = inTwo_sig_figs;
		}
		
		//System.out.println("result " + result + ", in1 " + inOne + ", in2 " + inTwo + ", inTwo sig figs: " + inTwo_sig_figs + ", reqd sig figs " + reqd_result_sig_figs);
		sigFigs = round_off(result, reqd_result_sig_figs);
		//System.out.println("Fixed: " + sigFigs);
		
		return sigFigs;
		
		/*
		int numPastDecimal = 0;
		
		//we'll use a string representation of the numbers in order to do this
		Conversion_Handler convert = new Conversion_Handler();
		String inOneString = convert.Double_to_dec_string(inOne);
		String inTwoString = convert.Double_to_dec_string(inTwo);
		String resultString = convert.Double_to_dec_string(result);
		
		//then count the number of digits past the decimal
		for(int i = 0; i < inOneString.length(); i++)
		{
			if(inOneString.charAt(i) == '.')
			{
				numPastDecimal = inOneString.length() - (i + 1);
				break;
			}
		}
		//then check the other input and take its value if it is smaller
		for(int i = 0; i < inTwoString.length(); i++)
		{
			if(inTwoString.charAt(i) == '.')
			{
				int numPast = inTwoString.length() - (i + 1);
				if(numPast < numPastDecimal)
				{
					numPastDecimal = numPast;
				}
				break;
			}
		}
		
		//System.out.println("Significant figures in " + resultString + " is: " + numPastDecimal);
		
		//now update the output as a rounded string based on the number of significant figures
		int indexOfDecimal = 0;
		if(resultString.contains("."))
		{
			//if the result has a decimal, use this as a starting point
			indexOfDecimal = resultString.indexOf(".");
			//let's check the value past the significant digit for rounding
			int valPastSigFig = 0;
			int indexPastSigFig = indexOfDecimal + numPastDecimal + 1;
			if(resultString.length() > indexPastSigFig)
			{
				String pastSigFig = String.valueOf(resultString.charAt(indexPastSigFig));//collect the digit
				valPastSigFig = convert.Hex_or_Dec_string_to_int(pastSigFig);
				//System.out.println("value past sig fig: " + valPastSigFig + " At index: " + indexPastSigFig);
			}
			boolean roundUp = false;
			if(valPastSigFig >= 5)
			{
				roundUp = true;
			}
			
			//then get ahold of the resulting value up to and including the significant digit
			if(resultString.length() > indexOfDecimal + numPastDecimal)
			{
				resultString = resultString.substring(0,indexOfDecimal + numPastDecimal + 1);
			}
			else
			{
				for(int i = 0; i < numPastDecimal - (resultString.length() - (indexOfDecimal + 1)); i++)
				{
					resultString = resultString.concat("0");
				}
			}
			
			//finally, convert the string to a double and round up if necessary
			sigFigs = convert.Dec_string_to_double(resultString);
			if(roundUp)
			{
				double add = 1.0;
				for(int i = 0; i < numPastDecimal; i ++)
				{
					add = add/10;
				}
				sigFigs += add;
			}
		}
		else
		{
			//if there is no decimal, add one and the necessary number of zeros
			resultString = resultString.concat(".");
			for(int i = 0; i < numPastDecimal; i++);
			{
				resultString = resultString.concat("0");
			}
			sigFigs = convert.Dec_string_to_double(resultString);
		}
		return sigFigs;
		*/
	}
	
	private double round_off(double input_val, int reqd_sig_figs) {
		//round the input value toward 'sig figs' of digits
		double sigFigs = input_val;

		//we'll use a string representation of the numbers in order to do this
		Conversion_Handler convert = new Conversion_Handler();
		String resultString = convert.Double_to_dec_string(input_val);
		int current_result_sig_figs = num_sig_figs(resultString);

		if(reqd_sig_figs == 0) {
			return input_val;
		}

		//then decide if the result needs to be shortened or lengthened
		if(current_result_sig_figs > reqd_sig_figs) {
			//the result string is too long
			//we need to shorten the result string by rounding beyond the decimal

			String shortened_resultString = "";
			char sig_digit = '0';
			char after_sig_digit = '0';
			int digit_count = 0;
			boolean decimal_found = false;
			for (int i = 0; i < resultString.length(); i++) {
				char current_char = resultString.charAt(i);
				if(current_char >= 48 && current_char <= 57) {
					digit_count += 1;
				} else if(current_char == 46) {
					decimal_found = true;
				}
				
				if(!decimal_found || decimal_found && digit_count <= reqd_sig_figs) {
					shortened_resultString = shortened_resultString + current_char;
					if(current_char != 46) {
						sig_digit = current_char;//we don't want the decimal as our significant digit
					}
				}

				if((digit_count > reqd_sig_figs && decimal_found) || i == resultString.length()-1) {
					if(current_char != 46) {
						after_sig_digit = current_char;//we take whatever the last character is in the number - be careful, we may end on a '.'
						break;
					}
				}
			}
			//System.out.println("Shortened number: " + shortened_resultString + " after sig digit: " + after_sig_digit + " sig digit: " + sig_digit);

			//then, decide if we should round the number
			int sig_digit_int = convert.Hex_or_Dec_string_to_int("" + sig_digit);//and now the char is a string... doh!
			double add_to_round = 0;
			if(after_sig_digit >= 53 && sig_digit_int % 2 != 0) {
				//we should round up!
				//first, find how far we are past the decimal
				int index_of_decimal = shortened_resultString.indexOf(".");
				if(index_of_decimal >= 0) {
					int pastDecimal = shortened_resultString.length() - 1 - index_of_decimal;
					add_to_round = Math.pow(10,-1*pastDecimal);
				} else {
					add_to_round = 1;
				}
				//System.out.println("We should round up! -> add: " + add_to_round + " due to index of decimal: " + index_of_decimal + " and length of string: " + shortened_resultString.length());
			}
			//convert our value back to a double
			sigFigs = convert.Dec_string_to_double(shortened_resultString);
			sigFigs = sigFigs + add_to_round;

		} else if (current_result_sig_figs < reqd_sig_figs) {
			//the result string is too short, so we'll append zeros
			if(!resultString.contains(".")) {
				resultString = resultString.concat(".");
			}

			//the decimal should already be written, so just append...
			int numZerosToAdd =  reqd_sig_figs - current_result_sig_figs;
			for(int i = 0; i < numZerosToAdd; i++) {
				resultString = resultString.concat("0");
			}
			sigFigs = convert.Dec_string_to_double(resultString);
		}
		//System.out.println("We've finished rounding: " + sigFigs);
		return sigFigs;

			/*
			//it'll be easier to use an integer version of the number
			boolean isNeg = false;
			int index_of_decimal = resultString.length();
			String resultInteger = "";
			for (int i = 0; i < resultString.length(); i++) {
				if(resultString.charAt(i) == '.') {
					index_of_decimal = i;//respecting the possibility of a "-" character at the beginning
					if(resultInteger.length() > reqd_sig_figs) { //at least include data up to the decimal point
						reqd_sig_figs = resultInteger.length();
					}
				} else if(resultString.charAt(i) == '-') {
					isNeg = true;
				} else {
					resultInteger = resultInteger + resultString.charAt(i);
				}
			}

			//now that we have the integer value, truncate it to line up with the required number of significant digits
			int index_of_sig_fig = reqd_sig_figs - 1;
			String past_sig_fig = String.valueOf(resultInteger.charAt(index_of_sig_fig + 1));//collect the digit that will dictate how we round
			String at_sig_fig = String.valueOf(resultInteger.charAt(index_of_sig_fig));
			int val_past_sig_fig = convert.Hex_or_Dec_string_to_int(past_sig_fig);
			int val_at_sig_fig = convert.Hex_or_Dec_string_to_int(at_sig_fig);
			//convert the shortened resultInteger to an integer, round, and convert back to string (this seems dumb)
			int resultInt = convert.Hex_or_Dec_string_to_int(resultInteger.substring(0,index_of_sig_fig+1));
			if(val_past_sig_fig >= 5 && val_at_sig_fig % 2 != 0) {
				//round up!
				if(isNeg) {
					resultInt -= 1;
				} else {
					resultInt += 1;
				}
			}
			resultString = convert.Int_to_dec_string(resultInt);
			System.out.println(resultString);

			//then add back in the "-" and "." characters if they are required
			if(isNeg) {
				resultString = "-" + resultString;
			}
			if(resultString.length() > index_of_decimal) {
				resultString = resultString.substring(0,index_of_decimal) + "." + resultString.substring(index_of_decimal);
			}


		} else if (current_result_sig_figs < reqd_sig_figs) {
			//the result string is too short, so we'll append zeros
			if(!resultString.contains(".")) {
				resultString = resultString.concat(".");
			}

			//the decimal should already be written, so just append...
			int numZerosToAdd =  reqd_sig_figs - current_result_sig_figs;
			for(int i = 0; i < numZerosToAdd; i++) {
				resultString = resultString.concat("0");
			}
		}

		sigFigs = convert.Dec_string_to_double(resultString);
		return sigFigs;
		*/
	}
	
	private int num_sig_figs(String num_to_eval) {
		//number of significant digits - be mindful of "." and "-" characters
		int skip_chars = 0;
		if(num_to_eval.contains(".")){
			skip_chars += 1;
		} 
		if(num_to_eval.contains("-")) {
			skip_chars += 1;
		}
		return num_to_eval.length() - skip_chars;
	}
	
	private int sig_figs_eqn(String orig_eqn) {
		//scan through the original equation and pull out each term's number of significant digits. I'll ignore rules for add/subt vs mult/div
		int sig_digits = 100;//an unreasonably high guess, but also a limitation on what the program can do
		
		int num_digits = 0;
		for(int i = 1; i < orig_eqn.length(); i++) {
			//collect numbers, stop and evaluate at each operator or space
			char current_char = orig_eqn.charAt(i);
			if(current_char >= 48 && current_char <= 57) {
				//we're on a number !
				num_digits += 1;
			} else if(current_char != '.' && current_char != ' '){
				//not on a number, compare to our number of significant digits, and reset the counter
				if(num_digits < sig_digits && num_digits != 0) {
					sig_digits = num_digits;
				}
				num_digits = 0;
			}
		}
		if(num_digits < sig_digits && num_digits != 0) {
			sig_digits = num_digits;
		}
		
		//System.out.println(orig_eqn + " has " + sig_digits + " significant digits.");
		return sig_digits;
	}
}

/*
 * //let's test the equation parser
		    String equation = "12/3 - (1+2) - 3*4";
			    //equation = " -4 - -3 ";
			    //equation = "(2^8 + (2^2))/8";
			    //equation = "exp(-2^3) + (1^2)^3";
			    //equation = "ln(exp(log(2)^3)) + (2^2)^3";
			    //equation = "10 >> 3";
			    //equation = "0xf0 xor 0x0f";
			    //equation = "32 | 8";
				//equation = "(3.000+16.000)*375.00/12.000";
		    	equation = "750/4";
			    Equation_Parser parse = new Equation_Parser();
			    //fix this! figure out "-" operator!
			    double solution = parse.evaluate(equation);
			    System.out.println(equation + " = " + solution);
	    */
