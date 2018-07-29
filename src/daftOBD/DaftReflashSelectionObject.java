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
	boolean encode_on_the_fly = false;
	
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
	
	//for managing connecting to the ECU
	boolean active = false;//a flag indicating that connection to ECU is active
	boolean begin_connect_to_ECU;//a flag indicating that we want to establish a connection to the ECU
	int reflash_msg_index = 0;//an index that allows us to keep track of where we are in the reflashing process
	
	Conversion_Handler convert = new Conversion_Handler();//allow us to convert between different number types
	
	
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
		this.stageI = true;
		this.specialRead = false;//cannot use this function if we're working with the stage I bootloader
		setTargetAddress(0x8000, 0x8000);//the stage I bootloader only reflashes data from 0x8000 - 0xFFFF (inclusive)
	}
	
	public void setTargetAddress(long addr, long len)
	{
		//this function sets the target address and length
		this.startReflashAddr = addr;
		this.reflashLen = len;
		
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
			this.running = true;
			this.begin_connect_to_ECU = true;//we want to connect to the ECU
			this.active = true;//we're activating communication with the ECU
			this.reflash_msg_index = 0;//reset the index for sending packets to the ECU
			this.read_ROM_data = new byte[0];

			//change the COM port settings to have shorter timeouts until we get our connection established
			if(!this.stageI)
			{
				this.ComPort.updateBaudRate(29769);//set baud rate for stage II bootloader (we're not changing this via the bootloader option
			}
			this.ComPort.changeRx_timeout(20);//

			
			//then schedule further action
			ScheduledExecutorService scheduler = Executors.newScheduledThreadPool(1);
			scheduler.schedule(this, 100, TimeUnit.MICROSECONDS);
			scheduler.shutdown();//make sure to tell the executor to shut down the thread when finished!

		}
		else
		{
			//stop
			this.running = false;
			this.active = false;
			
			this.statusText.setText("Inactive");
			this.statusText.repaint();
		}
	}
	
	public double get_bytes_to_reflash_stageII()
	{
		//this function collects the data necessary for reflashing using the stageII bootloader
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
			this.subPacketDataAddress = this.startReflashAddr;
			this.numBytesInEachSubPacket = new int[0];
			this.raw_send_packets = new Serial_Packet[0];
			this.sumOfallDecodedBytes = 0;
			
			this.encoded_send_packets = new Serial_Packet[0];
			
			//if we've already imported the data, we should figure out how many
			//sub-packets will be needed to send it
			long dataLeft = this.dataToReflash.length;
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
			
			//create a new file and overwrite the last one - this is where we'll save data
			try {
				saveDecodeFile.createNewFile();
				writer = new BufferedWriter(new FileWriter(saveDecodeFile));
				writer.close();
			} catch (IOException e) {
				// TODO Auto-generated catch block
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
						
						String data = convert.Int_to_hex_string(subPacket[i]);
						if(data.length() < 2)
						{
							data = "0".concat(data);
						}
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
				// TODO Auto-generated catch block
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
			// TODO Auto-generated catch block
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
				// TODO Auto-generated catch block
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
			saveSRECFile.createNewFile();
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		
		
		//first, handle the binary file
		OutputStream binFile;
		try {
			binFile = new FileOutputStream(saveBinFile);
			binFile.write(hexData);
			binFile.close();
		} catch (FileNotFoundException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		} catch (IOException e) {
			// TODO Auto-generated catch block
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
						// TODO Auto-generated catch block
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
						// TODO Auto-generated catch block
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
		String addrH_str = convert.Int_to_hex_string(addrH);
		if(addrH_str.length() < 2)
		{
			addrH_str = "0".concat(addrH_str);
		}
		String addrMH_str = convert.Int_to_hex_string(addrMH);
		if(addrMH_str.length() < 2)
		{
			addrMH_str = "0".concat(addrMH_str);
		}
		String addrML_str = convert.Int_to_hex_string(addrML);
		if(addrML_str.length() < 2)
		{
			addrML_str = "0".concat(addrML_str);
		}
		String addrL_str = convert.Int_to_hex_string(addrL);
		if(addrL_str.length() < 2)
		{
			addrL_str = "0".concat(addrL_str);
		}
		
		//create the byte count: we'll make 'S3' records
		int dataLen = hexData.length;
		if(dataLen > 16)
		{
			dataLen = 16;
		}
		int count = 4 + dataLen + 1;
		String count_str = convert.Int_to_hex_string(count);
		if(count_str.length() < 2)
		{
			count_str = "0".concat(count_str);
		}
		
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
			String byte_str = convert.Int_to_hex_string(thisByte);
			if(byte_str.length() < 2)
			{
				byte_str = "0".concat(byte_str);
			}
			line = line.concat(byte_str);
		}
		
		//invert the sum, then put it on the line
		sum = (~sum) & 0xFF;
		String sum_str = convert.Int_to_hex_string(sum);
		if(sum_str.length() < 2)
		{
			sum_str = "0".concat(sum_str);
		}
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
	}
	
	public boolean stageII_ECU_readiness()
	{
		//this function reads the ECU response to our query and responds with the appropriate inverted
		//checksum. It returns a boolean depending on the status that the ECU indicates
		boolean success = false;
		
		//then read back the ECU response saying it is ready for the next packet, or if there was an error
		byte[] ECU_readiness = this.ComPort.read(4);
		
		if(ECU_readiness.length == 4)
		{
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
		
		return success;
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
					// TODO Auto-generated catch block
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
	    		ScheduledExecutorService scheduler = Executors.newScheduledThreadPool(1);
	    		scheduler.schedule(this, 10, TimeUnit.MILLISECONDS);
	    		scheduler.shutdown();//make sure to tell the executor to shut down the thread when finished!
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
					ScheduledExecutorService scheduler = Executors.newScheduledThreadPool(1);
					scheduler.schedule(this, 10, TimeUnit.MILLISECONDS);
					scheduler.shutdown();//make sure to tell the executor to shut down the thread when finished!
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
			writeToReflashTextFile(this.raw_send_packets,"decodedFlashBytes.txt");
			
			Serial_Packet[] encoded_and_end_pckts = Arrays.copyOf(this.encoded_send_packets, this.encoded_send_packets.length + 1);
			Serial_Packet finalPacket = new Serial_Packet();
			finalPacket.appendByte(1);
			finalPacket.appendByte(115);
			finalPacket.appendByte(116);
			encoded_and_end_pckts[encoded_and_end_pckts.length - 1] = finalPacket;
			writeToReflashTextFile(encoded_and_end_pckts,"encodedFlashBytes.txt");
			
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
				if(!this.stageI)
				{
					//stage II bootloader interaction 
					
					if(this.begin_connect_to_ECU || !this.encodingFinished)
					{
						
						//try sending the [1 113 114] message
						byte[] initMsg = {1, 113, 114};
						ComPort.send(initMsg);

						//the try to read the inverted response byte 0x8D
						byte[] response = ComPort.read(1);

						//now change state if we read in the correct response
						if(response.length > 0)
						{
							//System.out.println("Response: " + response);
							if((int) (response[0] & 0xFF) == 0x8D && this.begin_connect_to_ECU)
							{
								this.begin_connect_to_ECU = false;//we're connected so we don't need to keep looping through

								//send the command to encode ECU data if it's not done
								resetEncodingFlags();

								//then change the Comport timeouts back to normal
								ComPort.changeRx_timeout(500);

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
							System.out.println("Index: " + this.reflash_msg_index);
							byte[] msg = this.encoded_send_packets[this.reflash_msg_index].getPacket();
							ComPort.send(msg);

							//then read back the ECU acknowledgement of packet reception: the inverted checksum byte
							byte[] check_response = ComPort.read(1);

							if(this.specialRead && this.reflash_msg_index > 0)
							{
								//read back the ECU's response to our special query for a ROM read
								byte[] read_ROM = ComPort.read((int) (msg[6] & 0xFF) + 3);//read back the number of bytes we requested and the header, checksum and packet length
								
								//then respond with the inverted checksum byte
								byte[] invert = {(byte) ~read_ROM[read_ROM.length -1]};
								ComPort.send(invert);
								
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
							ComPort.send(end_bootloader_session);
							
							//and read back the ECU's ack
							byte[] check_response = new byte[1];
							check_response = ComPort.read(1);
							
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
				}
			}
		
			//encode packets if the encoding is not done and if we're not trying to connect to the ECU
			if(!this.encodingFinished && !this.begin_connect_to_ECU)
			{
				if(this.hexData.length != 0)
				{
					//we will either encode stage I or stage II bootloader packets
					if(!this.stageI)
					{
						//System.out.println("Encoding stageII packet");
						//stage II bootloader 
						//first, we must load the data that we're going to use to build packets:
						double reflashing_data_getter = get_bytes_to_reflash_stageII();
						double sub_packet_fraction_complete = 0;
						double encoding_fraction_complete = 0;

						
						if(reflashing_data_getter >= 1.00)
						{
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
						else
						{
							//update the status to show how far we've completed collecting data for reflashing
							int frac = (int) (reflashing_data_getter * 100);
							//System.out.println("Getting reflash data: " + frac + "%");
							statusText.setText("Parsing hex data: " + frac + "%");
							this.statusText.repaint();
						}
					}
					else if(this.stageI)
					{
						//stage I bootloader
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
				
				if(!this.encodingFinished || this.decode_data)
				{
					keepRunning = true;
				}
			}
			else if(this.decode_data)
			{
				//decode packets
				double decoding_fraction = decode_StageII_packets();
				
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
			ScheduledExecutorService scheduler = Executors.newScheduledThreadPool(1);
    		scheduler.schedule(this, 100, TimeUnit.MICROSECONDS);
    		scheduler.shutdown();//make sure to tell the executor to shut down the thread when finished!
		}
		else
		{
			System.out.println("Stop reflash event scheduling");
			this.running = false;
			statusText.setText("Inactive");
			this.statusText.repaint();
		}
		
		return;//close the thread, else memory use continues to increase
	}
}
