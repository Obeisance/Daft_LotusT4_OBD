import java.io.File;
import java.io.IOException;

import javax.swing.JFrame;

import daftOBD.main_window_content;

public class DaftProgramWindow {

	
	public static void createAndShowGUI() {
		//a frame to put our user interface into
		JFrame main_window = new JFrame("Daft OBD v2.08a");
		main_window.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);//set what the window does when closed
		
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
		//draw the window to size
		main_window.pack();
		main_window.setVisible(true);//and then set it to visible
		
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
