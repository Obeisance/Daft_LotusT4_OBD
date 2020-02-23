package daftOBD;

import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.FocusEvent;
import java.awt.event.FocusListener;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.FileWriter;
import java.io.IOException;
import java.io.OutputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.text.SimpleDateFormat;
import java.util.Arrays;
import java.util.Date;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;

import javax.swing.JButton;
import javax.swing.JCheckBox;
import javax.swing.JFileChooser;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JPanel;
import javax.swing.JTextField;

public class DaftReflashSelectionObject implements ActionListener, FocusListener, Runnable {
	JPanel DaftPanel;//a JPanel to contain all the buttons associated with the reflashing behavior
	JCheckBox DaftSelectionCheckBox;//a checkbox that indicates we want to use the reflashing behavior
	String name;
	boolean isSelected = false;//a flag that states this reflashing is supposed to be used
	boolean isVisible = false;//a flag that indicates that the reflash mode should be displayed in the tree
	
	JButton encodeButton;//a button which begins the encoding process
	boolean showEncodeButton = false;
	JButton decodeButton;//a button which allows us to test the fidelity of the encoded bytes
	boolean showDecodeButton = false;
	JButton saveFileButton;//a button which saves to a text file the encoded byte set
	boolean showSaveFileButton = false;
	
	boolean running = false;//a flag used to indicate that the scheduler is up and running
	
	JFrame fileSelectFrame;
	JButton fileSelectButton;//a button which allows us to select a binary file
	JFileChooser binarySourceChooser;//a file selection window to pick the binary/s-record file
	File hexFile;//a file that contains the data we'll extract for reflashing
	int fileType = 0;//0 = binary, 1 = hex file, 2 = s_record
	
	JLabel addrLabel = new JLabel("Input starting address of hex file: ");
	JTextField inputHexStartAddr;//user input capability to set start address of hex file
	long hexFileStartAddr = 0;//in case we have an incomplete hex file that contains a portion of data- we need to index from an offset
	boolean showInputHexStartAddressTextField = false;
	byte[] hexData = new byte[0];//store the entire hex file as byte data
	
	//some of the buttons (encodeButton, decodeButton) are not necessary in case we want to try on-the-fly encoding
	
	//packet buffers which we'll fill during the encoding process
	//when these are fetched, only send one pair of send-rec. at a time
	//in order to allow for PC processing to continue
	Serial_Packet[] encoded_send_packets = new Serial_Packet[0];
	Serial_Packet[] raw_send_packets = new Serial_Packet[0];
	boolean[] packet_flow_control = new boolean[0];//show order to send/receive packets
	boolean encodingFinished = false;//flag that indicates we've performed the encoding process
	
	boolean stageI = false;//a flag that indicates to use the K4/T4 stage I (true) or stage II bootloader (false)
	boolean specialRead = false;//a flag that indicates we're going to use the modified stage II bootloader to read ROM data
	int collectReflashDataIndex = 0;//used to extract the hex data incrementally for both stageI and stageII reflashing
	
	//special flags/variables for the Stage II boot loader
	int[] stageIIdecodeBytes = new int[0];//an array that contains the 16 integers used to encode the packets for the stage II bootloader
	int stageIImod = 0x5D017;
	int stageIImult = 0xC6E;
	int[] stageIIencodeBytes = new int[0];//counterpart ints for stageIIdecodeBytes
	
	//special parts for decoding reflash data
	int decoding_packet_index = 0;//which packet are we decoding
	boolean decode_data = false;//a flag that tells the runnable to continue decoding data
	int index_of_last_encoded_byte = 0;//the packet index where we should find the final decoded byte
	int sum_of_all_encoded_bytes = 0;//as we decode, we'll sum up all the data
	int decodeKey = 0;//the inverted sum of 9744 and bytes from index_of_last_encoded_byte
	
	//for the sub_packet
	JLabel targetAddrLabel = new JLabel("Input address to reprogram: ");
	JTextField inputTargetAddr;//a user can enter the reflash start addr
	JLabel targetLenLabel = new JLabel("Input lenth of data to reprogram: ");
	JTextField inputTargetLen;//a user can enter the reflash length
	boolean showTargetAddrButton = false;
	long startReflashAddr = 0;//the integer address that represents where we'll begin retrieving data from
	long reflashLen = 0;//the integer length of data that we'll retrieve from the hex file
	boolean erase_flash_sector_before_write = true;//a flag that can be turned off in case we do not want to erase a sector before writing
	int numSubpackets = 0;//a counter containing the number of sub-packets that are needed
	byte[] dataToReflash = new byte[0];//contains only the data to reflash
	int[] numBytesInEachSubPacket = new int[0];//number of bytes to take from 'dataToReflash' and put into each sub-packet
	long subPacketDataAddress = 0;
	int sumOfallDecodedBytes = 0;
	
	//for the encoded packet
	long numberEncodedBytes = 0;//first this is the 32-bit index of the last encoded byte + 5, but it is immediately inverted bitwise to use in encoding
	
	//for the special read version of the stage II bootloader
	int number_special_read_packets = 0;//number of packets needed to read data
	long readDataAddr = 0;//address where we'll read data
	long readDataLen = 0;//byte amount of data to read
	byte[] read_ROM_data = new byte[0];
	boolean saveSREC = false;//a flag that tells us to save an s-record file from a ROM read
	
	//and some special variables for controlling the main program's components
	serial_USB_comm ComPort;//use the FTDI cable
	JTextField statusText;//update the status message
	File settings_file;//get log save location
	String[] string_comm_log = new String[0];
	
	//for managing connecting to the ECU
	boolean active = false;//a flag indicating that connection to ECU is active
	boolean begin_connect_to_ECU;//a flag indicating that we want to establish a connection to the ECU
	int reflash_msg_index = 0;//an index that allows us to keep track of where we are in the reflashing process
	
	Conversion_Handler convert = new Conversion_Handler();//allow us to convert between different number types
	ScheduledExecutorService scheduler = Executors.newScheduledThreadPool(0);//to allow time-scheduled tasks
	
	
	DaftReflashSelectionObject(String nameString) {
		//create the object and initialize the components
		this.DaftPanel = new JPanel();
		
		this.name = nameString;
		this.DaftSelectionCheckBox = new JCheckBox(this.name);
		this.DaftSelectionCheckBox.addActionListener(this);
		this.DaftSelectionCheckBox.setActionCommand("checkbox");
		this.DaftPanel.add(this.DaftSelectionCheckBox);
		
		//optional buttons and commands
		this.binarySourceChooser = new JFileChooser();
		this.fileSelectButton = new JButton("Select Hex File");
		this.fileSelectButton.addActionListener(this);
		this.fileSelectButton.setActionCommand("get hex");
		
		this.encodeButton = new JButton("Encode Flash");
		this.encodeButton.addActionListener(this);
		this.encodeButton.setActionCommand("encode");
		
		this.decodeButton = new JButton("Decode Flash");
		this.decodeButton.addActionListener(this);
		this.decodeButton.setActionCommand("decode");
		
		this.saveFileButton = new JButton("Save Encoded Data");
		this.saveFileButton.addActionListener(this);
		this.saveFileButton.setActionCommand("saveData");
		
		this.inputHexStartAddr = new JTextField("0x000000");
		this.inputHexStartAddr.addActionListener(this);
		this.inputHexStartAddr.setActionCommand("inputHexStAddr");
		this.inputHexStartAddr.addFocusListener(this);
		
		this.inputTargetAddr = new JTextField("0x000000");
		this.inputTargetAddr.addActionListener(this);
		this.inputTargetAddr.setActionCommand("inputTargetAddr");
		this.inputTargetAddr.addFocusListener(this);
		
		this.inputTargetLen = new JTextField("0x00");
		this.inputTargetLen.addActionListener(this);
		this.inputTargetLen.setActionCommand("inputTargetLen");
		this.inputTargetLen.addFocusListener(this);
	}
	
	public void setEncodeButtonVisibility(boolean set)
	{
		//this function allows a user to change the visibility of the 'encode' button
		this.showEncodeButton = set;
	}
	
	public void setDecodeButtonVisibility(boolean set)
	{
		//this function allows a user to change the visibility of the 'Decode' button
		this.showDecodeButton = set;
	}
	
	public void setSaveButtonVisibility(boolean set)
	{
		//this function allows a user to change the visibility of the 'Decode' button
		this.showSaveFileButton = set;
	}
	
	public void setInputAddressVisibility(boolean set)
	{
		//this function sets the visibility of the JTextField allowing the user to input an address for reading or reflashing
		this.showTargetAddrButton = set;
	}
	
	public void setInputHexAddressVisibility(boolean set)
	{
		//this function sets the visibility of the JTextField that allows the user to unput the starting address of the hex file
		this.showInputHexStartAddressTextField = set;
	}
	
	public void setStageIBootloader()
	{
		//this function allows a user to set this object to be related to the stage I bootloader
		setTargetAddress(0x8000, 0x7FFE);//the stage I bootloader only reflashes data from 0x8000 - 0xFFFF (inclusive)
		this.stageI = true;
		this.specialRead = false;//cannot use this function if we're working with the stage I bootloader
	}
	
	public void setTargetAddress(long addr, long len)
	{
		//this function sets the target address and length, but not if we're using the stageI bootloader
		if(!this.stageI)
		{
			this.startReflashAddr = addr;
			this.reflashLen = len;
		}
		
		//then update the values in the input JTextFields
		updateTargetAddress_JTextFields();
	}
	
	public void updateTargetAddress_JTextFields()
	{
		//this function updates the strings shown in the JTextFields related
		//to the target address and length for reflashing or reading
		if(this.specialRead)
		{
			String newAddrSt = convert.Int_to_hex_string((int) this.readDataAddr);
			this.inputTargetAddr.setText("0x".concat(newAddrSt));
			this.inputTargetAddr.repaint();
			
			String newAddrLen = convert.Int_to_hex_string((int) this.readDataLen);
			this.inputTargetLen.setText("0x".concat(newAddrLen));
			this.inputTargetLen.repaint();
		}
		else
		{
			String newAddrSt = convert.Int_to_hex_string((int) this.startReflashAddr);
			this.inputTargetAddr.setText("0x".concat(newAddrSt));
			this.inputTargetAddr.repaint();
			
			String newAddrLen = convert.Int_to_hex_string((int) this.reflashLen);
			this.inputTargetLen.setText("0x".concat(newAddrLen));
			this.inputTargetLen.repaint();
		}
	}
	
	public void appendToDecodeBytes(int nextLongWord)
	{
		//this function appends 'nextLongWord' to the list of data used to encode stage II encryption packets
		this.stageIIdecodeBytes = Arrays.copyOf(this.stageIIdecodeBytes, this.stageIIdecodeBytes.length + 1);
		this.stageIIdecodeBytes[this.stageIIdecodeBytes.length - 1] = nextLongWord;
		
		//by the time that this function is called the stageIImod and stageIImult variables are populated
		//so we'll use them to generate encoded counterpart numbers for encoding data
		int n = 0;
		long guess = Integer.remainderUnsigned( (nextLongWord + n*this.stageIImod) , this.stageIImult);
		//loop until we find an integer, or until we run out of guesses
		while(guess != 0)
		{
			n++;
			guess = Integer.remainderUnsigned( (nextLongWord + n*this.stageIImod) , this.stageIImult);
		}
		guess = Integer.divideUnsigned((nextLongWord + n*this.stageIImod) , this.stageIImult);
		
		this.stageIIencodeBytes = Arrays.copyOf(this.stageIIencodeBytes, this.stageIIencodeBytes.length + 1);
		this.stageIIencodeBytes[this.stageIIencodeBytes.length - 1] = (int) guess;
		
		/*
		if(this.stageIIdecodeBytes.length == 0x10)
		{
			for(int i = 0; i < 0x10; i++)
			{
				System.out.println("Decode byte: " + i + " = " + this.stageIIdecodeBytes[i]);
			}
			for(int i = 0; i < 0x10; i++)
			{
				System.out.println("Encode byte: " + i + " = " + this.stageIIencodeBytes[i]);
			}			
		}
		*/
		
		
	}
	
	public void setSpecialReadBootloader(long address, long lengthToRead)
	{
		//this function allows a user to set this object to be use the stage II bootloader to read ROM
		//this only works if the car has the modified stage II bootloader installed
		this.stageI = false;//the read function works with the stage II bootloader
		this.specialRead = true;
		
		this.readDataAddr = address;
		this.readDataLen = lengthToRead;

		//then update the values in the input JTextFields
		updateTargetAddress_JTextFields();
	}
	
	public void setComm_and_status(serial_USB_comm device, JTextField statusField, File setting)
	{
		//this function updates the comport device and jtextfield used for status messages
		this.ComPort = device;
		this.statusText = statusField;
		this.settings_file = setting;
	}
	
	public void setStageII_mod_and_mult(int newMod, int newMult)
	{
		//this function allows an update to the mod and mult parameters used in the encoding
		//of stage II reflashing packet data
		this.stageIImod = newMod;
		this.stageIImult = newMult;
	}
	
	public void setStageII_erase_flag(boolean state)
	{
		//this function allows the user to set the bootloader packet creator
		//not to set the 'erase before reflash' bytes in the message
		this.erase_flash_sector_before_write = state;
	}
	
	public void set_saveSREC(boolean state)
	{
		//this function allows us to set the flag to save an S-record during a ROM read
		this.saveSREC = state;
	}
	
	public JPanel getReflashPanel()
	{
		//this function builds and returns the JPanel associated with this object
		if(!this.specialRead) 
		{
			this.DaftPanel.add(this.fileSelectButton);
		}
		
		//add in the special behavior buttons in case we want them
		if(this.showInputHexStartAddressTextField && !this.specialRead)
		{
			DaftPanel.add(this.addrLabel);
			DaftPanel.add(this.inputHexStartAddr);
		}
		
		if(this.showTargetAddrButton) 
		{
			if(this.specialRead) {
				this.targetAddrLabel.setText("Input address to read: ");
				this.targetLenLabel.setText("Input lenth of data to read: ");
			}
			DaftPanel.add(this.targetAddrLabel);
			DaftPanel.add(this.inputTargetAddr);
			
			DaftPanel.add(this.targetLenLabel);
			DaftPanel.add(this.inputTargetLen);
		}
		
		if(this.showEncodeButton)
		{
			DaftPanel.add(this.encodeButton);
		}
		
		if(this.showDecodeButton)
		{
			DaftPanel.add(this.decodeButton);
		}
		
		if(this.showSaveFileButton)
		{
			DaftPanel.add(this.saveFileButton);
		}
		
		return this.DaftPanel;
	}
	
	public void begin()
	{
		//this function tells the reflash object to begin communicating with the ECU and to encode the data to send
		if(!this.running)
		{
			//System.out.println("begin!");
			this.running = true;
			this.begin_connect_to_ECU = true;//we want to connect to the ECU
			this.active = true;//we're activating communication with the ECU
			this.reflash_msg_index = 0;//reset the index for sending packets to the ECU
			this.read_ROM_data = new byte[0];
			this.string_comm_log = new String[0];//reset our log array
			

			//change the COM port settings to have shorter timeouts until we get our connection established
			if(!this.stageI)
			{
				this.ComPort.updateBaudRate(29769);//set baud rate for stage II bootloader (we're not changing this via the bootloader option)
			}
			else
			{
				this.ComPort.updateBaudRate(9600);//set baud rate for stage I bootloader
			}
			this.ComPort.changeRx_timeout(20);//

			//and we want to encode messages before connecting to the bootloader
			//send the command to encode ECU data if it's not done
			resetEncodingFlags();
			
			//then schedule further action
			//ScheduledExecutorService scheduler = Executors.newScheduledThreadPool(1);
			//scheduler.schedule(this, 100, TimeUnit.MICROSECONDS);
			//scheduler.shutdown();//make sure to tell the executor to shut down the thread when finished!
			this.scheduler.schedule(this, 100, TimeUnit.MICROSECONDS);
			
		}
		else
		{
			//stop
			this.running = false;
			this.active = false;
			
			//then schedule one further action
			//ScheduledExecutorService scheduler = Executors.newScheduledThreadPool(1);
			//scheduler.schedule(this, 100, TimeUnit.MICROSECONDS);
			//scheduler.shutdown();//make sure to tell the executor to shut down the thread when finished!
			this.scheduler.schedule(this, 100, TimeUnit.MICROSECONDS);
			
			this.statusText.setText("Inactive");
			this.statusText.repaint();
		}
	}
	
	public double get_bytes_to_reflash()
	{
		//this function collects the data necessary for reflashing using the stageI or stageII bootloader
		//it returns a double that represents the fraction complete. It only takes one step
		//at a time in order to allow for incremental operation, so it must be called continuously
		//until it returns 1.00
		
		double fraction_complete = 0;
		
		//figure out how many sub-packets need to be created
		//first, check to see if the reflash address and number of bytes have been set
		if(this.dataToReflash.length < this.reflashLen && this.reflashLen > 0)
		{
			//we need to read in data- be sure to get an even number of bytes
			//and to start on an even address
			if(this.startReflashAddr % 2 != 0)
			{
				this.startReflashAddr -= 1;//get one more byte to be on an even address
				this.reflashLen += 1;
			}
			
			if(this.reflashLen % 2 != 0)
			{
				this.reflashLen += 1;//get one more byte to have an even number of bytes
			}
			//System.out.println("collect the data to reflash");
			
			
			//import the data to reflash
			if(this.fileType == 0)
			{
				//get binary data - this is really straightforward
				//System.out.println("Get binary data...");
				if((this.hexFileStartAddr <= this.startReflashAddr) && (this.hexData.length + this.hexFileStartAddr >= this.startReflashAddr + this.reflashLen))
				{
					//the data we want is wholly contained within the file
					//so we collect it
					this.dataToReflash = Arrays.copyOfRange(this.hexData, (int) (this.startReflashAddr - this.hexFileStartAddr), (int) (this.startReflashAddr + this.reflashLen - this.hexFileStartAddr));
					//System.out.println("First byte at index: 0, is: " + (int) (this.dataToReflash[0] & 0xFF));
					//System.out.println("Last byte at index: " + (this.dataToReflash.length - 1) + ", is: " + (int) (this.dataToReflash[this.dataToReflash.length - 1] & 0xFF));
				}
			}
			else if(this.fileType == 1)
			{
				//get hex file data- convert from ASCII before returning
				if((this.hexFileStartAddr <= this.startReflashAddr))//cannot guess where file ends due to unknown number of line feed characters
				{
					//every data byte in the file is represented by two ASCII character bytes
					long currentAddr = this.hexFileStartAddr - 1;
					if(this.collectReflashDataIndex < this.hexData.length)
					{
						byte currentchar = this.hexData[this.collectReflashDataIndex];
						byte nextchar = this.hexData[this.collectReflashDataIndex + 1];
						if(currentchar != 0xA && currentchar != 0xD && nextchar != 0xA && nextchar != 0xD)
						{
							//we're not on a line feed character

							char firstHex = (char) this.hexData[this.collectReflashDataIndex];
							char seconHex = (char) this.hexData[this.collectReflashDataIndex + 1];

							//we've collected a second data character
							currentAddr += 1;

							if(currentAddr >= this.startReflashAddr && currentAddr < this.startReflashAddr + this.reflashLen)
							{
								//collect the data, convert from ASCII, append to data array
								String hexString = "0x".concat(Character.toString(firstHex)).concat(Character.toString(seconHex));//convert from the raw data to ASCII hex string
								byte currentData = (byte) (convert.Hex_or_Dec_string_to_int(hexString) & 0xFF);//convert from hex string to int

								//then append to data array
								this.dataToReflash = Arrays.copyOf(this.dataToReflash, this.dataToReflash.length + 1);
								this.dataToReflash[this.dataToReflash.length - 1] = currentData;
							}
						}
						this.collectReflashDataIndex += 2;
					}
				}
			}
			else if(this.fileType == 2)
			{
				//get data from S-record- use reference address and convert from ASCII before returning
				if(this.collectReflashDataIndex < this.hexData.length)
				{
					//loop over all data: look for the line feed and header information to inform us of what address
					//we're on

					//interpret the line
					byte currentchar = this.hexData[this.collectReflashDataIndex];
					//check that we have the correct structure
					if((char) currentchar == 'S')
					{
						//read in the rest of the header
						char srec_type = (char) this.hexData[this.collectReflashDataIndex+1];
						String lineLength_str = "0x".concat(Character.toString((char) this.hexData[this.collectReflashDataIndex+2])).concat(Character.toString((char) this.hexData[this.collectReflashDataIndex+3]));
						//System.out.println("Line length string: " + lineLength_str);
						int lineLength = convert.Hex_or_Dec_string_to_int(lineLength_str);
						lineLength -= 1;//remove the sum byte
						long address = -1;

						//adjust our file index:
						this.collectReflashDataIndex += 4;

						//the line address length depends on the s-record type
						//1, 2, and 3 contain data, 7, 8 and 9 contain a starting address
						if(srec_type == '1')
						{
							//address is 16 bits
							String lineAddr_str = "0x".concat(Character.toString((char) this.hexData[this.collectReflashDataIndex])).concat(Character.toString((char) this.hexData[this.collectReflashDataIndex+1])).concat(Character.toString((char) this.hexData[this.collectReflashDataIndex+2])).concat(Character.toString((char) this.hexData[this.collectReflashDataIndex+3]));
							address = (long) convert.Hex_or_Dec_string_to_int(lineAddr_str);
							//System.out.println("S1: " + address);
							this.collectReflashDataIndex += 4;
							lineLength -= 2;//remove address bytes
						}
						else if(srec_type == '2')
						{
							//address is 24 bits
							String lineAddr_str = "0x".concat(Character.toString((char) this.hexData[this.collectReflashDataIndex])).concat(Character.toString((char) this.hexData[this.collectReflashDataIndex+1])).concat(Character.toString((char) this.hexData[this.collectReflashDataIndex+2])).concat(Character.toString((char) this.hexData[this.collectReflashDataIndex+3])).concat(Character.toString((char) this.hexData[this.collectReflashDataIndex+4])).concat(Character.toString((char) this.hexData[this.collectReflashDataIndex+5]));
							address = (long) convert.Hex_or_Dec_string_to_int(lineAddr_str);
							//System.out.println("S2: " + address);
							this.collectReflashDataIndex += 6;
							lineLength -= 3;//remove address bytes
						}
						else if(srec_type == '3')
						{
							//address is 32 bits
							String lineAddr_str = "0x".concat(Character.toString((char) this.hexData[this.collectReflashDataIndex])).concat(Character.toString((char) this.hexData[this.collectReflashDataIndex+1])).concat(Character.toString((char) this.hexData[this.collectReflashDataIndex+2])).concat(Character.toString((char) this.hexData[this.collectReflashDataIndex+3])).concat(Character.toString((char) this.hexData[this.collectReflashDataIndex+4])).concat(Character.toString((char) this.hexData[this.collectReflashDataIndex+5])).concat(Character.toString((char) this.hexData[this.collectReflashDataIndex+6])).concat(Character.toString((char) this.hexData[this.collectReflashDataIndex+7]));
							address = (long) (convert.Hex_or_Dec_string_to_int(lineAddr_str) & 0xFFFFFFFF);
							//System.out.println("S3 address string: " + lineAddr_str + " converts to: " + address);
							this.collectReflashDataIndex += 8;
							lineLength -= 4;//remove address bytes
						}
						else if(srec_type == '7')
						{
							//32-bit address is start of binary file
							String lineAddr_str = "0x".concat(Character.toString((char) this.hexData[this.collectReflashDataIndex])).concat(Character.toString((char) this.hexData[this.collectReflashDataIndex+1])).concat(Character.toString((char) this.hexData[this.collectReflashDataIndex+2])).concat(Character.toString((char) this.hexData[this.collectReflashDataIndex+3])).concat(Character.toString((char) this.hexData[this.collectReflashDataIndex+4])).concat(Character.toString((char) this.hexData[this.collectReflashDataIndex+5])).concat(Character.toString((char) this.hexData[this.collectReflashDataIndex+6])).concat(Character.toString((char) this.hexData[this.collectReflashDataIndex+7]));
							this.hexFileStartAddr = (long) (convert.Hex_or_Dec_string_to_int(lineAddr_str) & 0xFFFFFFFF);
							this.collectReflashDataIndex += 8;
						}
						else if(srec_type == '8')
						{
							//24-bit address is start of binary file
							String lineAddr_str = "0x".concat(Character.toString((char) this.hexData[this.collectReflashDataIndex])).concat(Character.toString((char) this.hexData[this.collectReflashDataIndex+1])).concat(Character.toString((char) this.hexData[this.collectReflashDataIndex+2])).concat(Character.toString((char) this.hexData[this.collectReflashDataIndex+3])).concat(Character.toString((char) this.hexData[this.collectReflashDataIndex+4])).concat(Character.toString((char) this.hexData[this.collectReflashDataIndex+5]));
							this.hexFileStartAddr = (long) (convert.Hex_or_Dec_string_to_int(lineAddr_str) & 0xFFFFFFFF);
							this.collectReflashDataIndex += 6;
						}
						else if(srec_type == '9')
						{
							//16-bit address is start of binary file
							String lineAddr_str = "0x".concat(Character.toString((char) this.hexData[this.collectReflashDataIndex])).concat(Character.toString((char) this.hexData[this.collectReflashDataIndex+1])).concat(Character.toString((char) this.hexData[this.collectReflashDataIndex+2])).concat(Character.toString((char) this.hexData[this.collectReflashDataIndex+3]));
							this.hexFileStartAddr = (long) (convert.Hex_or_Dec_string_to_int(lineAddr_str) & 0xFFFFFFFF);
							this.collectReflashDataIndex += 4;
						}

						//now that we've read in the header, check to see if the line has data we're interested in

						//loop over the bytes in the line
						for(int j = 0; j < lineLength; j++)
						{
							long address_of_byte = address + j;
							//loop through the remaining bytes to get
							String dataByte_str = "0x".concat(Character.toString((char) this.hexData[this.collectReflashDataIndex+j*2])).concat(Character.toString((char) this.hexData[this.collectReflashDataIndex+j*2+1]));
							byte dataByte = (byte) (convert.Hex_or_Dec_string_to_int(dataByte_str) & 0xFF);

							if(address_of_byte == (this.startReflashAddr + this.dataToReflash.length) && this.dataToReflash.length < this.reflashLen)
							{
								//then append the byte to the overall array we're collecting
								this.dataToReflash = Arrays.copyOf(this.dataToReflash, this.dataToReflash.length + 1);
								this.dataToReflash[this.dataToReflash.length -1 ] = dataByte;
							}
						}
						this.collectReflashDataIndex += lineLength*2 + 2;//the data and sum bytes

						//then check to bump our index past the line feed so that the next function
						//call will deal directly with a new line
						if(this.hexData[this.collectReflashDataIndex] == 0xA || this.hexData[this.collectReflashDataIndex] == 0xD)
						{
							this.collectReflashDataIndex += 1;
							//once again in case we have a '\r\n' situation
							if(this.hexData[this.collectReflashDataIndex] == 0xA || this.hexData[this.collectReflashDataIndex] == 0xD)
							{
								this.collectReflashDataIndex += 1;
							}
						}

					}
					else
					{
						//we don't have the first character of
						//an S-record, so advance to the next one
						this.collectReflashDataIndex += 1;
					}
				}
			}
			
			//System.out.println("Data to reflash length: " + this.dataToReflash.length + " bytes");
			
		}
		fraction_complete = (double) (this.dataToReflash.length) / (double) (this.reflashLen);
		
		return fraction_complete;
	}
	
	public double build_decoded_stageII_packet()
	{
		//this function steps through the data file and collects the data into packets for encryption
		//it only builds one packet at a time, and will a double that represents the fraction completion
		
		//maximum number of bytes per encrypted packet: 255 (over ~5 sec)
		//first packet has special bytes: 
		//1 number byte
		//1 ID byte
		//4 'total bytes coming' bytes (only first packet)
		//12 unknown function bytes (only first packet)
		//1 sum byte
		
		//thus first packet can have 255 - 3 - 16 = 236 encoded bytes -> each 3 encoded bytes gives 2 decoded byte => 157.33
		//round down to 234 encoded bytes of sub-packet content in the first packet
		//this leaves 156 decoded bytes
		
		//the majority of packets have 255 - 3 = 252 => 168 decoded bytes of sub-packet content
		
		//the sub-packet also has some special bytes
		//{3 unknown bytes
		//8 'flash sector clear' flag bytes} <- only in first packet, not included in number byte or sum byte
		//1 ID byte (0x55)
		//1 number byte (number in packet excluding header, including sum)
		//3 address bytes (big endian)
		//1 sum byte (8-bit packet checksum)
		//{2 byte sum of all decoded bytes} <- only in final packet
		
		//this means that the first sub-packet can have
		//156 - 3 - 8 - 6 = 139 data bytes (or 137 if also the last packet) -> this needs to be an even number, so round down
		
		//most sub packets can have:
		//168 - 6 = 162 data bytes (or 160 if it is the last packet)
		
		double fractionComplete = 0;
		if(this.numSubpackets == 0)
		{
			//reset our counters
			long dataLeft = this.dataToReflash.length;
			//protect the bootloader region of memory
			if(this.startReflashAddr >= 0x8000)
			{
				this.subPacketDataAddress = this.startReflashAddr;
			}
			else
			{
				this.subPacketDataAddress = 0x8000;
				dataLeft -= (0x8000 - this.startReflashAddr);
			}
			this.numBytesInEachSubPacket = new int[0];
			this.raw_send_packets = new Serial_Packet[0];
			this.sumOfallDecodedBytes = 0;
			
			this.encoded_send_packets = new Serial_Packet[0];
			
			//if we've already imported the data, we should figure out how many
			//sub-packets will be needed to send it
			while(dataLeft > 0)
			{
				//System.out.println("Number of sub packets needed: " + this.numSubpackets + " packets, with " + dataLeft + " bytes left to accommodate");
				if(dataLeft == this.dataToReflash.length)
				{	
					//we're on the first packet
					if(dataLeft <= 136)
					{
						//this is both the first and last packet
						this.numSubpackets += 1;
						this.numBytesInEachSubPacket = Arrays.copyOf(this.numBytesInEachSubPacket, this.numBytesInEachSubPacket.length + 1);
						this.numBytesInEachSubPacket[this.numBytesInEachSubPacket.length - 1] = (int) dataLeft;
						dataLeft = 0;
					}
					else if(dataLeft > 136 && dataLeft <= 138)
					{
						//this is the first and second-to-last packet
						this.numSubpackets += 1;
						this.numBytesInEachSubPacket = Arrays.copyOf(this.numBytesInEachSubPacket, this.numBytesInEachSubPacket.length + 1);
						this.numBytesInEachSubPacket[this.numBytesInEachSubPacket.length - 1] = 136;
						dataLeft -= 136;
					}
					else if(dataLeft > 138)
					{
						//multiple packets will be used, so this is only the first
						this.numSubpackets += 1;
						this.numBytesInEachSubPacket = Arrays.copyOf(this.numBytesInEachSubPacket, this.numBytesInEachSubPacket.length + 1);
						this.numBytesInEachSubPacket[this.numBytesInEachSubPacket.length - 1] = 138;
						dataLeft -= 138;
					}
				}
				else
				{
					//we're not on the first packet, so
					if(dataLeft <= 160)
					{
						//we're either on the last packet
						this.numSubpackets += 1;
						this.numBytesInEachSubPacket = Arrays.copyOf(this.numBytesInEachSubPacket, this.numBytesInEachSubPacket.length + 1);
						this.numBytesInEachSubPacket[this.numBytesInEachSubPacket.length - 1] = (int) dataLeft;
						dataLeft = 0;
					}
					else if(dataLeft > 160 & dataLeft <= 162)
					{
						//or on the second-to-last packet
						this.numSubpackets += 1;
						this.numBytesInEachSubPacket = Arrays.copyOf(this.numBytesInEachSubPacket, this.numBytesInEachSubPacket.length + 1);
						this.numBytesInEachSubPacket[this.numBytesInEachSubPacket.length - 1] = 160;
						dataLeft -= 160;
					}
					else if(dataLeft > 162)
					{
						//or on a middle packet
						this.numSubpackets += 1;
						this.numBytesInEachSubPacket = Arrays.copyOf(this.numBytesInEachSubPacket, this.numBytesInEachSubPacket.length + 1);
						this.numBytesInEachSubPacket[this.numBytesInEachSubPacket.length - 1] = 162;
						dataLeft -= 162;
					}
				}
			}
		}
		else if(this.numSubpackets != this.raw_send_packets.length)
		{
			//from here on out we should populate the packets, one at a time
			int packetIndex = this.raw_send_packets.length;
			int packetSum = 0;
			
			Serial_Packet subPacket = new Serial_Packet();
			
			//System.out.println("Populating sub packet: " + packetIndex + "/" + this.numSubpackets);
			
			if(packetIndex == 0)
			{
				//special first packet behavior
				
				//3 unknown use bytes
				subPacket.appendByte(0x0);
				subPacket.appendByte(0x0);
				subPacket.appendByte(0x0);
				
				//8 sector-erase bytes
				long reflashBeginAddr = this.startReflashAddr;//inclusive
				long reflashEndAddr = this.startReflashAddr + this.reflashLen - 1;//inclusive
				
				int sectorBegin = 0x8000;//inclusive
				int sectorEnd = 0x10000;//exclusive
				if(this.erase_flash_sector_before_write && ( (sectorBegin >= reflashBeginAddr && sectorEnd < reflashEndAddr) || (sectorBegin <= reflashEndAddr && sectorEnd > reflashEndAddr) || (sectorBegin <= reflashBeginAddr && sectorEnd > reflashBeginAddr)))
				{
					subPacket.appendByte(0x1);
					this.sumOfallDecodedBytes = (this.sumOfallDecodedBytes + 1);
				}
				else
				{
					subPacket.appendByte(0x0);
				}
				
				sectorBegin = 0x10000;//inclusive
				sectorEnd = 0x20000;//exclusive
				if(this.erase_flash_sector_before_write && ( (sectorBegin >= reflashBeginAddr && sectorEnd < reflashEndAddr) || (sectorBegin <= reflashEndAddr && sectorEnd > reflashEndAddr) || (sectorBegin <= reflashBeginAddr && sectorEnd > reflashBeginAddr)))
				{
					subPacket.appendByte(0x1);
					this.sumOfallDecodedBytes = (this.sumOfallDecodedBytes + 1);
				}
				else
				{
					subPacket.appendByte(0x0);
				}
				
				sectorBegin = 0x20000;//inclusive
				sectorEnd = 0x30000;//exclusive
				if(this.erase_flash_sector_before_write && ( (sectorBegin >= reflashBeginAddr && sectorEnd < reflashEndAddr) || (sectorBegin <= reflashEndAddr && sectorEnd > reflashEndAddr) || (sectorBegin <= reflashBeginAddr && sectorEnd > reflashBeginAddr)))
				{
					subPacket.appendByte(0x1);
					this.sumOfallDecodedBytes = (this.sumOfallDecodedBytes + 1);
				}
				else
				{
					subPacket.appendByte(0x0);
				}
				
				sectorBegin = 0x30000;//inclusive
				sectorEnd = 0x40000;//exclusive
				if(this.erase_flash_sector_before_write && ( (sectorBegin >= reflashBeginAddr && sectorEnd < reflashEndAddr) || (sectorBegin <= reflashEndAddr && sectorEnd > reflashEndAddr) || (sectorBegin <= reflashBeginAddr && sectorEnd > reflashBeginAddr)))
				{
					subPacket.appendByte(0x1);
					this.sumOfallDecodedBytes = (this.sumOfallDecodedBytes + 1);
				}
				else
				{
					subPacket.appendByte(0x0);
				}
				
				sectorBegin = 0x40000;//inclusive
				sectorEnd = 0x50000;//exclusive
				if(this.erase_flash_sector_before_write && ( (sectorBegin >= reflashBeginAddr && sectorEnd < reflashEndAddr) || (sectorBegin <= reflashEndAddr && sectorEnd > reflashEndAddr) || (sectorBegin <= reflashBeginAddr && sectorEnd > reflashBeginAddr)))
				{
					subPacket.appendByte(0x1);
					this.sumOfallDecodedBytes = (this.sumOfallDecodedBytes + 1);
				}
				else
				{
					subPacket.appendByte(0x0);
				}
				
				sectorBegin = 0x50000;//inclusive
				sectorEnd = 0x60000;//exclusive
				if(this.erase_flash_sector_before_write && ( (sectorBegin >= reflashBeginAddr && sectorEnd < reflashEndAddr) || (sectorBegin <= reflashEndAddr && sectorEnd > reflashEndAddr) || (sectorBegin <= reflashBeginAddr && sectorEnd > reflashBeginAddr)))
				{
					subPacket.appendByte(0x1);
					this.sumOfallDecodedBytes = (this.sumOfallDecodedBytes + 1);
				}
				else
				{
					subPacket.appendByte(0x0);
				}
				
				sectorBegin = 0x60000;//inclusive
				sectorEnd = 0x70000;//exclusive
				if(this.erase_flash_sector_before_write && ( (sectorBegin >= reflashBeginAddr && sectorEnd < reflashEndAddr) || (sectorBegin <= reflashEndAddr && sectorEnd > reflashEndAddr) || (sectorBegin <= reflashBeginAddr && sectorEnd > reflashBeginAddr)))
				{
					subPacket.appendByte(0x1);
					this.sumOfallDecodedBytes = (this.sumOfallDecodedBytes + 1);
				}
				else
				{
					subPacket.appendByte(0x0);
				}
				
				sectorBegin = 0x70000;//inclusive
				sectorEnd = 0x80000;//exclusive
				if(this.erase_flash_sector_before_write && ( (sectorBegin >= reflashBeginAddr && sectorEnd < reflashEndAddr) || (sectorBegin <= reflashEndAddr && sectorEnd > reflashEndAddr) || (sectorBegin <= reflashBeginAddr && sectorEnd > reflashBeginAddr)))
				{
					subPacket.appendByte(0x1);
					this.sumOfallDecodedBytes = (this.sumOfallDecodedBytes + 1);
				}
				else
				{
					subPacket.appendByte(0x0);
				}
			}
			
			//then the sub-packet
			
			//header
			int header = 0x55;
			subPacket.appendByte(header);
			this.sumOfallDecodedBytes = (this.sumOfallDecodedBytes + header);
			packetSum = (packetSum + header);

			//number of bytes in the packet
			int numBytesInPacket = this.numBytesInEachSubPacket[packetIndex] + 5;
			subPacket.appendByte(numBytesInPacket);
			this.sumOfallDecodedBytes = (this.sumOfallDecodedBytes + numBytesInPacket);
			packetSum = (packetSum + numBytesInPacket);

			//address to reflash
			int highAddrByte =(int) (this.subPacketDataAddress >> 16) & 0xFF;//highest order byte of 24-bit address
			int midAddrByte =(int) (this.subPacketDataAddress >> 8) & 0xFF;//middle order byte of 24-bit address
			int lowAddrByte =(int) (this.subPacketDataAddress) & 0xFF;//low order byte of 24-bit address
			subPacket.appendByte(highAddrByte);
			this.sumOfallDecodedBytes = (this.sumOfallDecodedBytes + highAddrByte);
			packetSum = (packetSum + highAddrByte);
			subPacket.appendByte(midAddrByte);
			this.sumOfallDecodedBytes = (this.sumOfallDecodedBytes + midAddrByte);
			packetSum = (packetSum + midAddrByte);
			subPacket.appendByte(lowAddrByte);
			this.sumOfallDecodedBytes = (this.sumOfallDecodedBytes + lowAddrByte);
			packetSum = (packetSum + lowAddrByte);
			

			//then loop to insert the data
			int  offset =(int) (this.subPacketDataAddress - this.startReflashAddr);
			//System.out.println("collect: " + offset + "-" + (offset + this.numBytesInEachSubPacket[packetIndex]) + " compared to size: " + this.dataToReflash.length);
			
			//adjust the address to use in the next packet
			this.subPacketDataAddress += this.numBytesInEachSubPacket[packetIndex];
			
			for(int i = 0; i < this.numBytesInEachSubPacket[packetIndex]; i++)
			{
				int pcktData = (this.dataToReflash[i + offset] & 0xFF);
				subPacket.appendByte(pcktData);
				this.sumOfallDecodedBytes = (this.sumOfallDecodedBytes + pcktData);
				packetSum = (packetSum + pcktData);
			}
			
			//add in the byte-masked sub-packet sum
			subPacket.appendByte( (packetSum % 0x100) );
			this.sumOfallDecodedBytes = (this.sumOfallDecodedBytes + (packetSum % 0x100) );

			//and adjust to make sure we have an even number of bytes in each packet for encoding fidelity
			if(subPacket.getPacket().length % 2 != 0)
			{
				//System.out.println("Add extra byte to sub-packet to keep even length!");
				//add a byte outside of the sub-packet!
				subPacket.appendByte(0x0);
			}
			
			if(packetIndex == this.numSubpackets - 1)
			{
				//and finally the raw_sub_packet_sum, little-endian
				this.sumOfallDecodedBytes = this.sumOfallDecodedBytes % 0x10000;
				int all_pckt_sum_high_byte = (this.sumOfallDecodedBytes >> 8) & 0xFF;
				int all_pckt_sum_low_byte = (this.sumOfallDecodedBytes) & 0xFF;
				subPacket.appendByte(all_pckt_sum_low_byte);
				subPacket.appendByte(all_pckt_sum_high_byte);
			}

			//then add the packet into the list of raw packets
			this.raw_send_packets = Arrays.copyOf(this.raw_send_packets, this.raw_send_packets.length + 1);
			this.raw_send_packets[this.raw_send_packets.length - 1] = subPacket;
		}
		
		
		if(this.numSubpackets > 0)
		{
			fractionComplete = (double) (this.raw_send_packets.length) / (double) (this.numSubpackets);
			//System.out.println("Sub packet build completion " + fractionComplete + " from " + this.raw_send_packets.length + " and " + this.numSubpackets);
		}
		return fractionComplete;
	}
	
	
	private double encode_stageII_packet()
	{
		//this function takes the raw sub-packets for reflashing using the stage II bootloader
		//and encodes them for 'secure' transmission, one packet at a time
		//it returns a value that represents the fraction completed 
		double fractionComplete = 0;
		
		//packet structure looks like:
		//1 number byte
		//1 ID byte
		//4 'total bytes coming' bytes (only first packet)
		//12 unknown function bytes (only first packet)
		//encoded bytes
		//1 sum byte
		// = 19 bytes

		//then, check to see that we've built the sub_packets
		if(this.raw_send_packets.length == this.numSubpackets)
		{
			//then check to see if the encoding bytes have been set
			if(this.stageIIdecodeBytes.length == 0x10)
			{
				//now loop through the raw sub_packets and encrypt them
				//one by one
				if(this.numberEncodedBytes == 0)
				{
					//count up the number of encoded bytes
					for(int i = 0; i < this.raw_send_packets.length; i++)
					{
						this.numberEncodedBytes += this.raw_send_packets[i].getPacket().length;
					}
					this.numberEncodedBytes = this.numberEncodedBytes*3/2;//this had better be an integer!
					this.numberEncodedBytes -= 1;//index of final raw byte is one less than the number of encoded bytes
					//the first packet has an extra 16 index counts
					//every packet has an extra 2 header bytes and a sum byte, but these are not counted in the index
					this.numberEncodedBytes += 16 + 5;//add in the extra 16 bytes from the first packet, and the +5 offset dictated by the ECU code

					//now the value matches the index of the final encoded byte!
				}
				else if(this.encoded_send_packets.length != this.raw_send_packets.length)
				{
					//build packets!
					Serial_Packet encodedPacket = new Serial_Packet();
					int packetIndex = this.encoded_send_packets.length;
					int packetSum = 0;


					//special behaviors for the first packet
					if(packetIndex == 0)
					{
						int numBytesInPacket = 1 + 16 + (this.raw_send_packets[packetIndex].getPacket().length * 3) / 2; 
						encodedPacket.appendByte(numBytesInPacket);
						packetSum = (packetSum + numBytesInPacket);
						
						int headerByte = 0x70;
						encodedPacket.appendByte(headerByte);
						packetSum = (packetSum + headerByte);

						//then the 'index of last encoded byte'
						int high_LEB = (int) (this.numberEncodedBytes >> 24) & 0xFF;
						int mid_high_LEB = (int) (this.numberEncodedBytes >> 16) & 0xFF;
						int mid_low_LEB = (int) (this.numberEncodedBytes >> 8) & 0xFF;
						int low_LEB = (int) (this.numberEncodedBytes) & 0xFF;
						packetSum = (packetSum + high_LEB);
						packetSum = (packetSum + mid_high_LEB);
						packetSum = (packetSum + mid_low_LEB);
						packetSum = (packetSum + low_LEB);
						encodedPacket.appendByte(high_LEB);
						encodedPacket.appendByte(mid_high_LEB);
						encodedPacket.appendByte(mid_low_LEB);
						encodedPacket.appendByte(low_LEB);

						//prepare the index for encoding
						this.numberEncodedBytes = high_LEB + mid_high_LEB + mid_low_LEB + low_LEB + 9744;
						//then invert bitwise
						this.numberEncodedBytes = (~this.numberEncodedBytes) & 0xFFFF;

						for(int i = 0; i < 12; i ++)
						{
							encodedPacket.appendByte(0x0);//bytes unused in T4 and K4 bootloaders
						}
					}
					else
					{
						//subsequent packets
						int numBytesInPacket = 1 + (this.raw_send_packets[packetIndex].getPacket().length * 3) / 2; 
						encodedPacket.appendByte(numBytesInPacket);
						packetSum = (packetSum + numBytesInPacket);
						
						int headerByte = 0x70;
						encodedPacket.appendByte(headerByte);
						packetSum = (packetSum + headerByte);
					}
					
					/*
					if((this.raw_send_packets[packetIndex].getPacket().length) % 2 != 0)
					{
						System.out.println("Index: " + " packetIndex +  encoding packet does not have an even number of bytes");
					}
					*/

					//now let's encode bytes and add them to the packet
					
					//encode word length data pieces
					byte[] data_to_encode = this.raw_send_packets[packetIndex].getPacket();
					for(int i = 0; i < data_to_encode.length; i += 2)
					{
						//gather a word, little-endian (mind the sign! -> Java doesn't allow unsigned values)
						long W_le = ( (data_to_encode[i+1] & 0xFF) << 8) + (data_to_encode[i] & 0xFF) + this.numberEncodedBytes;
						
						//the ECU decodes the data in the following way:
						//Reflash program sends, little-endian: (24-bit encoded data)
						//decoded_sum_word = (stageIImult * (24-bit encoded data) ) % stageIImod
						
						//the decoded_sum_word is the sum of components of stageIIdecodeBytes whose indices are 
						//indicated by bits set in the W_le data that we just collected; 
						//decoding is done by setting the corresponding bit in the 16-bit decoded 
						//word each time we can subtract off one of the
						//components of the stageIIdecode bytes. The 'numberEncodedBytes' value
						//is then subtracted to yield the decoded data.
						
						
						//In order to encode data, we need to do the opposite of this process:
						//collect, little endian the 16 bits to be encoded and add the 'numberEncodedBytes'
						
						//then sum the components of the stageIIdecode bytes whose corresponding bit-index
						//is set in the above data.
						
						//now take the bit_flag_sum and find the equivalent with respect to
						//the multiplier such that:
						
						//bit_flag_sum = ( (stageIImult)*(24-bit encoded val) ) % stageIImod 
						
						//a bit of algebra:
						//bit_flag_sum + n*stageIImod = (stageIImult)*(24_bit encoded val)
						//[ (bit_flag_sum + n*stageIImod) / (stageIImult) ] = (24_bit_encoded_val)
						
						//take the first integer answer that is below 0x1000000 (16777216)
						//i.e. when (bit_flag_sum + n*(stageIImod)) % stageIImult == 0, we have a match

						//Interestingly, the 24-bit encoded value is just the sum of the encoded bytes
						//so it is possible to make better guesses for 'n' based on the set of
						//stageIIdecode bytes.
						
						//Each stageIIdecode byte has a corresponding encoded byte. In this case, I only
						//need to find the 'n' that corresponds to those 16 values (rather than the
						//n's for the entire data set that I want to encode).
						
						//In this case, the modulo operation is distributive as such:
						// S = sum(stageIIdecodeBytes) = ( stageIImult * encoded_val ) % stageIImod
						// each one of the stageIIdecodeBytes has a corresponding the encoded value 
						// in the stageII_ENCODE_bytes set, then:
						// S = sum(stageIIdecodeBytes) = ( sum(stageIImult * stageII_ENCODE_bytes % stageIImod) ) % stageIImod
						//thus, I can simply sum up the encoded equivalent of the decode byte set. This makes for a much faster
						//encoding process, because it is deterministic.
						
						int encoded_bit_flag_sum = 0;
						for(int j = 0; j < 16; j++)
						{
							//create the sum from the encode table based on bit flags
							if(((W_le >> j) & 0x1) == 1)
							{
								encoded_bit_flag_sum += this.stageIIencodeBytes[j];//the stageII encode bytes had better be in ascending order!
							}
						}
						
						
						//then put the lower 24-bits of 'guess' into the packet
						int encoded_bits = (int) (encoded_bit_flag_sum & 0xFFFFFF);
						int upper = (encoded_bits >> 16) & 0xFF;
						int mid = (encoded_bits >> 8) & 0xFF;
						int lower = (encoded_bits) & 0xFF;
						//put the encoded bytes onto the packet, little-endian
						encodedPacket.appendByte(lower);
						packetSum = (packetSum + lower);
						encodedPacket.appendByte(mid);
						packetSum = (packetSum + mid);
						encodedPacket.appendByte(upper);
						packetSum = (packetSum + upper);
					}

					//now that we've brought in all of the encoded bytes, put in the sum byte
					encodedPacket.appendByte( (packetSum % 0x100) );

					//and append this packet to the list of encoded packets
					this.encoded_send_packets = Arrays.copyOf(this.encoded_send_packets, this.encoded_send_packets.length + 1);
					this.encoded_send_packets[this.encoded_send_packets.length - 1] = encodedPacket;
				}
			}
		}


		if(this.encoded_send_packets.length > 0 && !this.specialRead)
		{
			fractionComplete = (double) (this.encoded_send_packets.length) / (double) (this.raw_send_packets.length);
		}
		
		return fractionComplete;
	}
	

	
	public double encode_stageII_read_packet()
	{
		//this function encodes stage II packets for the modified bootloader which can read data

		//we need to build packets for reading data using the modified bootloader
		//the same kinds of limits remain for read buffer sizes, so send a single '112' message
		//before sending '116' messages to ask for data
		double fractionComplete = 0.0;
		
		if(this.encoded_send_packets.length == 0)
		{
			//first, prepare a '112' message
			//be careful about the address chosen: if in flash memory it will create an error state
			this.startReflashAddr = 0x80000;
			this.reflashLen = 0x2;
			byte[] falseData = {0x0, 0x0};//a placeholder, more or less
			this.dataToReflash = falseData;
			this.hexFileStartAddr = 0x80000;
			
			//then build this data into a raw_data_packet to send
			double rawPckt = this.build_decoded_stageII_packet();
			
			//then build this data into an encoded packet
			if(rawPckt == 1.00)
			{
				this.encode_stageII_packet();//we should only add one packet
			}
			
			//then figure out how many packets will be needed
			this.number_special_read_packets = (int) (this.readDataLen / 192);//assembly code buffer can move data 254 bytes at a time
			if(this.readDataLen % 192 != 0)
			{
				this.number_special_read_packets += 1;
			}
			
			this.number_special_read_packets += 1;//add one for the '112' packet at the beginning
			
		}
		else if(this.encoded_send_packets.length != this.number_special_read_packets)
		{
			//then build the special read packets!
			Serial_Packet readPacket = new Serial_Packet();
			int packetSum = 0;
			
			//these packets tend to be the same length (four addr bytes and a length byte)
			int lengthByte = 0x6;
			readPacket.appendByte(lengthByte);
			packetSum = (packetSum + lengthByte);
			
			//put in the header byte
			int headerByte = 0x74;
			readPacket.appendByte(headerByte);
			packetSum = (packetSum + headerByte);
			
			//then figure out what address we're requesting
			long reqAddr = this.readDataAddr + (this.encoded_send_packets.length - 1)*192;
			
			int HHAddr = (int) ((reqAddr >> 24) & 0xFF);
			int MHAddr = (int) ((reqAddr >> 16) & 0xFF);
			int MLAddr = (int) ((reqAddr >> 8) & 0xFF);
			int LLAddr = (int) ((reqAddr) & 0xFF);
			readPacket.appendByte(HHAddr);
			packetSum = (packetSum + HHAddr);
			readPacket.appendByte(MHAddr);
			packetSum = (packetSum + MHAddr);
			readPacket.appendByte(MLAddr);
			packetSum = (packetSum + MLAddr);
			readPacket.appendByte(LLAddr);
			packetSum = (packetSum + LLAddr);
			
			//then figure out how many bytes we want to read
			long bytesRemaining = this.readDataLen - (this.encoded_send_packets.length - 1)*192;
			
			int numBytesInPacket = 0;
			if(bytesRemaining >= 192)
			{
				//read the full 192 bytes
				numBytesInPacket = 192;
				
			}
			else
			{
				//read only what remains
				numBytesInPacket = (int) bytesRemaining;
			}
			readPacket.appendByte(numBytesInPacket);
			packetSum = (packetSum + numBytesInPacket);
			
			//finally, put the sum byte into the packet
			readPacket.appendByte( (packetSum % 0x100) );
			
			//and then put the packet into our list
			this.encoded_send_packets = Arrays.copyOf(this.encoded_send_packets, this.encoded_send_packets.length + 1);
			this.encoded_send_packets[this.encoded_send_packets.length - 1] = readPacket;
		}
		
		if(this.specialRead)
		{
			fractionComplete = (double) (this.encoded_send_packets.length) / (double) (this.number_special_read_packets);
		}
		return fractionComplete;
	}
	
	
	public double decode_StageII_packets()
	{
		//this function decodes the stageII packets with the '112' header
		//it runs an iterative process and returns a double showing the fraction complete
		double fractionComplete = 0;
		
		int numMsgToDecode = 0;
		for(int i = 0; i < this.encoded_send_packets.length; i++)
		{
			byte[] thisPacket = this.encoded_send_packets[i].getPacket();
			if(thisPacket.length > 1)
			{
				if(thisPacket[1] == 112)
				{
					numMsgToDecode += 1;
				}
			}
		}
		
		//interpret the packet bytes
		int mainPacketSum = 0;
		int mainPacketLength = 0;
		boolean decodePacket = false;
		boolean onEncodedData = false;
		int packet_offset_to_encoded_data = 1;
		String decode_message_string = "";
		int[] subPacket = new int[0];
		int combined_encoded_byte = 0;
		
		//now work on one packet
		byte[] thisPacket = this.encoded_send_packets[this.decoding_packet_index].getPacket();
		
		//Figure out what header the packet is:
		if(thisPacket.length > 1)
		{
			if(thisPacket[1] == 112)
			{
				decodePacket = true;
			}
		}
		
		//collect the name of the file we'll write to
		DaftOBDFileHandlers fileHandeler = new DaftOBDFileHandlers();
		File logSaveLocation = fileHandeler.find_defn_fileLocation(this.settings_file, "Log save location: ");
		String saveDecodeFileName = logSaveLocation.getPath() + "\\decoded reflash file output.txt";
		File saveDecodeFile = new File(saveDecodeFileName);
		BufferedWriter writer;
		
		if(this.decoding_packet_index == 0)
		{
			this.index_of_last_encoded_byte = 0;//reset this counter when we start
			this.decodeKey = 0;//reset the decoding key too
			this.sum_of_all_encoded_bytes = 0;//reset the total packet sum too
			
			//create a new file and overwrite the last one - this is where we'll save data
			try {
				saveDecodeFile.createNewFile();
				writer = new BufferedWriter(new FileWriter(saveDecodeFile));
				writer.close();
			} catch (IOException e) {
				//Auto-generated catch block
				e.printStackTrace();
			}
		}
		
		for(int i = 0; i < thisPacket.length; i++)
		{
			//pull out the packet length
			if(i == 0)
			{
				mainPacketLength = (thisPacket[i] & 0xFF) + 1; 
			}
			
			//deal with the checksum
			if(i < mainPacketLength)
			{
				//add the byte into the packet sum
				mainPacketSum = (mainPacketSum + (thisPacket[i] & 0xFF)) % 0x100;
			}
			else
			{
				//System.out.println("Check main packet checksum..");
				
				//we should be on the sumByte - check the fidelity of the checksum!
				if((thisPacket[i] & 0xFF) != mainPacketSum)
				{
					//the checksum for the main packet is erroneous
					decode_message_string = decode_message_string.concat("Main packet #" + this.decoding_packet_index + " checksum error: calc'd: " + mainPacketSum + ", read: " + (thisPacket[i] & 0xFF) + ". ");
				}
				
				decodePacket = false;//don't decode data anymore
			}
			
			
			if(decodePacket && i > 1)
			{
				//we should either populate first-packet special stuff, or decode the packet and build the subpacket
				if(this.decoding_packet_index == 0)
				{
					//first packet special stuff:
					//pull out the index of the final decoded byte as well as the key for decoding
					if(i > 1 && i < 6)
					{
						this.index_of_last_encoded_byte = (this.index_of_last_encoded_byte << 8) + (thisPacket[i] & 0xFF);
						this.decodeKey += (thisPacket[i] & 0xFF);
						if(i == 5)
						{
							this.decodeKey = ( ~(this.decodeKey + 9744) ) & 0xFFFF;
							this.index_of_last_encoded_byte -= ( 5 + 4 + 12 );
							
							//System.out.println("read decode key: " + this.decodeKey + " encode key: " + this.numberEncodedBytes);
						}
					}
					
					packet_offset_to_encoded_data = 17;
					
					if(i > 17)
					{
						onEncodedData = true;
					}
				}
				else
				{
					//not the first packet, so just decode data, populate the subPacket byte set
					onEncodedData = true;
				}
				
				if(onEncodedData)
				{	
					//grab 24-bits from the packet bytes, little-endian 
					if((i - packet_offset_to_encoded_data) % 3 == 0)
					{
						combined_encoded_byte += ((thisPacket[i] & 0xFF) << 16);

						//decode the 24-bit encoded data into 16 bit data
						long multFactor = this.stageIImult*(Integer.remainderUnsigned(combined_encoded_byte, this.stageIImod));
						int combined_decoded_byte = Integer.remainderUnsigned((int) multFactor, this.stageIImod);

						//then de-convolute this bit-flag int back into the data we want
						int flag_word = 0;
						for(int j = 15; j > -1; j--)
						{
							if(combined_decoded_byte >= this.stageIIdecodeBytes[j])
							{
								flag_word += (0x1 << j);
								combined_decoded_byte -= this.stageIIdecodeBytes[j];
							}
						}

						//then subtract off the stageII decode key and offset - this is our ROM data (little-endian)
						//be mindful of integer rollover
						flag_word = (flag_word - this.decodeKey) & 0xFFFF;


						if(this.index_of_last_encoded_byte == 0)
						{
							//we're on the final encoded byte, so the data we decoded should match
							//the sum of all encoded bytes
							int sum_from_msg = flag_word;//some how this comes out with the correct endian-ness-> just like in the assembly code
							if(this.sum_of_all_encoded_bytes != sum_from_msg)
							{
								//we have a mismatch!
								decode_message_string = decode_message_string.concat("Sum of all encoded bytes error: calc'd: " + this.sum_of_all_encoded_bytes + ", read: " + sum_from_msg + ". ");
							}
						}
						else
						{
							//we've decoded sub-packet data, so put this into the sub-packet and add the data to the sum
							this.sum_of_all_encoded_bytes = (this.sum_of_all_encoded_bytes + ((flag_word >> 8) & 0xFF) ) % 0x10000;
							this.sum_of_all_encoded_bytes = (this.sum_of_all_encoded_bytes + ((flag_word) & 0xFF) ) % 0x10000;
							//System.out.println("sum of all decoded: " + this.sum_of_all_encoded_bytes);

							subPacket = Arrays.copyOf(subPacket, subPacket.length + 1);
							subPacket[subPacket.length - 1] = (flag_word) & 0xFF;

							subPacket = Arrays.copyOf(subPacket, subPacket.length + 1);
							subPacket[subPacket.length - 1] = (flag_word >> 8) & 0xFF;
						}

						combined_encoded_byte = 0;

					}
					else if((i - packet_offset_to_encoded_data) % 3 == 1)
					{
						combined_encoded_byte = (thisPacket[i] & 0xFF);
					}
					else if((i - packet_offset_to_encoded_data) % 3 == 2)
					{
						combined_encoded_byte += ((thisPacket[i] & 0xFF) << 8);
					}
					
					//adjust our accounting of the count down to the last encoded byte
					this.index_of_last_encoded_byte -= 1;
				}
			}
			
		}
		
		//now, parse the sub-packet and print to a file
		//if we successfully created/found the file, proceed
		if(saveDecodeFile.exists()) {
			//System.out.println("Created file to save data of length: " + dataSet.length + " packets");

			//open the file
			//then append the line to the file

			try {
				writer = new BufferedWriter(new FileWriter(saveDecodeFile,true));
				
				if(this.decoding_packet_index == 0)
				{
					writer.append("Flash memory sector erase flags: ");
					writer.newLine();
				}
				else
				{
					writer.newLine();
					writer.newLine();
				}
				
				//packet with data
				int subCheckSum = 0;
				int pcktStartIndex = 0;
				int pcktEndIndex = subPacket.length - 1;
				int reflashAddr = 0;
				for(int i = 0; i < subPacket.length; i++)
				{	
					if(this.decoding_packet_index == 0)
					{
						//get the 'flash memory clear' flags
						if(i == 3) {
							writer.append("0x8000: " + subPacket[i]);
							writer.newLine();
						} 
						else if(i == 4) {
							writer.append("0x10000: " + subPacket[i]);
							writer.newLine();
						}
						else if(i == 5) {
							writer.append("0x20000: " + subPacket[i]);
							writer.newLine();
						}
						else if(i == 6) {
							writer.append("0x30000: " + subPacket[i]);
							writer.newLine();
						}
						else if(i == 7) {
							writer.append("0x40000: " + subPacket[i]);
							writer.newLine();
						}
						else if(i == 8) {
							writer.append("0x50000: " + subPacket[i]);
							writer.newLine();
						}
						else if(i == 9) {
							writer.append("0x60000: " + subPacket[i]);
							writer.newLine();
						}
						else if(i == 10) {
							writer.append("0x70000: " + subPacket[i]);
							writer.newLine();
							writer.newLine();
						}
						
						pcktStartIndex = 11;
					}

					//increment our sum
					if(i >= pcktStartIndex && i < pcktEndIndex)
					{
						subCheckSum = (subCheckSum + subPacket[i]) %0x100;
					}
					
					//header
					if(i == pcktStartIndex)
					{
						if(subPacket[i] != 85)
						{
							//we don't have the correct header
							writer.append("Incorrect packet header: " + subPacket[i] + ", should be: 85");
							writer.newLine();
						}
					}
					
					//length
					if(i == pcktStartIndex + 1)
					{
						//get the packet length, less 5 this is the reflash length
						writer.append("Reflash " + (subPacket[i] - 5) + " bytes at: ");
						pcktEndIndex = subPacket[i] + pcktStartIndex;//update our ending point
					}
					
					//address
					if(i >= pcktStartIndex + 2 && i <= pcktStartIndex + 4)
					{
						//read in the big-endian address
						if(i == pcktStartIndex + 2)
						{
							reflashAddr = subPacket[i] << 16;
						}
						else if(i == pcktStartIndex + 3)
						{
							reflashAddr += (subPacket[i] << 8);
						}
						else if(i == pcktStartIndex + 4)
						{
							reflashAddr += subPacket[i];
							String refAddr = "0x".concat(convert.Int_to_hex_string(reflashAddr));
							writer.append(refAddr);
							//writer.newLine();
						}
					}
					
					//data - collect this as sets of 16 for easier reading (and comparison to s-record)
					if(i >= pcktStartIndex + 5 && i < pcktEndIndex)
					{
						//split into 16 byte pairs
						if( (i - (pcktStartIndex + 5)) % 16 == 0)
						{
							writer.newLine();
						}
						
						String data = convert.Int_to_hex_string(subPacket[i],2);
						/*if(data.length() < 2)
						{
							data = "0".concat(data);
						}*/
						writer.append(data);
						
					}
					
					
					//checksum
					if(i == pcktEndIndex)
					{
						if(subPacket[i] != subCheckSum)
						{
							//we have a checksum issue
							writer.append("Incorrect packet checksum; read: " + subPacket[i] + ", calc'd: " + subCheckSum);
							writer.newLine();
						}
					}
				}
				
				//finally add the error message, if it's there
				if(decode_message_string.length() > 0)
				{
					//System.out.println(decode_message_string);
					writer.newLine();
					writer.append(decode_message_string);
				}
				
				writer.close();//close the file
			} catch (IOException e) {
				e.printStackTrace();
			}
		}
		
		
		//prepare for the next time this function is called- get the next packet
		if(this.decoding_packet_index < this.encoded_send_packets.length)
		{
			this.decoding_packet_index += 1;//next time we'll work on the subsequent packet
		}
		
		if(numMsgToDecode > 0)
		{
			fractionComplete = (double) (this.decoding_packet_index) / (double) (numMsgToDecode);
		}
		
		return fractionComplete;
	}
	
	public double build_stageI_packet()
	{
		//this function encodes the stageI bootloader recovery reflashing packets
		//we'll create all 206 and 207 messages, step-by-step
		double fractionComplete = 0;

		int max_stgI_pckt_len = 254;//9998
		
		//check to be sure we have the data necessary to reflash
		if(this.dataToReflash.length == 0x7FFE)
		{
			if(this.numSubpackets == 0)
			{
				//figure out how many packets we'll need to make
				//we can send as many packets as we want, but are limited to even byte amounts
				//of data and 9999 bytes per packet (less the check bytes)
				//there are likely real limits to the number of bytes we can send at once, but until
				//I find them, I'll just try the big number
				
				//packet structure = [2,#,#,#,#,#,ID,#,#,#,#,#,#,..data..,3,C1,C2,C3,C4] 
				//where the first four ascii are the number of bytes in the packet, less the C1-C4 bytes
				//the fifth ascii is a counter, mod 10
				//ID is 206 for reflash data or 207 for the CRC word
				//and the last six ascii are the count of reflash messages sent
				
				this.numSubpackets = (int) (this.reflashLen / (max_stgI_pckt_len - 14));//the large ID = 206 packets
				this.numSubpackets += 1;//the final ID = 206 packet
				this.numSubpackets += 1;//for the ID = 207 packet with the CRC in it
				
				//also, reset our counters
				this.subPacketDataAddress = this.startReflashAddr;//this will be set to 0x8000
				this.encoded_send_packets = new Serial_Packet[0];//empty set of packets
			}
			else 
			{
				//populate packets
				Serial_Packet thisPacket = new Serial_Packet();
				
				thisPacket.appendByte(0x2);
				//then the length bytes
				int numDataBytes = 0;
				int packetLength = 0;
				int ID = 206;
				if(this.encoded_send_packets.length < this.numSubpackets - 2)
				{
					//9998 bytes per packet is a maximum
					numDataBytes = max_stgI_pckt_len - 14;
					packetLength = numDataBytes + 14;
				}
				else if(this.encoded_send_packets.length == this.numSubpackets - 2)
				{
					//final ID=206 packet
					numDataBytes = (int) this.reflashLen % (max_stgI_pckt_len - 14);
					packetLength = numDataBytes + 14;
				}
				else
				{
					//the last packet, an ID=207 packet
					numDataBytes = 5;
					packetLength = numDataBytes + 8;
					ID = 207;
				}
				
				//now turn that into ascii number of bytes
				int[] ascii = {48, 49, 50, 51, 52, 53, 54, 55, 56, 57};
				int hi_index = packetLength/1000;
				int hi_mid_index = (packetLength - hi_index*1000) / 100;
				int lo_mid_index = (packetLength - hi_index*1000 - hi_mid_index*100) / 10;
				int lo_index = (packetLength - hi_index*1000 - hi_mid_index*100 - lo_mid_index*10);
				
				thisPacket.appendByte(ascii[hi_index]);
				thisPacket.appendByte(ascii[hi_mid_index]);
				thisPacket.appendByte(ascii[lo_mid_index]);
				thisPacket.appendByte(ascii[lo_index]);
				
				
				//followed by the mod 10 counter
				thisPacket.appendByte((int) '0');//although the ECU increments this counter, it does not check to see that the value is incremented
				
				//the the ID
				thisPacket.appendByte(ID);
				
				//then the 'number of reflash messages sent' or the CRC
				if(ID != 207)
				{
					//6 ascii characters representing the reflash message index
					int msg_index = this.encoded_send_packets.length;
					
					int index = msg_index / 100000;
					msg_index -= index * 100000;
					thisPacket.appendByte(ascii[index]);
					
					index = msg_index / 10000;
					msg_index -= index * 10000;
					thisPacket.appendByte(ascii[index]);
					
					index = msg_index / 1000;
					msg_index -= index * 1000;
					thisPacket.appendByte(ascii[index]);
					
					index = msg_index / 100;
					msg_index -= index * 100;
					thisPacket.appendByte(ascii[index]);
					
					index = msg_index / 10;
					msg_index -= index * 10;
					thisPacket.appendByte(ascii[index]);
					
					index = msg_index;
					thisPacket.appendByte(ascii[index]);
				}
				else
				{
					//5 ascii characters representing the 16 bit CRC
					int crc_int = this.stageI_reflash_crc(this.dataToReflash);
					//System.out.println("crc_int: " + crc_int);
					
					int index = crc_int / 10000;
					crc_int -= index * 10000;
					thisPacket.appendByte(ascii[index]);
					
					index = crc_int / 1000;
					crc_int -= index * 1000;
					thisPacket.appendByte(ascii[index]);
					
					index = crc_int / 100;
					crc_int -= index * 100;
					thisPacket.appendByte(ascii[index]);
					
					index = crc_int / 10;
					crc_int -= index * 10;
					thisPacket.appendByte(ascii[index]);
					
					index = crc_int;
					thisPacket.appendByte(ascii[index]);
				}
				
				if(ID == 206)
				{
					//then reflashing data
					int  offset =(int) (this.subPacketDataAddress - this.startReflashAddr);
					//System.out.println("collect: " + offset + "-" + (offset + this.numBytesInEachSubPacket[packetIndex]) + " compared to size: " + this.dataToReflash.length);

					//adjust the address to use in the next packet
					this.subPacketDataAddress += numDataBytes;
					
					//then loop to add the bytes
					for(int i = 0; i < numDataBytes; i++)
					{
						thisPacket.appendByte((int) (this.dataToReflash[i + offset] & 0xFF));
					}
				}

				//the '3' byte
				thisPacket.appendByte(0x3);
				
				//then the four message CRC bytes
				byte[] msg_crc = stageI_pckt_check_bytes(thisPacket);
				thisPacket.appendByte((int) (msg_crc[0] & 0xFF));
				thisPacket.appendByte((int) (msg_crc[1] & 0xFF));
				thisPacket.appendByte((int) (msg_crc[2] & 0xFF));
				thisPacket.appendByte((int) (msg_crc[3] & 0xFF));
				
				//finally, add the packet to our list of packets to send
				this.encoded_send_packets = Arrays.copyOf(this.encoded_send_packets, this.encoded_send_packets.length + 1);
				this.encoded_send_packets[this.encoded_send_packets.length - 1] = thisPacket;
			}
		}
				
		if(this.numSubpackets > 0)
		{
			fractionComplete = (double) this.encoded_send_packets.length / (double) this.numSubpackets;
		}
		
		return fractionComplete;
	}
	
	
	public double decode_StageI_packets()
	{
		//this function steps through each stageI bootloader packet and decodes
		//it while writing to a text file the output. This is done step-by-step
		double fractionComplete = 0;

		//interpret the packet bytes
		int mainPacketLength = 0;
		int packetLength = 0;
		int ID = 0;
		int reflashPcktNo = 0;
		
		//now work on one packet
		byte[] thisPacket = this.encoded_send_packets[this.decoding_packet_index].getPacket();

		//collect the name of the file we'll write to
		DaftOBDFileHandlers fileHandeler = new DaftOBDFileHandlers();
		File logSaveLocation = fileHandeler.find_defn_fileLocation(this.settings_file, "Log save location: ");
		String saveDecodeFileName = logSaveLocation.getPath() + "\\decoded bootloader recovery file output.txt";
		File saveDecodeFile = new File(saveDecodeFileName);
		BufferedWriter writer;

		if(this.decoding_packet_index == 0)
		{
			this.index_of_last_encoded_byte = 0;//reset this counter when we start

			//create a new file and overwrite the last one - this is where we'll save data
			try {
				saveDecodeFile.createNewFile();
				writer = new BufferedWriter(new FileWriter(saveDecodeFile));
				writer.close();
			} catch (IOException e) {
				e.printStackTrace();
			}
		}

		//now, parse the packet and print to a file
		//if we successfully created/found the file, proceed
		if(saveDecodeFile.exists()) {
			//System.out.println("Created file to save data of length: " + dataSet.length + " packets");

			//open the file
			//then append the line to the file
			try {
				writer = new BufferedWriter(new FileWriter(saveDecodeFile,true));//the ',true' ensures we're appending

				//loop over the serial packet and interpret the bytes
				for(int i = 0; i < thisPacket.length - 4; i++)
				{
					//the packet takes a regular form:
					//packet structure = [2,#,#,#,#,#,ID,#,#,#,#,#,#,..data..,3,C1,C2,C3,C4]

					if(i == 0)
					{
						if(thisPacket[i] != 2)
						{
							writer.append("Incorrect packet header: " + (int) (thisPacket[i] & 0xFF) + ". Should be: 2.");
							writer.newLine();
						}
					}
					else if(i >= 1 && i <= 4)
					{
						packetLength = packetLength*10 + (thisPacket[i] - 48);
					}
					else if(i == 6)
					{
						ID = (int) (thisPacket[i] & 0xFF);
					}
					else if(i > 6)
					{
						//interpret based on the ID
						if(i < packetLength - 1)
						{
							if(ID == 206)
							{
								if(i >= 7 && i <= 12)
								{
									reflashPcktNo = reflashPcktNo * 10 + (thisPacket[i] - 48);

									if(i == 12)
									{
										writer.append("Reflash packet number: " + reflashPcktNo + " contains: " + (packetLength - 14) + " bytes of data.");
										writer.newLine();
									}
								}
								else if(i > 12)
								{
									//we're on data! -> convert to Hex byte, print and change line every 16 bytes
									String hexData = convert.Int_to_hex_string((int) thisPacket[i] & 0xFF,2);
									/*
									if(hexData.length() < 2)
									{
										hexData = "0".concat(hexData);
									}*/
									writer.append(hexData);

									if( (i - 12) % 16 == 0)
									{
										writer.newLine();
									}
								}
							}
							else if(ID == 207)
							{
								if(i >= 7 && i <= 11)
								{
									//this is actually the 16-bit CRC used to lock the bootloader
									reflashPcktNo = (reflashPcktNo*10) + (thisPacket[i] - 48);
									if(i == 11)
									{
										String hex_CRC = convert.Int_to_hex_string(reflashPcktNo);
										writer.append("Data CRC as read from packet: " + hex_CRC);
										writer.newLine();
									}
								}
							}
							else
							{
								writer.append("Unknown packet ID: " + ID + ". ");
								writer.newLine();
								writer.newLine();
							}

						}
						else if(i == packetLength - 1)
						{
							writer.newLine();
							if(thisPacket[i] != 3)
							{
								writer.append("Incorrect 'end of text' byte: " + (int) (thisPacket[i] & 0xFF));
								writer.newLine();
							}
						}
						else
						{
							//check the packet CRC bytes

							//calculate the packet CRC bytes
							Serial_Packet no_check_bytes = new Serial_Packet();
							no_check_bytes.setPacket(Arrays.copyOf(thisPacket, thisPacket.length - 4));
							byte[] pckt_CRC = stageI_pckt_check_bytes(no_check_bytes);

							//then compare to what is in the packet
							for(int j = 0; j < 4; j++)
							{
								if(pckt_CRC[j] != thisPacket[i + j])
								{
									writer.append("Incorrect check byte at index: " + (i+j) + " with value of: " + thisPacket[i + j] + " should be: " + pckt_CRC[j]);
									writer.newLine();
								}
							}

							break;
						}
					}



				}
				writer.newLine();
				writer.close();//close the file
			} catch (IOException e) {
				e.printStackTrace();
			}

		}


		//prepare for the next time this function is called- get the next packet
		if(this.decoding_packet_index < this.encoded_send_packets.length)
		{
			this.decoding_packet_index += 1;//next time we'll work on the subsequent packet
		}


		fractionComplete = (double) (this.decoding_packet_index) / (double) (this.encoded_send_packets.length);


		return fractionComplete;
	}
	
	
	public byte[] getBytesFromFile(long addr, long numBytesToGet)
	{
		//this function returns a subset of bytes from the loaded file
		//but it may do so one step at a time...
		byte[] data = new byte[0];
		
		if(this.fileType == 0)
		{
			//get binary data
			//System.out.println("Get binary data...");
			if((this.hexFileStartAddr <= addr) && (this.hexData.length + this.hexFileStartAddr >= addr + numBytesToGet))
			{
				//the data we want is wholly contained within the file
				//so we collect it
				//System.out.println("begin at index " + (addr - this.hexFileStartAddr) + " and stop after we've collected " + numBytesToGet + " bytes");
				
				data = Arrays.copyOfRange(this.hexData, (int) (addr - this.hexFileStartAddr), (int) (addr + numBytesToGet - this.hexFileStartAddr));
			}
			//System.out.println("Collected " + data.length + " bytes");
		}
		else if(this.fileType == 1)
		{
			//get hex file data- convert from ASCII before returning
			if((this.hexFileStartAddr <= addr))//cannot guess where file ends due to unknown number of line feed characters
			{
				//every data byte in the file is represented by two ASCII character bytes
				long currentAddr = this.hexFileStartAddr - 1;
				int byteCharCount = 1;
				char firstHex = '0';
				char seconHex = '0';
				for(int i = 0; i < this.hexData.length; i++)
				{
					byte currentchar = this.hexData[i];
					if(currentchar != 0xA && currentchar != 0xD)
					{
						//we're not on a line feed character
						byteCharCount = (byteCharCount + 1) % 2;
						if(byteCharCount == 1)
						{
							seconHex = (char) currentchar;
							
							//we've collected a second data character
							currentAddr += 1;
							
							if(currentAddr >= addr && currentAddr < addr + numBytesToGet)
							{
								//collect the data, convert from ASCII, append to data array
								String hexString = "0x".concat(Character.toString(firstHex)).concat(Character.toString(seconHex));//convert from the raw data to ASCII hex string
								byte currentData = (byte) (convert.Hex_or_Dec_string_to_int(hexString) & 0xFF);//convert from hex string to int
								
								//then append to data array
								data = Arrays.copyOf(data, data.length + 1);
								data[data.length - 1] = currentData;
							}
						}
						else
						{
							firstHex = (char) currentchar;
						}
					}
				}
			}
		}
		else if(this.fileType == 2)
		{
			//get data from S-record- use reference address and convert from ASCII before returning
			boolean lastCharLineFeed = true;
			for(int i = 0; i < this.hexData.length; i++)
			{
				//loop over all data: look for the line feed and header information to inform us of what address
				//we're on
				byte currentchar = this.hexData[i];
				if(currentchar == 0xA || currentchar == 0xD)
				{
					//we're on a line feed
					lastCharLineFeed = true;
				}
				else
				{
					
					//interpret the line
					//if the most recent character was a line feed, let's get the header info
					if(lastCharLineFeed)
					{
						//check that we have the correct structure
						if((char) currentchar == 'S')
						{
							//read in the rest of the header
							char srec_type = (char) this.hexData[i+1];
							String lineLength_str = "0x".concat(Character.toString((char) this.hexData[i+2])).concat(Character.toString((char) this.hexData[i+3]));
							//System.out.println("Line length string: " + lineLength_str);
							int lineLength = convert.Hex_or_Dec_string_to_int(lineLength_str);
							lineLength -= 1;//remove the sum byte
							long address = -1;
							
							//adjust our file index:
							i += 4;
							
							//the line address length depends on the s-record type
							//1, 2, and 3 contain data, 7, 8 and 9 contain a starting address
							if(srec_type == '1')
							{
								//address is 16 bits
								String lineAddr_str = "0x".concat(Character.toString((char) this.hexData[i])).concat(Character.toString((char) this.hexData[i+1])).concat(Character.toString((char) this.hexData[i+2])).concat(Character.toString((char) this.hexData[i+3]));
								address = (long) convert.Hex_or_Dec_string_to_int(lineAddr_str);
								//System.out.println("S1: " + address);
								i += 4;
								lineLength -= 2;//remove address bytes
							}
							else if(srec_type == '2')
							{
								//address is 24 bits
								String lineAddr_str = "0x".concat(Character.toString((char) this.hexData[i])).concat(Character.toString((char) this.hexData[i+1])).concat(Character.toString((char) this.hexData[i+2])).concat(Character.toString((char) this.hexData[i+3])).concat(Character.toString((char) this.hexData[i+4])).concat(Character.toString((char) this.hexData[i+5]));
								address = (long) convert.Hex_or_Dec_string_to_int(lineAddr_str);
								//System.out.println("S2: " + address);
								i += 6;
								lineLength -= 3;//remove address bytes
							}
							else if(srec_type == '3')
							{
								//address is 32 bits
								String lineAddr_str = "0x".concat(Character.toString((char) this.hexData[i])).concat(Character.toString((char) this.hexData[i+1])).concat(Character.toString((char) this.hexData[i+2])).concat(Character.toString((char) this.hexData[i+3])).concat(Character.toString((char) this.hexData[i+4])).concat(Character.toString((char) this.hexData[i+5])).concat(Character.toString((char) this.hexData[i+6])).concat(Character.toString((char) this.hexData[i+7]));
								address = (long) (convert.Hex_or_Dec_string_to_int(lineAddr_str) & 0xFFFFFFFF);
								//System.out.println("S3 address string: " + lineAddr_str + " converts to: " + address);
								i += 8;
								lineLength -= 4;//remove address bytes
							}
							else if(srec_type == '7')
							{
								//32-bit address is start of binary file
								String lineAddr_str = "0x".concat(Character.toString((char) this.hexData[i])).concat(Character.toString((char) this.hexData[i+1])).concat(Character.toString((char) this.hexData[i+2])).concat(Character.toString((char) this.hexData[i+3])).concat(Character.toString((char) this.hexData[i+4])).concat(Character.toString((char) this.hexData[i+5])).concat(Character.toString((char) this.hexData[i+6])).concat(Character.toString((char) this.hexData[i+7]));
								this.hexFileStartAddr = (long) (convert.Hex_or_Dec_string_to_int(lineAddr_str) & 0xFFFFFFFF);
							}
							else if(srec_type == '8')
							{
								//24-bit address is start of binary file
								String lineAddr_str = "0x".concat(Character.toString((char) this.hexData[i])).concat(Character.toString((char) this.hexData[i+1])).concat(Character.toString((char) this.hexData[i+2])).concat(Character.toString((char) this.hexData[i+3])).concat(Character.toString((char) this.hexData[i+4])).concat(Character.toString((char) this.hexData[i+5]));
								this.hexFileStartAddr = (long) (convert.Hex_or_Dec_string_to_int(lineAddr_str) & 0xFFFFFFFF);
							}
							else if(srec_type == '9')
							{
								//16-bit address is start of binary file
								String lineAddr_str = "0x".concat(Character.toString((char) this.hexData[i])).concat(Character.toString((char) this.hexData[i+1])).concat(Character.toString((char) this.hexData[i+2])).concat(Character.toString((char) this.hexData[i+3]));
								this.hexFileStartAddr = (long) (convert.Hex_or_Dec_string_to_int(lineAddr_str) & 0xFFFFFFFF);
							}
							
							//now that we've read in the header, check to see if the line has data we're interested in
							
							//loop over the bytes in the line
							for(int j = 0; j < lineLength; j++)
							{
								long address_of_byte = address + j;
								//loop through the remaining bytes to get
								String dataByte_str = "0x".concat(Character.toString((char) this.hexData[i+j*2])).concat(Character.toString((char) this.hexData[i+j*2+1]));
								byte dataByte = (byte) (convert.Hex_or_Dec_string_to_int(dataByte_str) & 0xFF);

								if(address_of_byte == (addr + data.length) && data.length < numBytesToGet)
								{
									//then append the byte to the overall array we're collecting
									data = Arrays.copyOf(data, data.length + 1);
									data[data.length -1 ] = dataByte;
								}
							}
						}
					}
					
					//finally, indicate that we are beyond the line feed
					lastCharLineFeed = false;
				}
				
				if(data.length == numBytesToGet)
				{
					//we're done!
					break;
				}
			}
		}
		
		return data;
	}
	
	public void writeToReflashTextFile(Serial_Packet[] dataSet, String fileName)
	{
		//this function writes a text file that contains the data from
		//the input serial packet. The individual packets are separated
		//on different lines, and the bytes from each packet are separated
		//by spaces
		DaftOBDFileHandlers fileHandeler = new DaftOBDFileHandlers();
		File logSaveLocation = fileHandeler.find_defn_fileLocation(this.settings_file, "Log save location: ");
		String saveReflashFileName = logSaveLocation.getPath() + "\\" + fileName;
		File saveReflashFile = new File(saveReflashFileName);

		//create a new file and overwrite the last one
		try {
			saveReflashFile.createNewFile();
		} catch (IOException e) {
			e.printStackTrace();
		}

		//if we successfully created/found the file, proceed
		if(saveReflashFile.exists()) {
			//System.out.println("Created file to save data of length: " + dataSet.length + " packets");
			
			//open the file
			//then append the line to the file
			BufferedWriter writer;
			
			try {
				writer = new BufferedWriter(new FileWriter(saveReflashFile));
				for(int i = 0; i < dataSet.length; i++)
				{
					byte[] thisSet = dataSet[i].getPacket();
					for(int j = 0; j < thisSet.length; j++)
					{
						writer.append(convert.Int_to_dec_string((int) (thisSet[j] & 0xFF)));
						writer.append(" ");

						if(j == thisSet.length - 1)
						{
							writer.newLine();
						}
					}
				}
				writer.close();//close the file
			} catch (IOException e) {
				e.printStackTrace();
			}
		}
	}

	
	public void writeSRECandBinary(byte[] hexData)
	{
		//this function outputs two files, one a binary file and the other an S-record
		//based on the input file
		DaftOBDFileHandlers fileHandeler = new DaftOBDFileHandlers();
		File logSaveLocation = fileHandeler.find_defn_fileLocation(this.settings_file, "Log save location: ");//get the root directory to save the file into
		
		String timeStamp = new SimpleDateFormat("yyyy.MM.dd HH.mm.ss").format(new Date());//we'll use the timestamp in the filename
		
		//then, create the file names
		String binFileName = logSaveLocation.getPath() + "\\" + "DaftOBD ROM read ".concat(timeStamp).concat(".bin");
		File saveBinFile = new File(binFileName);
		String srecFileName = logSaveLocation.getPath() + "\\" + "DaftOBD ROM read ".concat(timeStamp).concat(".srec");
		File saveSRECFile = new File(srecFileName);

		//create new files: this will not overwrite the last one
		try {
			saveBinFile.createNewFile();
			if(this.saveSREC)
			{
				saveSRECFile.createNewFile();
			}
		} catch (IOException e) {
			e.printStackTrace();
		}
		
		
		//first, handle the binary file
		OutputStream binFile;
		try {
			binFile = new FileOutputStream(saveBinFile);
			binFile.write(hexData);
			binFile.close();
		} catch (FileNotFoundException e) {
			e.printStackTrace();
		} catch (IOException e) {
			e.printStackTrace();
		}
		
		//the S-record is more involved than that
		if(this.saveSREC)
		{
			BufferedWriter writer;

			byte[] thisSet = new byte[0];
			long atAddr = this.readDataAddr;

			for(int i = 0; i < hexData.length; i++)
			{
				thisSet = Arrays.copyOf(thisSet, thisSet.length + 1);
				thisSet[thisSet.length - 1] = hexData[i];

				if(thisSet.length == 16)
				{
					//we have read in 16 bytes, so add them to the S-record file
					//build the line address and string representations of it
					String line = srec_S3_record_creator(atAddr,thisSet);
					atAddr += 16;
					thisSet = new byte[0];

					try {
						writer = new BufferedWriter(new FileWriter(saveSRECFile,true));//set to append data
						writer.append(line);
						writer.newLine();
						writer.close();//close the file
					} catch (IOException e) {
						e.printStackTrace();
					}
				}

				//then collect the final bytes
				if(thisSet.length > 0 && i == hexData.length - 1)
				{
					String line = srec_S3_record_creator(atAddr,thisSet);
					try {
						writer = new BufferedWriter(new FileWriter(saveSRECFile,true));
						writer.append(line);
						writer.newLine();
						writer.close();//close the file
					} catch (IOException e) {
						e.printStackTrace();
					}
				}
			}
		}
	}
	
	public String srec_S3_record_creator(long addr, byte[] hexData)
	{
		//this function takes in a set of bytes of data and creates a string
		//that is an 'S3' record - it uses up to the first 16 bytes of data
		
		int addrH = (int) ((addr >> 24) & 0xFF);
		int addrMH = (int) ((addr >> 16) & 0xFF);
		int addrML = (int) ((addr >> 8) & 0xFF);
		int addrL = (int) ((addr) & 0xFF);
		String addrH_str = convert.Int_to_hex_string(addrH,2);
		/*if(addrH_str.length() < 2)
		{
			addrH_str = "0".concat(addrH_str);
		}*/
		String addrMH_str = convert.Int_to_hex_string(addrMH,2);
		/*if(addrMH_str.length() < 2)
		{
			addrMH_str = "0".concat(addrMH_str);
		}*/
		String addrML_str = convert.Int_to_hex_string(addrML,2);
		/*if(addrML_str.length() < 2)
		{
			addrML_str = "0".concat(addrML_str);
		}*/
		String addrL_str = convert.Int_to_hex_string(addrL,2);
		/*if(addrL_str.length() < 2)
		{
			addrL_str = "0".concat(addrL_str);
		}*/
		
		//create the byte count: we'll make 'S3' records
		int dataLen = hexData.length;
		if(dataLen > 16)
		{
			dataLen = 16;
		}
		int count = 4 + dataLen + 1;
		String count_str = convert.Int_to_hex_string(count,2);
		/*if(count_str.length() < 2)
		{
			count_str = "0".concat(count_str);
		}*/
		
		//then work to sum up the data
		int sum = 0;
		sum = (sum + addrH) & 0xFF;
		sum = (sum + addrMH) & 0xFF;
		sum = (sum + addrML) & 0xFF;
		sum = (sum + addrL) & 0xFF;
		sum = (sum + count) & 0xFF;
		
		//start the line for the file
		String line = "S3".concat(count_str).concat(addrH_str).concat(addrMH_str).concat(addrML_str).concat(addrL_str);
		
		//then loop through the 16 bytes we've collected
		for(int i = 0; i < dataLen; i++)
		{
			int thisByte = ((int) hexData[i]) & 0xFF;
			sum = (sum + thisByte) & 0xFF;
			String byte_str = convert.Int_to_hex_string(thisByte,2);
			/*if(byte_str.length() < 2)
			{
				byte_str = "0".concat(byte_str);
			}*/
			line = line.concat(byte_str);
		}
		
		//invert the sum, then put it on the line
		sum = (~sum) & 0xFF;
		String sum_str = convert.Int_to_hex_string(sum,2);
		/*if(sum_str.length() < 2)
		{
			sum_str = "0".concat(sum_str);
		}*/
		line = line.concat(sum_str);
		return line;
	}
	
	public void resetEncodingFlags()
	{
		//This function resets flags so that packet encoding can begin again
		this.encodingFinished = false;
		this.numSubpackets = 0;//allows the encoding to start over from the beginning
		this.dataToReflash = new byte[0];//reset our collection of data used to reflash so we re-collect it
		this.collectReflashDataIndex = 0;//reset our index for building the data set so we re-collect it
		this.encoded_send_packets = new Serial_Packet[0];//reset our encoded data
	}
	
	public boolean stageII_ECU_readiness()
	{
		//this function reads the ECU response to our query and responds with the appropriate inverted
		//checksum. It returns a boolean depending on the status that the ECU indicates
		boolean success = false;
		
		//then read back the ECU response saying it is ready for the next packet, or if there was an error
		byte[] ECU_readiness = this.ComPort.read(4);
		
		//check to see that what we read is what we expect, too	
		
		//then act depending on the ECU response
		if(ECU_readiness.length == 4)
		{
			//make an effort to deal with dropped packets
			if((int) (ECU_readiness[0] & 0xFF) != 2 || (int) (ECU_readiness[1] &0xFF) != 0x72)
			{
				//we should read in another byte, assuming that we missed the check byte earlier
				//and read it in with the current four bytes
				byte[] missed = this.ComPort.read(1);
				
				String readData = "Read: [ ";
				if(missed.length > 0)
				{
					//save the check byte data in our byte log string
					readData = readData.concat(this.convert.Int_to_hex_string((int) ECU_readiness[0] & 0xFF,2)).concat(" ");
					
					//and shift the data that is in the readiness byte packet so we can treat it as normal
					ECU_readiness[0] = ECU_readiness[1];
					ECU_readiness[1] = ECU_readiness[2];
					ECU_readiness[2] = ECU_readiness[3];
					ECU_readiness[3] = missed[0];
				}
				readData = readData.concat("]");

				this.string_comm_log = Arrays.copyOf(this.string_comm_log, this.string_comm_log.length + 1);
				this.string_comm_log[this.string_comm_log.length - 1] = readData;
			}
			
			if((int) (ECU_readiness[2] & 0xFF) == 0)
			{
				//2 114 0 116 - message received successfully - continue on
				//we must respond with the inverted checksum byte, 0x8B
				byte[] invert = {(byte) ~(ECU_readiness[3] & 0xFF)};
				this.ComPort.send(invert);
				success = true;
			}
			else
			{
				//2 114 255 115 - there was an error
				//we must respond with the inverted checksum byte, 0x8C
				byte[] invert = {(byte) ~(ECU_readiness[3] & 0xFF)};
				this.ComPort.send(invert);
			}
		}
		
		//save the data in our byte log string
		String readData = "Read: [ ";
		for(int i = 0; i < ECU_readiness.length; i++)
		{
			readData = readData.concat(this.convert.Int_to_hex_string((int) ECU_readiness[i] & 0xFF, 2)).concat(" ");
		}
		readData = readData.concat("]");

		this.string_comm_log = Arrays.copyOf(this.string_comm_log, this.string_comm_log.length + 1);
		this.string_comm_log[this.string_comm_log.length - 1] = readData;
		
		return success;
	}
	
	public byte[] stageI_pckt_check_bytes(Serial_Packet pckt)
	{
		//this function calculates the four check bytes that are used to validate a serial packet
		//sent during the stage I bootloader routines
		
		byte[] msg = pckt.getPacket();
		
		int polynomial = 291;//starting value of polynomial, a uint32_t
		//length 1024
		int[] lookupBytes = {0x00,0x00,0x00,0x00,0x77,0x07,0x30,0x96,0xEE,0x0E,0x61,0x2C,0x99,0x09,0x51,0xBA,0x07,0x6D,0xC4,0x19,0x70,0x6A,0xF4,0x8F,0xE9,0x63,0xA5,0x35,0x9E,0x64,0x95,0xA3,0x0E,0xDB,0x88,0x32,0x79,0xDC,0xB8,0xA4,0xE0,0xD5,0xE9,0x1E,0x97,0xD2,0xD9,0x88,0x09,0xB6,0x4C,0x2B,0x7E,0xB1,0x7C,0xBD,0xE7,0xB8,0x2D,0x07,0x90,0xBF,0x1D,0x91,0x1D,0xB7,0x10,0x64,0x6A,0xB0,0x20,0xF2,0xF3,0xB9,0x71,0x48,0x84,0xBE,0x41,0xDE,0x1A,0xDA,0xD4,0x7D,0x6D,0xDD,0xE4,0xEB,0xF4,0xD4,0xB5,0x51,0x83,0xD3,0x85,0xC7,0x13,0x6C,0x98,0x56,0x64,0x6B,0xA8,0xC0,0xFD,0x62,0xF9,0x7A,0x8A,0x65,0xC9,0xEC,0x14,0x01,0x5C,0x4F,0x63,0x06,0x6C,0xD9,0xFA,0x0F,0x3D,0x63,0x8D,0x08,0x0D,0xF5,0x3B,0x6E,0x20,0xC8,0x4C,0x69,0x10,0x5E,0xD5,0x60,0x41,0xE4,0xA2,0x67,0x71,0x72,0x3C,0x03,0xE4,0xD1,0x4B,0x04,0xD4,0x47,0xD2,0x0D,0x85,0xFD,0xA5,0x0A,0xB5,0x6B,0x35,0xB5,0xA8,0xFA,0x42,0xB2,0x98,0x6C,0xDB,0xBB,0xC9,0xD6,0xAC,0xBC,0xF9,0x40,0x32,0xD8,0x6C,0xE3,0x45,0xDF,0x5C,0x75,0xDC,0xD6,0x0D,0xCF,0xAB,0xD1,0x3D,0x59,0x26,0xD9,0x30,0xAC,0x51,0xDE,0x00,0x3A,0xC8,0xD7,0x51,0x80,0xBF,0xD0,0x61,0x16,0x21,0xB4,0xF4,0xB5,0x56,0xB3,0xC4,0x23,0xCF,0xBA,0x95,0x99,0xB8,0xBD,0xA5,0x0F,0x28,0x02,0xB8,0x9E,0x5F,0x05,0x88,0x08,0xC6,0x0C,0xD9,0xB2,0xB1,0x0B,0xE9,0x24,0x2F,0x6F,0x7C,0x87,0x58,0x68,0x4C,0x11,0xC1,0x61,0x1D,0xAB,0xB6,0x66,0x2D,0x3D,0x76,0xDC,0x41,0x90,0x01,0xDB,0x71,0x06,0x98,0xD2,0x20,0xBC,0xEF,0xD5,0x10,0x2A,0x71,0xB1,0x85,0x89,0x06,0xB6,0xB5,0x1F,0x9F,0xBF,0xE4,0xA5,0xE8,0xB8,0xD4,0x33,0x78,0x07,0xC9,0xA2,0x0F,0x00,0xF9,0x34,0x96,0x09,0xA8,0x8E,0xE1,0x0E,0x98,0x18,0x7F,0x6A,0x0D,0xBB,0x08,0x6D,0x3D,0x2D,0x91,0x64,0x6C,0x97,0xE6,0x63,0x5C,0x01,0x6B,0x6B,0x51,0xF4,0x1C,0x6C,0x61,0x62,0x85,0x65,0x30,0xD8,0xF2,0x62,0x00,0x4E,0x6C,0x06,0x95,0xED,0x1B,0x01,0xA5,0x7B,0x82,0x08,0xF4,0xC1,0xF5,0x0F,0xC4,0x57,0x65,0xB0,0xD9,0xC6,0x12,0xB7,0xE9,0x50,0x8B,0xBE,0xB8,0xEA,0xFC,0xB9,0x88,0x7C,0x62,0xDD,0x1D,0xDF,0x15,0xDA,0x2D,0x49,0x8C,0xD3,0x7C,0xF3,0xFB,0xD4,0x4C,0x65,0x4D,0xB2,0x61,0x58,0x3A,0xB5,0x51,0xCE,0xA3,0xBC,0x00,0x74,0xD4,0xBB,0x30,0xE2,0x4A,0xDF,0xA5,0x41,0x3D,0xD8,0x95,0xD7,0xA4,0xD1,0xC4,0x6D,0xD3,0xD6,0xF4,0xFB,0x43,0x69,0xE9,0x6A,0x34,0x6E,0xD9,0xFC,0xAD,0x67,0x88,0x46,0xDA,0x60,0xB8,0xD0,0x44,0x04,0x2D,0x73,0x33,0x03,0x1D,0xE5,0xAA,0x0A,0x4C,0x5F,0xDD,0x0D,0x7C,0xC9,0x50,0x05,0x71,0x3C,0x27,0x02,0x41,0xAA,0xBE,0x0B,0x10,0x10,0xC9,0x0C,0x20,0x86,0x57,0x68,0xB5,0x25,0x20,0x6F,0x85,0xB3,0xB9,0x66,0xD4,0x09,0xCE,0x61,0xE4,0x9F,0x5E,0xDE,0xF9,0x0E,0x29,0xD9,0xC9,0x98,0xB0,0xD0,0x98,0x22,0xC7,0xD7,0xA8,0xB4,0x59,0xB3,0x3D,0x17,0x2E,0xB4,0x0D,0x81,0xB7,0xBD,0x5C,0x3B,0xC0,0xBA,0x6C,0xAD,0xED,0xB8,0x83,0x20,0x9A,0xBF,0xB3,0xB6,0x03,0xB6,0xE2,0x0C,0x74,0xB1,0xD2,0x9A,0xEA,0xD5,0x47,0x39,0x9D,0xD2,0x77,0xAF,0x04,0xDB,0x26,0x15,0x73,0xDC,0x16,0x83,0xE3,0x63,0x0B,0x12,0x94,0x64,0x3B,0x84,0x0D,0x6D,0x6A,0x3E,0x7A,0x6A,0x5A,0xA8,0xE4,0x0E,0xCF,0x0B,0x93,0x09,0xFF,0x9D,0x0A,0x00,0xAE,0x27,0x7D,0x07,0x9E,0xB1,0xF0,0x0F,0x93,0x44,0x87,0x08,0xA3,0xD2,0x1E,0x01,0xF2,0x68,0x69,0x06,0xC2,0xFE,0xF7,0x62,0x57,0x5D,0x80,0x65,0x67,0xCB,0x19,0x6C,0x36,0x71,0x6E,0x6B,0x06,0xE7,0xFE,0xD4,0x1B,0x76,0x89,0xD3,0x2B,0xE0,0x10,0xDA,0x7A,0x5A,0x67,0xDD,0x4A,0xCC,0xF9,0xB9,0xDF,0x6F,0x8E,0xBE,0xEF,0xF9,0x17,0xB7,0xBE,0x43,0x60,0xB0,0x8E,0xD5,0xD6,0xD6,0xA3,0xE8,0xA1,0xD1,0x93,0x7E,0x38,0xD8,0xC2,0xC4,0x4F,0xDF,0xF2,0x52,0xD1,0xBB,0x67,0xF1,0xA6,0xBC,0x57,0x67,0x3F,0xB5,0x06,0xDD,0x48,0xB2,0x36,0x4B,0xD8,0x0D,0x2B,0xDA,0xAF,0x0A,0x1B,0x4C,0x36,0x03,0x4A,0xF6,0x41,0x04,0x7A,0x60,0xDF,0x60,0xEF,0xC3,0xA8,0x67,0xDF,0x55,0x31,0x6E,0x8E,0xEF,0x46,0x69,0xBE,0x79,0xCB,0x61,0xB3,0x8C,0xBC,0x66,0x83,0x1A,0x25,0x6F,0xD2,0xA0,0x52,0x68,0xE2,0x36,0xCC,0x0C,0x77,0x95,0xBB,0x0B,0x47,0x03,0x22,0x02,0x16,0xB9,0x55,0x05,0x26,0x2F,0xC5,0xBA,0x3B,0xBE,0xB2,0xBD,0x0B,0x28,0x2B,0xB4,0x5A,0x92,0x5C,0xB3,0x6A,0x04,0xC2,0xD7,0xFF,0xA7,0xB5,0xD0,0xCF,0x31,0x2C,0xD9,0x9E,0x8B,0x5B,0xDE,0xAE,0x1D,0x9B,0x64,0xC2,0xB0,0xEC,0x63,0xF2,0x26,0x75,0x6A,0xA3,0x9C,0x02,0x6D,0x93,0x0A,0x9C,0x09,0x06,0xA9,0xEB,0x0E,0x36,0x3F,0x72,0x07,0x67,0x85,0x05,0x00,0x57,0x13,0x95,0xBF,0x4A,0x82,0xE2,0xB8,0x7A,0x14,0x7B,0xB1,0x2B,0xAE,0x0C,0xB6,0x1B,0x38,0x92,0xD2,0x8E,0x9B,0xE5,0xD5,0xBE,0x0D,0x7C,0xDC,0xEF,0xB7,0x0B,0xDB,0xDF,0x21,0x86,0xD3,0xD2,0xD4,0xF1,0xD4,0xE2,0x42,0x68,0xDD,0xB3,0xF8,0x1F,0xDA,0x83,0x6E,0x81,0xBE,0x16,0xCD,0xF6,0xB9,0x26,0x5B,0x6F,0xB0,0x77,0xE1,0x18,0xB7,0x47,0x77,0x88,0x08,0x5A,0xE6,0xFF,0x0F,0x6A,0x70,0x66,0x06,0x3B,0xCA,0x11,0x01,0x0B,0x5C,0x8F,0x65,0x9E,0xFF,0xF8,0x62,0xAE,0x69,0x61,0x6B,0xFF,0xD3,0x16,0x6C,0xCF,0x45,0xA0,0x0A,0xE2,0x78,0xD7,0x0D,0xD2,0xEE,0x4E,0x04,0x83,0x54,0x39,0x03,0xB3,0xC2,0xA7,0x67,0x26,0x61,0xD0,0x60,0x16,0xF7,0x49,0x69,0x47,0x4D,0x3E,0x6E,0x77,0xDB,0xAE,0xD1,0x6A,0x4A,0xD9,0xD6,0x5A,0xDC,0x40,0xDF,0x0B,0x66,0x37,0xD8,0x3B,0xF0,0xA9,0xBC,0xAE,0x53,0xDE,0xBB,0x9E,0xC5,0x47,0xB2,0xCF,0x7F,0x30,0xB5,0xFF,0xE9,0xBD,0xBD,0xF2,0x1C,0xCA,0xBA,0xC2,0x8A,0x53,0xB3,0x93,0x30,0x24,0xB4,0xA3,0xA6,0xBA,0xD0,0x36,0x05,0xCD,0xD7,0x06,0x93,0x54,0xDE,0x57,0x29,0x23,0xD9,0x67,0xBF,0xB3,0x66,0x7A,0x2E,0xC4,0x61,0x4A,0xB8,0x5D,0x68,0x1B,0x02,0x2A,0x6F,0x2B,0x94,0xB4,0x0B,0xBE,0x37,0xC3,0x0C,0x8E,0xA1,0x5A,0x05,0xDF,0x1B,0x2D,0x02,0xEF,0x8D};//0x3878 to 0x3C74+4

		for(int i = 0; i < msg.length; i++)
		{
			int keyShift = (polynomial >> 8) & 0xFFFFFF;//shift the polynomial 8 bits right, mind the unsigned nature we want
			int packetByteLong = (msg[i] & 0xFF) + 170;//treat this as an unsigned, 32 bit integer
			int index = (packetByteLong ^ polynomial) & 0xFF;//exclusive OR operation to convolute the CRC, mask this to byte length
			int lookup = (lookupBytes[index*4] << 24) + (lookupBytes[index*4 + 1] << 16) + (lookupBytes[index*4 + 2] << 8) + (lookupBytes[index*4 + 3]);//collect the long word (32-bit) lookup value
			polynomial = (lookup ^ keyShift);//exclusive OR
		}
		
		byte[] crc = new byte[4];
		
		crc[0] = (byte) ((polynomial >> 24) & 0xFF);
		crc[1] = (byte) ((polynomial >> 16) & 0xFF);
		crc[2] = (byte) ((polynomial >> 8) & 0xFF);
		crc[3] = (byte) ((polynomial) & 0xFF);
		
		return crc;

		
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
		byte[] packet = {2,48,48,48,56,48,6,3};//this is the first 'acknowledge response' packet, less the four check bytes
		//byte[] packet = {2, 48, 48, 49, 52, 51, (byte) 203, 48, 48, 48, 48, 48, 48, 3};//this is the first packet from the ECU requesting reflash data
		Serial_Packet testPckt = new Serial_Packet();
		testPckt.setPacket(packet);
		byte[] crc = stageI_pckt_check_bytes(testPckt);

		System.out.print("[ ");
		for(int i = 0; i < packet.length; i++)
		{
			System.out.print( (int) (packet[i] & 0xFF) + " ");
		}
		for(int i = 0; i < crc.length; i++)
		{
			System.out.print( (int) (crc[i] & 0xFF) + " ");
		}
		System.out.println("]");
		//input: [ 2 48 48 48 56 48 6 3  ] (first ack)
		//output: [ 2 48 48 48 56 48 6 3 227 245 208 192 ]
		
		//input: [ 2 48 48 48 56 49 6 3 ] (second ack)
		//output: [ 2 48 48 48 56 49 6 3 226 55 186 247 ]
		 
		//input: [ 2 48 48 49 52 51 203 48 48 48 48 48 48 3 ] (first req for data)
		//output: [ 2 48 48 49 52 51 203 48 48 48 48 48 48 3 36 15 221 162 ]
		 */
	}
	
	public int stageI_reflash_crc(byte[] stgI_bootldr_bytes)
	{
		//this function loops over the entire set of stageII bootloader bytes and calculates 
		//the 16-bit crc that locks out access to the stageI bootloader and allows reflashing
		//of the stageII bootloader via the stageI bootloader
		
		//0x3CB0 to 0x3EAE + 2 from ROM, length 512
		int[] lookupBytes = {0x00,0x00,0x10,0x21,0x20,0x42,0x30,0x63,0x40,0x84,0x50,0xA5,0x60,0xC6,0x70,0xE7,0x81,0x08,0x91,0x29,0xA1,0x4A,0xB1,0x6B,0xC1,0x8C,0xD1,0xAD,0xE1,0xCE,0xF1,0xEF,0x12,0x31,0x02,0x10,0x32,0x73,0x22,0x52,0x52,0xB5,0x42,0x94,0x72,0xF7,0x62,0xD6,0x93,0x39,0x83,0x18,0xB3,0x7B,0xA3,0x5A,0xD3,0xBD,0xC3,0x9C,0xF3,0xFF,0xE3,0xDE,0x24,0x62,0x34,0x43,0x04,0x20,0x14,0x01,0x64,0xE6,0x74,0xC7,0x44,0xA4,0x54,0x85,0xA5,0x6A,0xB5,0x4B,0x85,0x28,0x95,0x09,0xE5,0xEE,0xF5,0xCF,0xC5,0xAC,0xD5,0x8D,0x36,0x53,0x26,0x72,0x16,0x11,0x06,0x30,0x76,0xD7,0x66,0xF6,0x56,0x95,0x46,0xB4,0xB7,0x5B,0xA7,0x7A,0x97,0x19,0x87,0x38,0xF7,0xDF,0xE7,0xFE,0xD7,0x9D,0xC7,0xBC,0x48,0xC4,0x58,0xE5,0x68,0x86,0x78,0xA7,0x08,0x40,0x18,0x61,0x28,0x02,0x38,0x23,0xC9,0xCC,0xD9,0xED,0xE9,0x8E,0xF9,0xAF,0x89,0x48,0x99,0x69,0xA9,0x0A,0xB9,0x2B,0x5A,0xF5,0x4A,0xD4,0x7A,0xB7,0x6A,0x96,0x1A,0x71,0x0A,0x50,0x3A,0x33,0x2A,0x12,0xDB,0xFD,0xCB,0xDC,0xFB,0xBF,0xEB,0x9E,0x9B,0x79,0x8B,0x58,0xBB,0x3B,0xAB,0x1A,0x6C,0xA6,0x7C,0x87,0x4C,0xE4,0x5C,0xC5,0x2C,0x22,0x3C,0x03,0x0C,0x60,0x1C,0x41,0xED,0xAE,0xFD,0x8F,0xCD,0xEC,0xDD,0xCD,0xAD,0x2A,0xBD,0x0B,0x8D,0x68,0x9D,0x49,0x7E,0x97,0x6E,0xB6,0x5E,0xD5,0x4E,0xF4,0x3E,0x13,0x2E,0x32,0x1E,0x51,0x0E,0x70,0xFF,0x9F,0xEF,0xBE,0xDF,0xDD,0xCF,0xFC,0xBF,0x1B,0xAF,0x3A,0x9F,0x59,0x8F,0x78,0x91,0x88,0x81,0xA9,0xB1,0xCA,0xA1,0xEB,0xD1,0x0C,0xC1,0x2D,0xF1,0x4E,0xE1,0x6F,0x10,0x80,0x00,0xA1,0x30,0xC2,0x20,0xE3,0x50,0x04,0x40,0x25,0x70,0x46,0x60,0x67,0x83,0xB9,0x93,0x98,0xA3,0xFB,0xB3,0xDA,0xC3,0x3D,0xD3,0x1C,0xE3,0x7F,0xF3,0x5E,0x02,0xB1,0x12,0x90,0x22,0xF3,0x32,0xD2,0x42,0x35,0x52,0x14,0x62,0x77,0x72,0x56,0xB5,0xEA,0xA5,0xCB,0x95,0xA8,0x85,0x89,0xF5,0x6E,0xE5,0x4F,0xD5,0x2C,0xC5,0x0D,0x34,0xE2,0x24,0xC3,0x14,0xA0,0x04,0x81,0x74,0x66,0x64,0x47,0x54,0x24,0x44,0x05,0xA7,0xDB,0xB7,0xFA,0x87,0x99,0x97,0xB8,0xE7,0x5F,0xF7,0x7E,0xC7,0x1D,0xD7,0x3C,0x26,0xD3,0x36,0xF2,0x06,0x91,0x16,0xB0,0x66,0x57,0x76,0x76,0x46,0x15,0x56,0x34,0xD9,0x4C,0xC9,0x6D,0xF9,0x0E,0xE9,0x2F,0x99,0xC8,0x89,0xE9,0xB9,0x8A,0xA9,0xAB,0x58,0x44,0x48,0x65,0x78,0x06,0x68,0x27,0x18,0xC0,0x08,0xE1,0x38,0x82,0x28,0xA3,0xCB,0x7D,0xDB,0x5C,0xEB,0x3F,0xFB,0x1E,0x8B,0xF9,0x9B,0xD8,0xAB,0xBB,0xBB,0x9A,0x4A,0x75,0x5A,0x54,0x6A,0x37,0x7A,0x16,0x0A,0xF1,0x1A,0xD0,0x2A,0xB3,0x3A,0x92,0xFD,0x2E,0xED,0x0F,0xDD,0x6C,0xCD,0x4D,0xBD,0xAA,0xAD,0x8B,0x9D,0xE8,0x8D,0xC9,0x7C,0x26,0x6C,0x07,0x5C,0x64,0x4C,0x45,0x3C,0xA2,0x2C,0x83,0x1C,0xE0,0x0C,0xC1,0xEF,0x1F,0xFF,0x3E,0xCF,0x5D,0xDF,0x7C,0xAF,0x9B,0xBF,0xBA,0x8F,0xD9,0x9F,0xF8,0x6E,0x17,0x7E,0x36,0x4E,0x55,0x5E,0x74,0x2E,0x93,0x3E,0xB2,0x0E,0xD1,0x1E,0xF0};

		int crc = 291;//initialize the CRC for calculation
		
		//let's loop over the reflash byte set that contains data
		//between 0x8000-0xFFFE (exclusive) to calculate the CRC
		for(int i = 0; i < stgI_bootldr_bytes.length; i++)
		{
			int index = (crc >> 8) & 0xFF;//calculate an index
			int lookup = (lookupBytes[index*2] << 8) + lookupBytes[index*2 + 1];//lookup the value based on the index, no larger than 16 bits
			
			//prepare the components of the convolution calculation
			int shift = crc << 8;//may be shifted to 24 bits, but we'll not use the highest 8 bits in the end
			int XORd = shift ^ lookup;
			int data =  ((int) stgI_bootldr_bytes[i] & 0xFF) + 170;//this won't be more than 9 bits
			
			//then perform the convolution, mind the 16 bit constraint
			crc = (XORd ^ data) & 0xFFFF;
			//System.out.println("Index" + i + ", CRC: " + crc);
		}
		
		int stageII_CRC = crc;
		
		return stageII_CRC;
		
		//the assembly code that motivated this:
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

	
	public void stageI_send_ack(boolean ackState)
	{
		//this function sends an acknowledge packet to the ECU
		
		byte[] ack = {2, 48, 48, 48, 56, 48, 6, 3};
		if(!ackState)
		{
			ack[6] = 21;//neg ack
		}
		Serial_Packet ack_pckt = new Serial_Packet();
		ack_pckt.setPacket(ack);
		byte[] crc = stageI_pckt_check_bytes(ack_pckt);
		this.ComPort.send(ack);
		this.ComPort.send(crc);
		
		/*
		//use a fixed packet
		byte[] ack = {2, 48, 48, 48, 56, 48, 6, 3, (byte) (227 & 0xFF), (byte) (245 & 0xFF), (byte) (208 & 0xFF), (byte) (192 & 0xFF)};
		this.ComPort.send(ack);
		*/
	}
	
	public void stageI_send_ack()
	{
		stageI_send_ack(true);
	}
	
	public boolean stageI_read_ack()
	{
		//this function reads back an acknowledge packet from the ECU and compares the checksum bytes
		//if there is an error, then return false
		boolean valid = true;
		byte[] read_ack = this.ComPort.read(12);
		
		if(read_ack.length == 12)
		{
			//check for the ascii 'ack' or 'neg ack'
			if(read_ack[6] != 6 || read_ack[6] != 21)
			{
				//not an acknowledge packet
				valid = false;
			}
			
			//calculate the check bytes
			Serial_Packet ack_pckt = new Serial_Packet();
			ack_pckt.setPacket(Arrays.copyOf(read_ack, read_ack.length - 4));
			byte[] crc = stageI_pckt_check_bytes(ack_pckt);
			for(int i = 0; i < crc.length; i++)
			{
				if(read_ack[read_ack.length - 4 + i] != crc[i])
				{
					valid = false;
					break;
				}
			}
		}
		
		return valid;
	}
	
	
	public int read_stgI_packet()
	{
		//this function reads in the stage I bootloader packet by interpreting the byte length
		//it checks the checksum bytes and then if the message is an ID = 203 message,
		//it returns the value represented by the ASCII message request index bytes
		int retIndex = -1;
		
		byte[] header = this.ComPort.read(5);
		
		if(header.length == 5)
		{
			//calculate the packet length
			int len = ((header[1] & 0xFF) - 48)*1000 + ((header[2] & 0xFF) - 48)*100 + ((header[3] & 0xFF) - 48)*10 + ((header[4] & 0xFF) - 48);
			int bytesLeft = len - 5 + 4;
			byte[] pckt = this.ComPort.read(bytesLeft);
			
			if(pckt.length == bytesLeft)
			{
				//re-assemble the packet and check fidelity
				Serial_Packet wholePacket = new Serial_Packet();
				byte[] packet = Arrays.copyOf(header, header.length);
				for(int i = 0; i < pckt.length - 4; i++)
				{
					packet = Arrays.copyOf(packet, packet.length + 1);
					packet[packet.length - 1] = pckt[i];
				}
				wholePacket.setPacket(packet);
				byte[] check = this.stageI_pckt_check_bytes(wholePacket);//calculate the correct check bytes
				//then compare these to what was read in:
				boolean crc_good = true;
				for(int i = 0; i < check.length; i++)
				{
					if(check[i] != pckt[i+pckt.length - 4])
					{
						//we have an error
						crc_good = false;
					}
				}
				
				if(crc_good)
				{
					//send the ack back to the ECU that we received the packet properly
					stageI_send_ack(true);
					
					//now process the packet
					if((int) (packet[6] & 0xFF) == 203)
					{
						if(packet.length >= 13)
						{
							//use the packet data to figure out which reflashing message to send next
							retIndex = ((packet[7] & 0xFF) - 48)*100000 + ((packet[8] & 0xFF) - 48)*10000 + ((packet[9] & 0xFF) - 48)*1000 + ((packet[10] & 0xFF) - 48)*100 + ((packet[11] & 0xFF) - 48)*10 + ((packet[12] & 0xFF) - 48);
						}
					}
					else
					{
						//we received a different packet ID, maybe 202, 204, 208 or 209
					}
				}
				else
				{
					//send the neg ack to the ECU due to a failed checksum
					stageI_send_ack(false);
				}
			}
		}
		return retIndex;
	}
	

	@Override
	public void actionPerformed(ActionEvent e) {
		//respond to action requests!
		
		if("get hex".equals(e.getActionCommand())) {
			//let the user choose the ECU program data file
			int returnVal = this.binarySourceChooser.showDialog(fileSelectFrame, "Done selecting hex file");

			if (returnVal == JFileChooser.APPROVE_OPTION) {
				//if the user selected a file, get the file
				this.hexFile = this.binarySourceChooser.getSelectedFile();

				Path hexPath = Paths.get(this.hexFile.toURI());//convert it to a path
				try {
					this.hexData = Files.readAllBytes(hexPath);//then read in all bytes of data from the hex file
				} catch (IOException e1) {
					e1.printStackTrace();
				}
				System.out.println("Done reading hex file. Length: " + this.hexData.length + " bytes");
				
				//reset the user's ability to edit the 'hex start address' input text field
				this.inputHexStartAddr.setEditable(true);
				
				
				//figure out which file type is used
				//we begin by making looking at the file extension
				String fileName = this.hexFile.getName();
				if(fileName.contains(".hex") || fileName.contains(".HEX"))
				{
					this.fileType = 1;
				}
				else if(fileName.contains(".bin") || fileName.contains(".BIN"))
				{
					this.fileType = 0;
				}
				else if(fileName.contains(".srec") || fileName.contains(".SREC") || fileName.contains(".S68"))
				{
					this.fileType = 2;
					this.inputHexStartAddr.setEditable(false);//the address is in the file
				}
				else
				{
					//if the extension did not tell us, figure out manually which file type we have
					//an s-record begins each line with an 'S' character
					//a hex file will only have line feed, number or letter ASCII characters
					
					//assume that we have no comment text in the files

					//first, check for the hex file, or S_record
					boolean isHex = true;
					boolean isSREC = false;
					for(int i = 0; i < this.hexData.length; i++)
					{
						//loop through the file and check the values of the characters
						byte currentchar = this.hexData[i];
						if(currentchar < 0x30 || currentchar > 0x39)
						{
							//not an ASCII number
							if(currentchar < 0x41 || currentchar > 0x46)
							{
								//not ASCII A-F
								if(currentchar < 0x61 || currentchar > 0x66)
								{
									//not ASCII a-f
									if(currentchar != 0xA || currentchar != 0xD)
									{
										//0xA == line feed
										//0xD == carriage return
										//not ASCII line change
										//so we do not have a hex file (this may be incorrect- i did not use the format for an Intel HEX file)
										isHex = false;
										if(currentchar == 0x53 || currentchar == 0x73)
										{
											//we have the 'S' or 's' character, so this could be
											//an S-record file
											System.out.println("Recognize input with no extension as s-record");
											isSREC = true;
										}
										break;
									}
								}
							}
						}
					}
					
					if(isHex)
					{
						this.fileType = 1;//we have a hex file
					}
					else if(this.hexData[0] == 'S' && isSREC)
					{
						this.fileType = 2;//we have an S-record
						this.inputHexStartAddr.setEditable(false);
					}
					else
					{
						//by process of elimination, we have a binary file
						this.fileType = 0;
					}
				}

			} else {
				System.out.println("Load command cancelled by user.");
			}
		}
		else if("encode".equals(e.getActionCommand())) {
			//schedule events to encode the message 
			resetEncodingFlags();
			
			if(!this.running)
			{
				//System.out.println("Begin encoding");
				this.running = true;
				//schedule the runnable function to begin encoding bytes
	    		//ScheduledExecutorService scheduler = Executors.newScheduledThreadPool(1);
	    		//scheduler.schedule(this, 10, TimeUnit.MILLISECONDS);
	    		//scheduler.shutdown();//make sure to tell the executor to shut down the thread when finished!
				this.scheduler.schedule(this, 10, TimeUnit.MILLISECONDS);
			}
			else
			{
				//System.out.println("Stop encoding");
				this.statusText.setText("Inactive");
				this.statusText.repaint();
				
				this.running = false;
			}
			
		}
		else if("decode".equals(e.getActionCommand())) {
			//schedule events to decode the encoded message 
			
			//reset our decoding index
			this.decoding_packet_index = 0;
			this.decode_data = true;
			
			//if encoding has not been done, do that first.
			if(this.encoded_send_packets.length == 0)
			{
				this.encodeButton.doClick();
			}
			else
			{
				if(!this.running)
				{
					//System.out.println("Begin decoding");
					this.running = true;
					//schedule the runnable function to begin encoding bytes
					//ScheduledExecutorService scheduler = Executors.newScheduledThreadPool(1);
					//scheduler.schedule(this, 10, TimeUnit.MILLISECONDS);
					//scheduler.shutdown();//make sure to tell the executor to shut down the thread when finished!
					this.scheduler.schedule(this, 10, TimeUnit.MILLISECONDS);
				}
				else
				{
					//System.out.println("Stop decoding");
					this.statusText.setText("Inactive");
					this.statusText.repaint();

					this.running = false;
				}
			}
		}
		else if("saveData".equals(e.getActionCommand())) {
			//if we need to save data, open a file and write all of the data
			System.out.println("Save the encoded packets to text file.");
			if(this.raw_send_packets.length > 0)
			{
				writeToReflashTextFile(this.raw_send_packets,"decodedFlashBytes.txt");
			}
			
			/*
			Serial_Packet[] encoded_and_end_pckts = Arrays.copyOf(this.encoded_send_packets, this.encoded_send_packets.length + 1);
			Serial_Packet finalPacket = new Serial_Packet();
			finalPacket.appendByte(1);
			finalPacket.appendByte(115);
			finalPacket.appendByte(116);
			encoded_and_end_pckts[encoded_and_end_pckts.length - 1] = finalPacket;
			writeToReflashTextFile(encoded_and_end_pckts,"encodedFlashBytes.txt");
			*/
			writeToReflashTextFile(this.encoded_send_packets,"encodedFlashBytes.txt");
			
		}
		else if("inputHexStAddr".equals(e.getActionCommand())) {
			//update the stored address that shows where the hex file starts
			String newStAddr_string = this.inputHexStartAddr.getText();
			long newStAddr = (convert.Hex_or_Dec_string_to_int(newStAddr_string) & 0xFFFFFFFF);
			this.hexFileStartAddr = newStAddr;
		}
		else if("inputTargetAddr".equals(e.getActionCommand())) {
			//update the stored address that shows where to reflash data
			String newTargetAddr_string = this.inputTargetAddr.getText();
			long newTargetAddr = (convert.Hex_or_Dec_string_to_int(newTargetAddr_string) & 0xFFFFFFFF);
			
			if(this.specialRead)
			{
				this.readDataAddr = newTargetAddr;
			}
			else
			{
				this.startReflashAddr = newTargetAddr;
			}
		}
		else if("inputTargetLen".equals(e.getActionCommand())) {
			//update the stored address that shows how much data to reflash
			String newTargetLen_string = this.inputTargetLen.getText();
			long newTargetLen = (convert.Hex_or_Dec_string_to_int(newTargetLen_string) & 0xFFFFFFFF);
			
			if(this.specialRead)
			{
				this.readDataLen = newTargetLen;
			}
			else
			{
				this.reflashLen = newTargetLen;
			}
		}
		else if("checkbox".equals(e.getActionCommand()))
		{
			this.isSelected = !this.isSelected;
		}
	}

	@Override
	public void focusGained(FocusEvent arg0) {
		//nothing to do here
		
	}

	@Override
	public void focusLost(FocusEvent arg0) {
		//update the text in all JTextField objects
		
		String newHexSt = convert.Int_to_hex_string((int) this.hexFileStartAddr);
		this.inputHexStartAddr.setText("0x".concat(newHexSt));
		this.inputHexStartAddr.repaint();
		
		updateTargetAddress_JTextFields();
	}

	@Override
	public void run() {
		//choose what to do based on boolean flag settings
		boolean keepRunning = false;
		
		if(this.running) {
			//manage connection via OBD to ECU
			if(this.active)
			{
				if(this.encodingFinished)
				{
					if(!this.stageI)
					{
						//stage II bootloader interaction
						
						//initiate connection to ECU
						if(this.begin_connect_to_ECU)
						{

							//try sending the [1 113 114] message
							byte[] initMsg = {1, 113, 114};
							this.ComPort.send(initMsg);

							//the try to read the inverted response byte 0x8D
							byte[] response = this.ComPort.read(1);

							//now change state if we read in the correct response
							if(response.length > 0)
							{
								//System.out.println("Response: " + response);
								if((int) (response[0] & 0xFF) == 0x8D && this.begin_connect_to_ECU)
								{
									this.string_comm_log = Arrays.copyOf(this.string_comm_log, this.string_comm_log.length + 1);
									this.string_comm_log[this.string_comm_log.length - 1] = "Generic reflash bootloader session";
									
									this.string_comm_log = Arrays.copyOf(this.string_comm_log, this.string_comm_log.length + 1);
									this.string_comm_log[this.string_comm_log.length - 1] = "Send: [ 01 71 72 ]";//"Send: [ 1 113 114 ]";
										
									this.string_comm_log = Arrays.copyOf(this.string_comm_log, this.string_comm_log.length + 1);
									this.string_comm_log[this.string_comm_log.length - 1] = "Read: [ 8D ]";//"Read: [ 141 ]";
											
									this.begin_connect_to_ECU = false;//we're connected so we don't need to keep looping through

									//then change the Comport timeouts to be long so we can accommodate the 
									//flash erase timescale
									this.ComPort.changeRx_timeout(4000);

									this.statusText.setText("Connected to ECU");
									this.statusText.repaint();
								}
							}
							else
							{
								//update the status test to instruct the user to turn on the ECU
								String stats = this.statusText.getText();
								if(!stats.equals("Turn on power to ECU"))
								{
									this.statusText.setText("Turn on power to ECU");
									this.statusText.repaint();
								}
							}

							//set flag to keep re-scheduling the Com interaction
							keepRunning = true;
						}
						else
						{
							//we're going to send encoded messages to the ECU
							if(this.reflash_msg_index < this.encoded_send_packets.length)
							{
								//System.out.println("Index: " + this.reflash_msg_index);
								byte[] msg = this.encoded_send_packets[this.reflash_msg_index].getPacket();
								this.ComPort.send(msg);
								
								String sentData = "Send: [ ";
								for(int i = 0; i < msg.length; i++)
								{
									sentData = sentData.concat(this.convert.Int_to_hex_string((int) msg[i] & 0xFF, 2)).concat(" ");
								}
								sentData = sentData.concat("]");
								
								this.string_comm_log = Arrays.copyOf(this.string_comm_log, this.string_comm_log.length + 1);
								this.string_comm_log[this.string_comm_log.length - 1] = sentData;

								//then read back the ECU acknowledgement of packet reception: the inverted checksum byte
								byte[] check_response = this.ComPort.read(1);
								String readData = "Read: [ ";
								if(check_response.length > 0)
								{
									readData = readData.concat(this.convert.Int_to_hex_string( (int) check_response[0] & 0xFF, 2) ).concat(" ");
								}
								readData = readData.concat("]");
								
								this.string_comm_log = Arrays.copyOf(this.string_comm_log, this.string_comm_log.length + 1);
								this.string_comm_log[this.string_comm_log.length - 1] = readData;
								

								if(this.specialRead && this.reflash_msg_index > 0)
								{
									//read back the ECU's response to our special query for a ROM read
									//this is not robust to missing the ECU's ack byte above
									
									byte[] read_ROM = ComPort.read((int) (msg[6] & 0xFF) + 3);//read back the number of bytes we requested and the header, checksum and packet length

									readData = "Read: [ ";
									for(int i = 0; i < read_ROM.length; i++)
									{
										readData = readData.concat(this.convert.Int_to_hex_string((int) read_ROM[i] & 0xFF, 2)).concat(" ");
									}
									readData = readData.concat("]");

									this.string_comm_log = Arrays.copyOf(this.string_comm_log, this.string_comm_log.length + 1);
									this.string_comm_log[this.string_comm_log.length - 1] = readData;

									//then respond with the inverted checksum byte
									byte[] invert = {(byte) ~read_ROM[read_ROM.length -1]};
									this.ComPort.send(invert);
									sentData = "Send: [ ".concat(this.convert.Int_to_hex_string( (int) invert[0] & 0xFF,2) ).concat(" ]");
									this.string_comm_log = Arrays.copyOf(this.string_comm_log, this.string_comm_log.length + 1);
									this.string_comm_log[this.string_comm_log.length - 1] = sentData;

									//then save the data into our repository of read data for saving
									if(read_ROM.length > 2)
									{
										if(read_ROM[1] == 112)
										{
											for(int i = 2; i < read_ROM.length - 1; i++)
											{
												this.read_ROM_data = Arrays.copyOf(this.read_ROM_data, this.read_ROM_data.length + 1);
												this.read_ROM_data[this.read_ROM_data.length - 1] = read_ROM[i];
											}
											this.reflash_msg_index += 1;
										}
									}
									//we don't need to check the ECU readiness state if we're just reading the ROM
								}
								else if(this.stageII_ECU_readiness())
								{
									//then read back the ECU response saying it is ready for the next packet, or if there was an error
									this.reflash_msg_index += 1;
									if(this.reflash_msg_index == 1)
									{
										this.ComPort.changeRx_timeout(1000);//shorten the timeout so that we can recover from dropped packets by resending 
									}

								}


								keepRunning = true;

								int frac_complete =  ( this.reflash_msg_index * 10000 ) /  this.encoded_send_packets.length;
								double frac_int = (double) frac_complete / 100;//try to keep two decimal places

								if(frac_int == 100 && this.specialRead)
								{
									this.statusText.setText("Saving data, be patient...");
									this.statusText.repaint();
								}
								else
								{
									this.statusText.setText("Sending messages: " + frac_int + "%");
									this.statusText.repaint();
								}
							}
							else
							{
								//send the end session message 1 115 116
								byte[] end_bootloader_session = {1, 115, 116};
								this.ComPort.send(end_bootloader_session);
								String finalSend = "Send: [ 01 73 74 ]";//"Send: [ 1 115 116 ]";
								this.string_comm_log = Arrays.copyOf(this.string_comm_log, this.string_comm_log.length + 1);
								this.string_comm_log[this.string_comm_log.length - 1] = finalSend;

								//and read back the ECU's ack
								byte[] check_response = new byte[1];
								check_response = this.ComPort.read(1);
								String readData = "Read: [ ";
								if(check_response.length > 0)
								{
									readData = readData.concat(this.convert.Int_to_hex_string( (int) check_response[0] & 0xFF, 2) ).concat(" ");
								}
								readData = readData.concat("]");
								
								this.string_comm_log = Arrays.copyOf(this.string_comm_log, this.string_comm_log.length + 1);
								this.string_comm_log[this.string_comm_log.length - 1] = readData;

								//then read back the ECU response saying we had success or if there was an error
								this.stageII_ECU_readiness();

								this.active = false;//end activity with the bootloader

								//and if we're doing the special read functionality, save the data to a text file
								if(this.specialRead)
								{
									writeSRECandBinary(this.read_ROM_data);
								}
							}
						}
					}
					else
					{
						//stage I bootloader interaction
						if(this.begin_connect_to_ECU)
						{
							//establish connection to ECU
							//try sending the [83 89 78] message, byte by byte
							byte[] initMsg = {0x53};
							this.ComPort.send(initMsg);

							//then try to read the inverted response byte 0x73
							byte[] response = this.ComPort.read(1);

							//now change state if we read in the correct response
							if(response.length > 0)
							{
								//slow the serial timeout so we can take our time
								this.ComPort.changeRx_timeout(5000);
								
								//exchange the other two entry bytes
								initMsg[0] = 0x59;
								this.ComPort.send(initMsg);

								//then try to read the inverted response byte 0x79
								response = ComPort.read(1);
								
								initMsg[0] = 0x4E;
								this.ComPort.send(initMsg);

								//then try to read the inverted response byte 0x6E
								response = ComPort.read(1);
								
								//send the command to clear bootloader ROM in order to get things started
								byte[] msg = {2, 48, 48, 48, 56, 48,(byte) (200 & 0xFF), 3, 26, 57, 101, 12};
								this.ComPort.send(msg);
								
								//and read back the ECU's ack response
								stageI_read_ack();
								
								//and read back the ECU's proceed to erase ROM msg
								byte[] proceed = this.ComPort.read(12);//should be the ID = 201 packet
								
								//then give an acknowledgement that we received the packet
								stageI_send_ack();
								
								//--there's a delay here while the ECU bootloader ROM is cleared--
								
								if(this.begin_connect_to_ECU)
								{
									this.begin_connect_to_ECU = false;//we're connected so we don't need to keep looping through

									this.statusText.setText("Connected to ECU");
									this.statusText.repaint();
								}
							}
							else
							{
								//update the status test to instruct the user to turn on the ECU
								String stats = this.statusText.getText();
								if(!stats.equals("Turn on power to ECU"))
								{
									this.statusText.setText("Turn on power to ECU");
									this.statusText.repaint();
								}
							}

							//set flag to keep re-scheduling the Com interaction
							keepRunning = true;
						}
						else
						{
							//manage StageI bootloader interactions
								
							//read the ECU's ID = 203 message to get the packet index to send
							int index = read_stgI_packet();
							
							if(index >= 0 && this.encoded_send_packets.length > index)
							{
								byte[] send_packet = this.encoded_send_packets[index].getPacket();
								this.ComPort.send(send_packet);
								
								this.stageI_read_ack();//read back the ECU's acknowledgement of our packet
								
								int frac_complete =  ( index * 10000 ) /  this.encoded_send_packets.length;
								double frac_int = (double) frac_complete / 100;//try to keep two decimal places
								
								this.statusText.setText("Sending messages: " + frac_int + "%");
								this.statusText.repaint();
								
								keepRunning = true;//keep comms open with the ECU
							}
							else
							{
								//we did not receive a request for more of the reflashing data
								//make one more attempt to read a packet from the ECU
								byte[] lastMsg = this.ComPort.read(12);
								
								//we'll only read something back if we are down the logic branch for a successful 
								//bootloader reflash- we'd get an ID = 204 message in confirmation
								if(lastMsg.length > 0)
								{
									stageI_send_ack();
									
									//we're completed successfully
									this.statusText.setText("Reflashing complete");
									this.statusText.repaint();
								}
								else
								{
									//something went wrong
									this.statusText.setText("Reflashing error");
									this.statusText.repaint();
								}
								
								this.active = false;//end activity with the bootloader
							}
						}
					}
				}
			}
		
			//encode packets if the encoding is not done and if we're not trying to connect to the ECU
			if(!this.encodingFinished)
			{
				//have we read in a binary/hex file?
				if(this.hexData.length != 0)
				{
					//we will either encode stage I or stage II bootloader packets
					
					//first, we must load/parse the data that we're going to use to build packets:
					double reflashing_data_getter = get_bytes_to_reflash();
					
					if(reflashing_data_getter >= 1.00)
					{
						if(!this.stageI)
						{
							//System.out.println("Encoding stageII packet");
							//stage II bootloader 
							double sub_packet_fraction_complete = 0;
							double encoding_fraction_complete = 0;


							//once we have all of the data, we'll build sub-packets
							sub_packet_fraction_complete = build_decoded_stageII_packet();
							if(sub_packet_fraction_complete >= 1.00)
							{
								//begin the encoding process
								//encode regular reflashing packet
								encoding_fraction_complete = encode_stageII_packet();					

								if(encoding_fraction_complete >= 1.00)
								{
									//we're done 
									this.encodingFinished = true;
									//System.out.println("Encoding complete");
									this.statusText.setText("Encoding complete");
									this.statusText.repaint();
								}
								else
								{
									//update the status to show how far we've completed encoding
									int frac = (int) (encoding_fraction_complete * 100);
									//System.out.println("Encoding packet: " + frac + "%");
									this.statusText.setText("Encoding packets: " + frac + "%");
									this.statusText.repaint();
								}
							}
							else
							{
								//update the status to show how far we've completed the sub-packet collection
								int frac = (int) (sub_packet_fraction_complete * 100);
								//System.out.println("Building sub packet: " + frac + "%");
								statusText.setText("Building reflash packets: " + frac + "%");
								this.statusText.repaint();
							}

						}
						else if(this.stageI)
						{
							//stage I bootloader
							//let's build packets!
							double encoding_fraction_complete = build_stageI_packet();
							
							if(encoding_fraction_complete >= 1.00)
							{
								//we're done 
								this.encodingFinished = true;
								//System.out.println("Encoding complete");
								this.statusText.setText("Encoding complete");
								this.statusText.repaint();
							}
							else
							{
								//update the status to show how far we've completed encoding
								int frac = (int) (encoding_fraction_complete * 100);
								//System.out.println("Encoding packet: " + frac + "%");
								this.statusText.setText("Encoding packets: " + frac + "%");
								this.statusText.repaint();
							}
						}
					}
					else
					{
						//update the status to show how far we've completed collecting data for reflashing
						int frac = (int) (reflashing_data_getter * 100);
						//System.out.println("Getting reflash data: " + frac + "%");
						statusText.setText("Parsing hex data: " + frac + "%");
						this.statusText.repaint();
					}
				}
				else if(this.specialRead)
				{
					//special read function for modified stageII bootloader
					double read_packet_build_fraction = encode_stageII_read_packet();

					if(read_packet_build_fraction >= 1.00)
					{
						this.encodingFinished = true;
						this.statusText.setText("Encoding complete");
						this.statusText.repaint();
					}
					else
					{
						int frac = (int) (read_packet_build_fraction * 100);
						statusText.setText("Encoding read packets: " + frac + "%");
						this.statusText.repaint();
					}
				}
				else
				{
					//nothing can be done yet because
					//we have not selected a binary file to work with
					//System.out.println("Choose binary file");
					this.statusText.setText("Choose binary file");
					this.statusText.repaint();
				}
				
				if(!this.encodingFinished || this.decode_data || this.begin_connect_to_ECU)
				{
					//System.out.println("keep running!");
					keepRunning = true;
				}
			}
			else if(this.decode_data)
			{
				//decode packets
				double decoding_fraction = 0;
				if(!this.stageI)
				{
					decoding_fraction = decode_StageII_packets();
				}
				else
				{
					decoding_fraction = decode_StageI_packets();
				}
				
				if(decoding_fraction >= 1.00)
				{
					//we're finished decoding
					this.encodingFinished = true;
					this.statusText.setText("Decoding complete");
					this.statusText.repaint();
					this.decode_data = false;
				}
				else
				{
					int frac = (int) (decoding_fraction * 100);
					statusText.setText("Decoding packets: " + frac + "%");
					this.statusText.repaint();
				}
				
				if(this.decode_data) {
					keepRunning = true;
				}

			}
			
		}
		
		if(keepRunning)
		{
			//System.out.println("Schedule next event");
			//schedule another runnable call
			//ScheduledExecutorService scheduler = Executors.newScheduledThreadPool(1);
    		//scheduler.schedule(this, 100, TimeUnit.MICROSECONDS);
    		//scheduler.shutdown();//make sure to tell the executor to shut down the thread when finished!
			this.scheduler.schedule(this, 100, TimeUnit.MICROSECONDS);
		}
		else
		{
			System.out.println("Stop reflash event scheduling");
			this.running = false;
			statusText.setText("Inactive");
			this.statusText.repaint();
			
			//save our data log if it has contents
			if(this.string_comm_log.length > 0)
			{
				DaftOBDFileHandlers handler = new DaftOBDFileHandlers();
				handler.appendStrArray_byteLogFile(this.settings_file, "", this.string_comm_log);
			}
		}
		
		return;//close the thread, else memory use continues to increase
	}
}
