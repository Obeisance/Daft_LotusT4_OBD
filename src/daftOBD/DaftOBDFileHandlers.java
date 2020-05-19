package daftOBD;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.util.Arrays;

public class DaftOBDFileHandlers {


	public boolean replace_line_in_text_file(File filename, String lineLabel, String newLineText)
	{
		/*
		 * This function searches a File for the ascii text line that begins with lineLabel
		 * and then creates a new file that contains all of the original file's data
		 * but has newLineText replacing whatever came after the lineLabel
		 */
		boolean successFlag = false;//the variable that we return indicating we had success
		
		//let's prepare a text file to copy the data to as we are modifying it
		//when we're done, we'll rename the temp file to overwrite the old file
		File tempFile = new File("temporary_DaftOBD_file.txt");
		
		/*
		boolean writeFileIsOpen = true;
		BufferedWriter write_out_text;
		try {
			write_out_text = new BufferedWriter(new FileWriter(tempFile));
		} catch (IOException e) {
			// we failed to open the file to write data to
			e.printStackTrace();
			writeFileIsOpen = false;
		}
		*/
	
		
		if(filename.exists())
		{
			try {
				BufferedReader read_in_text = new BufferedReader(new FileReader(filename));
				BufferedWriter write_out_text = new BufferedWriter(new FileWriter(tempFile));
				String fileText = null;
				//lets loop through the file to read in all of its data
				while((fileText = read_in_text.readLine()) != null)
				{
					//System.out.println("Line read in:" + fileText);
					if(fileText.startsWith(lineLabel))
					{
						//we've found the line to modify!
						//System.out.println("found the line!");
						successFlag = true;
						String text = lineLabel + newLineText;

						try {
							write_out_text.write(text);
							write_out_text.newLine();
						} catch (IOException e) {
							// failed to write data to file
							e.printStackTrace();
						}

					}
					else
					{
						//just copy data from this line to the temporary file
						//System.out.println("not the line of interest; copy to new file");
						try {
							write_out_text.write(fileText);
							write_out_text.newLine();
						} catch (IOException e) {
							// failed to write data to file
							e.printStackTrace();
						}

					}
				}
				read_in_text.close();
				write_out_text.close();
			} catch (FileNotFoundException e1) {
				// Failed to open the file to read in
				e1.printStackTrace();
			} catch (IOException e1) {
				// Failed to open the file to write to
				e1.printStackTrace();
			};
			
			//now delete the old text file and rename the temporary one
			filename.delete();
			tempFile.renameTo(filename);
		}
		return successFlag;
	}
	
	
	public void replace_or_append_line_in_text_file(File filename, String lineLabel, String newLineText)
	{
		/*
		 * this function reads through the file looking for the lineLabel
		 * when the label is found the line is replaced with the label and the newLineText
		 * if the label is not found, the new line is appended to the end of the file
		 */
		
		if(!replace_line_in_text_file(filename, lineLabel, newLineText))
        {
        	//we failed to replace the line in the file, so let's just append the line to the file
        	//System.out.println("Failed to find the header line, appending to the file");
        	BufferedWriter writer;
			try {
				writer = new BufferedWriter(new FileWriter(filename,true));//set to append
				writer.append(lineLabel + newLineText);
				writer.newLine();
            	writer.close();
			} catch (IOException e1) {
				// Failed to open the file to write data to
				e1.printStackTrace();
			}
        }
	}
	
	public void update_settings_logDefn(File filename, String newLineText)
	{
		/*
		 * this function looks for the line label: "Definition File: " and replaces the
		 * line with the text: "Definition File: " and newLineText
		 */
		replace_or_append_line_in_text_file(filename, "Definition File: ", newLineText);
	}
	
	public void update_settings_logSaveLocation(File filename, String newLineText)
	{
		/*
		 * this function looks for the line label: "Log save location: " and replaces the
		 * line with the text: "Log save location: " and newLineText
		 */
		replace_or_append_line_in_text_file(filename, "Log save location: ", newLineText);
	}
	
	public void update_settings_logParameters(File filename, String newLineText)
	{
		/*
		 * this function looks for the line label: "PIDs active bitmask: " and replaces the
		 * line with the text: "PIDs active bitmask: " and newLineText
		 */
		replace_or_append_line_in_text_file(filename, "PIDs active bitmask: ", newLineText);
	}
	
	public String find_data_after_label_in_file(File filename, String lineLabel) {
		//search through the input file and return the string after the lineLabel
		String string_after_label = "";
		
		if(filename.exists()) 
		{
			try {
				//let's read through the text file using a buffered reader
				BufferedReader reader = new BufferedReader(new FileReader(filename));
				//loop through file lines
				String fileLine = null;
				while((fileLine = reader.readLine()) != null)
				{
					//let's look for the header
					if(fileLine.contains(lineLabel))
					{
						//we've found the line we want, so let's collect the
						//string that shows the file path
						string_after_label = fileLine.substring(lineLabel.length());
						break;
					}
				}
				reader.close();
			} catch (FileNotFoundException e) {
				// failed to open the file
				e.printStackTrace();
			} catch (IOException e) {
				// failed to read the line String from the file
				e.printStackTrace();
			}
		}
		return string_after_label;
	}
	
	public File find_defn_fileLocation(File filename, String lineLabel)
	{
		/*
		 * this function reads through the 'filename' file to find the line label
		 * and returns the file generated based on that text
		 */
		
		File returnFile = null;
		String fileName = find_data_after_label_in_file(filename, lineLabel);
		if(!fileName.isEmpty()) {
			returnFile = new File(fileName);
		}
		return returnFile;
	}
	
	public boolean[] import_PID_reload_list(File settings_file) {
		//import the flag that shows the PIDs that were active at last operation
		boolean[] PIDs_reload = {false};
		String PIDs_mask_string = find_data_after_label_in_file(settings_file, "PIDs active bitmask: ");
		if(!PIDs_mask_string.isEmpty())
		{
			PIDs_reload = new boolean[PIDs_mask_string.length()];
			for(int i = 0; i < PIDs_mask_string.length(); i++) {
				if(PIDs_mask_string.substring(i,i+1).equals("1"))
				{
					PIDs_reload[i] = true;
				}
			}
		}
		return PIDs_reload;
	}
	
	public DaftTreeComponent import_Log_definition_list(File settings_file) {
		DaftTreeComponent treeDefinitionList = new DaftTreeComponent("Parameters");
		
		//look into the settings file to get the filename for the definition file
		File definitionFile = find_defn_fileLocation(settings_file, "Definition File: ");
		boolean onParameterList = false;
		boolean onParameter = false;
		boolean onInit = false;
		boolean receiveFlag = false;
		boolean sendFlag = false;
		boolean onDataList = false;
		boolean onReflash = false;
		boolean onRAMwrite = false;
		
		String mode = null;
		String name = null;
		String initName = null;
		int messagePeriod = 0;
		boolean init_needed = false;
		Serial_Packet send_packet = new Serial_Packet();
		Serial_Packet[] sendPacketList = new Serial_Packet[0];
		Serial_Packet read_packet = new Serial_Packet();
		Serial_Packet[] readPacketList = new Serial_Packet[0];
		boolean repeatRead = false;//a flag indicating that a receive packet line may represent multiple packets
		int replicate = 0;//if a packet is repeated, this is the max number of times it will occur
		boolean read_only_once = false;//a flag associated with a PID indicating that it should only be read once
		boolean beepOnPoll = false;//a flag associated with a PID that is used to signal the user that the value is being read
		
		boolean[] flowControl = new boolean[0];//a vector showing the send/receive order
		
		
		Conversion_Handler convert = new Conversion_Handler();
		
		if(definitionFile != null)
		{
			//loop through the file to find the parameters
			if(definitionFile.exists()) 
			{
				try {
					//let's read through the text file using a buffered reader
					BufferedReader reader = new BufferedReader(new FileReader(definitionFile));
					//loop through file lines
					String fileLine = null;
					while((fileLine = reader.readLine()) != null)
					{
						//let's look for the header tags where the parameter list exists
						//any text outside of this tag set is not to be used
						if(fileLine.contains("<parameter list>"))
						{
							onParameterList = true;
						}
						else if(fileLine.contains("</parameter list>"))
						{
							onParameterList = false;
							//now that we're finished, 'button things up' by setting the list of inter-dependencies
							//for each of the inputs and outputs
							
							//first, get all inputs and outputs
							Input[] inputList = treeDefinitionList.getAllChildren_inputs();
							Output[] outputList = treeDefinitionList.getAllChildren_outputs();
							
							//then loop through each input and each output, submitting each list to the input
							//or output in order to build each ones array of references to the values they
							//depend upon
							for(int i = 0; i < inputList.length; i++)
							{
								inputList[i].update_dependents_from_list_of_candidates(inputList, outputList);
							}
							for(int i = 0; i < outputList.length; i++)
							{
								outputList[i].update_dependents_from_list_of_candidates(inputList, outputList);
							}
							
							//we also want to update the dependents for reading special Serial_Packets
							DaftOBDSelectionObject[] PIDS = treeDefinitionList.getComponents();
							for(int i = 0; i < PIDS.length; i++)
							{
								Serial_Packet[] readList = PIDS[i].getReadPacketList();
								for(int j = 0; j < readList.length; j++)
								{
									readList[j].updateDependentsFromCandidates(outputList);
								}
							}
						}

						//now check for the beginning of a parameter if we're on the parameter list
						if(onParameterList)
						{
							if(fileLine.contains("<pid") & !onInit)
							{
								//we can read in the PID parameters-> set a flag to do so
								onParameter = true;

								//then read in the parts of this line that define the PID
								if(fileLine.contains("name="))
								{
									name = stringBetween(fileLine, "name=\"","\"");
								}
								if(fileLine.contains("mode="))
								{
									mode = stringBetween(fileLine, "mode=\"","\"");
								}
								if(fileLine.contains("init="))
								{
									initName = stringBetween(fileLine, "init=\"","\"");
									init_needed = true;
								}
								else
								{
									init_needed = false;
								}
								
								if(fileLine.contains("read_only_once=\"true\""))
								{
									read_only_once = true;
								}
								else
								{
									read_only_once = false;
								}
								
								if(fileLine.contains("messagePeriodms="))
								{
									String periodString = stringBetween(fileLine, "messagePeriodms=\"","\"");
									messagePeriod = convert.Hex_or_Dec_string_to_int(periodString); 
								}
								
								if(fileLine.contains("beepOnPoll=\"true\""))
								{
									beepOnPoll = true;
								}
								else
								{
									beepOnPoll = false;
								}

								sendPacketList = new Serial_Packet[0];//reset the list of send data packets associated with the parameter
								readPacketList = new Serial_Packet[0];
								flowControl = new boolean[0];

							}
							else if(fileLine.contains("</pid>"))
							{
								/*
							System.out.println("Name: " + name);
							System.out.println("Mode: " + mode);
							System.out.println("DataToSend: " + dataToSend);
							System.out.println("Number of inputs: " + numberInputBoxes);
								 */

								//we should stop and create the DaftOBDSelectionObject
								//DaftOBDSelectionObject DaftTreeLeaf = new DaftOBDSelectionObject(mode, name, dataToSend, numberInputBoxes);
								DaftOBDSelectionObject DaftTreeLeaf = new DaftOBDSelectionObject(mode, name, sendPacketList, readPacketList);
								DaftTreeLeaf.flowControl = flowControl;
								
								if(init_needed)
								{
									DaftTreeLeaf.setInit(initName);
								}
								
								if(read_only_once)
								{
									DaftTreeLeaf.setReadOnce(read_only_once);
								}
								
								if(beepOnPoll)
								{
									DaftTreeLeaf.setBeepOnPoll(beepOnPoll);
								}
								DaftTreeLeaf.setPeriod(messagePeriod);
								
								//then add this object to our tree
								treeDefinitionList.add(DaftTreeLeaf);

								//and clear our PID data buffers
								mode = null;
								name = null;
								messagePeriod = 0;

								//and no longer process our lines to fill the PID data buffers
								onParameter = false;
							}
							else if(fileLine.contains("<init") & !onParameter)
							{
								onInit = true;
								if(fileLine.contains("name="))
								{
									initName = stringBetween(fileLine,"name=\"","\"");
								}
								sendPacketList = new Serial_Packet[0];//reset the list of send data packets associated with the init sequence
								readPacketList = new Serial_Packet[0];
								flowControl = new boolean[0];
							}
							else if(fileLine.contains("</init>"))
							{
								onInit = false;
								//save the init list
								//we should stop and create the DaftOBDSelectionObject
								//DaftOBDSelectionObject DaftTreeLeaf = new DaftOBDSelectionObject(mode, name, dataToSend, numberInputBoxes);
								DaftOBDSelectionObject initObject = new DaftOBDSelectionObject("Initialization", initName, sendPacketList, readPacketList);
								initObject.flowControl = flowControl;
								initObject.initID = initName;
								
								//then add the initialization to the tree of objects (the tree will handle
								//the special case that this is an init)
								treeDefinitionList.addInit(initObject);
							}
							else if(fileLine.contains("<reflash"))
							{
								//we'll read in parameters that define the reflash mode
								onReflash = true;
								String reflashName = "";
								if(fileLine.contains("name="))
								{
									reflashName = stringBetween(fileLine,"name=\"","\"");
								}
								
								//that's all we need in order to make the reflash object
								DaftReflashSelectionObject reflash = new DaftReflashSelectionObject(reflashName);
								
								//now check for option settings - this is a bit of a mess
								if(fileLine.contains("encodeButton=\"true\""))
								{
									reflash.setEncodeButtonVisibility(true);//option to show the 'encode message' button
								}
								
								if(fileLine.contains("decodeButton=\"true\""))
								{
									reflash.setDecodeButtonVisibility(true);//option to show the 'decode message' button
								}
								
								if(fileLine.contains("saveButton=\"true\""))
								{
									reflash.setSaveButtonVisibility(true);//option to show the 'save encoded message' button
								}
								
								long targetAddr = 0x0;
								if(fileLine.contains("targetAddress=\""))//check to see if the target reflash address has been outlined
								{
									String addr = stringBetween(fileLine,"targetAddress=\"","\"");
									targetAddr = (convert.Hex_or_Dec_string_to_int(addr) & 0xFFFFFFFF);
								}
								
								long targetLen = 0x80000;//true for T4, not K4
								if(fileLine.contains("targetLength=\""))//check to see if the target reflash length has been outlined
								{
									String len = stringBetween(fileLine,"targetLength=\"","\"");
									targetLen = (convert.Hex_or_Dec_string_to_int(len) & 0xFFFFFFFF);
								}
								
								if(fileLine.contains("stageI=\"true\""))
								{
									reflash.setStageIBootloader();//option to use the 'StageI' verision of the bootloader
								}
								else if(fileLine.contains("stageIIread=\"true\""))
								{
									//if not using the stageI bootloader, we may choose to use the special 
									//version of the stageII bootloader to read addresses
									reflash.setSpecialReadBootloader(targetAddr,targetLen);
									
									if(fileLine.contains("saveSREC=\"true\""))
									{
										reflash.set_saveSREC(true);//change the setting so that we save the s-record during a ROM read - the s-record saving process is very slow due to parsing into strings 
									}
								}
								else
								{
									//we're using the stage II bootloader, so set the target address and length for reflashing
									reflash.setTargetAddress(targetAddr, targetLen);
								}
								
								
								if(fileLine.contains("inputAddr=\"true\""))
								{
									reflash.setInputAddressVisibility(true);//show the JTextFields that allow the user to unput the target address and length for reflashing 
								}
								else if(fileLine.contains("inputAddr=\"false\""))
								{
									reflash.setInputAddressVisibility(false);
								}
								
								if(fileLine.contains("userSetHexStartAddr=\"true\""))
								{
									reflash.setInputHexAddressVisibility(true);//show the JTextField that allows the user to input the address denoting the start address of the input Hex file
								}
								
								if(fileLine.contains("erase_before_reflash=\"false\""))
								{
									reflash.setStageII_erase_flag(false);//set the reflash object to not erase flash sectors when using the stageII bootloader
								}
								
								if(fileLine.contains("stageIImod=\"") && fileLine.contains("stageIImult=\""))
								{
									//set the 'modulo' and 'mult' parameter for encoding stageII reflash messages
									String mod_str = stringBetween(fileLine,"stageIImod=\"","\"");
									String mult_str = stringBetween(fileLine,"stageIImult=\"","\"");
									int mod = convert.Hex_or_Dec_string_to_int(mod_str);
									int mult = convert.Hex_or_Dec_string_to_int(mult_str);
									reflash.setStageII_mod_and_mult(mod, mult);
								}
								
								//and add the object to the tree
								treeDefinitionList.addReflash(reflash);
								
								//then if we're done, remove the flag setting
								if(fileLine.contains("/>"))
								{
									onReflash = false;
								}
							}
							else if(fileLine.contains("</reflash>"))
							{
								onReflash = false;
							}
							else if(fileLine.contains("<RAMwrite")) {
								onRAMwrite = true;
								String data_source = "0x70000";
								String data_length = "0x6000";
								String data_destin = "0x84000";
								String bitRate = "10400";
								
								
								//then read in the parts of this line that define the PID
								if(fileLine.contains("name="))
								{
									name = stringBetween(fileLine, "name=\"","\"");
								}
								if(fileLine.contains("mode="))
								{
									mode = stringBetween(fileLine, "mode=\"","\"");
								}
								if(fileLine.contains("init="))
								{
									initName = stringBetween(fileLine, "init=\"","\"");
									init_needed = true;
								}
								else
								{
									init_needed = false;
								}
								
								if(fileLine.contains("read_only_once=\"true\""))
								{
									read_only_once = true;
								}
								else
								{
									read_only_once = false;
								}
								
								if(fileLine.contains("targetAddressROM="))
								{
									data_source = stringBetween(fileLine,"targetAddressROM=\"","\"");
								}
								if(fileLine.contains("targetLength="))
								{
									data_length = stringBetween(fileLine,"targetLength=\"","\"");
								}
								if(fileLine.contains("targetAddressRAM="))
								{
									data_destin = stringBetween(fileLine,"targetAddressRAM=\"","\"");
								}
								if(fileLine.contains("baud="))
								{
									bitRate = stringBetween(fileLine,"baud=\"","\"");
								}
								
								
								//create the RAM write object- this will ultimately auto-generate
								//the send, receive and flow control variables
								DaftOBDSelectionObject DaftTreeLeaf_RAM_write = new DaftOBDSelectionObject(mode, name, 1, data_destin, data_source, data_length, bitRate);								
								
								if(init_needed)
								{
									DaftTreeLeaf_RAM_write.setInit(initName);
								}
								
								if(read_only_once)
								{
									DaftTreeLeaf_RAM_write.setReadOnce(read_only_once);
								}
								
								//then add this object to our tree
								treeDefinitionList.add(DaftTreeLeaf_RAM_write);
								
								//and clear our PID data buffers
								mode = null;
								name = null;
								
								//then if we're done, remove the flag setting
								if(fileLine.contains("/>"))
								{
									onRAMwrite = false;
								}
							}
							else if(fileLine.contains("/RAMwrite>")) {
								onRAMwrite = false;
							}

							//if we're on a parameter, we should process the string to find the
							//characteristics of our parameter
							if(onParameter | onInit)
							{
								//some characteristics are not defined on the declaration line

								//for instance, data inputs are on their own line in between the "<send> </send>" tags
								if(fileLine.contains("<send"))
								{
									sendFlag = true;
									send_packet = new Serial_Packet();
									//the baud rate and byte length come with the send line
									if(fileLine.contains("baud="))
									{
										String baudString = stringBetween(fileLine, "baud=\"","\"");
										send_packet.setBaudrate(convert.Hex_or_Dec_string_to_int(baudString));
									}
									flowControl = Arrays.copyOf(flowControl, flowControl.length + 1);
									flowControl[flowControl.length - 1] = false;//send message
								}
								else if(fileLine.contains("</send>"))
								{
									sendFlag = false;
									//let's append the send data packet to the list of packets that must be sent for
									//this parameter
									sendPacketList = Arrays.copyOf(sendPacketList, sendPacketList.length + 1);
									sendPacketList[sendPacketList.length - 1] = send_packet;
								}
								else if(fileLine.contains("<receive"))
								{
									receiveFlag = true;//until we get to the end flag, we're reading in a read data packet
									read_packet = new Serial_Packet();
									if(fileLine.contains("baud="))
									{
										String baudString = stringBetween(fileLine, "baud=\"","\"");
										read_packet.setBaudrate(convert.Hex_or_Dec_string_to_int(baudString));
									}
									
									if(fileLine.contains("type="))
									{
										String typeString = stringBetween(fileLine,"type=\"","\"");
										if(typeString.equals("repeatif"))
										{
											repeatRead = true;
										}
										else
										{
											repeatRead = false;
										}
									}
									else
									{
										repeatRead = false;
										replicate = 0;
									}

									if(!repeatRead)
									{
										flowControl = Arrays.copyOf(flowControl, flowControl.length + 1);
										flowControl[flowControl.length - 1] = true;//read message
									}
									else
									{
										//we may repeat reading a packet
										//these have special cases: the number of times the read sequence can be repeated
										if(fileLine.contains("maxRepeatNum"))
										{
											//figure out the number of repeats to perform
											String repeatNum = stringBetween(fileLine,"maxRepeatNum=\"","\"");
											replicate = convert.Hex_or_Dec_string_to_int(repeatNum);
											for(int i = 0; i < replicate; i++)
											{
												flowControl = Arrays.copyOf(flowControl, flowControl.length + 1);
												flowControl[flowControl.length - 1] = true;//read message
											}
										}

										//the condition which indicates to read the packet
										if(fileLine.contains("condition="))
										{
											String equation = stringBetween(fileLine,"condition=\"","\"");
											read_packet.setCondition(equation);
										}

										//an equation to evaluate and compare to condition
										if(fileLine.contains("expression="))
										{
											String equation = stringBetween(fileLine,"expression=\"","\"");
											read_packet.setExpression(equation);
										}
										
										//and dependent variables in the equation
										if(fileLine.contains("dependent="))
										{
											//get and add the list of dependents
											String[] dependentList = getAllOccurrencesBetween(fileLine,"dependent=\"","\"");
											read_packet.setDependentStringList(dependentList);
										}
									}
								}
								else if(fileLine.contains("</receive>"))
								{
									receiveFlag = false;
									//let's append the send data packet to the list of packets that must be received for
									//this parameter

									if(repeatRead)
									{
										for(int i = 0; i < replicate; i++)
										{
											readPacketList = Arrays.copyOf(readPacketList, readPacketList.length + 1);
											readPacketList[readPacketList.length - 1] = read_packet;
										}
									}
									else
									{
										readPacketList = Arrays.copyOf(readPacketList, readPacketList.length + 1);
										readPacketList[readPacketList.length - 1] = read_packet;
									}

								}

								if(sendFlag)
								{
									//read in the bytes that are to be sent
									if(fileLine.contains("<data"))
									{
										String line_subString = stringAfter(fileLine,"<data");
										//we need to decide what kind of data we're reading in
										if(line_subString.length() > 0)
										{
											if(line_subString.charAt(0) == '>')
											{
												//we have a data string, so we can simply pull out the relevant part
												if(fileLine.contains("<data>") & fileLine.contains("</data>"))
												{
													String dataString = stringBetween(fileLine,"<data>","</data>");
													send_packet.appendByte(convert.Hex_or_Dec_string_to_int(dataString));
												}
											}
											else
											{
												//we may have a different data type
												if(line_subString.contains("type="))
												{
													String type = stringBetween(line_subString,"type=\"","\"");
													if(type.equals("sum"))//check for ' type="sum" '
													{												
														send_packet.appendChecksum();//make the last byte a checksum byte
													}
													else if(type.equals("input"))//check for ' type="input" '
													{
														//if we're on an input, look for size="char8" name="0", etc..
														if(line_subString.contains("size="))
														{
															String size = stringBetween(line_subString,"size=\"","\"");
															if(line_subString.contains("name="))
															{
																String input_data_default_val = stringBetween(line_subString,"name=\"","\"");

																//then use the data 'size' to update the send byte packet
																//use a conversion function to get the appropriate data type
																//to put into the send packet
																send_packet.appendInput(input_data_default_val, size);

																if(line_subString.contains("variable="))
																{
																	String varName = stringBetween(line_subString,"variable=\"","\"");
																	send_packet.inputList[send_packet.inputList.length - 1].set_variableName(varName);
																}
																
																//add the equation if it exists
																if(line_subString.contains("expression="))
																{
																	if(line_subString.contains("dependent="))
																	{
																		//get and add the list of dependents
																		String[] dependentList = getAllOccurrencesBetween(line_subString,"dependent=\"","\"");
																		send_packet.inputList[send_packet.inputList.length - 1].set_dependent_list(dependentList);
																	}
																	String equation = stringBetween(line_subString,"expression=\"","\"");
																	//System.out.println("there is an expression: " + equation);
																	send_packet.inputList[send_packet.inputList.length - 1].set_equation(equation);
																}
															}
														}
													}
												}
											}
										}
									}
								}
								else if(receiveFlag)
								{
									//read in the bytes that are to be sent
									if(fileLine.contains("<data"))
									{
										String line_subString = stringAfter(fileLine,"<data");
										//we need to decide what kind of data we're reading in
										if(line_subString.length() > 0)
										{
											if(line_subString.charAt(0) == '>')
											{
												//we have a data string, so we can simply pull out the relevant part
												if(fileLine.contains("<data>") & fileLine.contains("</data>"))
												{
													String dataString = stringBetween(fileLine,"<data>","</data>");
													read_packet.appendByte(convert.Hex_or_Dec_string_to_int(dataString));
												}
											}
											else
											{
												//we may have a different data type
												if(line_subString.contains("type="))
												{
													String type = stringBetween(line_subString,"type=\"","\"");
													if(type.equals("sum"))//check for ' type="sum" '
													{												
														read_packet.appendChecksum();//make the last byte a checksum byte
													}
													else if(type.equals("boolean") || type.equals("int") || type.equals("double") || type.equals("ASCII") || type.equals("lookup") || type.equals("hex"))//check for ' type="' followed by :boolean, int, double, ASCII
													{
														if(type.equals("lookup"))
														{
															onDataList = true;
														}
														//if we're on an interpreted value, look for size="char8" name="0", etc..
														if(line_subString.contains("size="))
														{
															String size = stringBetween(line_subString,"size=\"","\"");
															if(line_subString.contains("name="))
															{
																String interp_val_name = stringBetween(line_subString,"name=\"","\"");

																//then use the data 'size' to update the send byte packet
																//use a conversion function to get the appropriate data type
																//to put into the send packet
																read_packet.appendOutput(interp_val_name, type, size);

																//add the equation if it exists
																if(line_subString.contains("expression="))
																{
																	//check for any dependents, set the list of dependent names
																	if(line_subString.contains("dependent="))
																	{
																		String[] dependentList = getAllOccurrencesBetween(line_subString,"dependent=\"","\"");
																		read_packet.outputList[read_packet.outputList.length - 1].set_dependent_list(dependentList);
																	}
																	String equation = stringBetween(line_subString,"expression=\"","\"");
																	//System.out.println("there is an expression: " + equation);
																	read_packet.outputList[read_packet.outputList.length - 1].set_equation(equation);
																}
																
																if(line_subString.contains("variable="))
																{
																	String varName = stringBetween(line_subString,"variable=\"","\"");
																	read_packet.outputList[read_packet.outputList.length - 1].set_variableName(varName);
																}
																
																if(line_subString.contains("combined"))
																{
																	//this output should be combined with other outputs- figure out
																	//if it is a parent or a child by looping through all other outputs
																	//and looking for its name- if we find a match, this is a child
																	//if there is no match, this is a parent
																	
																	//check outputs that have been submitted to the tree
																	for(int i = 0; i < treeDefinitionList.outputList.length; i++)
																	{
																		//check for previous packet outputs that could be parents
																		//exclude those which are already children to others
																		if(treeDefinitionList.outputList[i].name.equals(interp_val_name) && !treeDefinitionList.outputList[i].child_to_other_output)
																		{
																			//we are currently on a child output, so add it to the parent output
																			treeDefinitionList.outputList[i].add_combined_output(read_packet.outputList[read_packet.outputList.length - 1]);
																		}
																	}
																	
																	
																	//check the outputs that have been submitted to the current PID, but not the tree
																	for(int i = 0; i < readPacketList.length; i++)
																	{
																		Serial_Packet current_read_packet = readPacketList[i];
																		for(int j = 0; j < current_read_packet.outputList.length; j++)
																		{
																			//then check for outputs in the current packet, excluding the one we just created
																			//and excluding those which are already children
																			if(current_read_packet.outputList[j].name.equals(interp_val_name) && !current_read_packet.outputList[j].child_to_other_output)
																			{
																				//we are currently on a child output, so add it to the parent output
																				current_read_packet.outputList[j].add_combined_output(read_packet.outputList[read_packet.outputList.length - 1]);
																			}
																		}
																	}
																	
																	
																	//check the outputs that have been submitted to the current receive packet, but not the current PID yet
																	for(int i = 0; i < read_packet.outputList.length - 1; i++)
																	{
																		//then check for outputs in the current packet, excluding the one we just created
																		//and excluding those which are already children
																		if(read_packet.outputList[i].name.equals(interp_val_name) && !read_packet.outputList[i].child_to_other_output)
																		{
																			//we are currently on a child output, so add it to the parent output
																			read_packet.outputList[i].add_combined_output(read_packet.outputList[read_packet.outputList.length - 1]);
																		}
																	}
																}
																
															}
														}
													}
												}
											}
										}
									}
									else if(onDataList)
									{
										//add components to the data/string list
										if(fileLine.contains("<lookup"))
										{
											//collect the output attributes of "data" and "conv"
											String newVal = stringBetween(fileLine,"conv=\"","\"");
											String dataString = stringBetween(fileLine,"data=\"","\"");
											int data = convert.Hex_or_Dec_string_to_int(dataString);
											
											//add the current list items to the most recent output
											read_packet.outputList[read_packet.outputList.length - 1].append_data_table(data, newVal);
										}
										else if(fileLine.contains("</data>"))
										{
											onDataList = false;
										}
									}
								}
							}//end onParameter/onInit
							else if(onReflash)
							{
								//the only thing to read in the reflash packet is encoding data
								if(fileLine.contains("<decode data>"))
								{
									String decode_data_str = stringBetween(fileLine,"<decode data>","</decode data>");
									int decode_data = convert.Hex_or_Dec_string_to_int(decode_data_str);
									treeDefinitionList.DaftReflash[treeDefinitionList.DaftReflash.length - 1].appendToDecodeBytes(decode_data);
								}
							}
							//System.out.println(fileLine);
						}
					}
					reader.close();
				} catch (FileNotFoundException e) {
					// failed to open the file
					e.printStackTrace();
				} catch (IOException e) {
					// failed to read the line String from the file
					e.printStackTrace();
				}
			}
		}	
		else
		{
			treeDefinitionList = new DaftTreeComponent("Select a valid definition file and restart program");
		}
		
		return treeDefinitionList;
	}
	
	
	public void appendln_byteLogFile(File settings_file, String filename_append, String lineToAppend)
	{
		//first, find the location that we should save the log file to
		File logSaveLocation = find_defn_fileLocation(settings_file, "Log save location: ");
		String byteLogFileName = logSaveLocation.getPath() + "\\Daft OBD byte log " + filename_append + ".txt";
		File byteLogFile = new File(byteLogFileName);
		
		//if the file does not exist, try creating it
		if(!byteLogFile.exists())
		{
			try {
				byteLogFile.createNewFile();
			} catch (IOException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
		}
		
		//if we successfully created/found the file, proceed
		if(byteLogFile.exists()) {
			
			//open the file
			//then append the line to the file
			BufferedWriter writer;
			try {
				writer = new BufferedWriter(new FileWriter(byteLogFile,true));//set to append
				writer.append(lineToAppend);
				writer.newLine();
				//close the file
				writer.close();
			} catch (IOException e) {
				// Failed to open the file to write data to
				e.printStackTrace();
			}
		}
	}
	
	public void appendln_dataLogFile(File settings_file, String filename_append, String lineToAppend)
	{
		//first, find the location that we should save the log file to
		File logSaveLocation = find_defn_fileLocation(settings_file, "Log save location: ");
		String dataLogFileName = logSaveLocation.getPath() + "\\Daft OBD data log " + filename_append + ".txt";
		File dataLogFile = new File(dataLogFileName);
		
		//if the file does not exist, try creating it
		if(!dataLogFile.exists())
		{
			try {
				dataLogFile.createNewFile();
			} catch (IOException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
		}
		
		//if we successfully created/found the file, proceed
		if(dataLogFile.exists()) {
			
			//open the file
			//then append the line to the file
			BufferedWriter writer;
			try {
				writer = new BufferedWriter(new FileWriter(dataLogFile,true));//set to append
				writer.append(lineToAppend);
				writer.newLine();
				//close the file
				writer.close();
			} catch (IOException e) {
				// Failed to open the file to write data to
				e.printStackTrace();
			}
		}
	}
	
	public void appendStrArray_byteLogFile(File settings_file, String filename_append, String[] byteSetString)
	{
		//this function loops through all of the strings in 'byteSetString' and
		//appends them line-by-line to the byte log file

		//first, find the location that we should save the log file to
		File logSaveLocation = find_defn_fileLocation(settings_file, "Log save location: ");
		String byteLogFileName = logSaveLocation.getPath() + "\\Daft OBD byte log " + filename_append + ".txt";
		File byteLogFile = new File(byteLogFileName);

		//if the file does not exist, try creating it
		if(!byteLogFile.exists())
		{
			try {
				byteLogFile.createNewFile();
			} catch (IOException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
		}

		//if we successfully created/found the file, proceed
		if(byteLogFile.exists()) {

			//open the file
			//then append the line to the file
			BufferedWriter writer;
			try {
				writer = new BufferedWriter(new FileWriter(byteLogFile,true));//set to append
				
				for(int i = 0; i < byteSetString.length; i++)
				{ 
					writer.append(byteSetString[i]);
					writer.newLine();
				}
				
				//close the file
				writer.close();
			} catch (IOException e) {
				// Failed to open the file to write data to
				e.printStackTrace();
			}
		}
	}
	
	public void appendStrArray_dataLogFile(File settings_file, String filename_append, String[] dataSetString)
	{
		//this function loops through all strings in 'dataSetString' and
		//appends them line-by-line to the data log file
		
		//first, find the location that we should save the log file to
		File logSaveLocation = find_defn_fileLocation(settings_file, "Log save location: ");
		String dataLogFileName = logSaveLocation.getPath() + "\\Daft OBD data log " + filename_append + ".txt";
		File dataLogFile = new File(dataLogFileName);

		//if the file does not exist, try creating it
		if(!dataLogFile.exists())
		{
			try {
				dataLogFile.createNewFile();
			} catch (IOException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
		}

		//if we successfully created/found the file, proceed
		if(dataLogFile.exists()) {

			//open the file
			//then append the line to the file
			BufferedWriter writer;
			try {
				writer = new BufferedWriter(new FileWriter(dataLogFile,true));//set to append
				
				for(int i = 0; i < dataSetString.length; i++)
				{
					writer.append(dataSetString[i]);
					writer.newLine();
				}
				
				//close the file
				writer.close();
			} catch (IOException e) {
				// Failed to open the file to write data to
				e.printStackTrace();
			}
		}
	}
	
	//this function extracts the string after the input string Tag in the line
	private String stringAfter(String line, String tag)
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
	
	private String stringBefore(String line, String tag)
	{
		String data = line;
		if(line.contains(tag))
		{
			int end = line.indexOf(tag,0);
			data = line.substring(0,end);
		}
		return data;
	}
	
	private String stringBetween(String line, String beginTag, String endTag)
	{
		String laterBit = stringAfter(line, beginTag);
		String data = stringBefore(laterBit, endTag);
		if(line.equals(data))//if we did not find the tag, we should not output a value
		{
			data = "";
		}
		return data;
	}
	
	private String[] getAllOccurrencesBetween(String line, String beginTag, String endTag)
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
