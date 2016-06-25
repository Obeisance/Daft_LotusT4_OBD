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
void serialPollingCycle();//perform the serial message exchange
int chars_to_int(char* number);//converts a char array to an integer
void writeToSerialPort(HANDLE serialPortHandle, byte* bytesToSend, int numberOfBytesToSend);
void readFromSerialPort(HANDLE serialPortHandle, byte* serialBytesRecvd, int sizeOfSerialByteBuffer, int delayMS);
HANDLE sendOBDInit(HANDLE serialPortHandle);


//global variables for window handles
HWND selectLogParams, logParamsTitle, logParamsScrollH, logParamsScrollV, logbutton, continuousButton, plotwindow, plot, plotParam, indicateBaudRate, baudrateSelect,mode1,mode2,mode3,mode4,mode5,mode6,mode7,mode8,mode9,mode22,mode2F,mode3B,modeSHORT;
HWND comSelect;

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

//for the scroll bars
SCROLLINFO si;

//OBD MODE 1 PID buttons, 28 PIDs
HWND OBD1PID0,OBD1PID1,OBD1PID2,OBD1PID3,OBD1PID4,OBD1PID5,OBD1PID6,OBD1PID7,OBD1PIDC,OBD1PIDD,OBD1PIDE,OBD1PIDF,OBD1PID10,OBD1PID11,OBD1PID13,OBD1PID14,OBD1PID15,OBD1PID1C,OBD1PID1F,OBD1PID20,OBD1PID21,OBD1PID2E,OBD1PID2F,OBD1PID33,OBD1PID40,OBD1PID42,OBD1PID43,OBD1PID45;
int mode1checkboxstate[28] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

//OBD MODE 2 PID buttons
HWND OBD2PID0,OBD2PID2,OBD2PID3,OBD2PID4,OBD2PID5,OBD2PID6,OBD2PID7,OBD2PID12,OBD2PID13,OBD2PID16,OBD2PID17;
int mode2checkboxstate[28] = {0,0,0,0,0,0,0,0,0,0,0};

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
			serialPollingCycle();
			//un-toggle the log button unless we are supposed to continuously log
			if(!continuousLogging)
			{
				logButtonToggle = false;
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
					logButtonToggle = false;
				}
				else
				{
					logButtonToggle = true;
					attemptsAtInit = 0;//reset our init attempt counter
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
	int modeSHORTHeight = listOccupantHeight*3+5;


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
	OBD2PID12 = CreateWindow("BUTTON", "Command secondary air status",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,8*listOccupantHeight,parameterTableWidth-20,18,mode2,(HMENU) 10,NULL,NULL);
	OBD2PID13 = CreateWindow("BUTTON", "Oxygen sensors present",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,9*listOccupantHeight,parameterTableWidth-20,18,mode2,(HMENU) 11,NULL,NULL);
	OBD2PID16 = CreateWindow("BUTTON", "Oxygen sensor 3",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,10*listOccupantHeight,parameterTableWidth-20,18,mode2,(HMENU) 12,NULL,NULL);
	OBD2PID17 = CreateWindow("BUTTON", "Oxygen sensor 4",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,11*listOccupantHeight,parameterTableWidth-20,18,mode2,(HMENU) 13,NULL,NULL);
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
	OBD9PID1 = CreateWindow("BUTTON", "Vehicle info 1",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,2*listOccupantHeight,parameterTableWidth-20,18,mode9,(HMENU) 4,NULL,NULL);
	OBD9PID2 = CreateWindow("BUTTON", "Vehcile info 2",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,3*listOccupantHeight,parameterTableWidth-20,18,mode9,(HMENU) 5,NULL,NULL);
	OBD9PID3 = CreateWindow("BUTTON", "Vehicle info 3",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,4*listOccupantHeight,parameterTableWidth-20,18,mode9,(HMENU) 6,NULL,NULL);
	OBD9PID4 = CreateWindow("BUTTON", "Vehicle info 4",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,5*listOccupantHeight,parameterTableWidth-20,18,mode9,(HMENU) 7,NULL,NULL);
	OBD9PID5 = CreateWindow("BUTTON", "Vehicle info 5",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,6*listOccupantHeight,parameterTableWidth-20,18,mode9,(HMENU) 8,NULL,NULL);
	OBD9PID6 = CreateWindow("BUTTON", "Vehicle info 6",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,7*listOccupantHeight,parameterTableWidth-20,18,mode9,(HMENU) 9,NULL,NULL);
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
	modeSHORT = CreateWindow("BUTTON", "MODE SHORT: Read veh info.",WS_VISIBLE | WS_CHILD|BS_GROUPBOX,x_offset,modeSHORTy_offset+y_offset,parameterTableWidth,modeSHORTHeight,parent,NULL,NULL,NULL);
	OBDSHORTPID2 = CreateWindow("BUTTON", "VIN",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,1*listOccupantHeight,parameterTableWidth-20,18,modeSHORT,(HMENU) 3,NULL,NULL);
	OBDSHORTPID4 = CreateWindow("BUTTON", "CAL ID, make, model and year",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,2*listOccupantHeight,parameterTableWidth-20,18,modeSHORT,(HMENU) 4,NULL,NULL);
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
	DestroyWindow(modeSHORT);
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
	//mode SHORT PIDs
	for(int i = 0; i < 2; i++)
	{
		modeSHORTcheckboxstate[i] = IsDlgButtonChecked(modeSHORT,3+i);
	}
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
	//mode SHORT PIDs
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
	}
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

void serialPollingCycle()
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

	//for each logging parameter, send a message, recieve a message, interpret the message and save the data

	//perform the OBD-II ISO-9141-2 slow init handshake sequence
	Port = sendOBDInit(Port);

	//

	//test the OBD mode III response:
	byte sendBytes[5] = {104,106,241,3,198};//init message for MODE 0x3 PID 0
	//byte sendBytes[5] = {104,106,241,4,199};//init message for MODE 0x4 PID 0
	Sleep(100);
	writeToSerialPort(Port, sendBytes, 5);//send the 6 bytes
	byte recvdBytes[100];//a buffer which can be used to store read bytes
	for(int i = 0; i < 100; i++)
	{
		recvdBytes[i] = 0;
	}
	Sleep(100);
	readFromSerialPort(Port, recvdBytes,100,1000);//read in bytes taking up to 10 ms
	for(int i = 0; i < 100; i++){
		std::cout << (int) recvdBytes[i] << ' ';//print the buffer
	}
	std::cout << '\n';

	//close the COM port
	CloseHandle(Port);
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

void readFromSerialPort(HANDLE serialPortHandle, byte* serialBytesRecvd, int sizeOfSerialByteBuffer, int delayMS)
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
	LARGE_INTEGER t1, t2;           // ticks
	double elapsedTime;

	// get ticks per second
	QueryPerformanceFrequency(&frequency);

	// start timer
	QueryPerformanceCounter(&t1);
	bool notFinished = true;

	while(notFinished)
	{
	    // stop timer
	    QueryPerformanceCounter(&t2);//get the loop position time
	    elapsedTime = (t2.QuadPart - t1.QuadPart)*1000 / frequency.QuadPart;// compute the elapsed time in milliseconds
	    ReadFile(serialPortHandle, &recvdByte, sizeof(recvdByte), &numberOfBytesReceived, NULL);//read in one byte at a time
	    if(numberOfBytesReceived > 0 && recvdBufferPosition < sizeOfSerialByteBuffer)
	    {
			serialBytesRecvd[recvdBufferPosition] = recvdByte;
			recvdBufferPosition += 1;
			//update the starttimer in order to extend the timeout
			//for reading-in bytes relative to the last received byte
	       	//QueryPerformanceCounter(&t1);
			//std::cout << (int) recvdByte << " ";//print the received bytes
	    }
	    if(elapsedTime >= delayMS || recvdBufferPosition >= sizeOfSerialByteBuffer)
	    {
	    	notFinished = false;//breaks the loop if we time out or if we have received all the bytes we wanted
	    }
	}
	//std::cout << '\n';

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
