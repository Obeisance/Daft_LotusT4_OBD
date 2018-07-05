package daftOBD;

import java.util.Arrays;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;

import com.ftdi.BitModes;
import com.ftdi.FTD2XXException;
import com.ftdi.FTDevice;
import com.ftdi.Parity;
import com.ftdi.StopBits;
import com.ftdi.WordLength;

public class serial_USB_comm {
	FTDevice FTDI_cable;//we'll use an FTDI USB-to-ttl chip to run our communications
	int baudRate = 10400;//we'll keep track of the baud rate that the device is set at
	int stopBits = 1;
	String parityType = "";
	int dataBits = 8;
	java.util.List<FTDevice> ftdiList;
	boolean ready_for_next_command = true;
	
	public serial_USB_comm() {
		//create the class -> choose the first FTDI device we find for now
		try {
			ftdiList = FTDevice.getDevices();
			if(!ftdiList.isEmpty())
			{
				this.FTDI_cable = ftdiList.get(0);
				this.FTDI_cable.open();
			}
		} catch (FTD2XXException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}

	}
	
	public String[] getDeviceStringList() {
		//create the class -> choose the first FTDI device we find for now
		String[] devList = new String[0];
		if(!ftdiList.isEmpty())
		{
			for(int i = 0; i < ftdiList.size(); i ++)
			{
				FTDevice item = ftdiList.get(i);
				devList = Arrays.copyOf(devList, devList.length + 1);
				devList[devList.length - 1] = item.toString();
			}
		}
		return devList;
	}
	
	public void setDevice(int i) {
		this.FTDI_cable = ftdiList.get(i);
	}
	
	public void close_USB_comm_device() {
		//close the device interface/com channel
		if(this.FTDI_cable != null)
		{
			if(this.FTDI_cable.isOpened())
			{
				try {
					this.FTDI_cable.resetDevice();
					this.FTDI_cable.close();
					this.ready_for_next_command = false;
				} catch (FTD2XXException e) {
					// TODO Auto-generated catch block
					e.printStackTrace();
				}
			}
		}
	}
	
	
	public void updateBaudRate(int newBaud) {
		//if the current baud rate and newBaud match, don't change anything
		while(!this.ready_for_next_command)
		{
			//we're not ready for a new command -> infinite loop
		}
		
		if(this.baudRate != newBaud)
		{
			/*
			//reset the device in order to change baud rate
			if(this.FTDI_cable.isOpened())
			{
				try {
					this.FTDI_cable.purgeBuffer(true, true);//clear both rx and tx buffers
					this.FTDI_cable.resetDevice();
				} catch (FTD2XXException e1) {
					// TODO Auto-generated catch block
					e1.printStackTrace();
				}
			}
			
			
			//close the device interface/com channel
			if(this.FTDI_cable.isOpened())
			{
				try {
					this.FTDI_cable.close();
					updateReadiness(false);//once the device is closed, it is not ready to receive a command
				} catch (FTD2XXException e) {
					// TODO Auto-generated catch block
					e.printStackTrace();
				}
			}
			
			//re-open the device so we can send messages
			try {
				this.FTDI_cable.open();
				updateReadiness(true);//now that the device is open, it is ready to receive commands
			} catch (FTD2XXException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
			*/
			
			//change the baud rate
			//System.out.println("Change baud rate to: " + newBaud);
			this.baudRate = newBaud;
			//check to see if we need to be in bitBang mode
			if(newBaud < 300)
			{
				//set the device to synchronous bit-bang mode so we can send bits individually
				try {
					this.FTDI_cable.purgeBuffer(true, true);//clear both rx and tx buffers
					this.FTDI_cable.setBitMode((byte) 0xFF, BitModes.BITMODE_RESET);//reset the device so we can use bitbang mode
					this.FTDI_cable.setBitMode((byte) 0x1, BitModes.BITMODE_SYNC_BITBANG);//flip only the first bit (Tx pin) when sending commands
					this.FTDI_cable.setBaudRate(115200);//set the rate at which we can update the bits
					this.FTDI_cable.setTimeouts(500, 500);//500 ms read timeout, 500 ms write timeout
				} catch (FTD2XXException e) {
					// TODO Auto-generated catch block
					e.printStackTrace();
				}
			}
			else if(newBaud < 1500000)
			{
				try {
					this.FTDI_cable.purgeBuffer(true, true);//clear both rx and tx buffers
					this.FTDI_cable.setBitMode((byte) 0xFF, BitModes.BITMODE_RESET);//reset the device so we can use the regular mode
					this.FTDI_cable.setBaudRate(newBaud);
					WordLength bitLength;
					StopBits numStopBits;
					Parity parityType;
					
					//we'll ignore the possibility for 'mark' or 'space' parity
					if(this.parityType.equals("EVEN"))
					{
						parityType = Parity.PARITY_EVEN;
					}
					else if(this.parityType.equals("ODD"))
					{
						parityType = Parity.PARITY_ODD;
					}
					else
					{
						parityType = Parity.PARITY_NONE;
					}
					
					if(this.stopBits == 2)
					{
						numStopBits = StopBits.STOP_BITS_2;
					}
					else
					{
						numStopBits = StopBits.STOP_BITS_1;
					}
					
					if(this.dataBits == 7)
					{
						bitLength = WordLength.BITS_7;
					}
					else
					{
						bitLength = WordLength.BITS_8;
					}
					this.FTDI_cable.setDataCharacteristics(bitLength, numStopBits, parityType);
					this.FTDI_cable.setTimeouts(500, 500);//500 ms read timeout, 500 ms write timeout
				} catch (FTD2XXException e) {
					// TODO Auto-generated catch block
					e.printStackTrace();
				}
			}
			
		}
	}
	
	public void send(byte[] data) {
		//send the data that is in the data array
		while(!this.ready_for_next_command)
		{
			//go into an infinite loop waiting for readiness
			//System.out.println("Send: Waiting for Com port readiness");
		}
		//we'll want to change our behavior in case we have to bit-bang the data out
		if(this.baudRate < 300)
		{
			//we'll use the bitbang method for this slower baud rate
			int timeDelay_betweenBits = 1000000/this.baudRate;//microsec per bit
			//System.out.println("Bitbang to send " + data.length + " bytes");
			//System.out.println("delay between bits: " + timeDelay_betweenBits + " microsec.");
			
			//build the bit string we'll send
			//begin by deciding if we'll include a parity bit
			int parityBit = 0;
			int numberParityBits = 0;
			if(this.parityType.equals("EVEN"))
			{
				parityBit = 0;
				numberParityBits = 1;
			}
			else if(this.parityType.equals("ODD"))
			{
				parityBit = 1;
				numberParityBits = 1;
			}
			
			
			int byteLength = 1 + this.dataBits + this.stopBits + numberParityBits;//always one start bit
			
			//loop through all bytes that need to be sent, converting them to bit versions of themselves
			//for serial transmission
			for(int i = 0; i < data.length; i++)
			{
				int[] byte_as_bits = new int[byteLength];
				byte_as_bits[0] = 0;//start bit
				
				//data bits
				for(int j = 0; j < this.dataBits; j++)
				{
					int dataBit = ((data[i] >> j) & 0x1);
					byte_as_bits[j+1] = dataBit;
					parityBit = (parityBit^dataBit) & 0x1;//compute the parity bit too
				}
				
				//parity bit
				for(int j = 1 + this.dataBits; j < (1 + this.dataBits + numberParityBits); j++)
				{
					//System.out.println("add parity bit at bit pos: " + j);
					byte_as_bits[j] = parityBit;
				}
				
				//stop bits
				for(int j = (1 + this.dataBits + numberParityBits); j < (1+ this.dataBits + this.stopBits + numberParityBits); j++)
				{
					//System.out.println("add stop bit at bit pos: " + j);
					byte_as_bits[j] = 1;
				}
				
				//System.out.println("bit length for bitbang: " + byte_as_bits.length);
				
				//then create a set of delayed execution tasks to send this byte, bit-by-bit
				ScheduledExecutorService scheduler = Executors.newScheduledThreadPool(1);
				for(int j = 0; j < byteLength; j++)
				{
					//System.out.println("Schedule: " + byte_as_bits[j]);
					scheduler.schedule(new sendBit(this.FTDI_cable, byte_as_bits[j],(byteLength - j),this), j*timeDelay_betweenBits, TimeUnit.MICROSECONDS);
				}
				//System.out.println("Done scheduling");
			}
		}
		else
		{
			//otherwise we can simply send data
			try {
				FTDI_cable.write(data);
			} catch (FTD2XXException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
			
			//since the VAG COM cable shares the TX and RX lines on one pin, we should read in our
			try {
				//byte[] readBackSentData = FTDI_cable.read(data.length);
				FTDI_cable.read(data.length);
			} catch (FTD2XXException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
		}
	}
	
	public byte[] read(int numBytesToRead)
	{
		while(!this.ready_for_next_command)
		{
			//wait until we're ready to read
		}
		
		byte[] readBytes = new byte[0];
		if(this.ready_for_next_command)
		{
			//if we have a low baud rate, we'll need to use the bit-bang mode to read in the data
			if(this.baudRate < 300)
			{
				//we'll use the bitbang method for this slower baud rate
				int timeDelay_betweenBits = 1000000/this.baudRate;//microsec per bit

				//build the bit string we'll send
				//begin by deciding if we'll include a parity bit
				int parityBit = 0;
				int numberParityBits = 0;
				if(this.parityType.equals("EVEN"))
				{
					parityBit = 0;
					numberParityBits = 1;
				}
				else if(this.parityType.equals("ODD"))
				{
					parityBit = 1;
					numberParityBits = 1;
				}

				int byteLength = this.dataBits;// + this.stopBits + numberParityBits;//always one start bit, but we'll read it in separate
				//and I'm being lazy about reading through to the stop bits and parity bits

				updateReadiness(false);//while waiting to read in bytes, we are not ready to receive more commands
				for(int i = 0; i < numBytesToRead; i++)
				{
					//loop and hold up the program until we read in the start bit -> this is clunky
					boolean finished = false;
					while(!finished)
					{
						try {
							int readByte = FTDI_cable.read();
							finished = (readByte & 0x1) == 0x0;//check the state of the first bit
							//System.out.println(readByte);
						} catch (FTD2XXException e) {
							// TODO Auto-generated catch block
							e.printStackTrace();
						}
					}
					//then schedule the bit-by-bit read in of the data 
					ScheduledExecutorService scheduler = Executors.newScheduledThreadPool(1);
					readBit buildTheByte = new readBit(this.FTDI_cable);
					for(int j = 1; j <= byteLength; j++)
					{
						scheduler.schedule(buildTheByte, j*timeDelay_betweenBits, TimeUnit.MICROSECONDS);
					}

					//now wait until the byte has been read in -> this is clunky
					while(buildTheByte.bits_read < byteLength)
					{
						//wait
					}
					readBytes = Arrays.copyOf(readBytes, readBytes.length + 1);
					readBytes[readBytes.length-1] = (byte) buildTheByte.data_byte;
				}
				updateReadiness(true);
			}
			else
			{
				//we're running normally and can simply read in the data
				try {
					readBytes = FTDI_cable.read(numBytesToRead);
				} catch (FTD2XXException e) {
					// TODO Auto-generated catch block
					e.printStackTrace();
				}
			}
		}
		return readBytes;
	}
	
	public void updateReadiness(boolean newState)
	{
		//this function is used to update the readiness state of the Com Port
		this.ready_for_next_command = newState;
	}
}

class sendBit implements Runnable {
	FTDevice ftdi_device;
	int bit = 0;
	int bitsLeft = 2;
	serial_USB_comm parentPort;
	
	public sendBit(FTDevice device, int bit_to_send, int bits_left_to_send, serial_USB_comm parent)
	{
		this.ftdi_device = device;
		this.bit = bit_to_send;
		this.bitsLeft = bits_left_to_send;
		this.parentPort = parent;
	}

	public void run() {
		//send the bit
		try {
			//this write command will output to all 8 pins 
			//of the FTDI device, depending on the written 
			//byte (int) and mask we set when configuring the bit bang mode
			//System.out.println("Send bit: " + bit);
			this.ftdi_device.write(bit);
		} catch (FTD2XXException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		this.parentPort.updateReadiness(this.bitsLeft - 1 <= 0);
	}	
}

class readBit implements Runnable {
	FTDevice ftdi_device;
	int data_byte = 0;
	int bits_read = 0;
	
	public readBit(FTDevice device)
	{
		this.ftdi_device = device;
	}

	public void run() {
		//send the bit
		try {
			//this read command will output to all 8 pins 
			//of the FTDI device, depending on the mask 
			//we set when configuring the bit bang mode
			int bit = this.ftdi_device.read();
			this.data_byte = (this.data_byte) | (bit & 0x1) << bits_read;
			this.bits_read += 1;
		} catch (FTD2XXException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
	}
}