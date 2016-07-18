//============================================================================
// Name        : DaftOBD.cpp
// Author      : Obeisance
// Version     :
// Copyright   : Free to use for all
// Description : An attempt to make an OBD II logger
//============================================================================
//
//I took code from: https://www.youtube.com/watch?v=RT_MGEcnHg4

#include <iostream>
#include <fstream>
#include <Unknwn.h>
#include <windows.h>
#include "ftd2xx.h"//included to directly drive the FTDI com port device
#include <string>
#include <sstream>
#include <objidl.h>
#include <gdiplus.h>
#include <time.h>
#include <stdlib.h>
using namespace Gdiplus;
//using namespace std;

VOID OnPaint(HDC hdc)
{
   Graphics graphics(hdc);
   Pen      pen(Color(255, 0, 0, 255));
   graphics.DrawLine(&pen, 0, 0, 200, 100);
}

/* declare Windows procedure */
LRESULT CALLBACK WindowProcedure (HWND, UINT, WPARAM, LPARAM);

//function list
void drawLogParamsTable(HWND parent, int parentWidth, int parentHeight, int scrollPosH, int scrollPosY);//make the list of loggable parameters
void clearLogParamsTable();//clear the list of loggable parameters (I create and destroy this list in order to scroll the list)
void saveLogCheckBoxSettings();//the loggable parameter checkbox settings are saved
void restoreLogCheckBoxSettings();//the loggable parameter checkbox settings are restored when the list is drawn
void updatePlot(HWND parent, int parentWidth, int parentHeight);//draw and update the data plot
void checkForAndPopulateComPortList(HWND comPortList);//enumerate the list of available com ports
bool serialPollingCycle(HANDLE serialPortHandle);//perform the serial message exchange
int chars_to_int(char* number);//converts a char array to an integer
void writeToSerialPort(HANDLE serialPortHandle, byte* bytesToSend, int numberOfBytesToSend);
int readFromSerialPort(HANDLE serialPortHandle, byte* serialBytesRecvd, int sizeOfSerialByteBuffer, int delayMS);
HANDLE sendOBDInit(HANDLE serialPortHandle);
HANDLE setupComPort();
void populateFileHeaders(std::ofstream &dataFile, std::ofstream &byteFile);
std::string decodeDTC(byte firstByte, byte secondByte);

//global variables for window handles
HWND selectLogParams, logParamsTitle, logParamsScrollH, logParamsScrollV, logbutton, continuousButton, plotwindow, plot, plotParam, indicateBaudRate, baudrateSelect,mode1,mode2,mode3,mode4,mode5,mode6,mode7,mode8,mode9,mode22,mode2F,mode3B,modeSHORT;
HWND comSelect;
HANDLE serialPort;

//set client relative window positions
int plotWindowX = 220;
int plotWindowY = 35;
int logParamsX = 5;
int logParamsY = 35;
int logParamsWidth = 0;
int logParamsHeight = 0;
int logParamsScrollPosX = 0;
int logParamsScrollPosY = 0;

//for the com port select
int ItemIndex = 0;
TCHAR  ListItem[256];//holds the COM port parameter which is selected when the log button is pressed

//for the logging
bool logButtonToggle = false;
bool continuousLogging = false;
char baudRateStore[5] = {'0','0','0','0','0'};
int attemptsAtInit = 0;

//global variables of files to save data logs
std::ofstream writeBytes;
std::ofstream writeData;

//global timer variables for data logging
//helped from: http://stackoverflow.com/questions/2150291/how-do-i-measure-a-time-interval-in-c
LARGE_INTEGER systemFrequency;        // ticks per second
LARGE_INTEGER startTime, currentTime;           // ticks
double elapsedLoggerTime;

//for the scroll bars
SCROLLINFO si;

//OBD MODE 1 PID buttons, 28 PIDs
HWND OBD1PID0,OBD1PID1,OBD1PID2,OBD1PID3,OBD1PID4,OBD1PID5,OBD1PID6,OBD1PID7,OBD1PIDC,OBD1PIDD,OBD1PIDE,OBD1PIDF,OBD1PID10,OBD1PID11,OBD1PID13,OBD1PID14,OBD1PID15,OBD1PID1C,OBD1PID1F,OBD1PID20,OBD1PID21,OBD1PID2E,OBD1PID2F,OBD1PID33,OBD1PID40,OBD1PID42,OBD1PID43,OBD1PID45;
int mode1checkboxstate[28] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

//OBD MODE 2 PID buttons
HWND OBD2PID0,OBD2PID2,OBD2PID3,OBD2PID4,OBD2PID5,OBD2PID6,OBD2PID7,OBD2PID12,OBD2PID13,OBD2PID16,OBD2PID17;
int mode2checkboxstate[11] = {0,0,0,0,0,0,0,0,0,0,0};

//OBD MODE 3 PID buttons
HWND OBD3PID0;
int mode3checkboxstate = 0;

//OBD MODE 4 PID buttons
HWND OBD4PID0;
int mode4checkboxstate = 0;

//OBD MODE 5 PID buttons
HWND OBD5PID0,OBD5PID1,OBD5PID2,OBD5PID3,OBD5PID4,OBD5PID5,OBD5PID6,OBD5PID7,OBD5PID8;
int mode5checkboxstate[9] = {0,0,0,0,0,0,0,0,0};

//OBD MODE 6 PID buttons
HWND OBD6PID0,OBD6PID1,OBD6PID2,OBD6PID3,OBD6PID4,OBD6PID5;
int mode6checkboxstate[6] = {0,0,0,0,0,0};

//OBD MODE 7 PID buttons
HWND OBD7PID0;
int mode7checkboxstate = 0;

//OBD MODE 8 PID buttons
HWND OBD8PID0,OBD8PID1;
int mode8checkboxstate[2] = {0,0};

//OBD MODE 9 PID buttons
HWND OBD9PID0,OBD9PID1,OBD9PID2,OBD9PID3,OBD9PID4,OBD9PID5,OBD9PID6;
int mode9checkboxstate[7] = {0,0,0,0,0,0,0};

//OBD MODE 0x22 PID buttons, 82 PIDs
HWND OBD22PID512,OBD22PID513,OBD22PID515,OBD22PID516,OBD22PID517,OBD22PID518,OBD22PID519,OBD22PID520,OBD22PID521,OBD22PID522,OBD22PID523,OBD22PID524,OBD22PID525,OBD22PID526to529,OBD22PID530,OBD22PID531,OBD22PID532,OBD22PID533,OBD22PID534,OBD22PID536,OBD22PID537,OBD22PID538,OBD22PID539,OBD22PID540to543,OBD22PID544,OBD22PID545to548,OBD22PID549,OBD22PID550,OBD22PID551,OBD22PID552,OBD22PID553,OBD22PID554,OBD22PID555,OBD22PID556,OBD22PID557,OBD22PID558,OBD22PID559,OBD22PID560,OBD22PID561,OBD22PID562,OBD22PID563,OBD22PID564,OBD22PID565,OBD22PID566,OBD22PID567,OBD22PID568,OBD22PID569,OBD22PID570,OBD22PID571,OBD22PID572,OBD22PID573,OBD22PID577,OBD22PID583,OBD22PID584,OBD22PID585,OBD22PID586,OBD22PID591,OBD22PID592,OBD22PID593,OBD22PID594,OBD22PID595,OBD22PID596,OBD22PID598,OBD22PID599,OBD22PID601,OBD22PID602,OBD22PID603,OBD22PID605,OBD22PID606,OBD22PID608,OBD22PID609,OBD22PID610,OBD22PID612,OBD22PID613,OBD22PID614,OBD22PID615,OBD22PID616,OBD22PID617,OBD22PID618,OBD22PID619,OBD22PID620,OBD22PID621;
int mode22checkboxstate[82] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 ,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 ,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 ,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 ,0,0};

//OBD MODE 0x2F PID buttons, 28 PIDs
HWND OBD2FPID100,OBD2FPID101,OBD2FPID102,OBD2FPID103,OBD2FPID104,OBD2FPID120,OBD2FPID121,OBD2FPID122,OBD2FPID125,OBD2FPID126,OBD2FPID127,OBD2FPID12A,OBD2FPID140,OBD2FPID141,OBD2FPID142,OBD2FPID143,OBD2FPID144,OBD2FPID146,OBD2FPID147,OBD2FPID148,OBD2FPID149,OBD2FPID14A,OBD2FPID14B,OBD2FPID160,OBD2FPID161,OBD2FPID162,OBD2FPID163,OBD2FPID164;
int mode2Fcheckboxstate[28] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

//OBD MODE 0x3B PID buttons
HWND OBD3BPID0,OBD3BPID1;
int mode3Bcheckboxstate = 0;
char mode3BVIN[18] = {'S','C','C','P','C','1','1','1','0','5','H','A','3','0','0','0','0'};

//OBD MODE short message PID buttons
HWND OBDSHORTPID2,OBDSHORTPID4;
int modeSHORTcheckboxstate[2] = {0,0};


/* Make the class name into a global variable */
char szClassName[ ] = "CodeBlocksWindowApp";

int WINAPI WinMain (HINSTANCE hThisInstance, HINSTANCE hPrevInstance, LPSTR lpszArgument, int nCmdShow)
{
 	HWND hwnd;		//this is the handle for our window
	MSG messages;	//here messages to the application window are saved
	WNDCLASSEX wincl;	//data strcuture for the windowclass
	GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR           gdiplusToken;

	// Initialize GDI+.
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

	/* the window structure */
	wincl.hInstance = hThisInstance;
	wincl.lpszClassName = szClassName;
	wincl.lpfnWndProc = WindowProcedure; //this function is called by windows
	wincl.style = CS_DBLCLKS;	//catch double-clicks
	wincl.cbSize = sizeof(WNDCLASSEX);

	/*use default icon and mouse pointer*/
	wincl.hIcon = LoadIcon (NULL, IDI_APPLICATION);
	wincl.hIconSm = LoadIcon (NULL,IDI_APPLICATION);
	wincl.hCursor = LoadCursor (NULL,IDC_ARROW);
	wincl.lpszMenuName = NULL;	//no menu
	wincl.cbClsExtra = 0;	//no extra bytes after the window class
	wincl.cbWndExtra = 0;	//structure of the window instance
	/*use window's default color as the background of the window*/
	wincl.hbrBackground = (HBRUSH) COLOR_BACKGROUND;

	/* register the windows class, and if it fails quit the program*/
	if(!RegisterClassEx (&wincl))
		return 0;

	/*the class is registered, lets create the program*/
	hwnd = CreateWindowEx(0,szClassName,"Daft OBD-II Logger",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,644,395,HWND_DESKTOP,NULL,hThisInstance,NULL);
			//extended possibilities for variation
			//classname
			//title
			//default window
			//windows decides the position
			//where the window ends up on the screen
			//the program's width
			//the program's height in pixels
			//the window is a child window to the desktop
			//no menu
			//program instance handler
			//no window creation data

	//make the window visible on the screen
	ShowWindow(hwnd,nCmdShow);

	//run the message loop, it will run until GetMessage() returns 0
	while(GetMessage(&messages, NULL, 0, 0))
	{
		//translate the virtual key messages into character messages
		TranslateMessage(&messages);
		//send message to WindowProcedure
		DispatchMessage(&messages);

		//if we should log, perform the cycle
		if(logButtonToggle)
		{
			serialPollingCycle(serialPort);//if the logging button is pushed, the handle 'serialPort' should be initialized

			//un-toggle the log button unless we are supposed to continuously log
			if(!continuousLogging)
			{
				logButtonToggle = false;

				//close the log files
				writeBytes.close();
				writeData.close();
				//close the COM port
				CloseHandle(serialPort);
			}
		}
	}
	GdiplusShutdown(gdiplusToken);
	//the program return value is 0 - the value that PostQuitMessage() gave
	return messages.wParam;
}

//this function is called by the windows function dispatchmessage()
LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	HDC          hdc;
	PAINTSTRUCT  ps;

	//cout << LOWORD (wParam) << ", " << HIWORD (wParam) << '\n';
	switch(message)
	{
		case WM_CREATE:	//this case occurs when the window is created
			//during this case, we draw all of the objects in our window

			//create the list of loggable parameters
			//selectLogParams = CreateWindowEx(0,"STATIC", "Select the parameters to log:",WS_VISIBLE | WS_CHILD | WS_BORDER,logParamsX,logParamsY,210,(HIWORD (lParam))-(20+logParamsY),hwnd,NULL,NULL,NULL);
			//drawLogParamsTable(selectLogParams, 210,(HIWORD (lParam))-(20+logParamsY), logParamsScrollPosX, logParamsScrollPosY);


			//prepare to plot some of the logged parameters
			plotwindow = CreateWindowEx(0,"STATIC", "Data Plot",WS_VISIBLE | WS_CHILD| WS_BORDER,plotWindowX,plotWindowY,80,18,hwnd,NULL,NULL,NULL);

			//allow user to set the baud rate for OBD serial comms
			indicateBaudRate = CreateWindow("STATIC", "Baud Rate:",WS_VISIBLE | WS_CHILD|SS_CENTER,285,8,80,19,hwnd,NULL,NULL,NULL);
			baudrateSelect = CreateWindow("EDIT", "10921",WS_VISIBLE | WS_CHILD| WS_BORDER,370,8,80,20,hwnd,NULL,NULL,NULL);

			//begin logging button
			logbutton = CreateWindow("BUTTON", "Begin Logging",WS_VISIBLE | WS_CHILD| WS_BORDER|BS_PUSHBUTTON,5,5,100,25,hwnd,(HMENU) 1,NULL,NULL);
			continuousButton = CreateWindow("BUTTON", "Continuous Logging",WS_VISIBLE | WS_CHILD| BS_AUTOCHECKBOX,110,8,150,19,hwnd,(HMENU) 2,NULL,NULL);
			CheckDlgButton(hwnd,2,BST_UNCHECKED);

			//initiate com port button
			//create the combo box dropdown list
			comSelect = CreateWindow("COMBOBOX", TEXT(""), WS_VISIBLE | WS_CHILD| WS_BORDER | CBS_DROPDOWNLIST,480,5,100,200,hwnd,(HMENU) 3,NULL,NULL);
			checkForAndPopulateComPortList(comSelect);//populate the com port dropdown list
			SendMessage(comSelect, CB_SETCURSEL, (WPARAM)0, (LPARAM)0);//set the cursor to the 0th item on the list (WPARAM)
			SendMessage(comSelect, CB_SETITEMHEIGHT, (WPARAM)0, (LPARAM)18);//set the height of the dropdown list items


			break;
		case WM_HSCROLL: //this case occurs when the horizontal window is scrolled
			//cout << "HSCROLL" << '\n';
			// Get all the horizontal scroll bar information.
		        si.cbSize = sizeof (si);
		        si.fMask  = SIF_ALL;

		        // Save the position for comparison later on.
		        GetScrollInfo (hwnd, SB_HORZ, &si);
		        logParamsScrollPosX = si.nPos;

		        switch (LOWORD (wParam))
		        {
		        // User clicked the left arrow.
		        case SB_LINELEFT:
		            si.nPos -= 1;
		            break;

		        // User clicked the right arrow.
		        case SB_LINERIGHT:
		            si.nPos += 1;
		            break;

		        // User clicked the scroll bar shaft left of the scroll box.
		        case SB_PAGELEFT:
		            si.nPos -= si.nPage;
		            break;

		        // User clicked the scroll bar shaft right of the scroll box.
		        case SB_PAGERIGHT:
		            si.nPos += si.nPage;
		            break;

		        // User dragged the scroll box.
		        case SB_THUMBTRACK:
		        	si.nPos = HIWORD(wParam);
		            break;

		        default :
		            break;
		        }
		        // Set the position and then retrieve it.  Due to adjustments
		        // by Windows it may not be the same as the value set.
		        clearLogParamsTable();
		        si.nTrackPos = si.nPos; //update the saved position of the trackbar
		        si.fMask = SIF_POS;
		        SetScrollInfo (logParamsScrollH, SB_CTL, &si, TRUE);
		        GetScrollInfo (logParamsScrollH, SB_CTL, &si);
		        logParamsScrollPosX = si.nPos;
		        drawLogParamsTable(selectLogParams, 210,(HIWORD (lParam))-(20+logParamsY), 18 * (logParamsScrollPosX), logParamsScrollPosY);

		        break;

		case WM_VSCROLL: //this case occurs when the vertical window is scrolled
		        // Get all the vertical scroll bar information.
		        si.cbSize = sizeof (si);
		        si.fMask  = SIF_ALL;
		        GetScrollInfo (hwnd, SB_VERT, &si);

		        switch (LOWORD (wParam))
		        {
		        // User clicked the HOME keyboard key.
		        case SB_TOP:
		            si.nPos = si.nMin;
		            break;

		        // User clicked the END keyboard key.
		        case SB_BOTTOM:
		            si.nPos = si.nMax;
		            break;

		        // User clicked the top arrow.
		        case SB_LINEUP:
		            si.nPos -= 10;
		            break;

		        // User clicked the bottom arrow.
		        case SB_LINEDOWN:
		            si.nPos += 10;
		            break;

		        // User clicked the scroll bar shaft above the scroll box.
		        case SB_PAGEUP:
		            si.nPos -= si.nPage;
		            break;

		        // User clicked the scroll bar shaft below the scroll box.
		        case SB_PAGEDOWN:
		            si.nPos += si.nPage;
		            break;

		        // User dragged the scroll box.
		        case SB_THUMBTRACK:
		            si.nPos = HIWORD(wParam);
		            break;

		        default:
		            break;
		        }

		        // Set the position and then retrieve it.  Due to adjustments
		        // by Windows it may not be the same as the value set.
		        clearLogParamsTable();
		        si.nTrackPos = si.nPos; //update the saved position of the trackbar
		        si.fMask = SIF_POS;
		        SetScrollInfo (logParamsScrollV, SB_CTL, &si, TRUE);
		        GetScrollInfo (logParamsScrollV, SB_CTL, &si);
		        logParamsScrollPosY = si.nPos;
		        drawLogParamsTable(selectLogParams, 210,(HIWORD (lParam))-(20+logParamsY), 18 * (logParamsScrollPosX), logParamsScrollPosY);


		        break;

		case WM_SIZE:
			//output the window dimensions
			//cout << "case WM_SIZE" << HIWORD (lParam) << "," << LOWORD (lParam) << '\n';

			//redraw some of the sub windows if the main window is resized:
			DestroyWindow(plotwindow);
			DestroyWindow(selectLogParams);
			DestroyWindow(logParamsScrollH);
			DestroyWindow(logParamsScrollV);
			plotwindow = CreateWindowEx(0,"STATIC", "Data Plot",WS_VISIBLE | WS_CHILD| WS_BORDER,plotWindowX,plotWindowY,80,18,hwnd,NULL,NULL,NULL);
			//updatePlot(plotwindow,(LOWORD (lParam))-(20+plotWindowX),(HIWORD (lParam))-(20+plotWindowY));

			logParamsWidth = 210;
			logParamsHeight = (HIWORD (lParam))-(20+logParamsY);
			logParamsTitle = CreateWindowEx(0,"STATIC", "Select Log/Plot Parameters",WS_VISIBLE | WS_CHILD | WS_BORDER | SS_CENTER ,logParamsX,logParamsY,logParamsWidth-20,20,hwnd,NULL,NULL,NULL);
			selectLogParams = CreateWindowEx(0,"STATIC", "",WS_VISIBLE | WS_CHILD | WS_BORDER,logParamsX,logParamsY+20,logParamsWidth-20,logParamsHeight - 40,hwnd,NULL,NULL,NULL);
			//si.nTrackPos = logParamsScrollPosX; //set horizontal scroll position
			//SetScrollInfo(logParamsScrollH, SB_HORZ,&si,TRUE);
			logParamsScrollV = CreateWindowEx(0,"SCROLLBAR", "Vertical scroll bar", WS_VISIBLE | WS_CHILD | SBS_VERT,logParamsX+logParamsWidth-20,logParamsY,20,logParamsHeight-20,hwnd,NULL,NULL,NULL);
			//si.nPage = logParamsHeight; //set vertical scroll height
			//si.nTrackPos = logParamsScrollPosY; //set vertical scroll position
			//SetScrollInfo(logParamsScrollH, SB_VERT,&si,TRUE);
			logParamsScrollH = CreateWindowEx(0,"SCROLLBAR", "Horizontal scroll bar", WS_VISIBLE | WS_CHILD | SBS_HORZ,logParamsX,logParamsY + logParamsHeight-20,logParamsWidth-20,20,hwnd,NULL,NULL,NULL);
			drawLogParamsTable(selectLogParams, 210,(HIWORD (lParam))-(20+logParamsY), logParamsScrollPosX, logParamsScrollPosY);



			break;

		case WM_COMMAND:
			//cout << LOWORD (wParam) << ", " << HIWORD (wParam) << '\n';
			switch(LOWORD (wParam))
			{
			case 1: //log button pressed
				//std::cout << "log now" << '\n';

				//update the stored com port value
				ItemIndex = SendMessage(comSelect, (UINT) CB_GETCURSEL, (WPARAM) 0, (LPARAM) 0);
				(TCHAR) SendMessage(comSelect, (UINT) CB_GETLBTEXT, (WPARAM) ItemIndex, (LPARAM) ListItem);
				//std::cout << ListItem << '\n';

				//update the stored baud rate setting
				GetWindowText(baudrateSelect,baudRateStore,6);
				//std::cout << baudRateStore << '\n';

				//update the stored logging parameters
				saveLogCheckBoxSettings();

				//update the stored button position
				if(logButtonToggle == true)
				{
					//end logging
					logButtonToggle = false;
					//close the log files
					writeBytes.close();
					writeData.close();
					//close the COM port
					CloseHandle(serialPort);
				}
				else
				{
					logButtonToggle = true;
					attemptsAtInit = 0;//reset our init attempt counter

					//prepare to save bytes and interpreted data
					std::ofstream writeBytes("daftOBD_byte_log.txt", std::ios::app);//append to the file
					std::ofstream writeData("daftOBD_data_log.txt", std::ios::app);//append to the file

					//begin a global timer
					QueryPerformanceFrequency(&systemFrequency);// get ticks per second
					QueryPerformanceCounter(&startTime);// start timer

					//for everything which has a check box, write the header in the files
					populateFileHeaders(writeData, writeBytes);

					//open the serial port
					serialPort = setupComPort();

					//perform the OBD-II ISO-9141-2 slow init handshake sequence
					serialPort = sendOBDInit(serialPort);
				}
				break;
			case 2: //continuous logging checkbox is clicked
				//std::cout << "continuously log toggle" << '\n';
				if(continuousLogging == true)
				{
					continuousLogging = false;
				}
				else
				{
					continuousLogging = true;
				}
				break;
			default:
				break;
			}
			break;
		case WM_PAINT:
		    hdc = BeginPaint(hwnd, &ps);
		    OnPaint(hdc);
		    EndPaint(hwnd, &ps);
		    break;
		case WM_DESTROY: //this case occurs when the "X" close button is pressed
			PostQuitMessage (0);	//send a WM_QUIT to the message queue
			break;
		default:					//for messages that we don't deal with
			return DefWindowProc(hwnd,message,wParam,lParam);
	}
	return 0;
}

void drawLogParamsTable(HWND parent, int parentWidth, int parentHeight, int scrollPosH, int scrollPosV)
{
	int x_offset = 5 - scrollPosH;
	int y_offset = 5 - scrollPosV;

	int listOccupantHeight = 18;
	int mode1Height = 29*listOccupantHeight+5;
	int mode2y_offset = mode1Height;
	int mode2Height = listOccupantHeight*12+5;
	int mode3y_offset = mode1Height+mode2Height;
	int mode3Height = listOccupantHeight*2+5;
	int mode4y_offset = mode3y_offset+mode3Height;
	int mode4Height = listOccupantHeight*2+5;
	int mode5y_offset = mode4y_offset+mode4Height;
	int mode5Height = listOccupantHeight*10+5;
	int mode6y_offset = mode5y_offset+mode5Height;
	int mode6Height = listOccupantHeight*7+5;
	int mode7y_offset = mode6y_offset+mode6Height;
	int mode7Height = listOccupantHeight*2+5;
	int mode8y_offset = mode7y_offset+mode7Height;
	int mode8Height = listOccupantHeight*3+5;
	int mode9y_offset = mode8y_offset+mode8Height;
	int mode9Height = listOccupantHeight*8+5;
	int mode22y_offset = mode9y_offset+mode9Height;
	int mode22Height = listOccupantHeight*83+5;
	int mode2Fy_offset = mode22y_offset+mode22Height;
	int mode2FHeight = listOccupantHeight*29+5;
	int mode3By_offset = mode2Fy_offset+mode2FHeight;
	int mode3BHeight = listOccupantHeight*2+5;
	int modeSHORTy_offset = mode3By_offset+mode3BHeight;
	int modeSHORTHeight = 0;//listOccupantHeight*3+5;


	int parameterTableWidth = 270;
	int parameterTableHeight = modeSHORTy_offset+modeSHORTHeight;

	//if(y_offset > -1*mode1Height)
	{
	//mode 1 parameter table
	mode1 = CreateWindow("BUTTON", "MODE 0x1:Real Time Data",WS_VISIBLE | WS_CHILD|BS_GROUPBOX ,x_offset,y_offset,parameterTableWidth,mode1Height,parent,NULL,NULL,NULL);
	OBD1PID0 = CreateWindow("BUTTON", "PIDs supported 1-20",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,1*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 3,NULL,NULL);
	OBD1PID1 = CreateWindow("BUTTON", "Monitor status since DTC clear",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,2*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 4,NULL,NULL);
	OBD1PID2 = CreateWindow("BUTTON", "Freeze DTC",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,3*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 5,NULL,NULL);
	OBD1PID3 = CreateWindow("BUTTON", "Fuel system status",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,4*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 6,NULL,NULL);
	OBD1PID4 = CreateWindow("BUTTON", "Calc'd engine load",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,5*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 7,NULL,NULL);
	OBD1PID5 = CreateWindow("BUTTON", "Engine coolant temperature",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,6*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 8,NULL,NULL);
	OBD1PID6 = CreateWindow("BUTTON", "Short term fuel trim",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,7*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 9,NULL,NULL);
	OBD1PID7 = CreateWindow("BUTTON", "Long term fuel trim",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,8*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 10,NULL,NULL);
	OBD1PIDC = CreateWindow("BUTTON", "Engine RPM",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,9*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 11,NULL,NULL);
	OBD1PIDD = CreateWindow("BUTTON", "Vehicle speed",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,10*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 12,NULL,NULL);
	OBD1PIDE = CreateWindow("BUTTON", "Timing advance",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,11*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 13,NULL,NULL);
	OBD1PIDF = CreateWindow("BUTTON", "Intake air temperaure",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,12*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 14,NULL,NULL);
	OBD1PID10 = CreateWindow("BUTTON", "Mass airflow rate",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,13*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 15,NULL,NULL);
	OBD1PID11 = CreateWindow("BUTTON", "Throttle position",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,14*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 16,NULL,NULL);
	OBD1PID13 = CreateWindow("BUTTON", "Oxygen sensors present",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,15*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 17,NULL,NULL);
	OBD1PID14 = CreateWindow("BUTTON", "Oxygen sensor 1",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,16*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 18,NULL,NULL);
	OBD1PID15 = CreateWindow("BUTTON", "Oxygen sensor 2",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,17*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 19,NULL,NULL);
	OBD1PID1C = CreateWindow("BUTTON", "OBD standard compliance",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,18*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 20,NULL,NULL);
	OBD1PID1F = CreateWindow("BUTTON", "Run time since engine start",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,19*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 21,NULL,NULL);
	OBD1PID20 = CreateWindow("BUTTON", "PIDs supported 21-40",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,20*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 22,NULL,NULL);
	OBD1PID21 = CreateWindow("BUTTON", "Distance traveled with MIL on",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,21*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 23,NULL,NULL);
	OBD1PID2E = CreateWindow("BUTTON", "Command evaporative purge",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,22*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 24,NULL,NULL);
	OBD1PID2F = CreateWindow("BUTTON", "Fuel tank level input",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,23*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 25,NULL,NULL);
	OBD1PID33 = CreateWindow("BUTTON", "Absolute barometric pressure",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,24*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 26,NULL,NULL);
	OBD1PID40 = CreateWindow("BUTTON", "PIDs supported 41-60",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,25*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 27,NULL,NULL);
	OBD1PID42 = CreateWindow("BUTTON", "Control module voltage",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,26*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 28,NULL,NULL);
	OBD1PID43 = CreateWindow("BUTTON", "Absolute load value",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,27*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 29,NULL,NULL);
	OBD1PID45 = CreateWindow("BUTTON", "Relative throttle position",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,28*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 30,NULL,NULL);
	}
	//if(y_offset+mode2y_offset < parentHeight && y_offset+mode2y_offset+mode2Height > 0)
	{
	mode2 = CreateWindow("BUTTON", "MODE 0x2:Freeze Frame Data",WS_VISIBLE | WS_CHILD|BS_GROUPBOX,x_offset,mode2y_offset+y_offset,parameterTableWidth,mode2Height,parent,NULL,NULL,NULL);
	OBD2PID0 = CreateWindow("BUTTON", "PIDs supported 1-20",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,1*listOccupantHeight,parameterTableWidth-20,18,mode2,(HMENU) 3,NULL,NULL);
	OBD2PID2 = CreateWindow("BUTTON", "DTC that caused freeze frame",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,2*listOccupantHeight,parameterTableWidth-20,18,mode2,(HMENU) 4,NULL,NULL);
	OBD2PID3 = CreateWindow("BUTTON", "Fuel system status",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,3*listOccupantHeight,parameterTableWidth-20,18,mode2,(HMENU) 5,NULL,NULL);
	OBD2PID4 = CreateWindow("BUTTON", "Calc'd engine load",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,4*listOccupantHeight,parameterTableWidth-20,18,mode2,(HMENU) 6,NULL,NULL);
	OBD2PID5 = CreateWindow("BUTTON", "Engine coolant temperature",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,5*listOccupantHeight,parameterTableWidth-20,18,mode2,(HMENU) 7,NULL,NULL);
	OBD2PID6 = CreateWindow("BUTTON", "Short term fuel trim",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,6*listOccupantHeight,parameterTableWidth-20,18,mode2,(HMENU) 8,NULL,NULL);
	OBD2PID7 = CreateWindow("BUTTON", "Long term fuel trim",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,7*listOccupantHeight,parameterTableWidth-20,18,mode2,(HMENU) 9,NULL,NULL);
	OBD2PID12 = CreateWindow("BUTTON", "Engine RPM",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,8*listOccupantHeight,parameterTableWidth-20,18,mode2,(HMENU) 10,NULL,NULL);
	OBD2PID13 = CreateWindow("BUTTON", "Vehicle speed",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,9*listOccupantHeight,parameterTableWidth-20,18,mode2,(HMENU) 11,NULL,NULL);
	OBD2PID16 = CreateWindow("BUTTON", "Mass airflow rate",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,10*listOccupantHeight,parameterTableWidth-20,18,mode2,(HMENU) 12,NULL,NULL);
	OBD2PID17 = CreateWindow("BUTTON", "Throttle position",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,11*listOccupantHeight,parameterTableWidth-20,18,mode2,(HMENU) 13,NULL,NULL);
	}
	//if(y_offset+mode3y_offset < parentHeight && y_offset+mode3y_offset+mode3Height > 0)
	{
	mode3 = CreateWindow("BUTTON", "MODE 0x3:Read DTCs",WS_VISIBLE | WS_CHILD|BS_GROUPBOX,x_offset,mode3y_offset+y_offset,parameterTableWidth,mode3Height,parent,NULL,NULL,NULL);
	OBD3PID0 = CreateWindow("BUTTON", "Read DTCs",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,1*listOccupantHeight,parameterTableWidth-20,18,mode3,(HMENU) 3,NULL,NULL);
	}
	//if(y_offset+mode4y_offset < parentHeight && y_offset+mode4y_offset+mode4Height > 0)
	{
	mode4 = CreateWindow("BUTTON", "MODE 0x4:Clear DTCs",WS_VISIBLE | WS_CHILD|BS_GROUPBOX,x_offset,mode4y_offset+y_offset,parameterTableWidth,mode4Height,parent,NULL,NULL,NULL);
	OBD4PID0 = CreateWindow("BUTTON", "Clear DTCs",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,1*listOccupantHeight,parameterTableWidth-20,18,mode4,(HMENU) 3,NULL,NULL);
	}
	//if(y_offset+mode5y_offset < parentHeight && y_offset+mode5y_offset+mode5Height > 0)
	{
	mode5 = CreateWindow("BUTTON", "MODE 0x5:O2 sensor",WS_VISIBLE | WS_CHILD|BS_GROUPBOX,x_offset,mode5y_offset+y_offset,parameterTableWidth,mode5Height,parent,NULL,NULL,NULL);
	OBD5PID0 = CreateWindow("BUTTON", "OBD monitor IDs supported 1-20",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,1*listOccupantHeight,parameterTableWidth-20,18,mode5,(HMENU) 3,NULL,NULL);
	OBD5PID1 = CreateWindow("BUTTON", "Oxygen sensor 1 bank 1",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,2*listOccupantHeight,parameterTableWidth-20,18,mode5,(HMENU) 4,NULL,NULL);
	OBD5PID2 = CreateWindow("BUTTON", "Oxygen sensor 2 bank 1",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,3*listOccupantHeight,parameterTableWidth-20,18,mode5,(HMENU) 5,NULL,NULL);
	OBD5PID3 = CreateWindow("BUTTON", "Oxygen sensor 3 bank 1",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,4*listOccupantHeight,parameterTableWidth-20,18,mode5,(HMENU) 6,NULL,NULL);
	OBD5PID4 = CreateWindow("BUTTON", "Oxygen sensor 4 bank 1",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,5*listOccupantHeight,parameterTableWidth-20,18,mode5,(HMENU) 7,NULL,NULL);
	OBD5PID5 = CreateWindow("BUTTON", "Oxygen sensor 1 bank 2",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,6*listOccupantHeight,parameterTableWidth-20,18,mode5,(HMENU) 8,NULL,NULL);
	OBD5PID6 = CreateWindow("BUTTON", "Oxygen sensor 2 bank 2",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,7*listOccupantHeight,parameterTableWidth-20,18,mode5,(HMENU) 9,NULL,NULL);
	OBD5PID7 = CreateWindow("BUTTON", "Oxygen sensor 3 bank 2",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,8*listOccupantHeight,parameterTableWidth-20,18,mode5,(HMENU) 10,NULL,NULL);
	OBD5PID8 = CreateWindow("BUTTON", "Oxygen sensor 4 bank 2",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,9*listOccupantHeight,parameterTableWidth-20,18,mode5,(HMENU) 11,NULL,NULL);
	}
	//if(y_offset+mode6y_offset < parentHeight && y_offset+mode6y_offset+mode6Height > 0)
	{
	mode6 = CreateWindow("BUTTON", "MODE 0x6:Test results",WS_VISIBLE | WS_CHILD|BS_GROUPBOX,x_offset,mode6y_offset+y_offset,parameterTableWidth,mode6Height,parent,NULL,NULL,NULL);
	OBD6PID0 = CreateWindow("BUTTON", "OBD monitor IDs supported 1-20",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,1*listOccupantHeight,parameterTableWidth-20,18,mode6,(HMENU) 3,NULL,NULL);
	OBD6PID1 = CreateWindow("BUTTON", "Test 1",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,2*listOccupantHeight,parameterTableWidth-20,18,mode6,(HMENU) 4,NULL,NULL);
	OBD6PID2 = CreateWindow("BUTTON", "Test 2",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,3*listOccupantHeight,parameterTableWidth-20,18,mode6,(HMENU) 5,NULL,NULL);
	OBD6PID3 = CreateWindow("BUTTON", "Test 3",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,4*listOccupantHeight,parameterTableWidth-20,18,mode6,(HMENU) 6,NULL,NULL);
	OBD6PID4 = CreateWindow("BUTTON", "Test 4",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,5*listOccupantHeight,parameterTableWidth-20,18,mode6,(HMENU) 7,NULL,NULL);
	OBD6PID5 = CreateWindow("BUTTON", "Test 5",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,6*listOccupantHeight,parameterTableWidth-20,18,mode6,(HMENU) 8,NULL,NULL);
	}
	//if(y_offset+mode7y_offset < parentHeight && y_offset+mode7y_offset+mode7Height > 0)
	{
	mode7 = CreateWindow("BUTTON", "MODE 0x7:Pending DTCs",WS_VISIBLE | WS_CHILD|BS_GROUPBOX,x_offset,mode7y_offset+y_offset,parameterTableWidth,mode7Height,parent,NULL,NULL,NULL);
	OBD7PID0 = CreateWindow("BUTTON", "Pending DTCs",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,1*listOccupantHeight,parameterTableWidth-20,18,mode7,(HMENU) 3,NULL,NULL);
	}
	//if(y_offset+mode8y_offset < parentHeight && y_offset+mode8y_offset+mode8Height > 0)
	{
	mode8 = CreateWindow("BUTTON", "MODE 0x8:Control components",WS_VISIBLE | WS_CHILD|BS_GROUPBOX,x_offset,mode8y_offset+y_offset,parameterTableWidth,mode8Height,parent,NULL,NULL,NULL);
	OBD8PID0 = CreateWindow("BUTTON", "Component IDs supported 1-20",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,1*listOccupantHeight,parameterTableWidth-20,18,mode8,(HMENU) 3,NULL,NULL);
	OBD8PID1 = CreateWindow("BUTTON", "Component 1",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,2*listOccupantHeight,parameterTableWidth-20,18,mode8,(HMENU) 4,NULL,NULL);
	}
	//if(y_offset+mode9y_offset < parentHeight && y_offset+mode9y_offset+mode9Height > 0)
	{
	mode9 = CreateWindow("BUTTON", "MODE 0x9:Request vehicle info",WS_VISIBLE | WS_CHILD|BS_GROUPBOX,x_offset,mode9y_offset+y_offset,parameterTableWidth,mode9Height,parent,NULL,NULL,NULL);
	OBD9PID0 = CreateWindow("BUTTON", "OBD monitor IDs supported 1-20",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,1*listOccupantHeight,parameterTableWidth-20,18,mode9,(HMENU) 3,NULL,NULL);
	OBD9PID1 = CreateWindow("BUTTON", "VIN message count",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,2*listOccupantHeight,parameterTableWidth-20,18,mode9,(HMENU) 4,NULL,NULL);
	OBD9PID2 = CreateWindow("BUTTON", "VIN",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,3*listOccupantHeight,parameterTableWidth-20,18,mode9,(HMENU) 5,NULL,NULL);
	OBD9PID3 = CreateWindow("BUTTON", "Cal ID message count",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,4*listOccupantHeight,parameterTableWidth-20,18,mode9,(HMENU) 6,NULL,NULL);
	OBD9PID4 = CreateWindow("BUTTON", "Calibration ID",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,5*listOccupantHeight,parameterTableWidth-20,18,mode9,(HMENU) 7,NULL,NULL);
	OBD9PID5 = CreateWindow("BUTTON", "CVN message count",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,6*listOccupantHeight,parameterTableWidth-20,18,mode9,(HMENU) 8,NULL,NULL);
	OBD9PID6 = CreateWindow("BUTTON", "Cal Validation No.",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,7*listOccupantHeight,parameterTableWidth-20,18,mode9,(HMENU) 9,NULL,NULL);
	}

	//if(y_offset+mode22y_offset < parentHeight && y_offset+mode22y_offset+mode22Height > 0)
	{
	//OBD MODE 0x22 PID buttons, 82 PIDs
	mode22 = CreateWindow("BUTTON", "MODE 0x22:Performance data",WS_VISIBLE | WS_CHILD|BS_GROUPBOX,x_offset,mode22y_offset+y_offset,parameterTableWidth,mode22Height,parent,NULL,NULL,NULL);
	OBD22PID512 = CreateWindow("BUTTON", "512",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,1*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 3,NULL,NULL);
	OBD22PID513 = CreateWindow("BUTTON", "513",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,2*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 4,NULL,NULL);
	OBD22PID515 = CreateWindow("BUTTON", "515",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,3*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 5,NULL,NULL);
	OBD22PID516 = CreateWindow("BUTTON", "516",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,4*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 6,NULL,NULL);
	OBD22PID517 = CreateWindow("BUTTON", "517",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,5*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 7,NULL,NULL);
	OBD22PID518 = CreateWindow("BUTTON", "518",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,6*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 8,NULL,NULL);
	OBD22PID519 = CreateWindow("BUTTON", "519",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,7*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 9,NULL,NULL);
	OBD22PID520 = CreateWindow("BUTTON", "520",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,8*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 10,NULL,NULL);
	OBD22PID521 = CreateWindow("BUTTON", "521",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,9*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 11,NULL,NULL);
	OBD22PID522 = CreateWindow("BUTTON", "522",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,10*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 12,NULL,NULL);
	OBD22PID523 = CreateWindow("BUTTON", "523",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,11*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 13,NULL,NULL);
	OBD22PID524 = CreateWindow("BUTTON", "524",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,12*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 14,NULL,NULL);
	OBD22PID525 = CreateWindow("BUTTON", "525",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,13*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 15,NULL,NULL);
	OBD22PID526to529 = CreateWindow("BUTTON", "ECU part no.",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,14*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 16,NULL,NULL);
	OBD22PID530 = CreateWindow("BUTTON", "530",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,15*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 17,NULL,NULL);
	OBD22PID531 = CreateWindow("BUTTON", "531",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,16*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 18,NULL,NULL);
	OBD22PID532 = CreateWindow("BUTTON", "532",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,17*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 19,NULL,NULL);
	OBD22PID533 = CreateWindow("BUTTON", "533",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,18*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 20,NULL,NULL);
	OBD22PID534 = CreateWindow("BUTTON", "534",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,19*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 21,NULL,NULL);
	OBD22PID536 = CreateWindow("BUTTON", "536",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,20*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 22,NULL,NULL);
	OBD22PID537 = CreateWindow("BUTTON", "537",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,21*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 23,NULL,NULL);
	OBD22PID538 = CreateWindow("BUTTON", "538",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,22*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 24,NULL,NULL);
	OBD22PID539 = CreateWindow("BUTTON", "539",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,23*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 25,NULL,NULL);
	OBD22PID540to543 = CreateWindow("BUTTON", "Calibration ID",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,24*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 26,NULL,NULL);
	OBD22PID544 = CreateWindow("BUTTON", "544",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,25*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 27,NULL,NULL);
	OBD22PID545to548 = CreateWindow("BUTTOn", "Make, model and year",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,26*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 28,NULL,NULL);
	OBD22PID549 = CreateWindow("BUTTON", "549",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,27*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 29,NULL,NULL);
	OBD22PID550 = CreateWindow("BUTTON", "550",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,28*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 30,NULL,NULL);
	OBD22PID551 = CreateWindow("BUTTON", "551",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,29*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 31,NULL,NULL);
	OBD22PID552 = CreateWindow("BUTTON", "552",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,30*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 32,NULL,NULL);
	OBD22PID553 = CreateWindow("BUTTON", "553",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,31*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 33,NULL,NULL);
	OBD22PID554 = CreateWindow("BUTTON", "554",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,32*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 34,NULL,NULL);
	OBD22PID555 = CreateWindow("BUTTON", "555",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,33*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 35,NULL,NULL);
	OBD22PID556 = CreateWindow("BUTTON", "556",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,34*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 36,NULL,NULL);
	OBD22PID557 = CreateWindow("BUTTON", "557",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,35*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 37,NULL,NULL);
	OBD22PID558 = CreateWindow("BUTTON", "558",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,36*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 38,NULL,NULL);
	OBD22PID559 = CreateWindow("BUTTON", "559",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,37*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 39,NULL,NULL);
	OBD22PID560 = CreateWindow("BUTTON", "560",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,38*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 40,NULL,NULL);
	OBD22PID561 = CreateWindow("BUTTON", "561",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,39*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 41,NULL,NULL);
	OBD22PID562 = CreateWindow("BUTTON", "562",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,40*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 42,NULL,NULL);
	OBD22PID563 = CreateWindow("BUTTON", "563",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,41*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 43,NULL,NULL);
	OBD22PID564 = CreateWindow("BUTTON", "564",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,42*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 44,NULL,NULL);
	OBD22PID565 = CreateWindow("BUTTON", "565",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,43*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 45,NULL,NULL);
	OBD22PID566 = CreateWindow("BUTTON", "566",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,44*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 46,NULL,NULL);
	OBD22PID567 = CreateWindow("BUTTON", "567",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,45*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 47,NULL,NULL);
	OBD22PID568 = CreateWindow("BUTTON", "568",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,46*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 48,NULL,NULL);
	OBD22PID569 = CreateWindow("BUTTON", "569",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,47*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 49,NULL,NULL);
	OBD22PID570 = CreateWindow("BUTTON", "570",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,48*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 50,NULL,NULL);
	OBD22PID571 = CreateWindow("BUTTON", "571",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,49*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 51,NULL,NULL);
	OBD22PID572 = CreateWindow("BUTTON", "572",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,50*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 52,NULL,NULL);
	OBD22PID573 = CreateWindow("BUTTON", "573",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,51*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 53,NULL,NULL);
	OBD22PID577 = CreateWindow("BUTTON", "577",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,52*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 54,NULL,NULL);
	OBD22PID583 = CreateWindow("BUTTON", "583",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,53*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 55,NULL,NULL);
	OBD22PID584 = CreateWindow("BUTTON", "584",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,54*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 56,NULL,NULL);
	OBD22PID585 = CreateWindow("BUTTON", "585",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,55*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 57,NULL,NULL);
	OBD22PID586 = CreateWindow("BUTTON", "586",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,56*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 58,NULL,NULL);
	OBD22PID591 = CreateWindow("BUTTON", "591",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,57*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 59,NULL,NULL);
	OBD22PID592 = CreateWindow("BUTTON", "592",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,58*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 60,NULL,NULL);
	OBD22PID593 = CreateWindow("BUTTON", "593",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,59*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 61,NULL,NULL);
	OBD22PID594 = CreateWindow("BUTTON", "594",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,60*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 62,NULL,NULL);
	OBD22PID595 = CreateWindow("BUTTON", "595",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,61*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 63,NULL,NULL);
	OBD22PID596 = CreateWindow("BUTTON", "596",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,62*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 64,NULL,NULL);
	OBD22PID598 = CreateWindow("BUTTON", "598",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,63*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 65,NULL,NULL);
	OBD22PID599 = CreateWindow("BUTTON", "599",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,64*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 66,NULL,NULL);
	OBD22PID601 = CreateWindow("BUTTON", "601",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,65*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 67,NULL,NULL);
	OBD22PID602 = CreateWindow("BUTTON", "602",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,66*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 68,NULL,NULL);
	OBD22PID603 = CreateWindow("BUTTON", "603",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,67*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 69,NULL,NULL);
	OBD22PID605 = CreateWindow("BUTTON", "605",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,68*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 70,NULL,NULL);
	OBD22PID606 = CreateWindow("BUTTON", "606",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,69*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 71,NULL,NULL);
	OBD22PID608 = CreateWindow("BUTTON", "608",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,70*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 72,NULL,NULL);
	OBD22PID609 = CreateWindow("BUTTON", "609",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,71*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 73,NULL,NULL);
	OBD22PID610 = CreateWindow("BUTTON", "610",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,72*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 74,NULL,NULL);
	OBD22PID612 = CreateWindow("BUTTON", "612",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,73*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 75,NULL,NULL);
	OBD22PID613 = CreateWindow("BUTTON", "613",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,74*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 76,NULL,NULL);
	OBD22PID614 = CreateWindow("BUTTON", "614",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,75*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 77,NULL,NULL);
	OBD22PID615 = CreateWindow("BUTTON", "615",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,76*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 78,NULL,NULL);
	OBD22PID616 = CreateWindow("BUTTON", "616",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,77*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 79,NULL,NULL);
	OBD22PID617 = CreateWindow("BUTTON", "617",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,78*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 80,NULL,NULL);
	OBD22PID618 = CreateWindow("BUTTON", "618",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,79*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 81,NULL,NULL);
	OBD22PID619 = CreateWindow("BUTTON", "619",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,80*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 82,NULL,NULL);
	OBD22PID620 = CreateWindow("BUTTON", "620",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,81*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 83,NULL,NULL);
	OBD22PID621 = CreateWindow("BUTTON", "621",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,82*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 84,NULL,NULL);
	}

	//if(y_offset+mode2Fy_offset < parentHeight && y_offset+mode2Fy_offset+mode2FHeight > 0)
	{
	//OBD MODE 0x2F PID buttons, 28 PIDs
	mode2F = CreateWindow("BUTTON", "MODE 0x2F:Veh.func.~~DANGER~~",WS_VISIBLE | WS_CHILD|BS_GROUPBOX ,x_offset,mode2Fy_offset+y_offset,parameterTableWidth,mode2FHeight,parent,NULL,NULL,NULL);
	OBD2FPID100 = CreateWindow("BUTTON", "0x100",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,1*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 3,NULL,NULL);
	OBD2FPID101 = CreateWindow("BUTTON", "0x101",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,2*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 4,NULL,NULL);
	OBD2FPID102 = CreateWindow("BUTTON", "0x102",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,3*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 5,NULL,NULL);
	OBD2FPID103 = CreateWindow("BUTTON", "0x103",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,4*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 6,NULL,NULL);
	OBD2FPID104 = CreateWindow("BUTTON", "0x104",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,5*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 7,NULL,NULL);
	OBD2FPID120 = CreateWindow("BUTTON", "0x120",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,6*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 8,NULL,NULL);
	OBD2FPID121 = CreateWindow("BUTTON", "0x121",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,7*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 9,NULL,NULL);
	OBD2FPID122 = CreateWindow("BUTTON", "0x122",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,8*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 10,NULL,NULL);
	OBD2FPID125 = CreateWindow("BUTTON", "0x125",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,9*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 11,NULL,NULL);
	OBD2FPID126 = CreateWindow("BUTTON", "0x126",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,10*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 12,NULL,NULL);
	OBD2FPID127 = CreateWindow("BUTTON", "0x127",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,11*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 13,NULL,NULL);
	OBD2FPID12A = CreateWindow("BUTTON", "0x12A",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,12*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 14,NULL,NULL);
	OBD2FPID140 = CreateWindow("BUTTON", "0x140",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,13*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 15,NULL,NULL);
	OBD2FPID141 = CreateWindow("BUTTON", "0x141",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,14*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 16,NULL,NULL);
	OBD2FPID142 = CreateWindow("BUTTON", "0x142",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,15*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 17,NULL,NULL);
	OBD2FPID143 = CreateWindow("BUTTON", "0x143",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,16*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 18,NULL,NULL);
	OBD2FPID144 = CreateWindow("BUTTON", "0x144",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,17*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 19,NULL,NULL);
	OBD2FPID146 = CreateWindow("BUTTON", "0x146",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,18*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 20,NULL,NULL);
	OBD2FPID147 = CreateWindow("BUTTON", "0x147",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,19*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 21,NULL,NULL);
	OBD2FPID148 = CreateWindow("BUTTON", "0x148",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,20*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 22,NULL,NULL);
	OBD2FPID149 = CreateWindow("BUTTON", "0x149",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,21*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 23,NULL,NULL);
	OBD2FPID14A = CreateWindow("BUTTON", "0x14A",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,22*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 24,NULL,NULL);
	OBD2FPID14B = CreateWindow("BUTTON", "0x14B",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,23*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 25,NULL,NULL);
	OBD2FPID160 = CreateWindow("BUTTON", "0x160",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,24*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 26,NULL,NULL);
	OBD2FPID161 = CreateWindow("BUTTON", "0x161",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,25*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 27,NULL,NULL);
	OBD2FPID162 = CreateWindow("BUTTON", "0x162",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,26*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 28,NULL,NULL);
	OBD2FPID163 = CreateWindow("BUTTON", "0x163",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,27*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 29,NULL,NULL);
	OBD2FPID164 = CreateWindow("BUTTON", "0x164",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,28*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 30,NULL,NULL);
	}

	//if(y_offset+mode3By_offset < parentHeight && y_offset+mode3By_offset+mode3BHeight > 0)
	{
	//OBD MODE 0x3B PID buttons
	mode3B = CreateWindow("BUTTON", "MODE 0x3B:Write VIN",WS_VISIBLE | WS_CHILD|BS_GROUPBOX ,x_offset,mode3By_offset+y_offset,parameterTableWidth,mode3BHeight,parent,NULL,NULL,NULL);
	OBD3BPID0 = CreateWindow("BUTTON", "",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,1*listOccupantHeight,20,18,mode3B,(HMENU) 3,NULL,NULL);
	OBD3BPID1 = CreateWindow("EDIT", mode3BVIN,WS_VISIBLE | WS_CHILD| WS_BORDER |ES_UPPERCASE ,25,1*listOccupantHeight,parameterTableWidth-95,18,mode3B,NULL,NULL,NULL);
	}
	//if(y_offset+modeSHORTy_offset < parentHeight && y_offset+modeSHORTy_offset+modeSHORTHeight > 0)
	{
	//OBD MODE short message PID buttons
	/*modeSHORT = CreateWindow("BUTTON", "MODE SHORT: Read veh info.",WS_VISIBLE | WS_CHILD|BS_GROUPBOX,x_offset,modeSHORTy_offset+y_offset,parameterTableWidth,modeSHORTHeight,parent,NULL,NULL,NULL);
	OBDSHORTPID2 = CreateWindow("BUTTON", "VIN",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,1*listOccupantHeight,parameterTableWidth-20,18,modeSHORT,(HMENU) 3,NULL,NULL);
	OBDSHORTPID4 = CreateWindow("BUTTON", "CAL ID, make, model and year",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,2*listOccupantHeight,parameterTableWidth-20,18,modeSHORT,(HMENU) 4,NULL,NULL);
	*/
	}


	//update the checkbox settings
	restoreLogCheckBoxSettings();

	// Set the vertical scrolling range and page size
    si.cbSize = sizeof(si);
    si.fMask  = SIF_RANGE | SIF_PAGE;
    si.nMin   = 0;
    si.nMax   = parameterTableHeight + 20;
    si.nPage  = (parameterTableHeight + 10) / 10;
    SetScrollInfo(logParamsScrollV, SB_CTL, &si, TRUE);

    // Set the horizontal scrolling range and page size.
    si.cbSize = sizeof(si);
    si.fMask  = SIF_RANGE | SIF_PAGE;
    si.nMin   = 0;
    si.nMax   = (parameterTableWidth)/14;
    si.nPage  = parameterTableWidth / 18;
    SetScrollInfo(logParamsScrollH, SB_CTL, &si, TRUE);

	return;
}

void clearLogParamsTable()
{
	//save the checkbox settings
	saveLogCheckBoxSettings();
	//destroy all of the windows
	DestroyWindow(mode1);
	DestroyWindow(mode2);
	DestroyWindow(mode3);
	DestroyWindow(mode4);
	DestroyWindow(mode5);
	DestroyWindow(mode6);
	DestroyWindow(mode7);
	DestroyWindow(mode8);
	DestroyWindow(mode9);
	DestroyWindow(mode22);
	DestroyWindow(mode2F);
	DestroyWindow(mode3B);
	//DestroyWindow(modeSHORT);
	return;
}

void saveLogCheckBoxSettings()
{
	if(IsWindowVisible(mode1))
	{
	//mode 1 PIDs
	for(int i = 0; i < 28; i++)
	{
		mode1checkboxstate[i] = IsDlgButtonChecked(mode1,3+i);
	}
	}
	if(IsWindowVisible(mode2))
	{
	//mode 2 PIDs
	for(int i = 0; i < 11; i++)
	{
		mode2checkboxstate[i] = IsDlgButtonChecked(mode2,3+i);
	}
	}
	if(IsWindowVisible(mode3))
	{
	//mode 3 PID
	mode3checkboxstate = IsDlgButtonChecked(mode3,3);
	}
	if(IsWindowVisible(mode4))
	{
	//mode 4 PID
	mode4checkboxstate = IsDlgButtonChecked(mode4,3);
	}
	if(IsWindowVisible(mode5))
	{
	//mode 5 PIDs
	for(int i = 0; i < 9; i++)
	{
		mode5checkboxstate[i] = IsDlgButtonChecked(mode5,3+i);
	}
	}
	if(IsWindowVisible(mode6))
	{
	//mode 6 PIDs
	for(int i = 0; i < 6; i++)
	{
		mode6checkboxstate[i] = IsDlgButtonChecked(mode6,3+i);
	}
	}
	if(IsWindowVisible(mode7))
	{
	//mode 7 PIDs
	mode7checkboxstate = IsDlgButtonChecked(mode7,3);
	}
	if(IsWindowVisible(mode8))
	{
	//mode 8 PIDs
	for(int i = 0; i < 2; i++)
	{
		mode8checkboxstate[i] = IsDlgButtonChecked(mode8,3+i);
	}
	}
	if(IsWindowVisible(mode9))
	{
	//mode 9 PIDs
	for(int i = 0; i < 7; i++)
	{
		mode9checkboxstate[i] = IsDlgButtonChecked(mode9,3+i);
	}
	}
	if(IsWindowVisible(mode22))
	{
	//mode 22 PIDs
	for(int i = 0; i < 82; i++)
	{
		mode22checkboxstate[i] = IsDlgButtonChecked(mode22,3+i);
	}
	}
	if(IsWindowVisible(mode2F))
	{
	//mode 2F PIDs
	for(int i = 0; i < 28; i++)
	{
		mode2Fcheckboxstate[i] = IsDlgButtonChecked(mode2F,3+i);
	}
	}
	if(IsWindowVisible(mode7))
	{
	//mode 3B PIDs
	mode3Bcheckboxstate = IsDlgButtonChecked(mode3B,3);
	GetWindowText(OBD3BPID1,mode3BVIN,18); //SCCPC111-5H------
	mode3BVIN[0] = 'S';
	mode3BVIN[1] = 'C';
	mode3BVIN[2] = 'C';
	mode3BVIN[3] = 'P';
	mode3BVIN[4] = 'C';
	mode3BVIN[5] = '1';
	mode3BVIN[6] = '1';
	mode3BVIN[7] = '1';
	mode3BVIN[9] = '5';
	mode3BVIN[10] = 'H';
	}
	/*//mode SHORT PIDs
	for(int i = 0; i < 2; i++)
	{
		modeSHORTcheckboxstate[i] = IsDlgButtonChecked(modeSHORT,3+i);
	}*/
	return;
}

void restoreLogCheckBoxSettings()
{
	//mode 1 PIDs
	for(int i = 0; i < 28; i++)
	{
		if(mode1checkboxstate[i] == BST_CHECKED)
		{
			CheckDlgButton(mode1,i+3,BST_CHECKED);
		}
		else if(mode1checkboxstate[i] == BST_INDETERMINATE)
		{
			CheckDlgButton(mode1,i+3,BST_INDETERMINATE);
		}
	}
	//mode 2 PIDs
	for(int i = 0; i < 11; i++)
	{
		if(mode2checkboxstate[i] == BST_CHECKED)
		{
			CheckDlgButton(mode2,i+3,BST_CHECKED);
		}
		else if(mode2checkboxstate[i] == BST_INDETERMINATE)
		{
			CheckDlgButton(mode2,i+3,BST_INDETERMINATE);
		}
	}

	//mode 3 PID
	if(mode3checkboxstate == BST_CHECKED)
	{
		CheckDlgButton(mode3,3,BST_CHECKED);
	}

	//mode 4 PID
	if(mode4checkboxstate == BST_CHECKED)
	{
		CheckDlgButton(mode4,3,BST_CHECKED);
	}


	//mode 5 PIDs
	for(int i = 0; i < 9; i++)
	{
		if(mode5checkboxstate[i] == BST_CHECKED)
		{
			CheckDlgButton(mode5,i+3,BST_CHECKED);
		}
		else if(mode5checkboxstate[i] == BST_INDETERMINATE)
		{
			CheckDlgButton(mode5,i+3,BST_INDETERMINATE);
		}
	}

	//mode 6 PIDs
	for(int i = 0; i < 6; i++)
	{
		if(mode6checkboxstate[i] == BST_CHECKED)
		{
			CheckDlgButton(mode6,i+3,BST_CHECKED);
		}
		else if(mode6checkboxstate[i] == BST_INDETERMINATE)
		{
			CheckDlgButton(mode6,i+3,BST_INDETERMINATE);
		}
	}

	//mode 7 PID
	if(mode7checkboxstate == BST_CHECKED)
	{
		CheckDlgButton(mode7,3,BST_CHECKED);
	}


	//mode 8 PIDs
	for(int i = 0; i < 2; i++)
	{
		if(mode8checkboxstate[i] == BST_CHECKED)
		{
			CheckDlgButton(mode8,i+3,BST_CHECKED);
		}
		else if(mode8checkboxstate[i] == BST_INDETERMINATE)
		{
			CheckDlgButton(mode8,i+3,BST_INDETERMINATE);
		}
	}

	//mode 9 PIDs
	for(int i = 0; i < 7; i++)
	{
		if(mode9checkboxstate[i] == BST_CHECKED)
		{
			CheckDlgButton(mode9,i+3,BST_CHECKED);
		}
		else if(mode9checkboxstate[i] == BST_INDETERMINATE)
		{
			CheckDlgButton(mode9,i+3,BST_INDETERMINATE);
		}
	}
	//mode 22 PIDs
	for(int i = 0; i < 82; i++)
	{
		if(mode22checkboxstate[i] == BST_CHECKED)
		{
			CheckDlgButton(mode22,i+3,BST_CHECKED);
		}
		else if(mode22checkboxstate[i] == BST_INDETERMINATE)
		{
			CheckDlgButton(mode22,i+3,BST_INDETERMINATE);
		}
	}
	//mode 2F PIDs
	for(int i = 0; i < 28; i++)
	{
		if(mode2Fcheckboxstate[i] == BST_CHECKED)
		{
			CheckDlgButton(mode2F,i+3,BST_CHECKED);
		}
		else if(mode2Fcheckboxstate[i] == BST_INDETERMINATE)
		{
			CheckDlgButton(mode2F,i+3,BST_INDETERMINATE);
		}
	}
	//mode 3B PIDs
	if(mode3Bcheckboxstate == BST_CHECKED)
	{
		CheckDlgButton(mode3B,3,BST_CHECKED);
	}
	/*//mode SHORT PIDs
	for(int i = 0; i < 2; i++)
	{
		if(modeSHORTcheckboxstate[i] == BST_CHECKED)
		{
			CheckDlgButton(modeSHORT,i+3,BST_CHECKED);
		}
		else if(modeSHORTcheckboxstate[i] == BST_INDETERMINATE)
		{
			CheckDlgButton(modeSHORT,i+3,BST_INDETERMINATE);
		}
	}*/
	return;
}


void updatePlot(HWND parent, int parentWidth, int parentHeight)
{


}


void checkForAndPopulateComPortList(HWND comPortList)
{
	int listNo = 0;
	for(int i = 1; i < 256; i++)
	{
		//for each iteration, write out "COM"
		//and add to that the number, and convert
		//to a char* to try and open the port
		std::string portBase = "\\\\.\\COM";
		char stringByte [14];
		std::string portNumber = itoa(i,stringByte,10);
		portBase += portNumber;
		const char *cstr = portBase.c_str();
		HANDLE Port = CreateFile(cstr,GENERIC_READ | GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);

		//if there was no error creating that com handle
		//add that item to the comPortList
		if(Port != INVALID_HANDLE_VALUE)
		{
			//std::cout << portBase << '\n';
			SendMessage(comPortList,(UINT) CB_ADDSTRING,(WPARAM) listNo,(LPARAM) cstr);//pass the text to the combo box
			listNo += 1;
		}
		CloseHandle(Port);
	}
}

bool serialPollingCycle(HANDLE serialPortHandle)
{
	bool success = true;
	/*//a handle for the data type which holds COM port settings
	DCB dcb;

	//open the COM port
	HANDLE Port = CreateFile(ListItem,GENERIC_READ | GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);

	//set up the COM port settings
	int baudRateInteger =  chars_to_int(baudRateStore);//numbers below 190 do not work
	//std::cout << baudRateInteger << '\n';

	//prepare the dcb data to configure the COM port
	dcb.DCBlength = sizeof(DCB);//the size of the dbc file
	GetCommState(Port, &dcb);//read in the current settings in order to modify them
	dcb.BaudRate = baudRateInteger;//  baud rate
	dcb.ByteSize = 8;              //  data size, xmit and rcv
	dcb.Parity   = NOPARITY;       //  parity bit
	dcb.StopBits = ONESTOPBIT;     //  stop bit
	SetCommState(Port, &dcb);//set the COM port with these options
	//one start bit, 8 data bits, one stop bit gives 10 bits per frame, just as the lotus is configured

	//set com port timeouts too
	COMMTIMEOUTS timeouts = { 0 };
	timeouts.ReadIntervalTimeout         = 50; // in milliseconds
	timeouts.ReadTotalTimeoutConstant    = 50; // in milliseconds
	timeouts.ReadTotalTimeoutMultiplier  = 10; // in milliseconds
	timeouts.WriteTotalTimeoutConstant   = 50; // in milliseconds
	timeouts.WriteTotalTimeoutMultiplier = 10; // in milliseconds
	SetCommTimeouts(Port,&timeouts);

	//perform the OBD-II ISO-9141-2 slow init handshake sequence
	//Port = sendOBDInit(Port);
	*/


	//for each logging parameter, send a message, recieve a message, interpret the message and save the data

	//initialize a read-in data buffer
	byte recvdBytes[100];//a buffer which can be used to store read-in bytes
	for(int i = 0; i < 100; i++)
	{
		recvdBytes[i] = 0;
	}

	//get the current timer value
    QueryPerformanceCounter(&currentTime);//get the loop position time
    double LoggerTimeMS = (currentTime.QuadPart - startTime.QuadPart)*1000 / systemFrequency.QuadPart;// compute the elapsed time in milliseconds
    elapsedLoggerTime = LoggerTimeMS/1000;//this is an attempt to get post-decimal points
    writeBytes << elapsedLoggerTime;
    writeData << elapsedLoggerTime;

    //scan through the different OBD PIDs, checking if they are to be polled
    //if they are to be polled, send a message, read a message and store the data

    //28 PIDs to check for mode 1
    //PID 0
    if(mode1checkboxstate[0] != BST_UNCHECKED && success == true)
    {
    	//mode 1 PID 0: list of valid PIDs 0-20
    	byte sendBytes[6] = {104,106,241,1,0,196};
    	writeToSerialPort(serialPortHandle, sendBytes, 6);
    	readFromSerialPort(serialPortHandle, recvdBytes,6,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,10,100);
    	if(numBytesRead < 10)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 4 bytes which are binary flags
    	//for the next 32 PIDs indicating if they work with this car
    	//the return packet looks like 72,107,16,65,0,x,x,x,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	int PIDplace = 0;
    	for(int i = 5; i < 9; i++)//loop through bytes
    	{
    		for(int j = 7; j > -1; j--)
    		{
    			PIDplace += 1;
    			if(((recvdBytes[i] >> j) & 0x1))//shift and mask to check bit flags
    			{
    				writeData << PIDplace << ' ';
    			}
    		}
    	}
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //mode 1 PID 1
    if(mode1checkboxstate[1] != BST_UNCHECKED && success == true)
    {
    	//mode 1 PID 1: Monitor status since DTCs cleared. (Includes malfunction indicator lamp (MIL) status and number of DTCs.)
    	byte sendBytes[6] = {104,106,241,1,1,197};
    	writeToSerialPort(serialPortHandle, sendBytes, 6);
    	readFromSerialPort(serialPortHandle, recvdBytes,6,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,10,100);
    	if(numBytesRead < 10)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 4 bytes which are binary/byte flags
    	//for vehicle monitors of this car
    	//the return packet looks like 72,107,16,65,1,x,x,x,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	if((recvdBytes[5] >> 7) & 0x1)
    	{
    		writeData << "CEL on: ";
    	}
    	else
    	{
    		writeData << "CEL off: ";
    	}
    	int numCELs = 0;
    	int multiplier = 1;
    	for(int i = 0; i < 7; i++)
    	{
    		numCELs += ((recvdBytes[5] >> i) & 0x1)*multiplier;
    		multiplier = multiplier*2;
    	}
    	writeData << numCELs << " codes indicated ";
    	if((recvdBytes[6] >> 0) & 0x1)
    	{
    		writeData << "misfire test available ";
    	}
    	else
    	{
    		writeData << "misfire test N/A ";
    	}
    	if((recvdBytes[6] >> 4) & 0x1)
    	{
    		writeData << "misfire test incomplete ";
    	}
    	else
    	{
    		writeData << "misfire test complete ";
    	}
    	if((recvdBytes[6] >> 1) & 0x1)
    	{
    		writeData << "fuel system test available ";
    	}
    	else
    	{
    		writeData << "fuel system test N/A ";
    	}
    	if((recvdBytes[6] >> 5) & 0x1)
    	{
    		writeData << "fuel system test incomplete ";
    	}
    	else
    	{
    		writeData << "fuel system test complete ";
    	}
    	if((recvdBytes[6] >> 2) & 0x1)
    	{
    		writeData << "components test available ";
    	}
    	else
    	{
    		writeData << "components test N/A ";
    	}
    	if((recvdBytes[6] >> 6) & 0x1)
    	{
    		writeData << "components test incomplete ";
    	}
    	else
    	{
    		writeData << "components test complete ";
    	}
    	if((recvdBytes[6] >> 3) & 0x1)
    	{
    		writeData << "Compression ignition monitors: ";
    		if((recvdBytes[7] >> 0) & 0x1)
    		{
    			writeData << "NMHC test available ";
    		}
    		else
    		{
    			writeData << "NMHC test N/A ";
    		}
    		if((recvdBytes[8] >> 0) & 0x1)
    		{
    			writeData << "NMHC test incomplete ";
    		}
    		else
    		{
    			writeData << "NMHC test complete ";
    		}
    		if((recvdBytes[7] >> 1) & 0x1)
    		{
    			writeData << "NOx/SCR monitor test available ";
    		}
    		else
    		{
    			writeData << "NOx/SCR monitor test N/A ";
    		}
    		if((recvdBytes[8] >> 1) & 0x1)
    		{
    			writeData << "NOx/SCR monitor test incomplete ";
    		}
    		else
    		{
    			writeData << "NOx/SCR monitor test complete ";
    		}
    		if((recvdBytes[7] >> 2) & 0x1)
    		{
    			writeData << "?? test available ";
    		}
    		else
    		{
    			writeData << "?? test N/A ";
    		}
    		if((recvdBytes[8] >> 2) & 0x1)
    		{
    			writeData << "?? test incomplete ";
    		}
    		else
    		{
    			writeData << "?? test complete ";
    		}
    		if((recvdBytes[7] >> 3) & 0x1)
    		{
    			writeData << "Boost pressure test available ";
    		}
    		else
    		{
    			writeData << "Boost pressure test N/A ";
    		}
    		if((recvdBytes[8] >> 3) & 0x1)
    		{
    			writeData << "Boost pressure test incomplete ";
    		}
    		else
    		{
    			writeData << "Boost pressure test complete ";
    		}
    		if((recvdBytes[7] >> 4) & 0x1)
    		{
    			writeData << "?? test available ";
    		}
    		else
    		{
    			writeData << "?? test N/A ";
    		}
    		if((recvdBytes[8] >> 4) & 0x1)
    		{
    			writeData << "?? test incomplete ";
    		}
    		else
    		{
    			writeData << "?? test complete ";
    		}
    		if((recvdBytes[7] >> 5) & 0x1)
    		{
    			writeData << "exhaust gas sensor test available ";
    		}
    		else
    		{
    			writeData << "exhaust gas sensor test N/A ";
    		}
    		if((recvdBytes[8] >> 5) & 0x1)
    		{
    			writeData << "exhaust gas sensor test incomplete ";
    		}
    		else
    		{
    			writeData << "exhaust gas sensor test complete ";
    		}
    		if((recvdBytes[7] >> 6) & 0x1)
    		{
    			writeData << "PM filter monitoring test available ";
    		}
    		else
    		{
    			writeData << "PM filter monitoring test N/A ";
    		}
    		if((recvdBytes[8] >> 6) & 0x1)
    		{
    			writeData << "PM filter monitoring test incomplete ";
    		}
    		else
    		{
    			writeData << "PM filter monitoring test complete ";
    		}
    		if((recvdBytes[7] >> 7) & 0x1)
    		{
    			writeData << "EGR/VVT system test available ";
    		}
    		else
    		{
    			writeData << "EGR/VVT system test N/A ";
    		}
    		if((recvdBytes[8] >> 7) & 0x1)
    		{
    			writeData << "EGR/VVT system test incomplete ";
    		}
    		else
    		{
    			writeData << "EGR/VVT system test complete ";
    		}
    	}
    	else
    	{
    		writeData << "Spark ignition monitors: ";
    		if((recvdBytes[7] >> 0) & 0x1)
    		{
    			writeData << "Catalyst test available ";
    		}
    		else
    		{
    			writeData << "Catalyst test N/A ";
    		}
    		if((recvdBytes[8] >> 0) & 0x1)
    		{
    			writeData << "Catalyst test incomplete ";
    		}
    		else
    		{
    			writeData << "Catalyst test complete ";
    		}
    		if((recvdBytes[7] >> 1) & 0x1)
    		{
    			writeData << "Heated catalyst test available ";
    		}
    		else
    		{
    			writeData << "Heated catalyst test N/A ";
    		}
    		if((recvdBytes[8] >> 1) & 0x1)
    		{
    			writeData << "Heated catalyst test incomplete ";
    		}
    		else
    		{
    			writeData << "Heated catalyst test complete ";
    		}
    		if((recvdBytes[7] >> 2) & 0x1)
    		{
    			writeData << "Evaporative system test available ";
    		}
    		else
    		{
    			writeData << "Evaporative system test N/A ";
    		}
    		if((recvdBytes[8] >> 2) & 0x1)
    		{
    			writeData << "Evaporative system test incomplete ";
    		}
    		else
    		{
    			writeData << "Evaporative system test complete ";
    		}
    		if((recvdBytes[7] >> 3) & 0x1)
    		{
    			writeData << "Secondary air system test available ";
    		}
    		else
    		{
    			writeData << "Secondary air system test N/A ";
    		}
    		if((recvdBytes[8] >> 3) & 0x1)
    		{
    			writeData << "Secondary air system test incomplete ";
    		}
    		else
    		{
    			writeData << "Secondary air system test complete ";
    		}
    		if((recvdBytes[7] >> 4) & 0x1)
    		{
    			writeData << "A/C refrigerant test available ";
    		}
    		else
    		{
    			writeData << "A/C refrigerant test N/A ";
    		}
    		if((recvdBytes[8] >> 4) & 0x1)
    		{
    			writeData << "A/C refrigerant test incomplete ";
    		}
    		else
    		{
    			writeData << "A/C refrigerant test complete ";
    		}
    		if((recvdBytes[7] >> 5) & 0x1)
    		{
    			writeData << "Oxygen sensor test available ";
    		}
    		else
    		{
    			writeData << "Oxygen sensor test N/A ";
    		}
    		if((recvdBytes[8] >> 5) & 0x1)
    		{
    			writeData << "Oxygen sensor test incomplete ";
    		}
    		else
    		{
    			writeData << "Oxygen sensor test complete ";
    		}
    		if((recvdBytes[7] >> 6) & 0x1)
    		{
    			writeData << "Oxygen sensor heater test available ";
    		}
    		else
    		{
    			writeData << "Oxygen sensor heater test N/A ";
    		}
    		if((recvdBytes[8] >> 6) & 0x1)
    		{
    			writeData << "Oxygen sensor heater test incomplete ";
    		}
    		else
    		{
    			writeData << "Oxygen sensor heater test complete ";
    		}
    		if((recvdBytes[7] >> 7) & 0x1)
    		{
    			writeData << "EGR system test available ";
    		}
    		else
    		{
    			writeData << "EGR system test N/A ";
    		}
    		if((recvdBytes[8] >> 7) & 0x1)
    		{
    			writeData << "EGR system test incomplete ";
    		}
    		else
    		{
    			writeData << "EGR system test complete ";
    		}
    	}

    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }

    //MODE 1 PID 2
    if(mode1checkboxstate[2] != BST_UNCHECKED && success == true)
    {
    	//mode 1 PID 2: freeze DTC
    	byte sendBytes[6] = {104,106,241,1,2,198};
    	writeToSerialPort(serialPortHandle, sendBytes, 6);
    	readFromSerialPort(serialPortHandle, recvdBytes,6,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,100);
    	if(numBytesRead < 8)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 2 bytes which are binary flags
    	//indicating which DTC triggered the freeze frame
    	//the return packet looks like 72,107,16,65,2,x,x,sum
    	writeBytes << ',';
    	writeData << ',';

    	if(recvdBytes[5] == 0 && recvdBytes[6] == 0)
    	{
    		writeData << "no DTC";
    	}
    	else
    	{
    		int letterCode = ((recvdBytes[5] >> 7) & 0x1)*2 + ((recvdBytes[5] >> 6) & 0x1);//will be 0,1,2, or 3
    		if(letterCode == 0)
    		{
    			writeData << 'P';
    		}
    		else if(letterCode == 1)
    		{
    			writeData << 'C';
    		}
    		else if(letterCode == 2)
    		{
    			writeData << 'B';
    		}
    		else if(letterCode == 3)
    		{
    			writeData << 'U';
    		}
    		int firstDecimal = ((recvdBytes[5] >> 5) & 0x1)*2 + ((recvdBytes[5] >> 4) & 0x1);
    		int secondDecimal = ((recvdBytes[5] >> 3) & 0x1)*8 + ((recvdBytes[5] >> 2) & 0x1)*4 + ((recvdBytes[5] >> 1) & 0x1)*2 + ((recvdBytes[5] >> 0) & 0x1);
    		int thirdDecimal = ((recvdBytes[6] >> 7) & 0x1)*8 + ((recvdBytes[6] >> 6) & 0x1)*4 + ((recvdBytes[6] >> 5) & 0x1)*2 + ((recvdBytes[6] >> 4) & 0x1);
    		int fourthDecimal = ((recvdBytes[6] >> 3) & 0x1)*8 + ((recvdBytes[6] >> 2) & 0x1)*4 + ((recvdBytes[6] >> 1) & 0x1)*2 + ((recvdBytes[6] >> 0) & 0x1);
    		writeData << firstDecimal << secondDecimal << thirdDecimal << fourthDecimal;
    	}
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //mode 1 PID 3
    if(mode1checkboxstate[3] != BST_UNCHECKED && success == true)
    {
    	//mode 1 PID 3: fuel system status
    	byte sendBytes[6] = {104,106,241,1,3,199};
    	writeToSerialPort(serialPortHandle, sendBytes, 6);
    	readFromSerialPort(serialPortHandle, recvdBytes,6,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,100);
    	if(numBytesRead < 8)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 2 bytes which are byte flags
    	//for the next 32 PIDs indicating if they work with this car
    	//the return packet looks like 72,107,16,65,3,x,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	if(recvdBytes[5] != 0){
    		writeData << " Fueling system 1: ";
    		if(((recvdBytes[5] >> 0) & 0x1))
    		{
    			writeData << "Open loop due to insufficient engine temperature";
    		}
    		else if(((recvdBytes[5] >> 1) & 0x1))
    		{
    			writeData << "Closed loop, using oxygen sensor feedback to determine fuel mix";
    		}
    		else if(((recvdBytes[5] >> 2) & 0x1))
    		{
    			writeData << "Open loop due to engine load OR fuel cut due to deceleration";
    		}
    		else if(((recvdBytes[5] >> 3) & 0x1))
    		{
    			writeData << "Open loop due to system failure";
    		}
    		else if(((recvdBytes[5] >> 4) & 0x1))
    		{
    			writeData << "Closed loop, using at least one oxygen sensor but there is a fault in the feedback system";
    		}
    	}

    	if(recvdBytes[6] != 0){
    		writeData << " Fueling system 2: ";
    		if(((recvdBytes[6] >> 0) & 0x1))
    		{
    			writeData << "Open loop due to insufficient engine temperature";
    		}
    		else if(((recvdBytes[6] >> 1) & 0x1))
    		{
    			writeData << "Closed loop, using oxygen sensor feedback to determine fuel mix";
    		}
    		else if(((recvdBytes[6] >> 2) & 0x1))
    		{
    			writeData << "Open loop due to engine load OR fuel cut due to deceleration";
    		}
    		else if(((recvdBytes[6] >> 3) & 0x1))
    		{
    			writeData << "Open loop due to system failure";
    		}
    		else if(((recvdBytes[6] >> 4) & 0x1))
    		{
    			writeData << "Closed loop, using at least one oxygen sensor but there is a fault in the feedback system";
    		}
    	}
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }

    //mode 1 PID 4
    if(mode1checkboxstate[4] != BST_UNCHECKED && success == true)
    {
    	//mode 1 PID 4: calculated engine load
    	byte sendBytes[6] = {104,106,241,1,4,200};
    	writeToSerialPort(serialPortHandle, sendBytes, 6);
    	readFromSerialPort(serialPortHandle, recvdBytes,6,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,7,100);
    	if(numBytesRead < 7)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 1 byte which indicates engine load
    	//the return packet looks like 72,107,16,65,4,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	double load = recvdBytes[5]/2.55;
    	writeData << load;
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //mode 1 PID 5
    if(mode1checkboxstate[5] != BST_UNCHECKED && success == true)
    {
    	//mode 1 PID 5: engine coolant temperature
    	byte sendBytes[6] = {104,106,241,1,5,201};
    	writeToSerialPort(serialPortHandle, sendBytes, 6);
    	readFromSerialPort(serialPortHandle, recvdBytes,6,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,7,100);
    	if(numBytesRead < 7)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 1 byte which indicates engine coolant temp
    	//the return packet looks like 72,107,16,65,5,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	int temp = recvdBytes[5]-40;
    	writeData << temp;
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //mode 1 PID 6
    if(mode1checkboxstate[6] != BST_UNCHECKED && success == true)
    {
    	//mode 1 PID 6: short term fuel trim: bank 1
    	byte sendBytes[6] = {104,106,241,1,6,202};
    	writeToSerialPort(serialPortHandle, sendBytes, 6);
    	readFromSerialPort(serialPortHandle, recvdBytes,6,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,7,100);
    	if(numBytesRead < 7)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 1 byte which indicates STFT
    	//the return packet looks like 72,107,16,65,6,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	double STFT = recvdBytes[5]/1.28-100;
    	writeData << STFT;
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //mode 1 PID 7
    if(mode1checkboxstate[7] != BST_UNCHECKED && success == true)
    {
    	//mode 1 PID 7: long term fuel trim: bank 1
    	byte sendBytes[6] = {104,106,241,1,7,203};
    	writeToSerialPort(serialPortHandle, sendBytes, 6);
    	readFromSerialPort(serialPortHandle, recvdBytes,6,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,7,100);
    	if(numBytesRead < 7)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 1 byte which indicates LTFT
    	//the return packet looks like 72,107,16,65,7,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	double LTFT = recvdBytes[5]/1.28-100;
    	writeData << LTFT;
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //mode 1 PID 0xC
    if(mode1checkboxstate[8] != BST_UNCHECKED && success == true)
    {
    	//mode 1 PID 0xC: engine RPM
    	byte sendBytes[6] = {104,106,241,1,12,208};
    	writeToSerialPort(serialPortHandle, sendBytes, 6);
    	readFromSerialPort(serialPortHandle, recvdBytes,6,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,100);
    	if(numBytesRead < 8)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 2 bytes which indicate engine RPM
    	//the return packet looks like 72,107,16,65,12,x,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	double RPM = (recvdBytes[5]*256+recvdBytes[6])/4;
    	writeData << RPM;
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //mode 1 PID 0xD
    if(mode1checkboxstate[9] != BST_UNCHECKED && success == true)
    {
    	//mode 1 PID 0xD: Vehicle speed
    	byte sendBytes[6] = {104,106,241,1,13,209};
    	writeToSerialPort(serialPortHandle, sendBytes, 6);
    	readFromSerialPort(serialPortHandle, recvdBytes,6,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,7,100);
    	if(numBytesRead < 7)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 1 byte which indicates vehicle speed in km/h
    	//the return packet looks like 72,107,16,65,13,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	double speed = recvdBytes[5];
    	writeData << speed;
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //mode 1 PID 0xE
    if(mode1checkboxstate[10] != BST_UNCHECKED && success == true)
    {
    	//mode 1 PID 0xE: Timing advance
    	byte sendBytes[6] = {104,106,241,1,14,210};
    	writeToSerialPort(serialPortHandle, sendBytes, 6);
    	readFromSerialPort(serialPortHandle, recvdBytes,6,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,7,100);
    	if(numBytesRead < 7)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 1 byte which indicates timing advance
    	//the return packet looks like 72,107,16,65,14,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	double timingAdvance = recvdBytes[5]/2 - 64;
    	writeData << timingAdvance;
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //mode 1 PID 0xF
    if(mode1checkboxstate[11] != BST_UNCHECKED && success == true)
    {
    	//mode 1 PID 0xF: Intake air temperature
    	byte sendBytes[6] = {104,106,241,1,15,211};
    	writeToSerialPort(serialPortHandle, sendBytes, 6);
    	readFromSerialPort(serialPortHandle, recvdBytes,6,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,7,100);
    	if(numBytesRead < 7)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 1 byte which indicates intake air temperature
    	//the return packet looks like 72,107,16,65,15,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	double IAT = recvdBytes[5] - 40;
    	writeData << IAT;
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //mode 1 PID 0x10
    if(mode1checkboxstate[12] != BST_UNCHECKED && success == true)
    {
    	//mode 1 PID 0x10: Mass air flow rate
    	byte sendBytes[6] = {104,106,241,1,16,212};
    	writeToSerialPort(serialPortHandle, sendBytes, 6);
    	readFromSerialPort(serialPortHandle, recvdBytes,6,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,100);
    	if(numBytesRead < 8)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 2 bytes which indicate mass airflow rate
    	//the return packet looks like 72,107,16,65,16,x,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	double MAF = (recvdBytes[5]*256 + recvdBytes[6])/100;
    	writeData << MAF;
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //mode 1 PID 0x11
    if(mode1checkboxstate[13] != BST_UNCHECKED && success == true)
    {
    	//mode 1 PID 0x11: Throttle position
    	byte sendBytes[6] = {104,106,241,1,17,213};
    	writeToSerialPort(serialPortHandle, sendBytes, 6);
    	readFromSerialPort(serialPortHandle, recvdBytes,6,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,7,100);
    	if(numBytesRead < 7)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 1 byte which indicates throttle position
    	//the return packet looks like 72,107,16,65,17,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	double TPS = recvdBytes[5]/2.55;
    	writeData << TPS;
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //mode 1 PID 0x13
    if(mode1checkboxstate[14] != BST_UNCHECKED && success == true)
    {
    	//mode 1 PID 0x13: Oxygen sensors present
    	byte sendBytes[6] = {104,106,241,1,19,215};
    	writeToSerialPort(serialPortHandle, sendBytes, 6);
    	readFromSerialPort(serialPortHandle, recvdBytes,6,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,7,100);
    	if(numBytesRead < 7)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 1 byte which indicates how many O2 sensors are present
    	//the return packet looks like 72,107,16,65,19,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	writeData << "Bank1 O2 sensors: ";
    	if(((recvdBytes[5] >> 0) & 0x1))
    	{
    		writeData << "1 ";
    	}
    	if(((recvdBytes[5] >> 1) & 0x1))
    	{
    		writeData << "2 ";
    	}
    	if(((recvdBytes[5] >> 2) & 0x1))
    	{
    		writeData << "3 ";
    	}
    	if(((recvdBytes[5] >> 3) & 0x1))
    	{
    		writeData << "4 ";
    	}
    	writeData << "Bank2 O2 sensors: ";
    	if(((recvdBytes[5] >> 4) & 0x1))
    	{
    		writeData << "1 ";
    	}
    	if(((recvdBytes[5] >> 5) & 0x1))
    	{
    		writeData << "2 ";
    	}
    	if(((recvdBytes[5] >> 6) & 0x1))
    	{
    		writeData << "3 ";
    	}
    	if(((recvdBytes[5] >> 7) & 0x1))
    	{
    		writeData << "4 ";
    	}
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //mode 1 PID 0x14
    if(mode1checkboxstate[15] != BST_UNCHECKED && success == true)
    {
    	//mode 1 PID 0x14: Oxygen sensor 1 voltage/short term fuel trim
    	byte sendBytes[6] = {104,106,241,1,20,216};
    	writeToSerialPort(serialPortHandle, sendBytes, 6);
    	readFromSerialPort(serialPortHandle, recvdBytes,6,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,100);
    	if(numBytesRead < 8)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 2 bytes which indicate oxygen sensor voltage and STFT
    	//the return packet looks like 72,107,16,65,20,x,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	double voltage = recvdBytes[5]/200;
    	double STFT = recvdBytes[6]*1.28 - 100;
    	writeData << voltage << ' ' << STFT;
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //mode 1 PID 0x15
    if(mode1checkboxstate[16] != BST_UNCHECKED && success == true)
    {
    	//mode 1 PID 0x15: Oxygen sensor 2 voltage/short term fuel trim
    	byte sendBytes[6] = {104,106,241,1,21,217};
    	writeToSerialPort(serialPortHandle, sendBytes, 6);
    	readFromSerialPort(serialPortHandle, recvdBytes,6,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,100);
    	if(numBytesRead < 8)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 2 bytes which indicate oxygen sensor voltage and STFT
    	//the return packet looks like 72,107,16,65,21,x,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	double voltage = recvdBytes[5]/200;
    	double STFT = recvdBytes[6]*1.28 - 100;
    	writeData << voltage << ' ' << STFT;
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //mode 1 PID 0x1C
    if(mode1checkboxstate[17] != BST_UNCHECKED && success == true)
    {
    	//mode 1 PID 0x1C: OBD standards this vehicle conforms to
    	byte sendBytes[6] = {104,106,241,1,28,224};
    	writeToSerialPort(serialPortHandle, sendBytes, 6);
    	readFromSerialPort(serialPortHandle, recvdBytes,6,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,7,100);
    	if(numBytesRead < 7)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 1 byte which indicates which OBD standards the vehicle conforms to
    	//the return packet looks like 72,107,16,65,28,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	if(recvdBytes[5] == 1)
    	{
    		writeData << "OBDII as defined by the CARB";
    	}
    	else if(recvdBytes[5] == 2)
    	{
    		writeData << "OBDII as defined by the EPA";
    	}
    	else if(recvdBytes[5] == 3)
    	{
    		writeData << "OBD and OBDII";
    	}
    	else if(recvdBytes[5] == 4)
    	{
    		writeData << "OBD-I";
    	}
    	else if(recvdBytes[5] == 5)
    	{
    		writeData << "Not OBD compliant";
    	}
    	else if(recvdBytes[5] == 6)
    	{
    		writeData << "EOBD (Europe)";
    	}
    	else if(recvdBytes[5] == 7)
    	{
    		writeData << "EOBD and OBDII";
    	}
    	else if(recvdBytes[5] == 8)
    	{
    		writeData << "EOBD and OBD";
    	}
    	else if(recvdBytes[5] == 9)
    	{
    		writeData << "EOBD, OBD and OBDII";
    	}
    	else if(recvdBytes[5] == 10)
    	{
    		writeData << "JOBD (Japan)";
    	}
    	else if(recvdBytes[5] == 11)
    	{
    		writeData << "JOBD and OBDII";
    	}
    	else if(recvdBytes[5] == 12)
    	{
    		writeData << "JOBD and EOBD";
    	}
    	else if(recvdBytes[5] == 13)
    	{
    		writeData << "JOBD, EOBD, and OBDII";
    	}
    	else if(recvdBytes[5] == 17)
    	{
    		writeData << "Engine Manufacturer Diagnostics (EMD)";
    	}
    	else if(recvdBytes[5] == 18)
    	{
    		writeData << "Engine Manufacturer Diagnostics Enhanced (EMD+)";
    	}
    	else if(recvdBytes[5] == 19)
    	{
    		writeData << "Heavy Duty On-Board Diagnostics (Child/Partial) (HD OBD-C)";
    	}
    	else if(recvdBytes[5] == 20)
    	{
    		writeData << "Heavy Duty On-Board Diagnostics (HD OBD)";
    	}
    	else if(recvdBytes[5] == 21)
    	{
    		writeData << "World Wide Harmonized OBD (WWH OBD)";
    	}
    	else if(recvdBytes[5] == 23)
    	{
    		writeData << "Heavy Duty Euro OBD Stage I without NOx control (HD EOBD-I)";
    	}
    	else if(recvdBytes[5] == 24)
    	{
    		writeData << "Heavy Duty Euro OBD Stage I with NOx control (HD EOBD-I N)";
    	}
    	else if(recvdBytes[5] == 25)
    	{
    		writeData << "Heavy Duty Euro OBD Stage II without NOx control (HD EOBD-II)";
    	}
    	else if(recvdBytes[5] == 26)
    	{
    		writeData << "Heavy Duty Euro OBD Stage II with NOx control (HD EOBD-II N)";
    	}
    	else if(recvdBytes[5] == 28)
    	{
    		writeData << "Brazil OBD Phase 1 (OBDBr-1)";
    	}
    	else if(recvdBytes[5] == 29)
    	{
    		writeData << "Brazil OBD Phase 2 (OBDBr-2)";
    	}
    	else if(recvdBytes[5] == 30)
    	{
    		writeData << "Korean OBD (KOBD)";
    	}
    	else if(recvdBytes[5] == 31)
    	{
    		writeData << "India OBD I (IOBD I)";
    	}
    	else if(recvdBytes[5] == 32)
    	{
    		writeData << "India OBD II (IOBD II)";
    	}
    	else if(recvdBytes[5] == 33)
    	{
    		writeData << "Heavy Duty Euro OBD Stage VI (HD EOBD-IV)";
    	}

    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //mode 1 PID 0x1F
    if(mode1checkboxstate[18] != BST_UNCHECKED && success == true)
    {
    	//mode 1 PID 0x1F: Run time since engine start
    	byte sendBytes[6] = {104,106,241,1,31,227};
    	writeToSerialPort(serialPortHandle, sendBytes, 6);
    	readFromSerialPort(serialPortHandle, recvdBytes,6,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,100);
    	if(numBytesRead < 8)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 2 bytes which indicate engine run time
    	//the return packet looks like 72,107,16,65,31,x,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	double runTime = recvdBytes[5]*256 + recvdBytes[6];
    	writeData << runTime;
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //mode 1 PID 0x20
    if(mode1checkboxstate[19] != BST_UNCHECKED && success == true)
    {
    	//mode 1 PID 0x20: PIDs supported 21-40
    	byte sendBytes[6] = {104,106,241,1,32,228};
    	writeToSerialPort(serialPortHandle, sendBytes, 6);
    	readFromSerialPort(serialPortHandle, recvdBytes,6,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,10,100);
    	if(numBytesRead < 10)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 4 bytes which indicate which
    	//PIDs are supported from 21-40
    	//the return packet looks like 72,107,16,65,32,x,x,x,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	int PIDplace = 32;
    	for(int i = 5; i < 9; i++)//loop through bytes
    	{
    		for(int j = 7; j > -1; j--)
    		{
    			PIDplace += 1;
    			if(((recvdBytes[i] >> j) & 0x1))//shift and mask to check bit flags
    			{
    				writeData << PIDplace << ' ';
    			}
    		}
    	}
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //mode 1 PID 0x21
    if(mode1checkboxstate[20] != BST_UNCHECKED && success == true)
    {
    	//mode 1 PID 0x21: Distance traveled with malfunction indicator lamp (MIL) on
    	byte sendBytes[6] = {104,106,241,1,33,229};
    	writeToSerialPort(serialPortHandle, sendBytes, 6);
    	readFromSerialPort(serialPortHandle, recvdBytes,6,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,100);
    	if(numBytesRead < 8)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 2 bytes which indicate distance traveled
    	//with the CEL on
    	//the return packet looks like 72,107,16,65,33,x,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	double distance = recvdBytes[5]*256 + recvdBytes[6];
    	writeData << distance;
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //mode 1 PID 0x2E
    if(mode1checkboxstate[21] != BST_UNCHECKED && success == true)
    {
    	//mode 1 PID 0x2E: commanded evaportative purge
    	byte sendBytes[6] = {104,106,241,1,46,242};
    	writeToSerialPort(serialPortHandle, sendBytes, 6);
    	readFromSerialPort(serialPortHandle, recvdBytes,6,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,7,100);
    	if(numBytesRead < 7)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 1 byte which indicates the commanded evap purge
    	//the return packet looks like 72,107,16,65,46,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	double evap = recvdBytes[5]/2.55;
    	writeData << evap;
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //mode 1 PID 0x2F
    if(mode1checkboxstate[22] != BST_UNCHECKED && success == true)
    {
    	//mode 1 PID 0x2F: Fuel tank level
    	byte sendBytes[6] = {104,106,241,1,47,243};
    	writeToSerialPort(serialPortHandle, sendBytes, 6);
    	readFromSerialPort(serialPortHandle, recvdBytes,6,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,7,100);
    	if(numBytesRead < 7)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 1 byte which indicates the fuel tank input level
    	//the return packet looks like 72,107,16,65,47,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	double fuelLevel = recvdBytes[5]/2.55;
    	writeData << fuelLevel;
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //mode 1 PID 0x33
    if(mode1checkboxstate[23] != BST_UNCHECKED && success == true)
    {
    	//mode 1 PID 0x33: Absolute barometric pressure
    	byte sendBytes[6] = {104,106,241,1,51,247};
    	writeToSerialPort(serialPortHandle, sendBytes, 6);
    	readFromSerialPort(serialPortHandle, recvdBytes,6,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,7,100);
    	if(numBytesRead < 7)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 1 byte which indicates the absolute barometric pressure
    	//the return packet looks like 72,107,16,65,51,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	double pressure = recvdBytes[5];
    	writeData << pressure;
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //mode 1 PID 0x40
    if(mode1checkboxstate[24] != BST_UNCHECKED && success == true)
    {
    	//mode 1 PID 0x40: PIDs supported 0x41 to 0x60
    	byte sendBytes[6] = {104,106,241,1,64,4};
    	writeToSerialPort(serialPortHandle, sendBytes, 6);
    	readFromSerialPort(serialPortHandle, recvdBytes,6,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,10,100);
    	if(numBytesRead < 10)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 4 bytes which indicate available OBD PIDs from 0x41 to 0x60
    	//the return packet looks like 72,107,16,65,64,x,x,x,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	int PIDplace = 64;
    	for(int i = 5; i < 9; i++)//loop through bytes
    	{
    		for(int j = 7; j > -1; j--)
    		{
    			PIDplace += 1;
    			if(((recvdBytes[i] >> j) & 0x1))//shift and mask to check bit flags
    			{
    				writeData << PIDplace << ' ';
    			}
    		}
    	}
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //mode 1 PID 0x42
    if(mode1checkboxstate[25] != BST_UNCHECKED && success == true)
    {
    	//mode 1 PID 0x42: Control module voltage
    	byte sendBytes[6] = {104,106,241,1,66,6};
    	writeToSerialPort(serialPortHandle, sendBytes, 6);
    	readFromSerialPort(serialPortHandle, recvdBytes,6,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,100);
    	if(numBytesRead < 8)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 2 byte which indicate the control module voltage
    	//the return packet looks like 72,107,16,65,66,x,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	double voltage = (recvdBytes[5]*256+recvdBytes[6])/1000;
    	writeData << voltage;
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //mode 1 PID 0x43
    if(mode1checkboxstate[26] != BST_UNCHECKED && success == true)
    {
    	//mode 1 PID 0x43: Absolute load value
    	byte sendBytes[6] = {104,106,241,1,67,7};
    	writeToSerialPort(serialPortHandle, sendBytes, 6);
    	readFromSerialPort(serialPortHandle, recvdBytes,6,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,100);
    	if(numBytesRead < 8)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 2 byte which indicate the absolute load
    	//the return packet looks like 72,107,16,65,67,x,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	double load = (recvdBytes[5]*256+recvdBytes[6])/2.55;
    	writeData << load;
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //mode 1 PID 0x45
    if(mode1checkboxstate[27] != BST_UNCHECKED && success == true)
    {
    	//mode 1 PID 0x45: Relative throttle position
    	byte sendBytes[6] = {104,106,241,1,69,9};
    	writeToSerialPort(serialPortHandle, sendBytes, 6);
    	readFromSerialPort(serialPortHandle, recvdBytes,6,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,7,100);
    	if(numBytesRead < 7)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 1 byte which indicates the relative throttle position
    	//the return packet looks like 72,107,16,65,69,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	double throttlePos = recvdBytes[5]/2.55;
    	writeData << throttlePos;
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }


    //******************************************************************************
    //11 PIDs to check for mode 2
    //PID 0
    if(mode2checkboxstate[0] != BST_UNCHECKED && success == true)
    {
    	//mode 2 PID 0: list of valid PIDs 0x1-0x20
    	byte sendBytes[7] = {104,106,241,2,0,0,197};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,100);
    	if(numBytesRead < 11)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 4 bytes which are binary flags
    	//for the next 32 PIDs indicating if they work with this car
    	//the return packet looks like 72,107,16,66,0,0,x,x,x,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	int PIDplace = 0;
    	for(int i = 6; i < 10; i++)//loop through bytes
    	{
    		for(int j = 7; j > -1; j--)
    		{
    			PIDplace += 1;
    			if(((recvdBytes[i] >> j) & 0x1))//shift and mask to check bit flags
    			{
    				writeData << PIDplace << ' ';
    			}
    		}
    	}
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //MODE 2 PID 2
    if(mode2checkboxstate[1] != BST_UNCHECKED && success == true)
    {
    	//mode 2 PID 2: freeze DTC
    	byte sendBytes[7] = {104,106,241,2,2,0,199};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,9,100);
    	if(numBytesRead < 9)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 2 bytes which are binary flags
    	//indicating which DTC triggered the freeze frame
    	//the return packet looks like 72,107,16,66,2,0,x,x,sum
    	writeBytes << ',';
    	writeData << ',';

    	if(recvdBytes[6] == 0 && recvdBytes[7] == 0)
    	{
    		writeData << "no DTC";
    	}
    	else
    	{
    		int letterCode = ((recvdBytes[6] >> 7) & 0x1)*2 + ((recvdBytes[6] >> 6) & 0x1);//will be 0,1,2, or 3
    		if(letterCode == 0)
    		{
    			writeData << 'P';
    		}
    		else if(letterCode == 1)
    		{
    			writeData << 'C';
    		}
    		else if(letterCode == 2)
    		{
    			writeData << 'B';
    		}
    		else if(letterCode == 3)
    		{
    			writeData << 'U';
    		}
    		int firstDecimal = ((recvdBytes[6] >> 5) & 0x1)*2 + ((recvdBytes[6] >> 4) & 0x1);
    		int secondDecimal = ((recvdBytes[6] >> 3) & 0x1)*8 + ((recvdBytes[6] >> 2) & 0x1)*4 + ((recvdBytes[6] >> 1) & 0x1)*2 + ((recvdBytes[6] >> 0) & 0x1);
    		int thirdDecimal = ((recvdBytes[7] >> 7) & 0x1)*8 + ((recvdBytes[7] >> 6) & 0x1)*4 + ((recvdBytes[7] >> 5) & 0x1)*2 + ((recvdBytes[7] >> 4) & 0x1);
    		int fourthDecimal = ((recvdBytes[7] >> 3) & 0x1)*8 + ((recvdBytes[7] >> 2) & 0x1)*4 + ((recvdBytes[7] >> 1) & 0x1)*2 + ((recvdBytes[7] >> 0) & 0x1);
    		writeData << firstDecimal << secondDecimal << thirdDecimal << fourthDecimal;
    	}
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //mode 2 PID 3
    if(mode2checkboxstate[2] != BST_UNCHECKED && success == true)
    {
    	//mode 2 PID 3: fuel system status
    	byte sendBytes[7] = {104,106,241,2,3,0,200};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,9,100);
    	if(numBytesRead < 9)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 2 bytes which are byte flags
    	//for the next 32 PIDs indicating if they work with this car
    	//the return packet looks like 72,107,16,66,3,0,x,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	if(recvdBytes[6] != 0){
    		writeData << " Fueling system 1: ";
    		if(((recvdBytes[6] >> 0) & 0x1))
    		{
    			writeData << "Open loop due to insufficient engine temperature";
    		}
    		else if(((recvdBytes[6] >> 1) & 0x1))
    		{
    			writeData << "Closed loop, using oxygen sensor feedback to determine fuel mix";
    		}
    		else if(((recvdBytes[6] >> 2) & 0x1))
    		{
    			writeData << "Open loop due to engine load OR fuel cut due to deceleration";
    		}
    		else if(((recvdBytes[6] >> 3) & 0x1))
    		{
    			writeData << "Open loop due to system failure";
    		}
    		else if(((recvdBytes[6] >> 4) & 0x1))
    		{
    			writeData << "Closed loop, using at least one oxygen sensor but there is a fault in the feedback system";
    		}
    	}

    	if(recvdBytes[7] != 0){
    		writeData << " Fueling system 2: ";
    		if(((recvdBytes[7] >> 0) & 0x1))
    		{
    			writeData << "Open loop due to insufficient engine temperature";
    		}
    		else if(((recvdBytes[7] >> 1) & 0x1))
    		{
    			writeData << "Closed loop, using oxygen sensor feedback to determine fuel mix";
    		}
    		else if(((recvdBytes[7] >> 2) & 0x1))
    		{
    			writeData << "Open loop due to engine load OR fuel cut due to deceleration";
    		}
    		else if(((recvdBytes[7] >> 3) & 0x1))
    		{
    			writeData << "Open loop due to system failure";
    		}
    		else if(((recvdBytes[7] >> 4) & 0x1))
    		{
    			writeData << "Closed loop, using at least one oxygen sensor but there is a fault in the feedback system";
    		}
    	}
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }

    //mode 2 PID 4
    if(mode2checkboxstate[3] != BST_UNCHECKED && success == true)
    {
    	//mode 2 PID 4: calculated engine load
    	byte sendBytes[7] = {104,106,241,2,4,0,201};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,100);
    	if(numBytesRead < 8)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 1 byte which indicates engine load
    	//the return packet looks like 72,107,16,66,4,0,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	double load = recvdBytes[6]/2.55;
    	writeData << load;
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //mode 2 PID 5
    if(mode2checkboxstate[4] != BST_UNCHECKED && success == true)
    {
    	//mode 2 PID 5: engine coolant temperature
    	byte sendBytes[7] = {104,106,241,2,5,0,202};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,100);
    	if(numBytesRead < 8)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 1 byte which indicates engine coolant temp
    	//the return packet looks like 72,107,16,66,5,0,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	int temp = recvdBytes[6]-40;
    	writeData << temp;
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //mode 2 PID 6
    if(mode2checkboxstate[5] != BST_UNCHECKED && success == true)
    {
    	//mode 2 PID 6: short term fuel trim: bank 1
    	byte sendBytes[7] = {104,106,241,2,6,0,203};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,100);
    	if(numBytesRead < 8)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 1 byte which indicates STFT
    	//the return packet looks like 72,107,16,66,6,0,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	double STFT = recvdBytes[6]/1.28-100;
    	writeData << STFT;
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //mode 2 PID 7
    if(mode2checkboxstate[6] != BST_UNCHECKED && success == true)
    {
    	//mode 2 PID 7: long term fuel trim: bank 1
    	byte sendBytes[7] = {104,106,241,2,7,0,204};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,100);
    	if(numBytesRead < 8)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 1 byte which indicates LTFT
    	//the return packet looks like 72,107,16,66,7,0,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	double LTFT = recvdBytes[6]/1.28-100;
    	writeData << LTFT;
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //mode 2 PID 0xC
    if(mode2checkboxstate[7] != BST_UNCHECKED && success == true)
    {
    	//mode 2 PID 0xC: engine RPM
    	byte sendBytes[7] = {104,106,241,2,12,0,209};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,9,100);
    	if(numBytesRead < 9)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 2 bytes which indicate engine RPM
    	//the return packet looks like 72,107,16,66,12,0,x,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	double RPM = (recvdBytes[6]*256+recvdBytes[7])/4;
    	writeData << RPM;
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //mode 2 PID 0xD
    if(mode2checkboxstate[8] != BST_UNCHECKED && success == true)
    {
    	//mode 2 PID 0xD: Vehicle speed
    	byte sendBytes[7] = {104,106,241,2,13,0,210};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,100);
    	if(numBytesRead < 8)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 1 byte which indicates vehicle speed in km/h
    	//the return packet looks like 72,107,16,66,13,0,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	double speed = recvdBytes[6];
    	writeData << speed;
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //mode 2 PID 0x10
    if(mode2checkboxstate[9] != BST_UNCHECKED && success == true)
    {
    	//mode 2 PID 0x10: Mass air flow rate
    	byte sendBytes[7] = {104,106,241,2,16,0,213};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,9,100);
    	if(numBytesRead < 9)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 2 bytes which indicate mass airflow rate
    	//the return packet looks like 72,107,16,66,16,0,x,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	double MAF = (recvdBytes[6]*256 + recvdBytes[7])/100;
    	writeData << MAF;
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //mode 2 PID 0x11
    if(mode2checkboxstate[10] != BST_UNCHECKED && success == true)
    {
    	//mode 2 PID 0x11: Throttle position
    	byte sendBytes[7] = {104,106,241,2,17,0,214};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,100);
    	if(numBytesRead < 8)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 1 byte which indicates throttle position
    	//the return packet looks like 72,107,16,66,17,0,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	double TPS = recvdBytes[6]/2.55;
    	writeData << TPS;
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }


    //****************************************************************************
    //mode 3
    if(mode3checkboxstate != BST_UNCHECKED && success == true)
    {
    	//mode 3: Read DTCs
    	byte sendBytes[5] = {104,106,241,3,198};//message for MODE 0x3 PID 0
    	writeToSerialPort(serialPortHandle, sendBytes, 5);
    	readFromSerialPort(serialPortHandle, recvdBytes,5,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,100,300);
    	if(numBytesRead < 11)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return pairs of bytes which indicate DTCs
    	//the return packet looks like 72,107,16,67,x,x,x,x,x,x,sum
    	writeBytes << ',';
    	writeData << ',';

    	if(recvdBytes[4] == 0 && recvdBytes[5] == 0)
    	{
    		writeData << "no DTC";
    	}
    	else
    	{
    		int index = 4;
    		for(int i = 0;i < 3; i++)//loop through the bytes to interpret DTCs
    		{
    			if(!(recvdBytes[i*2+index] == 0 && recvdBytes[i*2+1+index] == 0))
    			{
    				std::string DTC = decodeDTC(recvdBytes[i*2+index],recvdBytes[i*2+1+index]);
    				writeData << DTC << ' ';
    			}
    			if(numBytesRead > index+7 && i*2+4+index > index+7)
    			{
    				i = 0;
    				index += 11;
    			}
    		}
    	}

    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //********************************************************************************
    //mode 4
    if(mode4checkboxstate != BST_UNCHECKED && success == true)
    {
    	//mode 4: Clear DTCs
    	byte sendBytes[5] = {104,106,241,4,199};//message for MODE 0x4 PID 0
    	writeToSerialPort(serialPortHandle, sendBytes, 5);
    	readFromSerialPort(serialPortHandle, recvdBytes,5,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,5,100);
    	if(numBytesRead < 5)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return a blank message
    	//the return packet looks like 72,107,16,68,sum
    	writeBytes << ',';
    	writeData << ',';
    	writeData << "DTCs cleared";
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }

    //************************************************************************************

    //9 PIDs to check for mode 5
    //PID 0x100
    if(mode5checkboxstate[0] != BST_UNCHECKED && success == true)
    {
    	//mode 5 PID 0x100: list of valid PIDs 0x0-0x20
    	byte sendBytes[7] = {104,106,241,5,0,1,201};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,100);
    	if(numBytesRead < 11)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 4 bytes which are binary flags
    	//for the next 32 PIDs indicating if they work with this car
    	//the return packet looks like 72,107,16,69,0,1,x,x,x,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	int PIDplace = 0;
    	for(int i = 6; i < 10; i++)//loop through bytes
    	{
    		for(int j = 7; j > -1; j--)
    		{
    			PIDplace += 1;
    			if(((recvdBytes[i] >> j) & 0x1))//shift and mask to check bit flags
    			{
    				writeData << PIDplace << ' ';
    			}
    		}
    	}
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //mode 5 PID 0x101
    if(mode5checkboxstate[1] != BST_UNCHECKED && success == true)
    {
    	//mode 5 PID 0x101: Oxygen sensor monitor bank 1 sensor 1 voltage
    	byte sendBytes[7] = {104,106,241,5,1,1,202};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,100);
    	if(numBytesRead < 8)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 1 byte which indicates oxygen sensor voltage
    	//the return packet looks like 72,107,16,69,1,1,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	double voltage = recvdBytes[6]/200;
    	writeData << voltage;
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //mode 5 PID 0x102
    if(mode5checkboxstate[2] != BST_UNCHECKED && success == true)
    {
    	//mode 5 PID 0x102: Oxygen sensor monitor bank 1 sensor 2 voltage
    	byte sendBytes[7] = {104,106,241,5,2,1,203};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,100);
    	if(numBytesRead < 8)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 1 byte which indicates oxygen sensor voltage
    	//the return packet looks like 72,107,16,69,2,1,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	double voltage = recvdBytes[6]/200;
    	writeData << voltage;
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //mode 5 PID 0x103
    if(mode5checkboxstate[3] != BST_UNCHECKED && success == true)
    {
    	//mode 5 PID 0x103: Oxygen sensor monitor bank 1 sensor 3 voltage
    	byte sendBytes[7] = {104,106,241,5,3,1,204};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,100);
    	if(numBytesRead < 8)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 1 byte which indicates oxygen sensor voltage
    	//the return packet looks like 72,107,16,69,3,1,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	double voltage = recvdBytes[6]/200;
    	writeData << voltage;
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //mode 5 PID 0x104
    if(mode5checkboxstate[4] != BST_UNCHECKED && success == true)
    {
    	//mode 5 PID 0x104: Oxygen sensor monitor bank 1 sensor 4 voltage
    	byte sendBytes[7] = {104,106,241,5,4,1,205};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,100);
    	if(numBytesRead < 8)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 1 byte which indicates oxygen sensor voltage
    	//the return packet looks like 72,107,16,69,4,1,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	double voltage = recvdBytes[6]/200;
    	writeData << voltage;
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //mode 5 PID 0x105
    if(mode5checkboxstate[5] != BST_UNCHECKED && success == true)
    {
    	//mode 5 PID 0x105: Oxygen sensor monitor bank 2 sensor 1 voltage
    	byte sendBytes[7] = {104,106,241,5,5,1,206};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,100);
    	if(numBytesRead < 8)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 1 byte which indicates oxygen sensor voltage
    	//the return packet looks like 72,107,16,69,5,1,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	double voltage = recvdBytes[6]/200;
    	writeData << voltage;
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //mode 5 PID 0x106
    if(mode5checkboxstate[6] != BST_UNCHECKED && success == true)
    {
    	//mode 5 PID 0x106: Oxygen sensor monitor bank 2 sensor 2 voltage
    	byte sendBytes[7] = {104,106,241,5,6,1,207};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,100);
    	if(numBytesRead < 8)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 1 byte which indicates oxygen sensor voltage
    	//the return packet looks like 72,107,16,69,6,1,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	double voltage = recvdBytes[6]/200;
    	writeData << voltage;
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //mode 5 PID 0x107
    if(mode5checkboxstate[7] != BST_UNCHECKED && success == true)
    {
    	//mode 5 PID 0x107: Oxygen sensor monitor bank 2 sensor 3 voltage
    	byte sendBytes[7] = {104,106,241,5,7,1,208};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,100);
    	if(numBytesRead < 8)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 1 byte which indicates oxygen sensor voltage
    	//the return packet looks like 72,107,16,69,7,1,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	double voltage = recvdBytes[6]/200;
    	writeData << voltage;
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //mode 5 PID 0x108
    if(mode5checkboxstate[8] != BST_UNCHECKED && success == true)
    {
    	//mode 5 PID 0x108: Oxygen sensor monitor bank 2 sensor 4 voltage
    	byte sendBytes[7] = {104,106,241,5,8,1,209};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,100);
    	if(numBytesRead < 8)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 1 byte which indicates oxygen sensor voltage
    	//the return packet looks like 72,107,16,69,8,1,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	double voltage = recvdBytes[6]/200;
    	writeData << voltage;
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }

    //******************************************************************************

    //6 PIDs to check for mode 6
    //PID 0x0
    if(mode6checkboxstate[0] != BST_UNCHECKED && success == true)
    {
    	//mode 6 PID 0x0: list of valid PIDs 0x0-0x20?
    	byte sendBytes[6] = {104,106,241,6,0,201};
    	writeToSerialPort(serialPortHandle, sendBytes, 6);
    	readFromSerialPort(serialPortHandle, recvdBytes,6,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,100);
    	if(numBytesRead < 11)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 4 bytes which are (?) binary flags
    	//for the next 32 PIDs indicating if they work with this car
    	//the return packet looks like 72,107,16,70,0,255,x,x,x,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	int PIDplace = 0;
    	for(int i = 6; i < 10; i++)//loop through bytes
    	{
    		for(int j = 7; j > -1; j--)
    		{
    			PIDplace += 1;
    			if(((recvdBytes[i] >> j) & 0x1))//shift and mask to check bit flags
    			{
    				writeData << PIDplace << ' ';
    			}
    		}
    	}
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //PID 0x1
    if(mode6checkboxstate[1] != BST_UNCHECKED && success == true)
    {
    	//mode 6 PID 0x1: ?
    	byte sendBytes[6] = {104,106,241,6,1,202};
    	writeToSerialPort(serialPortHandle, sendBytes, 6);
    	readFromSerialPort(serialPortHandle, recvdBytes,6,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,100);
    	if(numBytesRead < 11)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 4 bytes which are (?)
    	//the return packet looks like 72,107,16,70,1,1,x,x,x,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	for(int i = 6; i < 10; i++)//loop through bytes
    	{
    		writeData << recvdBytes[i] << ' ';
    	}
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //PID 0x2
    if(mode6checkboxstate[2] != BST_UNCHECKED && success == true)
    {
    	//mode 6 PID 0x2: ?
    	byte sendBytes[6] = {104,106,241,6,2,203};
    	writeToSerialPort(serialPortHandle, sendBytes, 6);
    	readFromSerialPort(serialPortHandle, recvdBytes,6,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,100);
    	if(numBytesRead < 11)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 4 bytes which are (?)
    	//the return packet looks like 72,107,16,70,2,1,x,x,x,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	for(int i = 6; i < 10; i++)//loop through bytes
    	{
    		writeData << recvdBytes[i] << ' ';
    	}
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //PID 0x3
    if(mode6checkboxstate[3] != BST_UNCHECKED && success == true)
    {
    	//mode 6 PID 0x3: ?
    	byte sendBytes[6] = {104,106,241,6,3,204};
    	writeToSerialPort(serialPortHandle, sendBytes, 6);
    	readFromSerialPort(serialPortHandle, recvdBytes,6,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,100);
    	if(numBytesRead < 11)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 4 bytes which are (?)
    	//the return packet looks like 72,107,16,70,3,1,x,x,x,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	for(int i = 6; i < 10; i++)//loop through bytes
    	{
    		writeData << recvdBytes[i] << ' ';
    	}
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //PID 0x4
    if(mode6checkboxstate[4] != BST_UNCHECKED && success == true)
    {
    	//mode 6 PID 0x4: ?
    	byte sendBytes[6] = {104,106,241,6,4,205};
    	writeToSerialPort(serialPortHandle, sendBytes, 6);
    	readFromSerialPort(serialPortHandle, recvdBytes,6,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,100);
    	if(numBytesRead < 11)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 4 bytes which are (?)
    	//the return packet looks like 72,107,16,70,4,1,x,x,x,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	for(int i = 6; i < 10; i++)//loop through bytes
    	{
    		writeData << recvdBytes[i] << ' ';
    	}
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //PID 0x5
    if(mode6checkboxstate[5] != BST_UNCHECKED && success == true)
    {
    	//mode 6 PID 0x5: ?
    	byte sendBytes[6] = {104,106,241,6,5,206};
    	writeToSerialPort(serialPortHandle, sendBytes, 6);
    	readFromSerialPort(serialPortHandle, recvdBytes,6,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,100);
    	if(numBytesRead < 11)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 4 bytes which are (?)
    	//the return packet looks like 72,107,16,70,5,1,x,x,x,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	for(int i = 6; i < 10; i++)//loop through bytes
    	{
    		writeData << recvdBytes[i] << ' ';
    	}
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }

    //*************************************************************************

    //mode 7
    if(mode7checkboxstate != BST_UNCHECKED && success == true)
    {
    	//mode 7: Read pending DTCs
    	byte sendBytes[5] = {104,106,241,7,202};//message for MODE 0x7 PID 0
    	writeToSerialPort(serialPortHandle, sendBytes, 5);
    	readFromSerialPort(serialPortHandle, recvdBytes,5,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,100,300);
    	if(numBytesRead < 11)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return pairs of bytes which indicate DTCs
    	//the return packet looks like 72,107,16,71,x,x,x,x,x,x,sum
    	writeBytes << ',';
    	writeData << ',';

    	if(recvdBytes[4] == 0 && recvdBytes[5] == 0)
    	{
    		writeData << "no pending DTC";
    	}
    	else
    	{
    		int index = 4;
    		for(int i = 0;i < 3; i++)//loop through the bytes to interpret DTCs
    		{
    			if(!(recvdBytes[i*2+index] == 0 && recvdBytes[i*2+1+index] == 0))
    			{
    				std::string DTC = decodeDTC(recvdBytes[i*2+index],recvdBytes[i*2+1+index]);
    				writeData << DTC << ' ';
    			}
    			if(numBytesRead > index+7 && i*2+4+index > index+7)
    			{
    				i = 0;
    				index += 11;
    			}
    		}
    	}

    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //********************************************************************************

    //2 PIDs to check for mode 8
    //PID 0x0
    if(mode8checkboxstate[0] != BST_UNCHECKED && success == true)
    {
    	//mode 8 PID 0x0: ?
    	byte sendBytes[6] = {104,106,241,8,0,203};
    	writeToSerialPort(serialPortHandle, sendBytes, 6);
    	readFromSerialPort(serialPortHandle, recvdBytes,6,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,100);
    	if(numBytesRead < 11)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 4 bytes which are (?)
    	//the return packet looks like 72,107,16,72,0,0,x,x,x,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	for(int i = 6; i < 10; i++)//loop through bytes
    	{
    		writeData << recvdBytes[i] << ' ';
    	}
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //PID 0x1
    if(mode8checkboxstate[1] != BST_UNCHECKED && success == true)
    {
    	//mode 8 PID 0x1: ?
    	byte sendBytes[6] = {104,106,241,8,1,204};
    	writeToSerialPort(serialPortHandle, sendBytes, 6);
    	readFromSerialPort(serialPortHandle, recvdBytes,6,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,100);
    	if(numBytesRead < 7)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 4 bytes which are (?)
    	//the return packet looks like 72,107,16,72,1,0,0,0,0,0,sum
    	// or 72,107,16,127,8,34,sum
    	writeBytes << ',';
    	writeData << ',';
    	writeData << "see byte log";
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }

    //********************************************************************************

    //Mode 0x9
    //PID 0x0
    if(mode9checkboxstate[0] != BST_UNCHECKED && success == true)
    {
    	//mode 9 PID 0x0: list of valid PIDs 0x0-0x20?
    	byte sendBytes[6] = {104,106,241,9,0,204};
    	writeToSerialPort(serialPortHandle, sendBytes, 6);
    	readFromSerialPort(serialPortHandle, recvdBytes,6,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,100);
    	if(numBytesRead < 11)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 4 bytes which are (?) binary flags
    	//for the next 32 PIDs indicating if they work with this car
    	//the return packet looks like 72,107,16,73,0,1,x,x,x,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	int PIDplace = 0;
    	for(int i = 6; i < 10; i++)//loop through bytes
    	{
    		for(int j = 7; j > -1; j--)
    		{
    			PIDplace += 1;
    			if(((recvdBytes[i] >> j) & 0x1))//shift and mask to check bit flags
    			{
    				writeData << PIDplace << ' ';
    			}
    		}
    	}
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //PID 0x1
    if(mode9checkboxstate[1] != BST_UNCHECKED && success == true)
    {
    	//mode 9 PID 0x1: number of packets with VIN data
    	byte sendBytes[6] = {104,106,241,9,1,205};
    	writeToSerialPort(serialPortHandle, sendBytes, 6);
    	readFromSerialPort(serialPortHandle, recvdBytes,6,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,7,100);
    	if(numBytesRead < 7)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 1 byte which is the number of packets
    	//used to transmit the VIN data
    	//the return packet looks like 72,107,16,73,1,5,sum
    	writeBytes << ',';
    	writeData << ',';
        writeData << recvdBytes[5];
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //PID 0x2
    if(mode9checkboxstate[2] != BST_UNCHECKED && success == true)
    {
    	//mode 9 PID 0x2: VIN data
    	byte sendBytes[6] = {104,106,241,9,2,206};
    	writeToSerialPort(serialPortHandle, sendBytes, 6);
    	readFromSerialPort(serialPortHandle, recvdBytes,6,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,55,100);
    	if(numBytesRead < 11)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return multiple packets with 4 bytes each
    	//used to transmit the VIN data
    	//the return packet looks like 72,107,16,73,2,packet no.,X,X,X,X,sum
    	writeBytes << ',';
    	writeData << ',';
    	int packetPlace = 0;
    	for(int i = 0; i < numBytesRead;i++)
    	{
    		if((i-packetPlace) > 5 && (i-packetPlace) < 10)
    		{
    			byte data = recvdBytes[i-packetPlace];
    			if(data >= 48 && data <= 57 && data >= 65 && data <= 90)//include ascii 0-9 and A-Z
    			{
    				writeData << data;
    			}
    		}
    		else if((i-packetPlace) == 10)
    		{
    			packetPlace += 11;
    		}
    	}
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //PID 0x3
    if(mode9checkboxstate[3] != BST_UNCHECKED && success == true)
    {
    	//mode 9 PID 0x3: number of packets used to send calibration ID data
    	byte sendBytes[6] = {104,106,241,9,3,207};
    	writeToSerialPort(serialPortHandle, sendBytes, 6);
    	readFromSerialPort(serialPortHandle, recvdBytes,6,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,7,100);
    	if(numBytesRead < 7)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 1 byte which is the number of packets
    	//used to transmit the calibration ID data
    	//the return packet looks like 72,107,16,73,3,4,sum
    	writeBytes << ',';
    	writeData << ',';
        writeData << recvdBytes[5];
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //PID 0x4
    if(mode9checkboxstate[4] != BST_UNCHECKED && success == true)
    {
    	//mode 9 PID 0x4: calibration ID data
    	byte sendBytes[6] = {104,106,241,9,4,208};
    	writeToSerialPort(serialPortHandle, sendBytes, 6);
    	readFromSerialPort(serialPortHandle, recvdBytes,6,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,44,100);
    	if(numBytesRead < 11)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return multiple packets with 4 bytes each
    	//used to transmit the calibration ID data
    	//the return packet looks like 72,107,16,73,4,packet no.,X,X,X,X,sum
    	writeBytes << ',';
    	writeData << ',';
    	int packetPlace = 0;
    	for(int i = 0; i < numBytesRead;i++)
    	{
    		if((i-packetPlace) > 5 && (i-packetPlace) < 10)
    		{
    			byte data = recvdBytes[i-packetPlace];
    			if(data >= 32 && data <= 122)//include ascii chars
    			{
    				writeData << data;
    			}
    		}
    		else if((i-packetPlace) == 10)
    		{
    			packetPlace += 11;
    		}
    	}
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //PID 0x5
    if(mode9checkboxstate[5] != BST_UNCHECKED && success == true)
    {
    	//mode 9 PID 0x5: number of packets used to send calibration validation number data
    	byte sendBytes[6] = {104,106,241,9,5,209};
    	writeToSerialPort(serialPortHandle, sendBytes, 6);
    	readFromSerialPort(serialPortHandle, recvdBytes,6,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,7,100);
    	if(numBytesRead < 7)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 1 byte which is the number of packets
    	//used to transmit the calibration validation number data
    	//the return packet looks like 72,107,16,73,5,1,sum
    	writeBytes << ',';
    	writeData << ',';
        writeData << recvdBytes[5];
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //PID 0x6
    if(mode9checkboxstate[6] != BST_UNCHECKED && success == true)
    {
    	//mode 9 PID 0x6: calibration validation number data
    	byte sendBytes[6] = {104,106,241,9,6,210};
    	writeToSerialPort(serialPortHandle, sendBytes, 6);
    	readFromSerialPort(serialPortHandle, recvdBytes,6,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,100);
    	if(numBytesRead < 11)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 2 bytes which is the calibration validation number data
    	//the return packet looks like 72,107,16,73,6,1,0,0,x,x,sum
    	writeBytes << ',';
    	writeData << ',';
        writeData << recvdBytes[8]*256 + recvdBytes[9];
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }

    //**************************************************************************************

    //Mode 0x22
    //PID 512
    if(mode22checkboxstate[0] != BST_UNCHECKED && success == true)
    {
    	//mode 0x22 PID 512: list of valid PIDs 0x0-0x20?
    	byte sendBytes[7] = {104,106,241,34,2,0,231};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,100);
    	if(numBytesRead < 11)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//this should return 4 bytes which are (?) binary flags
    	//for the next 32 PIDs indicating if they work with this car
    	//the return packet looks like 72,107,16,98,2,0,x,x,x,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	int PIDplace = 0;
    	for(int i = 6; i < 10; i++)//loop through bytes
    	{
    		for(int j = 7; j > -1; j--)
    		{
    			PIDplace += 1;
    			if(((recvdBytes[i] >> j) & 0x1))//shift and mask to check bit flags
    			{
    				writeData << PIDplace << ' ';
    			}
    		}
    	}
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //PID 513
    if(mode22checkboxstate[1] != BST_UNCHECKED && success == true)
    {
    	//mode 0x22 PID 513: ?
    	byte sendBytes[7] = {104,106,241,34,2,1,232};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,100);
    	if(numBytesRead < 8)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//
    	//the return packet looks like 72,107,16,98,2,1,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	writeData << recvdBytes[6];
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //PID 515
    if(mode22checkboxstate[2] != BST_UNCHECKED && success == true)
    {
    	//mode 0x22 PID 515: ?
    	byte sendBytes[7] = {104,106,241,34,2,3,234};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,100);
    	if(numBytesRead < 11)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//
    	//the return packet looks like 72,107,16,98,2,3,x,x,x,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	writeData << 256*(256*((256*recvdBytes[6])+recvdBytes[7])+recvdBytes[8])+recvdBytes[9];
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //PID 516
    if(mode22checkboxstate[3] != BST_UNCHECKED && success == true)
    {
    	//mode 0x22 PID 516: ?
    	byte sendBytes[7] = {104,106,241,34,2,4,235};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,100);
    	if(numBytesRead < 8)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//
    	//the return packet looks like 72,107,16,98,2,4,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	writeData << recvdBytes[6];
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //PID 517
    if(mode22checkboxstate[4] != BST_UNCHECKED && success == true)
    {
    	//mode 0x22 PID 517: ?
    	byte sendBytes[7] = {104,106,241,34,2,5,236};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,9,100);
    	if(numBytesRead < 9)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//
    	//the return packet looks like 72,107,16,98,2,5,x,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	writeData << 256*recvdBytes[6]+recvdBytes[7];
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //PID 518
    if(mode22checkboxstate[5] != BST_UNCHECKED && success == true)
    {
    	//mode 0x22 PID 518: ?
    	byte sendBytes[7] = {104,106,241,34,2,6,237};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,100);
    	if(numBytesRead < 8)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//
    	//the return packet looks like 72,107,16,98,2,6,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	writeData << recvdBytes[6];
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //PID 519
    if(mode22checkboxstate[6] != BST_UNCHECKED && success == true)
    {
    	//mode 0x22 PID 519: ?
    	byte sendBytes[7] = {104,106,241,34,2,7,238};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,9,100);
    	if(numBytesRead < 9)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//
    	//the return packet looks like 72,107,16,98,2,7,x,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	writeData << 256*recvdBytes[6]+recvdBytes[7];
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //PID 520
    if(mode22checkboxstate[7] != BST_UNCHECKED && success == true)
    {
    	//mode 0x22 PID 520: ?
    	byte sendBytes[7] = {104,106,241,34,2,8,239};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,9,100);
    	if(numBytesRead < 9)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//
    	//the return packet looks like 72,107,16,98,2,8,x,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	writeData << 256*recvdBytes[6]+recvdBytes[7];
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //PID 521
    if(mode22checkboxstate[8] != BST_UNCHECKED && success == true)
    {
    	//mode 0x22 PID 521: ?
    	byte sendBytes[7] = {104,106,241,34,2,9,240};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,100);
    	if(numBytesRead < 8)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//
    	//the return packet looks like 72,107,16,98,2,9,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	writeData << recvdBytes[6];
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //PID 522
    if(mode22checkboxstate[9] != BST_UNCHECKED && success == true)
    {
    	//mode 0x22 PID 522: ?
    	byte sendBytes[7] = {104,106,241,34,2,10,241};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,9,100);
    	if(numBytesRead < 9)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//
    	//the return packet looks like 72,107,16,98,2,10,x,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	writeData << 256*recvdBytes[6]+recvdBytes[7];
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //PID 523
    if(mode22checkboxstate[10] != BST_UNCHECKED && success == true)
    {
    	//mode 0x22 PID 523: ?
    	byte sendBytes[7] = {104,106,241,34,2,11,242};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,9,100);
    	if(numBytesRead < 9)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//
    	//the return packet looks like 72,107,16,98,2,11,x,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	writeData << 256*recvdBytes[6]+recvdBytes[7];
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //PID 524
    if(mode22checkboxstate[11] != BST_UNCHECKED && success == true)
    {
    	//mode 0x22 PID 524: ?
    	byte sendBytes[7] = {104,106,241,34,2,12,243};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,100);
    	if(numBytesRead < 8)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//
    	//the return packet looks like 72,107,16,98,2,12,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	writeData << recvdBytes[6];
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //PID 525
    if(mode22checkboxstate[12] != BST_UNCHECKED && success == true)
    {
    	//mode 0x22 PID 525: ?
    	byte sendBytes[7] = {104,106,241,34,2,13,244};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,9,100);
    	if(numBytesRead < 9)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//
    	//the return packet looks like 72,107,16,98,2,13,x,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	writeData << recvdBytes[6]*256+recvdBytes[7];
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //PID 526-529
    if(mode22checkboxstate[13] != BST_UNCHECKED && success == true)
    {
    	//mode 0x22 PID 526-529: ECU part number in ASCII
    	byte sendBytes[7] = {104,106,241,34,2,14,245};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,100);
    	if(numBytesRead < 11)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//return groups of 4 ASCII characters
    	//the return packet looks like 72,107,16,98,2,14,x,x,x,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	int packetPlace = 0;
    	for(int i = 0; i < numBytesRead;i++)
    	{
    		if((i-packetPlace) > 5 && (i-packetPlace) < 10)
    		{
    			byte data = recvdBytes[i-packetPlace];
    			if(data >= 32 && data <= 122)//include ascii chars
    			{
    				writeData << data;
    			}
    		}
    		else if((i-packetPlace) == 10)
    		{
    			packetPlace += 11;
    		}
    	}
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}

    	//request next set of bytes- 527
    	byte sendBytesSetTwo[7] = {104,106,241,34,2,15,246};
    	writeToSerialPort(serialPortHandle, sendBytesSetTwo, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,100);
    	if(numBytesRead < 11)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//return groups of 4 ASCII characters
    	//the return packet looks like 72,107,16,98,2,15,x,x,x,x,sum
    	packetPlace = 0;
    	for(int i = 0; i < numBytesRead;i++)
    	{
    		if((i-packetPlace) > 5 && (i-packetPlace) < 10)
    		{
    			byte data = recvdBytes[i-packetPlace];
    			if(data >= 32 && data <= 122)//include ascii chars
    			{
    				writeData << data;
    			}
    		}
    		else if((i-packetPlace) == 10)
    		{
    			packetPlace += 11;
    		}
    	}
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}

    	//request next set of bytes- 528
    	byte sendBytesSetThree[7] = {104,106,241,34,2,16,247};
    	writeToSerialPort(serialPortHandle, sendBytesSetThree, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,100);
    	if(numBytesRead < 11)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//return groups of 4 ASCII characters
    	//the return packet looks like 72,107,16,98,2,16,x,x,x,x,sum
    	packetPlace = 0;
    	for(int i = 0; i < numBytesRead;i++)
    	{
    		if((i-packetPlace) > 5 && (i-packetPlace) < 10)
    		{
    			byte data = recvdBytes[i-packetPlace];
    			if(data >= 32 && data <= 122)//include ascii chars
    			{
    				writeData << data;
    			}
    		}
    		else if((i-packetPlace) == 10)
    		{
    			packetPlace += 11;
    		}
    	}
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}

    	//request next set of bytes- 529
    	byte sendBytesSetFour[7] = {104,106,241,34,2,17,248};
    	writeToSerialPort(serialPortHandle, sendBytesSetFour, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,100);
    	if(numBytesRead < 11)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//return groups of 4 ASCII characters
    	//the return packet looks like 72,107,16,98,2,17,x,x,x,x,sum
    	packetPlace = 0;
    	for(int i = 0; i < numBytesRead;i++)
    	{
    		if((i-packetPlace) > 5 && (i-packetPlace) < 10)
    		{
    			byte data = recvdBytes[i-packetPlace];
    			if(data >= 32 && data <= 122)//include ascii chars
    			{
    				writeData << data;
    			}
    		}
    		else if((i-packetPlace) == 10)
    		{
    			packetPlace += 11;
    		}
    	}
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //PID 530
    if(mode22checkboxstate[14] != BST_UNCHECKED && success == true)
    {
    	//mode 0x22 PID 530: ?
    	byte sendBytes[7] = {104,106,241,34,2,18,249};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,9,100);
    	if(numBytesRead < 9)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//
    	//the return packet looks like 72,107,16,98,2,18,x,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	writeData << recvdBytes[6]*256+recvdBytes[7];
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //PID 531
    if(mode22checkboxstate[15] != BST_UNCHECKED && success == true)
    {
    	//mode 0x22 PID 531: ?
    	byte sendBytes[7] = {104,106,241,34,2,19,250};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,9,100);
    	if(numBytesRead < 9)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//
    	//the return packet looks like 72,107,16,98,2,19,x,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	writeData << recvdBytes[6]*256+recvdBytes[7];
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //PID 532
    if(mode22checkboxstate[16] != BST_UNCHECKED && success == true)
    {
    	//mode 0x22 PID 532: ?
    	byte sendBytes[7] = {104,106,241,34,2,20,251};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,100);
    	if(numBytesRead < 8)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//
    	//the return packet looks like 72,107,16,98,2,20,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	writeData << recvdBytes[6];
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //PID 533
    if(mode22checkboxstate[17] != BST_UNCHECKED && success == true)
    {
    	//mode 0x22 PID 533: ?
    	byte sendBytes[7] = {104,106,241,34,2,21,252};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,100);
    	if(numBytesRead < 8)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//
    	//the return packet looks like 72,107,16,98,2,21,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	writeData << recvdBytes[6];
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //PID 534
    if(mode22checkboxstate[18] != BST_UNCHECKED && success == true)
    {
    	//mode 0x22 PID 534: ?
    	byte sendBytes[7] = {104,106,241,34,2,22,253};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,8,100);
    	if(numBytesRead < 8)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//
    	//the return packet looks like 72,107,16,98,2,22,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	writeData << recvdBytes[6];
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //PID 536
    if(mode22checkboxstate[19] != BST_UNCHECKED && success == true)
    {
    	//mode 0x22 PID 536: ?
    	byte sendBytes[7] = {104,106,241,34,2,24,255};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,9,100);
    	if(numBytesRead < 9)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//
    	//the return packet looks like 72,107,16,98,2,24,x,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	writeData << recvdBytes[6]*256+recvdBytes[7];
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //PID 537
    if(mode22checkboxstate[20] != BST_UNCHECKED && success == true)
    {
    	//mode 0x22 PID 537: ?
    	byte sendBytes[7] = {104,106,241,34,2,25,0};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,9,100);
    	if(numBytesRead < 9)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//
    	//the return packet looks like 72,107,16,98,2,25,x,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	writeData << recvdBytes[6]*256+recvdBytes[7];
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //PID 538
    if(mode22checkboxstate[21] != BST_UNCHECKED && success == true)
    {
    	//mode 0x22 PID 538: ?
    	byte sendBytes[7] = {104,106,241,34,2,26,1};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,9,100);
    	if(numBytesRead < 9)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//
    	//the return packet looks like 72,107,16,98,2,26,x,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	writeData << recvdBytes[6]*256+recvdBytes[7];
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //PID 539
    if(mode22checkboxstate[22] != BST_UNCHECKED && success == true)
    {
    	//mode 0x22 PID 539: ?
    	byte sendBytes[7] = {104,106,241,34,2,27,2};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,9,100);
    	if(numBytesRead < 9)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//
    	//the return packet looks like 72,107,16,98,2,26,x,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	writeData << recvdBytes[6]*256+recvdBytes[7];
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //PID 540-543
    if(mode22checkboxstate[23] != BST_UNCHECKED && success == true)
    {
    	//mode 0x22 PID 540-543: Calibration ID in ASCII
    	byte sendBytes[7] = {104,106,241,34,2,28,3};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,100);
    	if(numBytesRead < 11)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//return groups of 4 ASCII characters
    	//the return packet looks like 72,107,16,98,2,28,x,x,x,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	int packetPlace = 0;
    	for(int i = 0; i < numBytesRead;i++)
    	{
    		if((i-packetPlace) > 5 && (i-packetPlace) < 10)
    		{
    			byte data = recvdBytes[i-packetPlace];
    			if(data >= 32 && data <= 122)//include ascii chars
    			{
    				writeData << data;
    			}
    		}
    		else if((i-packetPlace) == 10)
    		{
    			packetPlace += 11;
    		}
    	}
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}

    	//request next set of bytes- 541
    	byte sendBytesSetTwo[7] = {104,106,241,34,2,29,4};
    	writeToSerialPort(serialPortHandle, sendBytesSetTwo, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,100);
    	if(numBytesRead < 11)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//return groups of 4 ASCII characters
    	//the return packet looks like 72,107,16,98,2,29,x,x,x,x,sum
    	packetPlace = 0;
    	for(int i = 0; i < numBytesRead;i++)
    	{
    		if((i-packetPlace) > 5 && (i-packetPlace) < 10)
    		{
    			byte data = recvdBytes[i-packetPlace];
    			if(data >= 32 && data <= 122)//include ascii chars
    			{
    				writeData << data;
    			}
    		}
    		else if((i-packetPlace) == 10)
    		{
    			packetPlace += 11;
    		}
    	}
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}

    	//request next set of bytes- 542
    	byte sendBytesSetThree[7] = {104,106,241,34,2,30,5};
    	writeToSerialPort(serialPortHandle, sendBytesSetThree, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,100);
    	if(numBytesRead < 11)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//return groups of 4 ASCII characters
    	//the return packet looks like 72,107,16,98,2,30,x,x,x,x,sum
    	packetPlace = 0;
    	for(int i = 0; i < numBytesRead;i++)
    	{
    		if((i-packetPlace) > 5 && (i-packetPlace) < 10)
    		{
    			byte data = recvdBytes[i-packetPlace];
    			if(data >= 32 && data <= 122)//include ascii chars
    			{
    				writeData << data;
    			}
    		}
    		else if((i-packetPlace) == 10)
    		{
    			packetPlace += 11;
    		}
    	}
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}

    	//request next set of bytes- 543
    	byte sendBytesSetFour[7] = {104,106,241,34,2,31,6};
    	writeToSerialPort(serialPortHandle, sendBytesSetFour, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,100);
    	if(numBytesRead < 11)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//return groups of 4 ASCII characters
    	//the return packet looks like 72,107,16,98,2,31,x,x,x,x,sum
    	packetPlace = 0;
    	for(int i = 0; i < numBytesRead;i++)
    	{
    		if((i-packetPlace) > 5 && (i-packetPlace) < 10)
    		{
    			byte data = recvdBytes[i-packetPlace];
    			if(data >= 32 && data <= 122)//include ascii chars
    			{
    				writeData << data;
    			}
    		}
    		else if((i-packetPlace) == 10)
    		{
    			packetPlace += 11;
    		}
    	}
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //PID 544
    if(mode22checkboxstate[24] != BST_UNCHECKED && success == true)
    {
    	//mode 0x22 PID 544: PIDs available 0x21 - 0x40 ?
    	byte sendBytes[7] = {104,106,241,34,2,32,7};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,100);
    	if(numBytesRead < 11)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//
    	//the return packet looks like 72,107,16,98,2,32,x,x,x,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	int PIDplace = 32;
    	for(int i = 6; i < 10; i++)//loop through bytes
    	{
    		for(int j = 7; j > -1; j--)
    		{
    			PIDplace += 1;
    			if(((recvdBytes[i] >> j) & 0x1))//shift and mask to check bit flags
    			{
    				writeData << PIDplace << ' ';
    			}
    		}
    	}
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //PIDs 545-548
    if(mode22checkboxstate[25] != BST_UNCHECKED && success == true)
    {
    	//mode 0x22 PID 545-548: Make, model and year in ASCII
    	byte sendBytes[7] = {104,106,241,34,2,33,8};
    	writeToSerialPort(serialPortHandle, sendBytes, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,100);
    	if(numBytesRead < 11)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//return groups of 4 ASCII characters
    	//the return packet looks like 72,107,16,98,2,33,x,x,x,x,sum
    	writeBytes << ',';
    	writeData << ',';
    	int packetPlace = 0;
    	for(int i = 0; i < numBytesRead;i++)
    	{
    		if((i-packetPlace) > 5 && (i-packetPlace) < 10)
    		{
    			byte data = recvdBytes[i-packetPlace];
    			if(data >= 32 && data <= 122)//include ascii chars
    			{
    				writeData << data;
    			}
    		}
    		else if((i-packetPlace) == 10)
    		{
    			packetPlace += 11;
    		}
    	}
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}

    	//request next set of bytes- 546
    	byte sendBytesSetTwo[7] = {104,106,241,34,2,34,9};
    	writeToSerialPort(serialPortHandle, sendBytesSetTwo, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,100);
    	if(numBytesRead < 11)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//return groups of 4 ASCII characters
    	//the return packet looks like 72,107,16,98,2,34,x,x,x,x,sum
    	packetPlace = 0;
    	for(int i = 0; i < numBytesRead;i++)
    	{
    		if((i-packetPlace) > 5 && (i-packetPlace) < 10)
    		{
    			byte data = recvdBytes[i-packetPlace];
    			if(data >= 32 && data <= 122)//include ascii chars
    			{
    				writeData << data;
    			}
    		}
    		else if((i-packetPlace) == 10)
    		{
    			packetPlace += 11;
    		}
    	}
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}

    	//request next set of bytes- 547
    	byte sendBytesSetThree[7] = {104,106,241,34,2,35,10};
    	writeToSerialPort(serialPortHandle, sendBytesSetThree, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,100);
    	if(numBytesRead < 11)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//return groups of 4 ASCII characters
    	//the return packet looks like 72,107,16,98,2,35,x,x,x,x,sum
    	packetPlace = 0;
    	for(int i = 0; i < numBytesRead;i++)
    	{
    		if((i-packetPlace) > 5 && (i-packetPlace) < 10)
    		{
    			byte data = recvdBytes[i-packetPlace];
    			if(data >= 32 && data <= 122)//include ascii chars
    			{
    				writeData << data;
    			}
    		}
    		else if((i-packetPlace) == 10)
    		{
    			packetPlace += 11;
    		}
    	}
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}

    	//request next set of bytes- 548
    	byte sendBytesSetFour[7] = {104,106,241,34,2,36,11};
    	writeToSerialPort(serialPortHandle, sendBytesSetFour, 7);
    	readFromSerialPort(serialPortHandle, recvdBytes,7,100);//read back what we just sent
    	numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,11,100);
    	if(numBytesRead < 11)
    	{
    		//we've failed the read and should flag to quit the polling
    		success = false;
    	}
    	//return groups of 4 ASCII characters
    	//the return packet looks like 72,107,16,98,2,36,x,x,x,x,sum
    	packetPlace = 0;
    	for(int i = 0; i < numBytesRead;i++)
    	{
    		if((i-packetPlace) > 5 && (i-packetPlace) < 10)
    		{
    			byte data = recvdBytes[i-packetPlace];
    			if(data >= 32 && data <= 122)//include ascii chars
    			{
    				writeData << data;
    			}
    		}
    		else if((i-packetPlace) == 10)
    		{
    			packetPlace += 11;
    		}
    	}
    	for(int k = 0; k < numBytesRead; k++)
    	{
    		writeBytes << recvdBytes[k] << ' ';
    		recvdBytes[k] = 0;//clear the read buffer for the next set of data
    	}
    }
    //PID 549




    //**************************************************************************************

    //Mode 0x2F

    //**************************************************************************************

    //Mode 0x3B
    if(mode3Bcheckboxstate != BST_UNCHECKED && success == true)
        {
        	//mode 3B: re-write VIN, only using the available characters: //SCCPC111-5H------
        	byte sendBytes[11] = {104,106,241,59,1,mode3BVIN[8],mode3BVIN[11],mode3BVIN[12],mode3BVIN[13],mode3BVIN[14],0};
        	//then compute the checksum
        	int checkSum = 0;
        	for(int i = 0; i < 10; i++)
        	{
        		checkSum += sendBytes[i];
        	}
        	sendBytes[10] = checkSum & 255;//byte mask
        	writeToSerialPort(serialPortHandle, sendBytes, 11);
        	readFromSerialPort(serialPortHandle, recvdBytes,11,100);//read back what we just sent
        	int numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,6,100);
        	if(numBytesRead < 6)
        	{
        		//we've failed the read and should flag to quit the polling
        		success = false;
        	}
        	//this should return an acknowledge packet
        	//the return packet looks like 72,107,16,123,1,sum
        	writeBytes << ',';
        	writeData << ',';
        	if(success)
        	{
        		writeData << "recv'd first 5 chars ";
        	}
        	else
        	{
        		writeData << "did not recv first 5 chars ";
        	}
        	for(int k = 0; k < numBytesRead; k++)
        	{
        		writeBytes << recvdBytes[k] << ' ';
        		recvdBytes[k] = 0;//clear the read buffer for the next set of data
        	}

        	//then send the last two chars
        	byte sendBytesForVin[8] = {104,106,241,59,2,mode3BVIN[15],mode3BVIN[16],0};
        	//then compute the checksum
        	checkSum = 0;
        	for(int i = 0; i < 7; i++)
        	{
        		checkSum += sendBytesForVin[i];
        	}
        	sendBytesForVin[7] = checkSum & 255;//byte mask
        	writeToSerialPort(serialPortHandle, sendBytesForVin, 8);
        	readFromSerialPort(serialPortHandle, recvdBytes,8,100);//read back what we just sent
        	numBytesRead = readFromSerialPort(serialPortHandle, recvdBytes,6,100);
        	if(numBytesRead < 6)
        	{
        		//we've failed the read and should flag to quit the polling
        		success = false;
        	}
        	//this should return an acknowledge packet
        	//the return packet looks like 72,107,16,123,1,sum

        	if(success)
        	{
        		writeData << "recv'd last 2 chars";
        	}
        	else
        	{
        		writeData << "did not recv last 2 chars";
        	}
        	for(int k = 0; k < numBytesRead; k++)
        	{
        		writeBytes << recvdBytes[k] << ' ';
        		recvdBytes[k] = 0;//clear the read buffer for the next set of data
        	}
        }






    /*
	//byte sendBytes[5] = {104,106,241,4,199};//init message for MODE 0x4 PID 0
	Sleep(100);
	writeToSerialPort(Port, sendBytes, 5);//send the 5 bytes
	Sleep(100);
	readFromSerialPort(Port, recvdBytes,100,1000);//read in up to 100 bytes taking up to 1000 ms
	for(int i = 0; i < 100; i++){
		std::cout << (int) recvdBytes[i] << ' ';//print the buffer
	}
	std::cout << '\n';*/


	//close the COM port
	//CloseHandle(Port);
    return success;//will be false if we had a timeout occur
}

int chars_to_int(char* number)
{
	//works only with an array of length 5
	int integerNumber = 0;
	int multiplier = 1;
	//int length = sizeof(number);
	//std::cout << length << ' ';
	for(int x = 4; x > -1; x--)
	{
		int charToNum = 0;
		//std::cout << number[x] -48 << ' ';
		if(number[x] >= 48 && number[x] <= 57)
		{
			charToNum = number[x] - 48;
			//std::cout << charToNum << ' ';
			integerNumber += multiplier*charToNum;
			multiplier = multiplier*10;
		}
		else
		{
			integerNumber = 0;
			multiplier = 1;
		}
	}
	//std::cout << '\n';
	return integerNumber;
}

void writeToSerialPort(HANDLE serialPortHandle, byte* bytesToSend, int numberOfBytesToSend)
{
	DWORD numberOfBytesSent = 0;     // No of bytes written to the port
	/*for(UINT8 i = 0; i < numberOfBytesToSend;i++)
	{
		std::cout << (int) bytesToSend[i] << ' ';
	}
	std::cout << '\n';*/
	WriteFile(serialPortHandle, bytesToSend, numberOfBytesToSend, &numberOfBytesSent, NULL);
	//std::cout << numberOfBytesSent << '\n';
}

int readFromSerialPort(HANDLE serialPortHandle, byte* serialBytesRecvd, int sizeOfSerialByteBuffer, int delayMS)
{
	/*
	//event flag method
	SetCommMask(serialPortHandle, EV_RXCHAR);//set up event when serial bytes have been received
	DWORD dwEventMask;
	WaitCommEvent(serialPortHandle, &dwEventMask, NULL);//wait until the "data has been received" event occurs
	byte recvdByte;
	//std::cout << (int) sizeOfSerialByteBuffer << '\n';
	DWORD numberOfBytesReceived = 1;
	int recvdBufferPosition = 0;
	while(numberOfBytesReceived > 0 && recvdBufferPosition < sizeOfSerialByteBuffer)
	{
		ReadFile(serialPortHandle, &recvdByte, sizeof(recvdByte), &numberOfBytesReceived, NULL);//read in one byte at a time
		serialBytesRecvd[recvdBufferPosition] = recvdByte;
		recvdBufferPosition += 1;
	}
	//std::cout << recvdBufferPosition << '\n';
	 *
	 */


	//continuous polling method:
	//variables for tracking received bytes:
	byte recvdByte;
	DWORD numberOfBytesReceived = 1;
	int recvdBufferPosition = 0;

	//helped from: http://stackoverflow.com/questions/2150291/how-do-i-measure-a-time-interval-in-c
	LARGE_INTEGER frequency;        // ticks per second
	LARGE_INTEGER t1, t2, lastByteTime;           // ticks
	double elapsedTime = 0;
	double elapsedTimeSinceLastByte = 0;

	// get ticks per second
	QueryPerformanceFrequency(&frequency);

	// start timer
	QueryPerformanceCounter(&t1);
	bool notFinished = true;
	bool haveReceivedBytes = false;

	while(notFinished)
	{
	    // stop timer
	    QueryPerformanceCounter(&t2);//get the loop position time
	    if(!haveReceivedBytes)
	    {
	    	elapsedTime = (t2.QuadPart - t1.QuadPart)*1000 / frequency.QuadPart;// compute the elapsed time in milliseconds
	    }
	    else if(haveReceivedBytes)
	    {
	    	elapsedTimeSinceLastByte = (t2.QuadPart - lastByteTime.QuadPart)*1000 / frequency.QuadPart;// compute the elapsed time in milliseconds
	    }
	    ReadFile(serialPortHandle, &recvdByte, sizeof(recvdByte), &numberOfBytesReceived, NULL);//read in one byte at a time
	    if(numberOfBytesReceived > 0 && recvdBufferPosition < sizeOfSerialByteBuffer)
	    {
			serialBytesRecvd[recvdBufferPosition] = recvdByte;
			recvdBufferPosition += 1;
			lastByteTime = t2;//the time that the previous byte was read in
			haveReceivedBytes = true;//raise a flag that we've begun receiving a response
			//update the starttimer in order to extend the timeout
			//for reading-in bytes relative to the last received byte
	       	//QueryPerformanceCounter(&t1);
			//std::cout << (int) recvdByte << " ";//print the received bytes
	    }

	    //if we've overrun our receive buffer, over run the timeout, or have overrun 10 ms since
	    //we last received a serial byte, exit the loop
	    if(elapsedTime >= delayMS || recvdBufferPosition >= sizeOfSerialByteBuffer || elapsedTimeSinceLastByte > 10)
	    {
	    	notFinished = false;//breaks the loop if we time out or if we have received all the bytes we wanted
	    }
	}
	//std::cout << '\n';
	return recvdBufferPosition;//this is also the number of received bytes total
}

HANDLE sendOBDInit(HANDLE serialPortHandle)
{
	//helped from: http://stackoverflow.com/questions/2150291/how-do-i-measure-a-time-interval-in-c

    //read in and save the serial port settings
    DCB dcb;
    COMMTIMEOUTS timeouts = { 0 };
    GetCommState(serialPortHandle, &dcb);//read in the current com config settings in order to save them
    GetCommTimeouts(serialPortHandle,&timeouts);//read in the current com timeout settings to save them

    //close the serial port which we need to access with another
    //API
    CloseHandle(serialPortHandle);

    //first, we'll send the '0x33' byte at an emulated 5 baud rate
    //we do this using FTDI's D2XX API
    FT_HANDLE ftdiHandle;//handle for the FTDI device
    FT_STATUS ftStatus;//store the status of each D2XX command
    DWORD numDevs;

    //first, read in the list of FTDI devices connected
    //into a set of arrays.
    char *BufPtrs[3];   // pointer to array of 3 pointers
    char Buffer1[64];      // buffer for description of first device
    char Buffer2[64];      // buffer for description of second device

    // initialize the array of pointers to hold the device names
    BufPtrs[0] = Buffer1;
    BufPtrs[1] = Buffer2;
    BufPtrs[2] = NULL;      // last entry should be NULL

    ftStatus = FT_ListDevices(BufPtrs,&numDevs,FT_LIST_ALL|FT_OPEN_BY_DESCRIPTION);//poll for the device names

    //print out the read in device names
    /*for(int i = 0; i < 64; i++)
    {
    	std::cout<< Buffer1[i];
    }
    std::cout << '\n';
    for(int i = 0; i < 64; i++)
    {
    	std::cout<< Buffer2[i];
    }
    std::cout << '\n';
    std::cout <<numDevs<< '\n';*/

    ftStatus = FT_OpenEx(Buffer1,FT_OPEN_BY_DESCRIPTION,&ftdiHandle);//initialize the FTDI device
    ftStatus = FT_SetBitMode(ftdiHandle, 0xFF, FT_BITMODE_RESET);//reset all FTDI pins
    ftStatus = FT_SetBitMode(ftdiHandle, 0x1, FT_BITMODE_SYNC_BITBANG);//set TxD pin as output in the bit bang operating mode
    ftStatus = FT_SetBaudRate(ftdiHandle, FT_BAUD_115200);//set the baud rate at which the pins can be updated
    ftStatus = FT_SetDataCharacteristics(ftdiHandle,FT_BITS_8,FT_STOP_BITS_1,FT_PARITY_NONE);//set up the com port parameters
    ftStatus = FT_SetTimeouts(ftdiHandle,200,100);//read timeout of 200ms, write timeout of 100 ms


    //0x33 is 0b00110011, include a stop and start bit
    //include a stop and start bit and we have our emulated byte
    byte fakeBytes[10] = {0x0,0x1,0x1,0x0,0x0,0x1,0x1,0x0,0x0,0x1};//array of bits to send
    byte fakeByte[1];//a byte to send (only the 0th bit matters, though)
    DWORD Written = 0;
    for(int i = 0; i < 10; i++)//loop through and send the 5 baud init '0x33' byte manually bit-by-bit
    {
    	Sleep(200);//delay to emulate a 5 bit/s rate
    	fakeByte[0] = fakeBytes[i];//update the bit to send from the array
    	ftStatus = FT_Write(ftdiHandle,fakeByte,1,&Written);//send one bit at a time
    }


    ftStatus = FT_SetBitMode(ftdiHandle, 0xFF, FT_BITMODE_RESET);//reset all FTDI pins or else the device stays in bit bang mode
    FT_Close(ftdiHandle);//close the FTDI device

    //re-open the serial port which is used for commuincation
    serialPortHandle = CreateFile(ListItem,GENERIC_READ | GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    SetCommState(serialPortHandle, &dcb);//restore the COM port config
	SetCommTimeouts(serialPortHandle,&timeouts);//restore the COM port timeouts

	byte recvdBytes[3] = {0,0,0};//a buffer which can be used to store read in bytes
	readFromSerialPort(serialPortHandle, recvdBytes,3,300);//read in 3 bytes in up to 300 ms

	//invert third byte of serial response and send it back
	byte toSend[1];
	toSend[0]= recvdBytes[2]^255;//invert the 2nd key byte
	//toSend[0]= 0x33;//this is a byte to send for troubleshooting
	Sleep(30);//according to the protocol, wait 25-50 ms
	writeToSerialPort(serialPortHandle, toSend, 1);//ack the received bytes

	//read in one more byte, which should be the inverse of 0x33 (51) => 0xCC (204)
	byte lastByte[2] = {0,0};
	//lastByte[0] = 0;//a buffer which can be used to store read in bytes
	readFromSerialPort(serialPortHandle, lastByte,2,300);//read in 2 bytes

	if(recvdBytes[0] != 0x55 || lastByte[1] != 0xCC)//the first response should be 0x55 (dec 85), the last response should be 0xCC (dec 204)
	{
		attemptsAtInit += 1;//increment our 'attempts at init' counter only when we fail
		if(attemptsAtInit < 3)
		{
			serialPortHandle = sendOBDInit(serialPortHandle);//retry the init
		}
	}
	if(ftStatus != FT_OK)
	{
		std::cout << "FTDI error resetting" << '\n';
	}


	//print out what we read in
	std::cout << "read: ";
	for(int i = 0; i < 3; i++)
	{
		std::cout << (int) recvdBytes[i] << ",";
	}
	std::cout << (int) lastByte[1] << '\n';

	return serialPortHandle;
}

HANDLE setupComPort()
{
	//a handle for the data type which holds COM port settings
	DCB dcb;

	//open the COM port
	HANDLE Port = CreateFile(ListItem,GENERIC_READ | GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);

	//set up the COM port settings
	int baudRateInteger =  chars_to_int(baudRateStore);//numbers below 190 do not work
	//std::cout << baudRateInteger << '\n';

	//prepare the dcb data to configure the COM port
	dcb.DCBlength = sizeof(DCB);//the size of the dbc file
	GetCommState(Port, &dcb);//read in the current settings in order to modify them
	dcb.BaudRate = baudRateInteger;//  baud rate
	dcb.ByteSize = 8;              //  data size, xmit and rcv
	dcb.Parity   = NOPARITY;       //  parity bit
	dcb.StopBits = ONESTOPBIT;     //  stop bit
	SetCommState(Port, &dcb);//set the COM port with these options
	//one start bit, 8 data bits, one stop bit gives 10 bits per frame, just as the lotus is configured

	//set com port timeouts too
	COMMTIMEOUTS timeouts = { 0 };
	timeouts.ReadIntervalTimeout         = 50; // in milliseconds
	timeouts.ReadTotalTimeoutConstant    = 50; // in milliseconds
	timeouts.ReadTotalTimeoutMultiplier  = 10; // in milliseconds
	timeouts.WriteTotalTimeoutConstant   = 50; // in milliseconds
	timeouts.WriteTotalTimeoutMultiplier = 10; // in milliseconds
	SetCommTimeouts(Port,&timeouts);

	return Port;
}

void populateFileHeaders(std::ofstream &dataFile, std::ofstream &byteFile)
{
	dataFile << "Time (s)";
	byteFile << "Time (s)";

	//28 1 2
	//mode 1
	std::string MODE1Titles[] = {"PIDs supported 1-20","Monitor Status Since DTCs cleared","DTC which triggered freeze frame","Fuel system status","Calc'd engine load (%)","Coolant Temp (C)","Short term fuel trim - Bank 1(%)","Long term fuel trim - Bank 1 (%)","Engine speed (rpm)","Vehicle speed (km/h)","Timing advance (deg. before TDC)","Intake air temp (C)","Mass air flow (g/sec)","Throttle position (%)","Oxygen sensors present","Oxygen sensor 1 (V or %)","Oxygen sensor 2 (V or %)","OBD standards this vehicle conforms to","Run time since engine start (s)","PIDs supported 21-40","Distance traveled with MIL on (km)","Command evaporative purge (%)","Fuel tank level (%)","Absolute pressure (kPa)","PIDs supported 41-60","Control module voltage (V)","Absolute load (%)","Relative throttle position (%)"};
	for(int a = 0; a < 28; a++)
	{
		if(mode1checkboxstate[a] != BST_UNCHECKED)
		{
			dataFile << ',' << MODE1Titles[a];
			byteFile << ',' << MODE1Titles[a];
		}
	}

	//mode 2
	std::string MODE2Titles[] = {"PIDs supported 1-20","DTC which triggered freeze frame","Fuel system status","Calc'd engine load (%)","Coolant Temp (C)","Short term fuel trim - Bank 1(%)","Long term fuel trim - Bank 1 (%)","Engine speed (rpm)","Vehicle speed (km/h)","Mass air flow (g/sec)","Throttle position (%)"};
	for(int a = 0; a < 11; a++)
	{
		if(mode2checkboxstate[a] != BST_UNCHECKED)
		{
			dataFile << ',' << MODE2Titles[a];
			byteFile << ',' << MODE2Titles[a];
		}
	}

	//mode 3
	if(mode3checkboxstate != BST_UNCHECKED)
	{
		dataFile << ',' << "Diagnostic Trouble Codes";
		byteFile << ',' << "Diagnostic Trouble Codes";
	}

	//mode 4 has no return values
	if(mode4checkboxstate != BST_UNCHECKED)
	{
		dataFile << ',' << "Clear Diagnostic Trouble Codes";
		byteFile << ',' << "Clear Diagnostic Trouble Codes";
	}

	//mode 5
	std::string MODE5Titles[] = {"OBD monitor IDs supported","Oxygen sensor voltage - bank 1 sensor 1","Oxygen sensor voltage - bank 1 sensor 2","Oxygen sensor voltage - bank 1 sensor 3","Oxygen sensor voltage - bank 1 sensor 4","Oxygen sensor voltage - bank 2 sensor 1","Oxygen sensor voltage - bank 2 sensor 2","Oxygen sensor voltage - bank 2 sensor 3","Oxygen sensor voltage - bank 2 sensor 4"};
	for(int a = 0; a < 9; a++)
	{
		if(mode5checkboxstate[a] != BST_UNCHECKED)
		{
			dataFile << ',' << MODE5Titles[a];
			byteFile << ',' << MODE5Titles[a];
		}
	}

	//mode 6
	std::string MODE6Titles[] = {"PID 0","PID 1","PID 2","PID 3","PID 4","PID 5"};
	for(int a = 0; a < 6; a++)
	{
		if(mode6checkboxstate[a] != BST_UNCHECKED)
		{
			dataFile << ',' << MODE6Titles[a];
			byteFile << ',' << MODE6Titles[a];
		}
	}

	//mode 7
	if(mode7checkboxstate != BST_UNCHECKED)
	{
		dataFile << ',' << "Pending Diagnostic Trouble Codes";
		byteFile << ',' << "Pending Diagnostic Trouble Codes";
	}

	//mode 8
	std::string MODE8Titles[] = {"PID 0","PID 1"};
	for(int a = 0; a < 2; a++)
	{
		if(mode8checkboxstate[a] != BST_UNCHECKED)
		{
			dataFile << ',' << MODE8Titles[a];
			byteFile << ',' << MODE8Titles[a];
		}
	}

	//mode 9
	std::string MODE9Titles[] = {"Mode 9 supported PIDs","VIN message count","VIN","CAL ID message count","CAL ID","CVN message count","CVN"};
	for(int a = 0; a < 7; a++)
	{
		if(mode9checkboxstate[a] != BST_UNCHECKED)
		{
			dataFile << ',' << MODE9Titles[a];
			byteFile << ',' << MODE9Titles[a];
		}
	}

	//mode 0x22
	std::string MODE22Titles[] = {"512","513","515","516","517","518","519","520","521","522","523","524","525","ECU part no.","530","531","532","533","534","536","537","538","539","Calibration ID","544","Make, model and year","549","550","551","552","553","554","555","556","557","558","559","560","561","562","563","564","565","566","567","568","569","570","571","572","573","577","583","584","585","586","591","592","593","594","595","596","598","599","601","602","603","605","606","608","609","610","612","613","614","615","616","617","618","619","620","621"};
	for(int a = 0; a < 82; a++)
	{
		if(mode22checkboxstate[a] != BST_UNCHECKED)
		{
			dataFile << ',' << MODE22Titles[a];
			byteFile << ',' << MODE22Titles[a];
		}
	}

	//mode 2F
	std::string MODE2FTitles[] = {"0x100","0x101","0x102","0x103","0x104","0x120","0x121","0x122","0x125","0x126","0x127","0x12A","0x140","0x141","0x142","0x143","0x144","0x146","0x147","0x148","0x149","0x14A","0x14B","0x160","0x161","0x162","0x163","0x164"};
	for(int a = 0; a < 28; a++)
	{
		if(mode2Fcheckboxstate[a] != BST_UNCHECKED)
		{
			dataFile << ',' << MODE2FTitles[a];
			byteFile << ',' << MODE2FTitles[a];
		}
	}

	//mode 3B
	if(mode3Bcheckboxstate != BST_UNCHECKED)
	{
			dataFile << ',' << "VIN write";
			byteFile << ',' << "VIN write";
	}

	//mode SHORT
	std::string MODESHORTTitles[] = {"PID 2","PID 4"};
	for(int a = 0; a < 2; a++)
	{
		if(modeSHORTcheckboxstate[a] != BST_UNCHECKED)
		{
			dataFile << ',' << MODESHORTTitles[a];
			byteFile << ',' << MODESHORTTitles[a];
		}
	}

	dataFile << '\n';
	byteFile << '\n';
}

std::string decodeDTC(byte firstByte, byte secondByte)
{
	std::string DTC = "";
	//loop through the pairs of bytes to interpret DTCs
	int letterCode = ((firstByte >> 7) & 0x1)*2 + ((firstByte >> 6) & 0x1);//will be 0,1,2, or 3
	if(letterCode == 0)
	{
		DTC.push_back('P');
	}
	else if(letterCode == 1)
	{
		DTC.push_back('C');
	}
	else if(letterCode == 2)
	{
		DTC.push_back('B');
	}
	else if(letterCode == 3)
	{
		DTC.push_back('U');
	}
	int firstDecimal = ((firstByte >> 5) & 0x1)*2 + ((firstByte >> 4) & 0x1);
	int secondDecimal = ((firstByte >> 3) & 0x1)*8 + ((firstByte >> 2) & 0x1)*4 + ((firstByte >> 1) & 0x1)*2 + ((firstByte >> 0) & 0x1);
	int thirdDecimal = ((secondByte >> 7) & 0x1)*8 + ((secondByte >> 6) & 0x1)*4 + ((secondByte >> 5) & 0x1)*2 + ((secondByte >> 4) & 0x1);
	int fourthDecimal = ((secondByte >> 3) & 0x1)*8 + ((secondByte >> 2) & 0x1)*4 + ((secondByte >> 1) & 0x1)*2 + ((secondByte >> 0) & 0x1);
	char stringByte [14];
	std::string convertedDecimal = itoa(firstDecimal,stringByte,10);
	DTC.append(convertedDecimal);
	convertedDecimal = itoa(secondDecimal,stringByte,10);
	DTC.append(convertedDecimal);
	convertedDecimal = itoa(thirdDecimal,stringByte,10);
	DTC.append(convertedDecimal);
	convertedDecimal = itoa(fourthDecimal,stringByte,10);
	DTC.append(convertedDecimal);
	return DTC;
}
