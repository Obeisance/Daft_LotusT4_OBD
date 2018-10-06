package daftOBD;

import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.FocusEvent;
import java.awt.event.FocusListener;
import java.util.Arrays;

import javax.swing.JCheckBox;
import javax.swing.JPanel;
import javax.swing.JTextField;

public class DaftOBDSelectionObject implements ActionListener {
	//this class contains the descriptors and functions that handle
	//behaviors of OBD select-able parameters and vehicle communication
	//functions
	
	//We'll want to present this class in a JPanel
	JPanel DaftPanel = new JPanel();
	JCheckBox DaftSelectionCheckBox;
	
	//parameters that define a selection object
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
	
	//constructor	
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
	
	
	public String toString() {
		return this.parameterID;
	}

	
	public JPanel getDaftPanel() {
		return DaftPanel;
	}
	
	public Serial_Packet[] getSendPacketList() {
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

	public void actionPerformed(ActionEvent arg0) {
		Object causedAction = arg0.getSource();
		if(causedAction instanceof JCheckBox)
		{
			this.isSelected = !this.isSelected;
			//System.out.println("Change selection state");
		}
	}
	
	
}
