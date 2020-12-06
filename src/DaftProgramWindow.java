import java.awt.Dimension;
import java.io.File;
import java.io.IOException;

import javax.swing.JFrame;

import daftOBD.main_window_content;

public class DaftProgramWindow {

	
	public static void createAndShowGUI() {
		//a frame to put our user interface into
		JFrame main_window = new JFrame("Daft OBD v2.15");
		//main_window.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);//set what the window does when closed
		
		//load or create our settings file
		File settings_file = new File("Daft OBD settings.txt");
		if(!settings_file.exists())
		{
			try {
				settings_file.createNewFile();
			} catch (IOException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
				//System.out.println("Failed to create new settings file");
			}
		}
		
		//add our primary content
		main_window_content windowContent = new main_window_content();//initialize the class that contains our objects
		main_window.setJMenuBar(windowContent.create_menu_bar());//then bring in the menu bar
		main_window.add(windowContent.loggingParameterSelectors());
		
		main_window.setResizable(true);//allow the window to be resized
		main_window.setMinimumSize(new Dimension(320,240));
		main_window.setPreferredSize(new Dimension(800,600));
		//draw the window to size
		main_window.pack();
		main_window.setVisible(true);//and then set it to visible
		
		//we would like to save the state of selected PIDs, so listen for the window close event
		main_window.addWindowListener(new java.awt.event.WindowAdapter() {
			@Override
			public void windowClosing(java.awt.event.WindowEvent windowEvent) {
				windowContent.save_PID_select_state();
				System.exit(0);
			}
		});
		
	}
	
	public static void main(String[] args) {
		//some parts of this code are borrowed from the java tutorials:
		//ex.: https://docs.oracle.com/javase/tutorial/displayCode.html?code=https://docs.oracle.com/javase/tutorial/uiswing/examples/components/MenuLookDemoProject/src/components/MenuLookDemo.java
		
		/*
		//set the look and feel of the program to that of the operating system it is open on
		try {
			// Set System L&F
			UIManager.setLookAndFeel(UIManager.getSystemLookAndFeelClassName());
		} 
		catch (UnsupportedLookAndFeelException e) {
			// handle exception
		}
		catch (ClassNotFoundException e) {
			// handle exception
		}
		catch (InstantiationException e) {
			// handle exception
		}
		catch (IllegalAccessException e) {
			// handle exception
		}
		*/
		
		
		//Schedule a job for the event-dispatching thread:
        //creating and showing this application's GUI.
        javax.swing.SwingUtilities.invokeLater(new Runnable() {
            public void run() {
                createAndShowGUI();
            }
        });
        
        
		//use system out calls to print diagnostic information
		System.out.println("Welcome to Daft OBD");
		
		

	}

}
