package daftOBD;
import java.awt.Color;
import java.awt.Dimension;
import java.awt.GridBagConstraints;
import java.awt.GridBagLayout;
import java.awt.Insets;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.KeyEvent;
import java.awt.event.MouseEvent;
import java.awt.event.MouseListener;
import java.io.File;
import java.time.LocalDateTime;
import java.util.Arrays;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;

import javax.swing.BorderFactory;
import javax.swing.BoxLayout;
import javax.swing.JFileChooser;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JMenu;
import javax.swing.JMenuBar;
import javax.swing.JMenuItem;
import javax.swing.JPanel;
import javax.swing.JRootPane;
import javax.swing.JScrollPane;
import javax.swing.JTextField;
import javax.swing.JTextPane;
import javax.swing.JToggleButton;
import javax.swing.JTree;

public class main_window_content implements ActionListener, MouseListener, Runnable {
	//we need constructors for our menu bar and for
	//our content panels
	JFrame fileSelectorPanel;
	JPanel daftTree;
	JToggleButton start_button;
	DaftTreeComponent daftTreeRoot;
	JTextField statusText;
	//JTree tree;
	serial_USB_comm ComPort;
	boolean running = false;
	double logStartTime = 0;
	String[] logDataList = new String[0];
	String[] logByteList = new String[0];
	String file_dateID = "";
	
	//and an Executor to handle time-based operations
	ScheduledExecutorService scheduler = Executors.newScheduledThreadPool(0);//we'll continuously reuse this pool
    
	//bring in the file handler class so that we can update our settings files and read in our
	//definitions in order to build the main window content
	DaftOBDFileHandlers fileHandler = new DaftOBDFileHandlers(); 
	File settings_file = new File("Daft OBD settings.txt");//this is a reference to the settings file
	
	//construct a menu bar
	public JMenuBar create_menu_bar() {
		//lets put together the objects in the menu bar
		JMenuBar menubar;//the menu bar itself
		JMenu menuBarItem;//for the parts of the menu bar- this can be reused
		JMenuItem dropDownMenuItem;//for the parts of each dropdown menu- this can be reused
		
		//Create the menu bar.
        menubar = new JMenuBar();
        
        //then create the parts of the menu bar and add them to it
        
        //start with the 'File' menu
        menuBarItem = new JMenu("Menu");
        menuBarItem.setMnemonic(KeyEvent.VK_M);//allow access to the menu via keyboard press
        menuBarItem.getAccessibleContext().setAccessibleDescription("This menu allows for changing settings");
        menubar.add(menuBarItem);

        
        //the file menu need to be able to:
        //allow the user to specify the location of the file that contains definitions for logging
        //allow the user to specify the location of the file to save a log to
        //force the program to exit

        //logger definition file
        dropDownMenuItem = new JMenuItem("Choose log defn. file");
        dropDownMenuItem.setMnemonic(KeyEvent.VK_D);
        dropDownMenuItem.getAccessibleContext().setAccessibleDescription("Choose the file path of the logger definition");
        menuBarItem.add(dropDownMenuItem);
        dropDownMenuItem.setActionCommand("logDefn");//included to allow for action to be taken
        dropDownMenuItem.addActionListener(this);
        
        //log file location
        dropDownMenuItem = new JMenuItem("Save log location");
        dropDownMenuItem.setMnemonic(KeyEvent.VK_L);
        dropDownMenuItem.getAccessibleContext().setAccessibleDescription("Choose the file path to save logs");
        menuBarItem.add(dropDownMenuItem);
        dropDownMenuItem.setActionCommand("logSave");
        dropDownMenuItem.addActionListener(this);

        //make the 'about' menu item too
        dropDownMenuItem = new JMenuItem("About");
        dropDownMenuItem.setMnemonic(KeyEvent.VK_A);
        dropDownMenuItem.getAccessibleContext().setAccessibleDescription("The About menu brings up a small dialog box");
        menuBarItem.add(dropDownMenuItem);
        dropDownMenuItem.setActionCommand("about");
        dropDownMenuItem.addActionListener(this);
        
        //choose the OBD interface cable
        ComPort = new serial_USB_comm();
        JMenu com_port_select = ComPortMenu("Select COM device");
        menuBarItem.add(com_port_select);
        
        
        //exit the program
        dropDownMenuItem = new JMenuItem("Exit");
        dropDownMenuItem.setMnemonic(KeyEvent.VK_E);
        dropDownMenuItem.getAccessibleContext().setAccessibleDescription("Exit the program");
        menuBarItem.add(dropDownMenuItem);
        dropDownMenuItem.setActionCommand("exit");
        dropDownMenuItem.addActionListener(this);
        
        
		return menubar;
	}
	
	public void save_PID_select_state() {
		//save the PIDs that are selected
		DaftOBDSelectionObject[] scanList = daftTreeRoot.getComponents();
		String PIDmask = "";
		for(int i = 0; i < scanList.length; i++) {
			if(scanList[i].isSelected) {
				PIDmask = PIDmask.concat("1");
			}
			else
			{
				PIDmask = PIDmask.concat("0");
			}
		}
		fileHandler.update_settings_logParameters(settings_file, PIDmask);
		//System.out.println(PIDmask);
	}
	
	public void actionPerformed(ActionEvent e) {
		//lets handle the events of pressing different menu items	
		if ("exit".equals(e.getActionCommand())) {
			//close the communication device
			ComPort.close_USB_comm_device();
			
			save_PID_select_state();
			
	    	//exit the program
	    	System.exit(0);
	    } else if("about".equals(e.getActionCommand())) {
	    	//bring up a dialog box to show information about the program
			//System.out.println("about window");
	    	
	    	JFrame frame = new JFrame("About");
	    	frame.setDefaultCloseOperation(JFrame.DISPOSE_ON_CLOSE);
	    	JPanel aboutPanel = new JPanel();
	    	JTextPane aboutText = new JTextPane();
	    	aboutText.setText("Created by Obeisance, 2018" + '\n' + "To learn more visit:" + '\n' + "http://www.lotustalk.com/forums/f171/daft-disassembly-352193/" + '\n' + '\n' + "Download from:" +'\n' + "https://github.com/Obeisance/Daft_LotusT4_OBD");
	    	aboutText.setEditable(false);
	    	aboutPanel.add(aboutText);
	    	frame.add(aboutPanel);
	    	frame.pack();
	    	frame.setVisible(true);
	    	
	    	//the message dialog does not allow highlighting of text
			//JOptionPane.showMessageDialog(frame, "Created by Obeisance" + '\n' + "To learn more visit:" + '\n' + "http://www.lotustalk.com/forums/f171/daft-disassembly-352193/" + '\n' + '\n' + "Download from:" +'\n' + "https://github.com/Obeisance/Daft_LotusT4_OBD");
	    } else if("logSave".equals(e.getActionCommand())) {
	    	//bring up a file chooser to allow the user to choose where to save
	    	//a log file
	    	JFileChooser logLocation = new JFileChooser();
	    	logLocation.setFileSelectionMode( JFileChooser.DIRECTORIES_ONLY);
	    	int returnVal = logLocation.showDialog(fileSelectorPanel, "Done selecting save location");
	    	
	    	if (returnVal == JFileChooser.APPROVE_OPTION) {
                File file = logLocation.getSelectedFile();
                //This is where the application saves the log files.
                //System.out.println("Saving: " + file.getName() );
                
                //update the record of the log file save location in the settings text file             
                fileHandler.update_settings_logSaveLocation(settings_file, file.getPath());

            } else {
            	System.out.println("Save command cancelled by user.");
            }
	    	
	    } else if("logDefn".equals(e.getActionCommand())) {
	    	//bring up a file chooser to pick the log definition file
	    	JFileChooser logLocation = new JFileChooser();
	    	int returnVal = logLocation.showOpenDialog(fileSelectorPanel);
	    	
	    	if (returnVal == JFileChooser.APPROVE_OPTION) {
                File file = logLocation.getSelectedFile();
                //This is where we will save the log files.
                //System.out.println("Definition File: " + file.getName() );
                
                //update the record of the log definition file location in the settings text file             
                fileHandler.update_settings_logDefn(settings_file, file.getPath());
                //we could immediately update the parameter list, but it may be easier to wait for a restart
        		daftTreeRoot = fileHandler.import_Log_definition_list(settings_file);
        		
        		fileHandler.update_settings_logParameters(settings_file, "0");//reset the saved PIDs 
            } else {
            	//System.out.println("Open command cancelled by user.");
            }
			
	    }
	    else if("reload device list".equals(e.getActionCommand())) {
	    	//if no devices are found, we re-load the device list
	    	Object obj = e.getSource();
	    	if(obj instanceof JMenuItem)
	    	{
	    		JMenuItem clicked = (JMenuItem) obj;
	    		ComPort.close_USB_comm_device();
	    		ComPort = new serial_USB_comm();//this selects the first device that is found
	    		/*JMenu replace = ComPortMenu("Select COM device");
	    		JPopupMenu parent = (JPopupMenu) clicked.getParent();
	    		JMenu topMenu = (JMenu) parent.getParent();
	    		topMenu.remove(parent);
	    		topMenu.add(replace);
	    		*/
	    		//since we grabbed the first device found, update the string indicating which device we are using
	    		String[] comList = ComPort.getDeviceStringList();
	    		if(comList.length > 0)
	    		{
	    			//System.out.println(comList[0]);
	    			clicked.setText(comList[0]);
	    		}
	    		clicked.revalidate();
	    		clicked.repaint();
	    	}
	    }
	    else if("start".equals(e.getActionCommand())) {
	    	running = !running;
	    	//for now, list out the serial packets which need to be sent (i.e. those that are checked)
	    	//eventually, this action will setup the init exchange as well
	    	//as the serial data packets
	    	if(running)
	    	{
	    		//schedule an event
	    		this.scheduler = Executors.newScheduledThreadPool(0);
	    		this.scheduler.schedule(this, 10, TimeUnit.MILLISECONDS);	    		
	    		//collect the time that we are using as the beginning of logging
	    		logStartTime = System.currentTimeMillis();
	    		LocalDateTime thisSec = LocalDateTime.now();
	    		file_dateID = thisSec.getYear() + "-" + thisSec.getMonth() + "-" + thisSec.getDayOfMonth() + " " + thisSec.getHour() + " " + thisSec.getMinute() + " " + thisSec.getSecond();
	    		//System.out.println(file_dateID);
	    	}
	    	else
	    	{
	    		//reset all init states-> a more rigorous treatment would use a timeout
	    		DaftOBDSelectionObject[] initList = daftTreeRoot.getInits();
				for(int i = 0; i < initList.length; i++)
				{
					initList[i].isSelected = false;
				}
				
				//reset the poll delay periods too
				DaftOBDSelectionObject[] scanList = daftTreeRoot.getComponents();
				for(int i = 0; i < scanList.length; i++)
				{
					scanList[i].resetTimeNextPoll();
				}
				
				//shutdown any active thread requests:
				this.scheduler.shutdown();//make sure to tell the executor to shut down open threads when they finished, and to stop accepting new thread requests.
				
				//reset the comport device to clear data buffers
				this.ComPort.reset();
				
				//reset the status text
				statusText.setText("Inactive");
				statusText.repaint();
				
				//save the byte log
				if(this.logByteList.length > 0)
				{
					this.fileHandler.appendStrArray_byteLogFile(this.settings_file, this.file_dateID, this.logByteList);
					this.logByteList = new String[0];
				}
				
				//save the data log
				if(this.logDataList.length > 0)
				{
					this.fileHandler.appendStrArray_dataLogFile(this.settings_file, this.file_dateID, this.logDataList);
					this.logDataList = new String[0];
				}
	    	}
	    }
	    else {
	    	//if an action commands switching the FTDI device
	    	String[] comList = ComPort.getDeviceStringList();
	    	boolean found = false;
	    	for(int i = 0; i < comList.length; i++)
	    	{
	    		if(comList[i].equals(e.getActionCommand()))
	    		{
	    			ComPort.setDevice(i);
	    			found = true;
	    			break;
	    		}
	    	}
	    	if(found == false)
	    	{
	    		System.out.println("unknown command");
	    	}
	    	
	    }

	}
	
	public JMenu ComPortMenu(String title) {
		JMenu submenu = new JMenu(title);
		String[] comList = ComPort.getDeviceStringList();
		if(comList.length != 0)
		{
			for(int i = 0; i < comList.length; i++)
			{
				JMenuItem comDevice = new JMenuItem(comList[i]);
				comDevice.setActionCommand(comList[i].toString());
				comDevice.addActionListener(this);
				submenu.add(comDevice);
			}
		} 
		else
		{
			JMenuItem empty = new JMenuItem("No devices detected - click to re-scan");
			empty.setActionCommand("reload device list");
			empty.addActionListener(this);
			submenu.add(empty);
		}
		return submenu;
	}
	
	//construct a panel with logging options
	public JPanel loggingParameterSelectors()
	{
		//we'll put our components into a JPanel container
		JPanel log_button_and_parameters = new JPanel(new GridBagLayout());
		GridBagConstraints layoutConstraints = new GridBagConstraints();
	
		
		//our first component is a button which can be pressed 
		//to start the logging action
		start_button = new JToggleButton("Start",running);
		start_button.addActionListener(this);
		start_button.setActionCommand("start");
		start_button.setPreferredSize(new Dimension(75, 30));
		layoutConstraints.gridx = 0;//position 0,0 in the grid
		layoutConstraints.gridy = 0;
		layoutConstraints.gridheight = 1;//only one grid position
		layoutConstraints.gridwidth = 1;
		layoutConstraints.weightx = 0.0;
		layoutConstraints.weighty = 0.0;
		layoutConstraints.anchor = GridBagConstraints.FIRST_LINE_START;//anchor to the top left
		layoutConstraints.insets = new Insets(10,10,10,10);//10 px all around buffer
		log_button_and_parameters.add(start_button, layoutConstraints);
		
		//then we need a panel which contains all of the parameter options
		JPanel loggingParameterBoxes = new JPanel();
		loggingParameterBoxes.setLayout(new BoxLayout(loggingParameterBoxes, BoxLayout.Y_AXIS));
		
		//we'll have to read the definition file and populate a list of check boxes
		JPanel modeBox = new JPanel(new GridBagLayout());
		GridBagConstraints modeBoxConstraints = new GridBagConstraints();
		modeBoxConstraints.anchor = GridBagConstraints.FIRST_LINE_START;
		modeBoxConstraints.weighty = 0.1;
		modeBoxConstraints.weightx = 1.0;
		modeBoxConstraints.ipadx = 10;
		modeBox.setBorder(BorderFactory.createLineBorder(Color.black));
		
		//these lines belong in the function that reads the file and populates the list
		//we'll test them here for now
		daftTreeRoot = fileHandler.import_Log_definition_list(settings_file);//import the log parameters
		boolean[] PIDs_to_activate = fileHandler.import_PID_reload_list(settings_file);//collect and update the PID selection status
		DaftOBDSelectionObject[] scanList = daftTreeRoot.getComponents();
		for(int i = 0; i < scanList.length; i++) {
			if(PIDs_to_activate.length > i) {
				if(PIDs_to_activate[i])
				{
					scanList[i].DaftSelectionCheckBox.setSelected(true);
					scanList[i].isSelected = true;
				}
			}
		}
		daftTree = daftTreeRoot.buildDaftTreePanel();

		modeBox.add(daftTree,modeBoxConstraints);
		modeBox.addMouseListener(this);//to collect events handed up by the child tree
		
		loggingParameterBoxes.add(modeBox);//put the daft tree into the JPanel called modeBox
		
		//now put this modeBox JPanel into a scroll pane so that it can be viewed more easily
		JScrollPane scrollableLoggingParameters = new JScrollPane(loggingParameterBoxes);//ScrollPaneConstants.VERTICAL_SCROLLBAR_AS_NEEDED,ScrollPaneConstants.HORIZONTAL_SCROLLBAR_AS_NEEDED);
		Dimension minSize = new Dimension(280,90);//used to be 900,500
		if(scrollableLoggingParameters.getSize().width < minSize.width) {
			scrollableLoggingParameters.setPreferredSize(minSize);
		}
		
		
		//finally, add the log parameters JScrollPane to the main JPanel that we're building
		layoutConstraints.gridx = 0;//position 0,1 (one row down)
		layoutConstraints.gridy = 1;
		layoutConstraints.gridheight = 1;//one row tall, two rows wide
		layoutConstraints.gridwidth = 2;
		layoutConstraints.weightx = 0.1;
		layoutConstraints.weighty = 1.0;
		layoutConstraints.anchor = GridBagConstraints.FIRST_LINE_START;
		layoutConstraints.insets = new Insets(0,10,0,10);
		layoutConstraints.fill = GridBagConstraints.BOTH;
		log_button_and_parameters.add(scrollableLoggingParameters,layoutConstraints);
		
		//add a status indicating message
		JPanel statusBox = new JPanel();
		JLabel statusLabel = new JLabel("Status:");
		statusText = new JTextField("Inactive",15);
		statusText.setEditable(false);
		statusBox.add(statusLabel);
		statusBox.add(statusText);
		//then define how it is added to the main window, and add it
		layoutConstraints.gridx = 0;//position 0,2 (two rows down)
		layoutConstraints.gridy = 2;
		layoutConstraints.gridheight = 1;//one row tall, two rows wide
		layoutConstraints.gridwidth = 1;
		layoutConstraints.weightx = 0;
		layoutConstraints.weighty = 0;
		layoutConstraints.anchor = GridBagConstraints.LAST_LINE_START;
		layoutConstraints.insets = new Insets(0,10,0,0);
		layoutConstraints.fill = GridBagConstraints.VERTICAL;
		log_button_and_parameters.add(statusBox,layoutConstraints);
		
		
		//after we've initialized the 'statusText' object, we can share it
		daftTreeRoot.updateReflashControlOver(this.statusText, this.ComPort, this.settings_file);//add the controllable parameters to the tree reflash objects
		return log_button_and_parameters;
	}
	
	public boolean send_and_receive_message_cycle(Serial_Packet[] dataToSend, Serial_Packet[] dataToRead, boolean[] messageFlowOrder)
	{	
		boolean success = false;
		int sendIndex = 0;
		int readIndex = 0;
		String logBytes = "";
		Conversion_Handler convert = new Conversion_Handler();
		
		//check to see if we've collected our polling headers:
		//System.out.println("Write new file headers, if necessary...");
		if(this.logByteList.length == 0)
		{
			//write the byte names that we're polling into the byte log
    		String logByteHeaders = daftTreeRoot.getLogPIDnames();
    		logByteHeaders = "Time(s),".concat(logByteHeaders);
    		//System.out.println("Byte headers: " + logByteHeaders);
    		//fileHandler.appendln_byteLogFile(settings_file, logByteHeaders);
    		this.logByteList = Arrays.copyOf(this.logByteList, this.logByteList.length + 1);
    		this.logByteList[this.logByteList.length - 1] = logByteHeaders;
    		//System.out.println("First string in byte list: " + this.logByteList[0]);
    		
    		//and write the output data names that we're polling into the data log
    		String dataByteHeaders = daftTreeRoot.getLogHeaders();
    		dataByteHeaders = "Time(s),".concat(dataByteHeaders);
    		//System.out.println("Data headers: " + dataByteHeaders);
    		//fileHandler.appendln_dataLogFile(settings_file, dataByteHeaders);
    		this.logDataList = Arrays.copyOf(this.logDataList, this.logDataList.length + 1);
    		this.logDataList[this.logDataList.length - 1] = dataByteHeaders;
		}
		
		//loop over the message flow order
		//System.out.println("Number of steps: " + messageFlowOrder.length);
		for(int j = 0; j < messageFlowOrder.length; j++)
		{
			//System.out.print("msg index: " + j);
			logBytes = String.valueOf( (System.currentTimeMillis() - logStartTime) / 1000);
			byte[] thisPacket = new byte[0];
			if(messageFlowOrder[j] == false)
			{
				//System.out.println(" send");
				//send message: loop through all of the serial packets
				//first, we'll update the packet data
				dataToSend[sendIndex].refreshPacket();

				//then, grab the bytes to print/send
				thisPacket = dataToSend[sendIndex].getPacket();
				
				//dataToSend[sendIndex].print_packet();//for diagnostics of input changes

				//update the baudrate if necessary
				ComPort.updateBaudRate(dataToSend[sendIndex].getBaudrate());
				ComPort.send(thisPacket);//and send the bytes


				//then also print for diagnostics
				//System.out.print("send: [ ");
				logBytes = logBytes.concat(" Send: [ ");//and save to byte log
				sendIndex ++;
			}
			else if(messageFlowOrder[j] == true)
			{
				//read message
				//loop through all of the serial packets
				//System.out.println(" read");
				
				//update the baudrate if necessary
				//System.out.println("Changing baud rate for reading: " + dataToRead[readIndex].getBaudrate());
				ComPort.updateBaudRate(dataToRead[readIndex].getBaudrate());
				byte[] readPacket = ComPort.read(dataToRead[readIndex].packetLength);//read the number of bytes we're looking for
				//thisPacket = dataToRead[readIndex].getPacket();
				thisPacket = readPacket;
				
				//System.out.print("Length: " + dataToRead[readIndex].packetLength + " data: ");
				//dataToRead[readIndex].print_packet();//for diagnostics of how the packet was read in by the definition interpreter
				
				//System.out.print("read: [ ");
				logBytes = logBytes.concat(" Read: [ ");
				
				if(thisPacket.length > 0)//read success
				{
					boolean state = dataToRead[readIndex].compare_pckt_and_update_outputs(readPacket);//compare the read-in data
					if(!success && state)
					{
						success = state;//any 'true' will be latched, so the only return of 'false' is if all polling efforts fail
					}
				}
				readIndex ++;
			}
			
			if(thisPacket.length > 0)
			{
				for(int k = 0; k < thisPacket.length; k++)
				{
					//System.out.print((int) (thisPacket[k] & 0xFF) + " ");
					String hexByte = convert.Int_to_hex_string(thisPacket[k], 2);
					logBytes = logBytes.concat(hexByte).concat(" ");
				}
			}
			//System.out.println("]");
			logBytes = logBytes.concat("]");
			//fileHandler.appendln_byteLogFile(settings_file, logBytes);
			
			this.logByteList = Arrays.copyOf(this.logByteList, this.logByteList.length+1);
			this.logByteList[this.logByteList.length-1] = logBytes;
		}
		
		//then collect all of the logged outputs and write these to the data file
		String logData = String.valueOf( (System.currentTimeMillis() - logStartTime) / 1000);
		logData = logData.concat(",").concat(daftTreeRoot.getLogOutputs());
		//fileHandler.appendln_dataLogFile(settings_file, logData);
		//System.out.println(logData);
		
		this.logDataList = Arrays.copyOf(this.logDataList, this.logDataList.length+1);
		this.logDataList[this.logDataList.length-1] = logData;
		
		
		
		return success;
	}


	@Override
	public void mouseClicked(MouseEvent arg0) {
		//act on a mouse click
		Object sourceOfEvent = arg0.getSource();
		//some of these events will have been handed up from a child component
		if(sourceOfEvent instanceof JPanel)
		{
			JPanel source = (JPanel) sourceOfEvent;
			//if the JPanel contains the daftTree object, we'll re-draw that so it behaves like a 
			//tree
			//JPanel contains JScrollPane contains JPanel contains daftTree
			if(source.contains(daftTree.getLocation()))
			{
				JPanel parent = (JPanel) source.getParent();
				source.remove(daftTree);
				daftTree = daftTreeRoot.buildDaftTreePanel();
				GridBagConstraints c = new GridBagConstraints();
				c.anchor = GridBagConstraints.FIRST_LINE_START;
				c.weighty = 0.1;
				c.weightx = 0.1;
				c.ipadx = 10;
				source.add(daftTree,c);
				
				parent.revalidate();//to get the change to show immediately
				parent.repaint();
			}
		}
		
	}

	@Override
	public void mouseEntered(MouseEvent e) {
		
	}

	@Override
	public void mouseExited(MouseEvent e) {

	}

	@Override
	public void mousePressed(MouseEvent e) {

	}

	@Override
	public void mouseReleased(MouseEvent e) {

	}
	
	public void run() {
		//this contains the runnable behavior necessary to implement the
		//use callable if we must return a parameter
		if(running)
		{
			DaftOBDSelectionObject[] scanList = daftTreeRoot.getComponents();
			DaftOBDSelectionObject[] initList = daftTreeRoot.getInits();
			boolean[] finished_reading_ThisTime = new boolean[scanList.length];
			
			int countActive = 0;
			
			for(int i = 0; i < scanList.length; i++)
			{
				if(scanList[i].isSelected)
				{
					//System.out.println("Scanning: " + scanList[i].parameterID);
					countActive++;
					if(scanList[i].requiresInit)
					{
						//System.out.println(scanList[i].parameterID + " requires init: " + scanList[i].initID);
						boolean initFinished = false;
						while(!initFinished)
						{
							for(int j = 0; j < initList.length; j++)
							{
								if(initList[j].initID.equals(scanList[i].initID))
								{
									//we've found the init of interest
									//now check to see if the init has been sent
									if(!initList[j].isSelected)
									{
										//we need to send the init
										String current_status = statusText.getText();
										if(!current_status.equals("Sending Init."))
										{
											statusText.setText("Sending Init.");
											statusText.repaint();
										}

										Serial_Packet[] InitToSend = initList[j].getSendPacketList();
										Serial_Packet[] InitToRead = initList[j].getReadPacketList();
										boolean[] messageFlowOrder = initList[j].getFlowControl();
										
										//System.out.println("Sending Init...");
										//System.out.println("Sending " + InitToSend.length + " messages");
										//System.out.println("Reading " + InitToRead.length + " messages");
										
										initFinished = send_and_receive_message_cycle(InitToSend, InitToRead, messageFlowOrder);
										if(initFinished)
										{
											current_status = statusText.getText();
											if(!current_status.equals("Init. success"))
											{
												statusText.setText("Init. success");
												statusText.repaint();
											}
											
											initList[j].isSelected = true;//set the flag saying that our init was complete
										}
										else
										{
											current_status = statusText.getText();
											if(!current_status.equals("Init. fail"))
											{
												statusText.setText("Init. fail");
												statusText.repaint();
											}
										}
										break;
									}
									else
									{		
										initFinished = true;
										break;
									}
								}
							}
						}
					}
					//compare current polling time to scheduled poll time
					int current_poll_time = (int) (System.currentTimeMillis() - logStartTime);
					boolean msgCycleSuccess = false;
					if(current_poll_time >= scanList[i].timeNextPoll_ms)
					{
						scanList[i].incrementTimeNextPoll();
						Serial_Packet[] dataToSend = scanList[i].getSendPacketList();
						Serial_Packet[] dataToRead = scanList[i].getReadPacketList();
						boolean[] messageFlowOrder = scanList[i].getFlowControl();

						msgCycleSuccess = send_and_receive_message_cycle(dataToSend, dataToRead, messageFlowOrder);
						
						//make a beep if we're supposed to
						if(scanList[i].beepOnPoll && msgCycleSuccess) {
							java.awt.Toolkit.getDefaultToolkit().beep();//lol, I bet this will be fun... -_-
						}
					}
					else
					{
						//we skip a polling cycle due to scheduled rate limitation
						msgCycleSuccess = true;
					}

					if(msgCycleSuccess == false)
					{
						//we had a polling failure:
						String current_status = statusText.getText();
						if(!current_status.equals("Poll failure"))
						{
							statusText.setText("Poll failure");
							statusText.repaint();
						}
						
						finished_reading_ThisTime[i] = false;
					}
					else
					{
						//we are running correctly - this update may eat a bunch of processor time.. but we'll keep it for now
						statusText.setText("Running " + String.valueOf( (System.currentTimeMillis() - logStartTime) / 1000));
						statusText.repaint();
						/*
						String current_status = statusText.getText();
						if(!current_status.equals("Running"))
						{
							statusText.setText("Running ");
							statusText.repaint();
						}
						*/
						
						finished_reading_ThisTime[i] = true;
					}
				}
			}
			
			//loop through the data files and adjust the poll selection: if a parameter should only be read once, de-select it
			for(int i = 0; i < finished_reading_ThisTime.length; i++)
			{
				if(finished_reading_ThisTime[i] && scanList[i].readOnce)
				{
					//update the checkbox in case the PID is a single read-case
					scanList[i].stopPolling();
				}
			}
			
			if(countActive > 0)
			{
				//schedule another serial data exchange
				this.scheduler.schedule(this, 1, TimeUnit.MILLISECONDS);
			}
			else
			{
				//since we're not running any OBD polling,
				//check to see if any reflash parameters have been clicked, we'll act on the first one
				//we find.
				DaftReflashSelectionObject[] reflash = this.daftTreeRoot.getReflashObjects();
				//first, loop to see if we have any unselected reflash objects that are active
				boolean active_non_selected = false;
				for(int i = 0; i < reflash.length; i++)
				{
					if(!reflash[i].isSelected && reflash[i].active)
					{
						active_non_selected = true;
						break;
					}
				}
				
				//now loop again and send a command if we don't have an unselected but active reflash object
				if(!active_non_selected)
				{
					for(int i = 0; i < reflash.length; i++)
					{
						if(reflash[i].isSelected)
						{
							reflash[i].begin();//send the command to either begin or end communication with the ECU
							break;//don't send commands to any other reflash objects
						}
						else
						{
							if(reflash[i].active)
							{
								//we do not want to command any other reflash object if we have one
								//that is active but not selected
								break;
							}
						}
					}
				}
				
				//stop polling if there are no more parameters active
				this.start_button.doClick();
			}
			
			//in case someone clicked the 'start' button in the mean time
			if(!running)
			{
				//reset the status text
				statusText.setText("Inactive");
				statusText.repaint();
			}
			
			//practice other code
			
		}
		return;
	}
	
}
