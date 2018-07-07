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
	DaftTreeComponent daftTreeRoot;
	JTextField statusText;
	//JTree tree;
	serial_USB_comm ComPort;
	boolean running = false;
	double logStartTime = 0;
    
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
	
	public void actionPerformed(ActionEvent e) {
		//lets handle the events of pressing different menu items	
		if ("exit".equals(e.getActionCommand())) {
			//close the communication device
			ComPort.close_USB_comm_device();
			
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
            } else {
            	//System.out.println("Open command cancelled by user.");
            }
			
	    }
	    else if("reload device list".equals(e.getActionCommand())) {
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
	    		ScheduledExecutorService scheduler = Executors.newScheduledThreadPool(1);
	    		scheduler.schedule(this, 10, TimeUnit.MILLISECONDS);
	    		//collect the time that we are using as the beginning of logging
	    		logStartTime = System.currentTimeMillis();
	    		
	    		//write the byte names that we're polling into the byte log
	    		String logByteHeaders = daftTreeRoot.getLogPIDnames();
	    		logByteHeaders = "Time(s),".concat(logByteHeaders);
	    		fileHandler.appendln_byteLogFile(settings_file, logByteHeaders);
	    		
	    		//and write the output data names that we're polling into the data log
	    		String dataByteHeaders = daftTreeRoot.getLogHeaders();
	    		dataByteHeaders = "Time(s),".concat(dataByteHeaders);
	    		fileHandler.appendln_dataLogFile(settings_file, dataByteHeaders);
	    		
	    		
	    		//adjust this to allow for variable length number of receive messages
	    	}
	    	else
	    	{
	    		//reset all init states-> a more rigorous treatment would use a timeout
	    		DaftOBDSelectionObject[] initList = daftTreeRoot.getInits();
				for(int i = 0; i < initList.length; i++)
				{
					initList[i].isSelected = false;
				}
				
				//reset the status text
				statusText.setText("Inactive");
				statusText.repaint();
	    	}
	    }
	    else {
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
		JToggleButton start_button = new JToggleButton("Start",running);
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
		modeBoxConstraints.weightx = 0.1;
		modeBoxConstraints.ipadx = 10;
		modeBox.setBorder(BorderFactory.createLineBorder(Color.black));
		
		//these lines belong in the function that reads the file and populates the list
		//we'll test them here for now
		daftTreeRoot = fileHandler.import_Log_definition_list(settings_file);
		daftTree = daftTreeRoot.buildDaftTreePanel();

		modeBox.add(daftTree,modeBoxConstraints);
		modeBox.addMouseListener(this);//to collect events handed up by the child tree
		
		loggingParameterBoxes.add(modeBox);
		
		//now put this into a scroll pane so that it can be viewed more easily
		JScrollPane scrollableLoggingParameters = new JScrollPane(loggingParameterBoxes);//ScrollPaneConstants.VERTICAL_SCROLLBAR_AS_NEEDED,ScrollPaneConstants.HORIZONTAL_SCROLLBAR_AS_NEEDED);
		scrollableLoggingParameters.setPreferredSize(new Dimension(150,300));
		
		//finally, add the log parameters to the main JPanel that we're building
		layoutConstraints.gridx = 0;//position 0,1 (one row down)
		layoutConstraints.gridy = 1;
		layoutConstraints.gridheight = 1;//one row tall, two rows wide
		layoutConstraints.gridwidth = 2;
		layoutConstraints.weightx = 0.1;
		layoutConstraints.weighty = 1.0;
		layoutConstraints.anchor = GridBagConstraints.FIRST_LINE_START;
		layoutConstraints.insets = new Insets(0,10,0,0);
		layoutConstraints.fill = GridBagConstraints.VERTICAL;
		log_button_and_parameters.add(scrollableLoggingParameters,layoutConstraints);
		
		//add a status indicating message
		JPanel statusBox = new JPanel();
		JLabel statusLabel = new JLabel("Status:");
		statusText = new JTextField("Inactive",10);
		statusText.setEditable(false);
		statusBox.add(statusLabel);
		statusBox.add(statusText);
		//then define how it is added to the main window, and add it
		layoutConstraints.gridx = 0;//position 0,2 (two rows down)
		layoutConstraints.gridy = 2;
		layoutConstraints.gridheight = 1;//one row tall, two rows wide
		layoutConstraints.gridwidth = 2;
		layoutConstraints.weightx = 0.1;
		layoutConstraints.weighty = 1.0;
		layoutConstraints.anchor = GridBagConstraints.FIRST_LINE_START;
		layoutConstraints.insets = new Insets(0,10,0,0);
		layoutConstraints.fill = GridBagConstraints.VERTICAL;
		log_button_and_parameters.add(statusBox,layoutConstraints);
		
		
		return log_button_and_parameters;
	}
	
	public boolean send_and_receive_message_cycle(Serial_Packet[] dataToSend, Serial_Packet[] dataToRead, boolean[] messageFlowOrder)
	{	
		boolean success = false;
		int sendIndex = 0;
		int readIndex = 0;
		String logBytes = "";
		
		//loop over the message flow order
		for(int j = 0; j < messageFlowOrder.length; j++)
		{
			logBytes = String.valueOf( (System.currentTimeMillis() - logStartTime) / 1000);
			byte[] thisPacket = new byte[0];
			if(messageFlowOrder[j] == false)
			{
				//send message: loop through all of the serial packets
				//first, we'll update the packet data
				dataToSend[sendIndex].refreshPacket();

				//then, grab the bytes to print/send
				thisPacket = dataToSend[sendIndex].getPacket();

				//update the baudrate if necessary
				ComPort.updateBaudRate(dataToSend[sendIndex].getBaudrate());
				ComPort.send(thisPacket);//and send the bytes


				//then also print for diagnostics
				System.out.print("send: [ ");
				logBytes = logBytes.concat(" Send: [ ");//and save to byte log
				sendIndex ++;
			}
			else if(messageFlowOrder[j] == true)
			{
				//read message
				//loop through all of the serial packets
				//System.out.println("Attempt read");
				
				//update the baudrate if necessary
				ComPort.updateBaudRate(dataToRead[readIndex].getBaudrate());
				byte[] readPacket = ComPort.read(dataToRead[readIndex].packetLength);//read the number of bytes we're looking for
				//thisPacket = dataToRead[readIndex].getPacket();
				thisPacket = readPacket;
				
				success = false;
				if(thisPacket.length > 0)//read success
				{
					success = dataToRead[readIndex].compare_pckt_and_update_outputs(readPacket);//compare the read-in data
					System.out.print("read: [ ");
					logBytes = logBytes.concat(" Read: [ ");
				}
				readIndex ++;
			}
			
			if(thisPacket.length > 0)
			{
				for(int k = 0; k < thisPacket.length; k++)
				{
					System.out.print((int) (thisPacket[k] & 0xFF) + " ");
					logBytes = logBytes.concat(String.valueOf((int) (thisPacket[k] & 0xFF))).concat(" ");
				}
				System.out.println("]");
				logBytes = logBytes.concat("]");
				fileHandler.appendln_byteLogFile(settings_file, logBytes);
			}
		}
		
		//then collect all of the logged outputs and write these to the data file
		String logData = String.valueOf( (System.currentTimeMillis() - logStartTime) / 1000);
		logData = logData.concat(",").concat(daftTreeRoot.getLogOutputs());
		fileHandler.appendln_dataLogFile(settings_file, logData);
		
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
		// TODO Auto-generated method stub
		
	}

	@Override
	public void mouseExited(MouseEvent e) {
		// TODO Auto-generated method stub
		
	}

	@Override
	public void mousePressed(MouseEvent e) {
		// TODO Auto-generated method stub
		
	}

	@Override
	public void mouseReleased(MouseEvent e) {
		// TODO Auto-generated method stub
		
	}
	
	public void run() {
		//this contains the runnable behavior necessary to implement the
		//use callable if we must return a parameter
		if(running)
		{
			DaftOBDSelectionObject[] scanList = daftTreeRoot.getComponents();
			DaftOBDSelectionObject[] initList = daftTreeRoot.getInits();
			
			for(int i = 0; i < scanList.length; i++)
			{
				if(scanList[i].isSelected)
				{
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
									statusText.setText("Sending Init.");
									statusText.repaint();
									
									Serial_Packet[] InitToSend = initList[j].getSendPacketList();
									Serial_Packet[] InitToRead = initList[j].getReadPacketList();
									boolean[] messageFlowOrder = initList[j].getFlowControl();
									
									initFinished = send_and_receive_message_cycle(InitToSend, InitToRead, messageFlowOrder);
									if(initFinished)
									{
										initList[j].isSelected = true;//set the flag saying that our init was complete
									}
									else
									{
										statusText.setText("Init. fail");
										statusText.repaint();
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
					Serial_Packet[] dataToSend = scanList[i].getSendPacketList();
					Serial_Packet[] dataToRead = scanList[i].getReadPacketList();
					boolean[] messageFlowOrder = scanList[i].getFlowControl();
					
					
					String current_status = statusText.getText();
					if(!current_status.equals("Running"))
					{
						statusText.setText("Running");
						statusText.repaint();
					}
					
					
					boolean msgCycleSuccess = send_and_receive_message_cycle(dataToSend, dataToRead, messageFlowOrder);

					if(msgCycleSuccess == false)
					{
						//we had a polling failure:
						statusText.setText("Poll failure");
						statusText.repaint();
					}
				}
			}

			//schedule another serial data exchange
			ScheduledExecutorService scheduler = Executors.newScheduledThreadPool(1);
			scheduler.schedule(this, 1, TimeUnit.MILLISECONDS);
			
			
			//practice other code
		}
	}
	
}