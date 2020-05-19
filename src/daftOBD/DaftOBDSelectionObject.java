package daftOBD;

import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.FocusEvent;
import java.awt.event.FocusListener;
import java.io.File;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Arrays;

import javax.swing.JButton;
import javax.swing.JCheckBox;
import javax.swing.JFileChooser;
import javax.swing.JFrame;
import javax.swing.JPanel;
import javax.swing.JTextField;

public class DaftOBDSelectionObject implements ActionListener {
	//this class contains the descriptors and functions that handle
	//behaviors of OBD select-able parameters and vehicle communication
	//functions
	
	//We'll want to present this class in a JPanel
	JPanel DaftPanel = new JPanel();
	public JCheckBox DaftSelectionCheckBox;
	
	
	//parameters that define a selection object
	int special_operation_mode = 0;
	public String mode;//OBD mode such as '0x1' or 'Init', or comm mode such as 'Reflash'
	public String parameterID;//OBD PID such as '0x1' or 'Vehicle speed'
	public String[] dataToSend = new String[0];//a set of strings describing data to send using the control JTextField boxes
	public boolean isSelected;//a flag showing if this parameter is to be polled when the 'start' button is pressed
		//if this is an init, then this flag shows false if the init needs to be sent, or true if it has been completed already
	public String initID;//either define which 'Init' this object is, or which 'Init' it needs to have completed
	public boolean requiresInit = false;//a flag stating that this parameter needs an init
	public int numberOfInputBoxes = 0;//number of user input boxes to be included with this object
	 
	public boolean isVisible;//as part of a tree, the visibility can be toggled on or off		
	Serial_Packet[] sendPacketList;//the list of bytes (and associated conversions/controls) that compose the serial data
	Serial_Packet[] receivePacketList;//bytes and conversions showing what a response packet looks like
	boolean[] flowControl = new boolean[0];//'0' = send message, '1' = receive message, '2' = eval. cond. before receive
	
	boolean readOnce = false;//a flag that states that the PID should be read only once per polling cycle
	boolean beepOnPoll = false;//a flag that indicates the program should make a beep when the PID is polled
	int messagePeriod_ms = 0;
	int timeNextPoll_ms = 0;
	
	//Stuff for the special operation mode for RAM write
	Conversion_Handler convert = new Conversion_Handler();//allow us to convert between different number types
	public int RAMtargetAddress = 0x84000;
	public int ROMtargetAddress = 0x70000;
	public int ROMtoRAMlength = 0x6000;
	public int specialModeBaud = 10400;
	int file_chooser_return_value = 1;//0x0 means that we have a hex file chosen
	//extra GUI parts in case we have a special RAMwrite use for this object
	JFrame fileSelectFrame;
	JButton fileSelectButton;//a button which allows us to select a binary file
	JFileChooser binarySourceChooser;//a file selection window to pick the binary file
	File hexFile;//a file that contains the data we'll extract for reflashing
	byte[] hexData = new byte[0];//store the entire hex file as byte data
	byte[] prev_hexData = new byte[0];//store the entire hex file as byte data
	boolean dataSentOnce = false;
	
	//constructors
	public DaftOBDSelectionObject(String m, String pid, Serial_Packet[] list_of_packets_to_send, Serial_Packet[] list_of_packets_to_read) {	
		//set the parameters that define the object
		this.mode = m;
		this.parameterID = pid;
		this.isSelected = false;
		this.isVisible = false;
		this.sendPacketList = list_of_packets_to_send;
		this.receivePacketList = list_of_packets_to_read;
		
		//Initialize the parameters that allow us to visually present this object to the user
		//the object consists of a JCheckBox and a possible set of JTextFields
		//first, add the JCheckBox that allows the parameter to be selected
		DaftSelectionCheckBox = new JCheckBox(this.parameterID);
		DaftSelectionCheckBox.setSelected(this.isSelected);
		DaftSelectionCheckBox.addActionListener(this);
		DaftPanel.add(DaftSelectionCheckBox);

		
		//then retrieve the JTextFields for the inputs
		for(int i = 0; i < this.sendPacketList.length; i++)
		{
			//each packet of data to be sent may have inputs associated with it
			Input[] inputList = this.sendPacketList[i].getInputList();
			//loop through all inputs and add their JTextFields to the JPanel for this selection object
			for(int j = 0; j < inputList.length; j++)
			{
				DaftPanel.add(inputList[j].getJTextField());
			}
		}
	}
	
	public DaftOBDSelectionObject(String m, String pid, int specialMode, String RAMtarget, String ROMtarget, String ROMlength, String baudRate) {
		//set the parameters that define the object
		this.mode = m;
		this.parameterID = pid;
		this.isSelected = false;
		this.isVisible = false;
		this.special_operation_mode = specialMode;
		
		//Initialize the parameters that allow us to visually present this object to the user
		//the object consists of a JCheckBox and a possible set of JTextFields
		//first, add the JCheckBox that allows the parameter to be selected
		DaftSelectionCheckBox = new JCheckBox(this.parameterID);
		DaftSelectionCheckBox.setSelected(this.isSelected);
		DaftSelectionCheckBox.addActionListener(this);
		this.DaftPanel.add(DaftSelectionCheckBox);
		
		if(this.special_operation_mode == 1)
		{
			//this is a RAMwrite special mode object!! we should add a file selection button
			this.RAMtargetAddress = (convert.Hex_or_Dec_string_to_int(RAMtarget) & 0xFFFFFFFF);
			this.ROMtargetAddress = (convert.Hex_or_Dec_string_to_int(ROMtarget) & 0xFFFFFFFF);
			this.ROMtoRAMlength = (convert.Hex_or_Dec_string_to_int(ROMlength) & 0xFFFFFFFF);
			this.specialModeBaud = (convert.Hex_or_Dec_string_to_int(baudRate) & 0xFFFFFFFF);
			
			this.binarySourceChooser = new JFileChooser();
			this.fileSelectButton = new JButton("Select Hex File");
			this.fileSelectButton.addActionListener(this);
			this.fileSelectButton.setActionCommand("get hex");
			this.DaftPanel.add(this.fileSelectButton);
		}
	}
	
	public String toString() {
		return this.parameterID;
	}

	
	public JPanel getDaftPanel() {
		return DaftPanel;
	}
	
	public Serial_Packet[] getSendPacketList() {
		if(this.special_operation_mode == 1)
		{
			//build the send and return packet list, and the flow control!
			this.sendPacketList = buildSendPacketsFromFile();
			this.receivePacketList = buildReadPacketsFromFile();
			this.flowControl = buildFlowControlFromFile();
		}
		return this.sendPacketList;
	}
	
	public Serial_Packet[] getReadPacketList() {
		return this.receivePacketList;
	}
	
	public boolean[] getFlowControl() {
		return this.flowControl;
	}
	
	public String getOutputValues() {
		//this function returns a comma separated string list
		//containing the output values associated with this PID
		String outputValueString = "";
		for(int i = 0; i < this.receivePacketList.length; i++)
		{
			//loop through all received packets and collect the associated output values
			outputValueString = outputValueString.concat(this.receivePacketList[i].getOutputValues());
		}
		return outputValueString;
	}
	
	public Output[] get_subordinate_output_list() {
		//this function scans through the receive packets in this PID
		//and returns a list of outputs
		Output[] list = new Output[0];
		for(int i = 0; i < this.receivePacketList.length; i++)
		{
			//get the subordinate outputs
			Output[] shortList = this.receivePacketList[i].getOutputList();
			//append these outputs to the list
			for(int j = 0; j < shortList.length; j++)
			{
				list = Arrays.copyOf(list, list.length + 1);
				list[list.length - 1] = shortList[j];
			}
			
		}
		return list;
	}
	
	public void setReadOnce(boolean state) {
		//this function updates the read-once flag based on the input
		this.readOnce = state;
	}
	
	public void setBeepOnPoll(boolean state) {
		//this function updates the flag that indicates a system beep should be produced each time the polling cycle completes
		this.beepOnPoll = state;
	}
	
	public void setPeriod(int newPeriod) {
		//this function sets a delay time in milliseconds between message polling cycles
		this.messagePeriod_ms = newPeriod;
	}
	
	public void incrementTimeNextPoll() {
		//this function updates the accounting of the next time we'll poll this parameter
		this.timeNextPoll_ms += this.messagePeriod_ms;
	}
	
	public void resetTimeNextPoll() {
		//reset the next poll time delay
		this.timeNextPoll_ms = 0;
	}
	
	public void stopPolling() {
		//this function toggles the checkbox state and isSelected flag
		this.isSelected = false;
		DaftSelectionCheckBox.setSelected(this.isSelected);
	}
	
	public void setInit(String initName) {
		//this function sets the init name for this parameter
		this.initID = initName;
		this.requiresInit = true;
	}
	
	public void readBinaryFile() {
		//read in the binary file that was selected with the file chooser
		if (this.file_chooser_return_value == JFileChooser.APPROVE_OPTION) {
			//if the user selected a file, get the file
			this.hexFile = this.binarySourceChooser.getSelectedFile();

			Path hexPath = Paths.get(this.hexFile.toURI());//convert it to a path
			String fileName = this.hexFile.getName();
			if(fileName.contains(".bin") || fileName.contains(".BIN"))
			{
				try {
					this.prev_hexData = Arrays.copyOf(this.hexData, this.hexData.length);
					this.hexData = Files.readAllBytes(hexPath);//then read in all bytes of data from the hex file
				} catch (IOException e1) {
					e1.printStackTrace();
				}
				//System.out.println("Done reading hex file. Length: " + this.hexData.length + " bytes");
			}
		}
	}
	
	public Serial_Packet[] buildSendPacketsFromFile() {
		//build the data packets automatically to send to the ECU
		readBinaryFile();//first, we refresh the binary file that we read in
		
		Serial_Packet[] newListOfPackets = new Serial_Packet[0];
		Serial_Packet newPacket = new Serial_Packet();
		newPacket.setBaudrate(this.specialModeBaud);
		newPacket.appendByte(0x55);//header
		newPacket.appendByte(0x3);//length -> update later
		newPacket.appendByte(0x88);//write data command
		int contiguous_data_index = newPacket.packetLength;//index for 'number of bytes to write' count
		
		//then, figure out how much of the binary file has changed since last time
		//and add changed bytes to the serial data message. We're only interested
		//in a segment of the file...
		for(int i = this.ROMtargetAddress; i < this.hexData.length && i < (this.ROMtargetAddress + this.ROMtoRAMlength); i++) 
		{
			byte byte_to_send = this.hexData[i];
			
			//decide if we should send this byte
			boolean send_the_byte = false;
			if(this.prev_hexData.length >= i && this.dataSentOnce)
			{
				if(this.prev_hexData[i] != byte_to_send)
				{
					//the data has been changed, so we'll send the byte
					send_the_byte = true;
				}
			}
			else
			{
				//no previous data exists, so we'll send the byte
				send_the_byte = true;
			}
			
			//if we should send the byte, check to see if we should also send its address
			if(send_the_byte)
			{
				if(newPacket.packetLength == contiguous_data_index)
				{
					//add a placeholder for 'number of bytes to write' as 
					//well as the address of data to send
					newPacket.appendByte(0);//number of bytes to write
					int displacement = (int)(i - this.ROMtargetAddress + this.RAMtargetAddress);
					int addrHi = (displacement & 0xFF0000) >> 16;
					int addrMid = (displacement & 0x00FF00) >> 8;
					int addrLo = (displacement & 0x0000FF);
					newPacket.appendByte(addrHi);
					newPacket.appendByte(addrMid);
					newPacket.appendByte(addrLo);
					
					//System.out.println(convert.Int_to_hex_string(addrHi, 2) + convert.Int_to_hex_string(addrMid, 2) + convert.Int_to_hex_string(addrLo, 2));
					/*if(this.dataSentOnce) {
						System.out.println(convert.Int_to_hex_string(addrHi, 2) + convert.Int_to_hex_string(addrMid, 2) + convert.Int_to_hex_string(addrLo, 2));
						System.out.println(byte_to_send + " " + convert.Int_to_hex_string( byte_to_send, 2));
					}*/
				}
				newPacket.appendByte(byte_to_send);
				/*if(this.dataSentOnce && newPacket.packetLength-5 == contiguous_data_index) {
					newPacket.print_packet();
				}*/
			}
			else
			{
				//we've broken the contiguous data
				//so we should write the 'length of data to send' byte
				if(newPacket.packetLength > contiguous_data_index)
				{
					int length_of_data = newPacket.packetLength - (contiguous_data_index + 1) - 3;
					newPacket.changeByte(length_of_data, contiguous_data_index);
				}
				contiguous_data_index = newPacket.packetLength;
			}
			
			if(newPacket.packetLength > 70)
			{
				//it's time to switch packets
				//complete the last packet: write the 'length of data to send'
				if(newPacket.packetLength > contiguous_data_index)
				{
					int length_of_data = newPacket.packetLength - (contiguous_data_index + 1) - 3;
					newPacket.changeByte(length_of_data, contiguous_data_index);
				}
				
				//and write the 'total length of packet' byte
				newPacket.changeByte(newPacket.packetLength + 1, 1);
				
				//append a checksum byte
				newPacket.appendChecksum();
				
				//append that packet to the set of packets
				newListOfPackets = Arrays.copyOf(newListOfPackets, newListOfPackets.length + 1);
				newListOfPackets[newListOfPackets.length - 1] = newPacket;
				
				//and start a new packet
				newPacket = new Serial_Packet();
				newPacket.setBaudrate(this.specialModeBaud);
				newPacket.appendByte(0x55);//header
				newPacket.appendByte(0x3);//length -> update later
				newPacket.appendByte(0x88);//write data command
				contiguous_data_index = newPacket.packetLength;//index for 'number of bytes to write' count
			}
			
		}
		//it's time to finalize the last packet
		//complete the packet: write the 'length of data to send'
		if(newPacket.packetLength > contiguous_data_index)
		{
			int length_of_data = newPacket.packetLength - (contiguous_data_index + 1) - 3;
			newPacket.changeByte(length_of_data, contiguous_data_index);
		} else if(newPacket.packetLength == 3) {
			//make sure we send enough bytes for an OBD message (at least 5)
			newPacket.appendByte(0);//add a byte saying 'write zero bytes' -> if less than serial bytes are sent, then no OBD response occurs
		}
		
		//and write the 'total length of packet' byte
		newPacket.changeByte(newPacket.packetLength + 1, 1);
		
		//append a checksum byte
		newPacket.appendChecksum();
		
		//append that packet to the set of packets
		newListOfPackets = Arrays.copyOf(newListOfPackets, newListOfPackets.length + 1);
		newListOfPackets[newListOfPackets.length - 1] = newPacket;
		
		if(!this.dataSentOnce)
		{
			this.dataSentOnce = true;
		}
		
		return newListOfPackets;
	}
	
	public Serial_Packet[] buildReadPacketsFromFile() {
		//build the set of data packets automatically to read from the ECU
		
		Serial_Packet[] newListOfPackets = new Serial_Packet[0];
		Serial_Packet newPacket;
		
		for(int i = 0; i < this.sendPacketList.length; i++)
		{
			newPacket = new Serial_Packet();
			newPacket.setBaudrate(this.specialModeBaud);
			newPacket.appendByte(0xAA);//header
			newPacket.appendByte(0x4);//length
			newPacket.appendByte(0x88);//write data command
			newPacket.appendChecksum();//checksum
			
			newListOfPackets = Arrays.copyOf(newListOfPackets, newListOfPackets.length + 1);
			newListOfPackets[newListOfPackets.length - 1] = newPacket;
		}
		
		
		return newListOfPackets;
	}
	
	public boolean[] buildFlowControlFromFile() {
		//build the list of flow control packets, assume send/rec. etc..
		boolean[] newFlowControlPacket = new boolean[this.sendPacketList.length*2];
		for(int i = 0; i < newFlowControlPacket.length; i+=2)
		{
			newFlowControlPacket[i] = false;//send/write
			newFlowControlPacket[i+1] = true;//read
		}
		return newFlowControlPacket;
	}

	public void actionPerformed(ActionEvent arg0) {
		Object causedAction = arg0.getSource();
		if(causedAction instanceof JCheckBox)
		{
			this.isSelected = !this.isSelected;
			//System.out.println("Change selection state: " + this.isSelected);
		}
		else if("get hex".equals(arg0.getActionCommand())) {
			//let the user choose the ECU program data file
			file_chooser_return_value = this.binarySourceChooser.showDialog(fileSelectFrame, "Done selecting hex file");
			readBinaryFile();
			this.dataSentOnce = false;
		}
	}
	
	
}
