package daftOBD;

import java.awt.GridBagConstraints;
import java.awt.GridBagLayout;
import java.awt.event.MouseEvent;
import java.awt.event.MouseListener;
import java.io.File;
import java.util.Arrays;

import javax.swing.JLabel;
import javax.swing.JPanel;
import javax.swing.JTextField;
import javax.swing.SwingUtilities;

public class DaftTreeComponent implements MouseListener {

	String TreeName;
	JPanel DaftTreePanel;
	GridBagLayout DaftTreeLayout;
	DaftOBDSelectionObject[] DaftObjects;//the PID list
	DaftOBDSelectionObject[] DaftInits;//the Init list
	DaftReflashSelectionObject[] DaftReflash;//the reflash object list
	String[] listOfModes;
	boolean listOfModeVisible[];
	
	Input[] inputList = new Input[0];//keep track of all inputs which are children to this object
	Output[] outputList = new Output[0];//keep track of all outputs which are children to this object
	
	public DaftTreeComponent (String TreeName) {
		//The tree is mostly a list of JPanels with JCheckBoxes and JTextFields
		//and headers of JLabels
		this.TreeName = TreeName;
		this.DaftObjects = new DaftOBDSelectionObject[0];
		this.DaftInits = new DaftOBDSelectionObject[0];
		this.DaftReflash = new DaftReflashSelectionObject[0];
		this.listOfModeVisible = new boolean[0];
		this.listOfModes = new String[0];
	}
	
	public JPanel buildDaftTreePanel() {
		//first, bring the root label into the JPanel
		//System.out.println("Build the tree panel");
		DaftTreePanel = new JPanel();
		DaftTreeLayout = new GridBagLayout();
		DaftTreePanel.setLayout(DaftTreeLayout);
		GridBagConstraints layoutConstraints = new GridBagConstraints();
		JLabel TreeTitle = new JLabel(TreeName);
		layoutConstraints.gridx = 0;//0,0 position
		layoutConstraints.gridy = 0;
		layoutConstraints.gridwidth = 3;//three columns wide
		layoutConstraints.gridheight = 1;
		layoutConstraints.anchor = GridBagConstraints.FIRST_LINE_START;//at the top left
		TreeTitle.addMouseListener(this);//check to see if this has been clicked
		DaftTreePanel.add(TreeTitle, layoutConstraints);
		
		//we should scan through the list of DaftOBDSelectionObjects and build the tree
		
		//first, scan the list to get all of the modes we'll populate
		//only do this if the list is not initiated
		if(listOfModes.length == 0)
		{
			//System.out.println("Find modes");
			for(int i = 0; i < DaftObjects.length; i++)
			{
				listOfModes = addToModeList(listOfModes, DaftObjects[i].mode);
			}
			
			if(this.DaftReflash.length > 0)
			{
				listOfModes = addToModeList(listOfModes,"Reflash");
			}
		}

		//then scan the list and match each parameter to its mode sub-tree
		//and add the parameter if it isVisible
		int y_index = 1;//the gridBagConstraints Y index
		for(int i = 0; i < listOfModes.length; i++)
		{
			//System.out.println("Loop through modes: " + i);
			String currentMode = listOfModes[i];
			//draw the mode into the tree
			if(listOfModeVisible[i] == true)
			{
				GridBagConstraints modeConstraint = new GridBagConstraints();
				JLabel treeSign = new JLabel(" - ");
				modeConstraint.gridx = 0;
				modeConstraint.gridy = y_index;
				modeConstraint.gridwidth = 1;
				modeConstraint.gridheight = 1;
				modeConstraint.anchor = GridBagConstraints.LINE_START;
				DaftTreePanel.add(treeSign,modeConstraint);
				
				JLabel modeLabel = new JLabel(currentMode);	
				modeConstraint.gridx = 1;
				modeConstraint.gridy = y_index;
				modeConstraint.gridwidth = 2;
				modeConstraint.gridheight = 1;
				modeConstraint.anchor = GridBagConstraints.LINE_START;
				modeLabel.addMouseListener(this);//listen if this label has been clicked
				//System.out.println("Add mode node");
				DaftTreePanel.add(modeLabel,modeConstraint);
				y_index += 1;
				for(int j = 0; j < DaftObjects.length; j++)
				{
					if(currentMode.equals(DaftObjects[j].mode) && DaftObjects[j].isVisible)
					{
						//we have a match, so we should draw the object into the tree
						GridBagConstraints leafConstraint = new GridBagConstraints();
						leafConstraint.gridx = 2;
						leafConstraint.gridy = y_index;
						leafConstraint.gridwidth = 2;
						leafConstraint.gridheight = 1;
						leafConstraint.anchor = GridBagConstraints.LINE_START;
						//System.out.println("Add PID leaf");
						DaftTreePanel.add(DaftObjects[j].getDaftPanel(),leafConstraint);
						y_index += 1;
					}
				}
				
				if(currentMode.equals("Reflash"))
				{
					//special treatment for Reflash mode since it is not a DaftOBDSelectionObject
					for(int j = 0; j < this.DaftReflash.length; j++)
					{
						if(DaftReflash[j].isVisible)
						{
							GridBagConstraints leafConstraint = new GridBagConstraints();
							leafConstraint.gridx = 2;
							leafConstraint.gridy = y_index;
							leafConstraint.gridwidth = 2;
							leafConstraint.gridheight = 1;
							leafConstraint.anchor = GridBagConstraints.LINE_START;
							//System.out.println("Add Reflash leaf");
							DaftTreePanel.add(DaftReflash[j].getReflashPanel(),leafConstraint);
							y_index += 1;
						}
					}
				}
			}
		}
		
		
		return DaftTreePanel;
	}
	
	
	public JPanel getDaftTreePanel() {
		return DaftTreePanel;
	}
	
	public void add(DaftOBDSelectionObject newDaftObject) {
		//this function adds another object to our list
		DaftObjects = Arrays.copyOf(DaftObjects, DaftObjects.length + 1);
		DaftObjects[DaftObjects.length - 1] = newDaftObject;
		
		//and append all inputs and outputs from this object to the list for this tree
		append_child_inputs_outputs_to_tree_list(newDaftObject);
	}
	
	public void addInit(DaftOBDSelectionObject newInitObject) {
		//this function adds another init mode to our list of inits
		DaftInits = Arrays.copyOf(DaftInits, DaftInits.length +1);
		DaftInits[DaftInits.length - 1] = newInitObject;
		
		//and append all inputs and outputs from this object to the list for this tree
		append_child_inputs_outputs_to_tree_list(newInitObject);
	}
	
	public void addReflash(DaftReflashSelectionObject newReflashObject) {
		//this function appends the input reflash object to the list of reflash objects associated with this tree
		this.DaftReflash = Arrays.copyOf(this.DaftReflash, this.DaftReflash.length + 1);
		this.DaftReflash[this.DaftReflash.length - 1] = newReflashObject;
	}
	
	public void append_child_inputs_outputs_to_tree_list(DaftOBDSelectionObject newDaftObject)
	{
		//this function appends any Input or Output objects which are children
		//to the newDaftObject to the 'this.inputList' and 'this.outputList'
		//belonging to the DaftTreeComponent
		Serial_Packet[] OBDpckt = newDaftObject.getSendPacketList();		
		for(int h = 0; h < 2; h++)
		{
			for(int i = 0; i < OBDpckt.length; i++)
			{
				Input[] smallList_in = OBDpckt[i].getInputList();
				for(int j = 0; j < smallList_in.length; j++)
				{
					this.inputList = Arrays.copyOf(this.inputList, this.inputList.length + 1);
					this.inputList[this.inputList.length - 1] = smallList_in[j];
				}
				Output[] smallList_out = OBDpckt[i].getOutputList();
				for(int j = 0; j < smallList_out.length; j++)
				{
					this.outputList = Arrays.copyOf(this.outputList, this.outputList.length + 1);
					this.outputList[this.outputList.length - 1] = smallList_out[j];
				}
			}
			OBDpckt = newDaftObject.getReadPacketList();
		}
	}
	
	public DaftOBDSelectionObject[] getComponents() {
		return this.DaftObjects;
	}
	
	public DaftOBDSelectionObject[] getInits() {
		return this.DaftInits;
	}
	
	public String getLogHeaders() {
		//this function returns a comma separated string list of 
		//the output names which are selected for logging
		String outputList = "";
		for(int i = 0; i < this.DaftObjects.length; i++)
		{
			if(this.DaftObjects[i].isSelected == true)
			{
				DaftOBDSelectionObject currentOBDParam = DaftObjects[i];
				Serial_Packet[] receivePackets = currentOBDParam.getReadPacketList();
				//System.out.println("Collect variable names from " + receivePackets.length + " packets");
				for(int j = 0; j < receivePackets.length; j++)
				{
					String output_names_in_this_packet = receivePackets[j].getOutputNameList();
					//System.out.println("Output names on packet " + (j+1) + ": " + output_names_in_this_packet);
					outputList = outputList.concat(output_names_in_this_packet);
				}
				//System.out.println("All Output names: " + outputList);
			}
		}
		return outputList;
	}
	
	public String getLogPIDnames() {
		//this function returns a comma separated string list of 
		//the PIDs which are selected for logging
		String nameList = "";
		for(int i = 0; i < this.DaftObjects.length; i++) {
			if(this.DaftObjects[i].isSelected == true)
			{
				nameList = nameList.concat(this.DaftObjects[i].parameterID).concat(",");
			}
		}
		return nameList;
	}
	
	public String getLogOutputs() {
		//this function returns a comma separated string list of
		//the output values which are selected for logging
		String valueList = "";
		for(int i = 0; i < this.DaftObjects.length; i++)
		{
			//loop over all PIDs
			if(this.DaftObjects[i].isSelected)
			{
				//if we've chosen a PID for logging, collect all associated output values
				valueList = valueList.concat(this.DaftObjects[i].getOutputValues());
			}
		}
		return valueList;
	}
	
	private String[] addToModeList(String[] modeList, String newMode) {
		//scan the modeList to see if the string we're adding is new
		//then add it to the list if it does not yet exist
		boolean notInList = true;
		for(int i = 0; i < modeList.length; i++)
		{
			if(modeList[i].equals(newMode))
			{
				notInList = false;
				break;
			}
		}
		
		String[] returnString;
		if(notInList)
		{
			returnString = Arrays.copyOf(modeList,modeList.length + 1);
			returnString[returnString.length- 1] = newMode;
			listOfModeVisible = Arrays.copyOf(listOfModeVisible, listOfModeVisible.length + 1);
			listOfModeVisible[listOfModeVisible.length - 1] = true;
		}
		else
		{
			returnString = modeList;
		}
		
		return returnString;
	}
	
	public Input[] getAllChildren_inputs()
	{
		//this function returns a list of all inputs that are children to this DaftTreeComponent
		return this.inputList;
	}
	
	public Output[] getAllChildren_outputs()
	{
		//this function returns a list of all inputs that are children to this DaftTreeComponent
		return this.outputList;
	}
	
	public void updateReflashControlOver(JTextField field, serial_USB_comm com, File file)
	{
		//this function allows the main panel to update the components that the Reflash
		//PIDs also want to have access over
		for(int i = 0; i < this.DaftReflash.length; i++)
		{
			this.DaftReflash[i].setComm_and_status(com, field, file);
		}
	}
	
	public DaftReflashSelectionObject[] getReflashObjects()
	{
		//this function returns a list of the reflash objects associated with the tree
		return this.DaftReflash;
	}
	
	@Override
	public void mouseClicked(MouseEvent arg0) {
		Object causedAction = arg0.getSource();
		
		if(causedAction instanceof JLabel)
		{
			JLabel actionable = (JLabel) causedAction;
			//System.out.println("a JLabel caused this event: " + actionable.getText());
			
			//scan all of the "mode" strings to see if we have a match
			int mode_stringIndex = -1;
			for(int i = 0; i < listOfModes.length; i++)
			{
				String compareToThis = actionable.getText();
				if(compareToThis.equals(listOfModes[i]))
				{
					//we have a match
					mode_stringIndex = i;
					break;
				}
			}
			
			//if we don't match, we clicked on the root and should collapse/expand the entire tree
			if(mode_stringIndex == -1)
			{
				//toggle the visibility of all mode JLabels
				for(int i = 0; i < listOfModeVisible.length; i++)
				{
					listOfModeVisible[i] = !listOfModeVisible[i];
					//System.out.println("mode: " + i + " visibility set to " + listOfModeVisible[i]);
				}
			}
			else
			{
			//if we had a match, scan all DaftOBDSelectionObjects and set those under this mode to invisible/visible
				String modeMatch = listOfModes[mode_stringIndex];
				for(int i = 0; i < DaftObjects.length; i++)
				{
					if(modeMatch.equals(DaftObjects[i].mode)) {
						DaftObjects[i].isVisible = !DaftObjects[i].isVisible;
						//System.out.println(DaftObjects[i].parameterID + " visibility set to " + DaftObjects[i].isVisible);
					}
				}
				
				//or act on the reflash modes
				if(modeMatch.equals("Reflash"))
				{
					for(int i = 0; i < this.DaftReflash.length; i++)
					{
						this.DaftReflash[i].isVisible = !this.DaftReflash[i].isVisible;
					}
				}
			}
			//then, we want to hand the event out to whoever holds this classes JPanel
			MouseEvent parentEvent = SwingUtilities.convertMouseEvent(DaftTreePanel, arg0, DaftTreePanel.getParent());//convert the event to be relative to the parent coordinate system
			DaftTreePanel.getParent().dispatchEvent(parentEvent);//pass the event to the parent
		}
		
	}

	@Override
	public void mouseEntered(MouseEvent arg0) {
		// TODO Auto-generated method stub
		
	}

	@Override
	public void mouseExited(MouseEvent arg0) {
		// TODO Auto-generated method stub
		
	}

	@Override
	public void mousePressed(MouseEvent arg0) {
		// TODO Auto-generated method stub
		
	}

	@Override
	public void mouseReleased(MouseEvent arg0) {
		// TODO Auto-generated method stub
		
	}


}

/*
in order to use this custom tree, in the main function where the tree
is built the following code needs to be present:

//there is a JPanel active prior to this code: a modebox panel which will contain the tree
		/*
		DefaultMutableTreeNode treeBaseNode = new DefaultMutableTreeNode("Parameters");//the root of a tree for mode and parameter selection		
		DefaultMutableTreeNode mode = null;
		DefaultMutableTreeNode parameter = null;
		
		mode = new DefaultMutableTreeNode("Mode 0x1");
		treeBaseNode.add(mode);
		*/

//add a DaftOBDObject to the tree node (or any other object like a checkbox)
		/*
		parameter = new DefaultMutableTreeNode(DaftOBD_obj);
		mode.add(parameter);
		*/


		/*
		parameter = new DefaultMutableTreeNode(DaftOBD_obj);//add a DaftOBDObject to the tree (or any other object like a checkbox)
		mode.add(parameter);
		*/

//then set the renderer that we'll use- this accommodates the strange functions of the tree like checkboxes
		/*
		tree = new JTree(treeBaseNode);
		TreeCellRenderer renderer = new DaftOBDSelectionObjectCellRenderer();
	    tree.setCellRenderer(renderer);
	    tree.addTreeSelectionListener(this);

//finally add the tree to the JPanel
		modeBox.add(tree);
		*/

//we have a tree event listener too
	/*
	//for the tree selection event listener
	public void valueChanged(TreeSelectionEvent tree_event) {
		DefaultMutableTreeNode node = (DefaultMutableTreeNode) tree.getLastSelectedPathComponent();
		//System.out.println("Selection: " + node.toString());
		
		Object userObject = node.getUserObject();

		//if the object is a DaftOBDSelectionObject, we're going to make it what we want
		if (userObject instanceof DaftOBDSelectionObject) {
			//update the text to render for this object
			DaftOBDSelectionObject e = (DaftOBDSelectionObject) userObject;
			
			//e.requestFocusInWindow();
			//Object focusedObject = fileSelectorPanel.getFocusOwner();
			//System.out.println(e.toString());
		}
		
	}
    */