package daftOBD;

import java.awt.Color;
import java.awt.Component;
import java.awt.GridBagConstraints;
import java.awt.GridBagLayout;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.KeyEvent;
import java.awt.event.KeyListener;

import javax.swing.JCheckBox;
import javax.swing.JPanel;
import javax.swing.JTextArea;
import javax.swing.JTextField;
import javax.swing.JTree;
import javax.swing.text.BadLocationException;
import javax.swing.text.DefaultHighlighter;
import javax.swing.text.Highlighter;
import javax.swing.text.Highlighter.HighlightPainter;
import javax.swing.tree.DefaultMutableTreeNode;
import javax.swing.tree.DefaultTreeCellRenderer;
import javax.swing.tree.TreeCellRenderer;

public class DaftOBDSelectionObjectCellRenderer implements TreeCellRenderer {

	JCheckBox parameterSelectCheckBox = new JCheckBox(" ");

	JTextField sendDataToOBD = new JTextField("");

	JPanel renderer = new JPanel();
	
	boolean trackSelected = false;

	DefaultTreeCellRenderer defaultRenderer = new DefaultTreeCellRenderer();

	public DaftOBDSelectionObjectCellRenderer() {
		
		//add the components to the renderer JPanel
		renderer.setLayout(new GridBagLayout());
		GridBagConstraints c = new GridBagConstraints();//use a GridBagLayout to put the components into a row
		c.gridx = 0;
		c.gridy = 0;
		renderer.add(parameterSelectCheckBox,c);
		c.gridx = 1;
		c.gridy = 0;
		renderer.add(sendDataToOBD,c);
		
	}

	public boolean isFocusable() {
		return true;
	}

	public Component getTreeCellRendererComponent(JTree tree, Object value, boolean selected,
			boolean expanded, boolean leaf, int row, boolean hasFocus) {
		Component returnValue = null;
		if ((value != null) && (value instanceof DefaultMutableTreeNode)) {
			//get the object that is being being rendered
			Object userObject = ((DefaultMutableTreeNode) value).getUserObject();

			//if the object is a DaftOBDSelectionObject, we're going to make it what we want
			if (userObject instanceof DaftOBDSelectionObject) {
				//update the text to render for this object
				DaftOBDSelectionObject e = (DaftOBDSelectionObject) userObject;
				//renderer = e.getDaftPanel();
				
				parameterSelectCheckBox.setText(e.parameterID);
				sendDataToOBD.setText(e.dataToSend[0]);
				
				/*
				if(e.numberOfInputBoxes == 0)
				{
					Component[] objects = renderer.getComponents();
					for(int i = 0; i < objects.length; i++)
					{
						System.out.println(objects[i].toString());
						if(objects[i] instanceof JTextField)
						{
							renderer.remove(objects[i]);
						}
					}
				}
				*/
				
				if (selected) {
					//adjust the check box state flag
					if(hasFocus)
					{
						e.isSelected = !e.isSelected;
					}


					//let's highlight the 'data to send' text to show that it is editable
					Highlighter highlighter = sendDataToOBD.getHighlighter();
					HighlightPainter painter = new DefaultHighlighter.DefaultHighlightPainter(Color.yellow);
					String text = sendDataToOBD.getText();
					int p1 = text.length();
					//then try to add the highlight
					try {
						highlighter.addHighlight(0, p1, painter );
					} catch (BadLocationException e1) {
						// in case we have trouble, print the stack
						e1.printStackTrace();
					}
					
					
				} else {
					//should we do something special to the unselected tree leafs?
				}
				//update the check box state based on its flag
				parameterSelectCheckBox.setSelected(e.isSelected);
				
				renderer.setEnabled(tree.isEnabled());
				returnValue = renderer;
			}
		}

		//if we have somehow not managed to set the renderer to a DaftOBDSelectionObject
		//then we'll treat it like a normal DefaultTreeCellRenderer
		if (returnValue == null) {
			returnValue = defaultRenderer.getTreeCellRendererComponent(tree, value, selected, expanded,
					leaf, row, hasFocus);
		}

		//finally return the value that we've prepared
		return returnValue;
	}


	
}
