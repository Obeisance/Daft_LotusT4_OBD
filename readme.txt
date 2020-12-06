This is a very daft implementation of an OBD logger.

Using the ISO-9141 OBD-II protocol and a VAG COM cable
(VAG COM cable, KKL 409.1, having the FT232RL chip by FTDI)

This program was written in order to interface with
a 2005 lotus elise (with the T4 ECU), however a different
definition file may be used in order to interface with
a variety of cars.

Within the program there are some simple controls including
a check list of loggable parameters. 

The program is a work in progress.

This program uses 32-bit Java 8. It also uses a Java D2XX 
library for the FTDI cable: "JavaFTD2XX-0.2.6.jar"
Embarrassingly, I cannot figure out where I got this library from.


Download the FTDI drivers (for the VAG com cable) at:
http://www.ftdichip.com/Drivers/D2XX.htm

In order to use the pre-compiled version of Daft OBD
download the following files and place them in the same folder
on your computer:

DaftOBD.jar

also download the FTDI drivers as a setup executable, unzip and
install them. This should enable the OBD program to work.

When the program first runs, it requires that you select a save location
for data logs that the program generates. It also requires you
to select a valid definition file which outlines PIDs which
are able to be polled. After choosing a definition file, restart the
program (or click on the top of the PID tree). 
An example definition file is included: 
"2005 Elise OBD parameter definition.txt"

When selecting an OBD cable to use, the program is a bit buggy if 
the cable is not plugged in before the program is started.

Note: some of the 'special use' definition file function is enabled
by customization of the ROM on the vehicle (read function in
both bootloader and runtime, etc.) See other repositories for the
customized ROM images.

find my threads on Lotustalk.com to contact me:
https://www.lotustalk.com/threads/daft-disassembly.352193/
https://www.lotustalk.com/threads/notes-about-2005-elise-fueling-and-timing-control.463664/

Troubleshooting: 
1) Program seems to run, but no communication occurs
I find that if I update to a newer version of Java
there is trouble making the program properly use the FTDI library.
In order to overcome this, install the Java 8 runtime environment
and create a shortcut that points to the the 'Javaw.exe' program for
Java 8. The shortcut must dictate the '-jar' command and must also point
to the DaftOBD program.

Here's an example shortcut target from my computer:
"C:\Program Files (x86)\Java\jre1.8.0_231\bin\javaw.exe" -jar "C:\Users\David\Documents\Elise\Elise ECU assembly Code\DaftOBD\DaftOBD.jar"

