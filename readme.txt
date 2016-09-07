This is a very daft implementation of an OBD logger.

Using the ISO-9141 OBD-II protocol and a VAG COM cable
similar to:
https://www.amazon.com/Obdii-Diagnostic-Adapter-Converter-Cable/dp/B00M3TZT02/184-8304260-2868605?ie=UTF8&keywords=ftdi%20odb&qid=1354729983&ref_=sr_1_1&sr=8-1

This program was written in order to interface with
a 2005 lotus elise (with the T4 ECU). The lower OBD
modes and PIDs may also work with other cars, though.

Within the program there are some simple controls including
a check list of loggable parameters. The check box
can be switched between 'do not log', 'log' or 
'log and plot'. Some parameters cannot be plotted, though.

The program is a work in progress.

This program uses the windows API and the FTDI API
Download the FTDI drivers at:
http://www.ftdichip.com/Drivers/D2XX.htm

In order to use the pre-compiled version of Daft OBD
download the following files and places them in the same folder
on your computer:

DaftOBD.exe
libgcc_s_dw2-1.dll
libstdc++-6.dll

also download the FTDI drivers as a setup executable, unzip and
install them. This should enable the OBD program to work.