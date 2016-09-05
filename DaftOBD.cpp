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
#include <iomanip>
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
#include <stdint.h>
#include "OBDpolling.h"
using namespace Gdiplus;
//using namespace std;

/* declare Windows procedure */
LRESULT CALLBACK WindowProcedure (HWND, UINT, WPARAM, LPARAM);

//function list
void drawLogParamsTable(HWND parent, int parentWidth, int parentHeight, int scrollPosH, int scrollPosY);//make the list of loggable parameters
void clearLogParamsTable();//clear the list of loggable parameters (I create and destroy this list in order to scroll the list)
void saveLogCheckBoxSettings();//the loggable parameter checkbox settings are saved
void restoreLogCheckBoxSettings();//the loggable parameter checkbox settings are restored when the list is drawn
void updatePlot(HWND parent, int parentWidth, int parentHeight);//draw and update the data plot
void checkForAndPopulateComPortList(HWND comPortList);//enumerate the list of available com ports
void updatePlottingData(int mode, int pid, double data, int &numVariablesPlotted);
void populateFileHeaders(std::ofstream &dataFile, std::ofstream &byteFile);//uses global vars to write the log file header
void reDrawAll(HWND hwnd, LPARAM lParam); //uses global variables to redraw all windows


//herein I greatly abuse global variables

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
int windowWidth = 644;
int windowHeight = 395;

//for the com port select
int ItemIndex = 0;
TCHAR  ListItem[256];//holds the COM port parameter which is selected when the log button is pressed

//for the logging
bool logButtonToggle = false;
bool continuousLogging = false;
char baudRateStore[5] = {'0','0','0','0','0'};
int attemptsAtInit = 0;
std::ofstream writeData;
std::ofstream writeBytes;
const int logPollTimer = 0;//an ID for the timer that queues logging
bool initFlag0 = false;
bool initFlag1 = false;
bool initFlag2 = false;
bool initFlagRedraw = false;
int placeInPoll = 0;
int mode = 1;
int pid = 0;
double dataFromPoll = 0;
int numLogParams = 0;
int logMode[50] = {0};
int logPID[50] = {0};
uint8_t logDataSend[50][7] = {0};

//for data plotting
int plotXOrigin = 0;
int plotYOrigin = 0;
int plotWidth = 0;
int plotHeight = 0;
double xMin = 0;
double xMax = 0;
double yMin = 0;
double yMax = 0;
double times[100] = {0};// {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99,100};
double allData[100][10] = {0};
int numPlottedVariables = 0;
int plotModes[10] = {0};
int plotPIDs[10] = {0};
HWND xMinLabel, xMaxLabel, xLabel1, xLabel2, xLabel3, xTitle;
HWND yMinLabel, yMaxLabel, yLabel1, yLabel2, yLabel3, yLabel4, yTitle;

//for the status window
HWND statusLabel, status;
char statusTextStore[50] = {' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',};

//global variables for data logging
//helped from: http://stackoverflow.com/questions/2150291/how-do-i-measure-a-time-interval-in-c
LARGE_INTEGER systemFrequency;        // ticks per second
LARGE_INTEGER startTime, currentTime, lastPlotUpdate, lastPollUpdate, programTime;           // ticks
double elapsedLoggerTime;
bool lastLogButtonState = false;

//for the scroll bars
SCROLLINFO siY, siX;

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


/* Make the class name into a global variable */
char szClassName[ ] = "CodeBlocksWindowApp";

VOID clearGraphics(HDC hdc)
{
	Graphics graphics(hdc);
	graphics.Clear(Color::LightGray);
}

VOID OnPaint(HDC hdc)
{
   Graphics graphics(hdc);
   plotXOrigin = plotWindowX + 100;
   plotYOrigin = plotWindowY + 20;
   plotWidth = windowWidth - (plotWindowX + 100 + 50);
   plotHeight = windowHeight - (plotWindowY+100);
   Pen      pen(Color(0,0,0));
   SolidBrush* brush = new SolidBrush(Color::White);
   graphics.Clear(Color::LightGray);
   graphics.FillRectangle(brush,plotXOrigin, plotYOrigin, plotWidth, plotHeight);
   graphics.DrawRectangle(&pen,plotXOrigin, plotYOrigin, plotWidth, plotHeight);

   //check for maximum plot values
   for(int a = 0; a < 100; a++)
   {
	   //first, check the x-axis
	   if(times[a] > xMax)
	   {
		   xMax = times[a];
	   }
	   else if(times[a] < xMin)
	   {
		   xMin = times[a];
	   }

	   //next, check the 10 y-axes
	   for(int b = 0; b < 10; b++)
	   {
	   double dataPoint = 0;
	   dataPoint = allData[a][b];
		   if(dataPoint > yMax)
		   {
			   yMax = dataPoint;
		   }
		   else if(dataPoint < yMin)
		   {
			   yMin = dataPoint;
		   }
	   }

   }
   //make sure we set the boundaries
   if(xMax == xMin)
   {
	   xMax +=1;
	   xMin -=1;
   }
   if(yMax == yMin)
   {
	   yMax +=1;
	   yMin -=1;
   }

   //std::cout << "drawing plot: ";
   //draw the lines
   //this needs refinement because it is slow
   for(int c = 0; c < 99; c++)
   {
	   int x1 = (int) (plotXOrigin + (times[c]-xMin)/(xMax-xMin)*plotWidth);
	   int x2 = (int) (plotXOrigin + (times[c+1]-xMin)/(xMax-xMin)*plotWidth);
	   if(numPlottedVariables > 0)
	   {
		   //std::cout << ".";
		   Pen      pen(Color::Red);
		   graphics.DrawLine(&pen, x1, (int) (plotYOrigin+plotHeight-(allData[c][0]-yMin)/(yMax-yMin)*plotHeight), x2, (int) (plotYOrigin+plotHeight-(allData[c+1][0]-yMin)/(yMax-yMin)*plotHeight));
	   }
	   if(numPlottedVariables > 1)
	   {
		   Pen      pen1(Color::Green);
		   graphics.DrawLine(&pen1, x1, (int) (plotYOrigin+plotHeight-(allData[c][1]-yMin)/(yMax-yMin)*plotHeight), x2, (int) (plotYOrigin+plotHeight-(allData[c+1][1]-yMin)/(yMax-yMin)*plotHeight));
	   }
	   if(numPlottedVariables > 2)
	   {
		   Pen      pen2(Color::Blue);
		   graphics.DrawLine(&pen2, x1, (int) (plotYOrigin+plotHeight-(allData[c][2]-yMin)/(yMax-yMin)*plotHeight), x2, (int) (plotYOrigin+plotHeight-(allData[c+1][2]-yMin)/(yMax-yMin)*plotHeight));
	   }
	   if(numPlottedVariables > 3)
	   {
		   Pen      pen3(Color::Orange);
		   graphics.DrawLine(&pen3, x1, (int) (plotYOrigin+plotHeight-(allData[c][3]-yMin)/(yMax-yMin)*plotHeight), x2, (int) (plotYOrigin+plotHeight-(allData[c+1][3]-yMin)/(yMax-yMin)*plotHeight));
	   }
	   if(numPlottedVariables > 4)
	   {
		   Pen      pen4(Color::Crimson);
		   graphics.DrawLine(&pen4, x1, (int) (plotYOrigin+plotHeight-(allData[c][4]-yMin)/(yMax-yMin)*plotHeight), x2, (int) (plotYOrigin+plotHeight-(allData[c+1][4]-yMin)/(yMax-yMin)*plotHeight));
	   }
	   if(numPlottedVariables > 5)
	   {
		   Pen      pen5(Color::LightGreen);
		   graphics.DrawLine(&pen5, x1, (int) (plotYOrigin+plotHeight-(allData[c][5]-yMin)/(yMax-yMin)*plotHeight), x2, (int) (plotYOrigin+plotHeight-(allData[c+1][5]-yMin)/(yMax-yMin)*plotHeight));
	   }
	   if(numPlottedVariables > 6)
	   {
		   Pen      pen6(Color::LightBlue);
		   graphics.DrawLine(&pen6, x1, (int) (plotYOrigin+plotHeight-(allData[c][6]-yMin)/(yMax-yMin)*plotHeight), x2, (int) (plotYOrigin+plotHeight-(allData[c+1][6]-yMin)/(yMax-yMin)*plotHeight));
	   }
	   if(numPlottedVariables > 7)
	   {
		   Pen      pen7(Color::Purple);
		   graphics.DrawLine(&pen7, x1, (int) (plotYOrigin+plotHeight-(allData[c][7]-yMin)/(yMax-yMin)*plotHeight), x2, (int) (plotYOrigin+plotHeight-(allData[c+1][7]-yMin)/(yMax-yMin)*plotHeight));
	   }
	   if(numPlottedVariables > 8)
	   {
		   Pen      pen8(Color::Gray);
		   graphics.DrawLine(&pen8, x1, (int) (plotYOrigin+plotHeight-(allData[c][8]-yMin)/(yMax-yMin)*plotHeight), x2, (int) (plotYOrigin+plotHeight-(allData[c+1][8]-yMin)/(yMax-yMin)*plotHeight));
	   }
	   if(numPlottedVariables > 9)
	   {
		   Pen      pen9(Color::Black);
		   graphics.DrawLine(&pen9, x1, (int) (plotYOrigin+plotHeight-(allData[c][9]-yMin)/(yMax-yMin)*plotHeight), x2, (int) (plotYOrigin+plotHeight-(allData[c+1][9]-yMin)/(yMax-yMin)*plotHeight));
	   }
   }
   //std::cout << '\n';
}

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
	hwnd = CreateWindowEx(0,szClassName,"Daft OBD-II Logger v0.3",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,windowWidth,windowHeight,HWND_DESKTOP,NULL,hThisInstance,NULL);
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

	/*
	//begin a global timer
	LARGE_INTEGER startProgramTime;
	QueryPerformanceFrequency(&systemFrequency);// get ticks per second
	QueryPerformanceCounter(&startProgramTime);// start timer
	*/

	//run the message loop, it will run until GetMessage() returns 0
	while(GetMessage(&messages, NULL, 0, 0))
	{
		//translate the virtual key messages into character messages
		TranslateMessage(&messages);
		//send message to WindowProcedure
		DispatchMessage(&messages);

		/*
		QueryPerformanceCounter(&programTime);

		//if(initFlag2)
		{
			//make sure we don't poll too often
			double runTime = (programTime.QuadPart - startProgramTime.QuadPart)*1000 / systemFrequency.QuadPart;// compute the elapsed time in milliseconds
			double pollTimeMS = (programTime.QuadPart - lastPollUpdate.QuadPart)*1000 / systemFrequency.QuadPart;// compute the elapsed time in milliseconds
			double elapsedRunTime = runTime/1000;//this is an attempt to get post-decimal points
			double elapsedPollTime = pollTimeMS/1000;//this is an attempt to get post-decimal points
			//std::cout << "runtime: " << elapsedRunTime << '\n';
			if(initFlag2)
			{
				std::cout << "time since last poll: " << elapsedPollTime << '\n';
				lastPollUpdate = programTime;
			}
		}*/

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
		        siX.cbSize = sizeof (siX);
		        siX.fMask  = SIF_ALL;

		        // Save the position for comparison later on.
		        GetScrollInfo (hwnd, SB_HORZ, &siX);
		        logParamsScrollPosX = siX.nPos;

		        switch (LOWORD (wParam))
		        {
		        // User clicked the left arrow.
		        case SB_LINELEFT:
		            siX.nPos -= 1;
		            break;

		        // User clicked the right arrow.
		        case SB_LINERIGHT:
		            siX.nPos += 1;
		            break;

		        // User clicked the scroll bar shaft left of the scroll box.
		        case SB_PAGELEFT:
		            siX.nPos -= siX.nPage;
		            break;

		        // User clicked the scroll bar shaft right of the scroll box.
		        case SB_PAGERIGHT:
		            siX.nPos += siX.nPage;
		            break;

		        // User dragged the scroll box.
		        case SB_THUMBTRACK:
		        	siX.nPos = HIWORD(wParam);
		            break;

		        default :
		            break;
		        }
		        // Set the position and then retrieve it.  Due to adjustments
		        // by Windows it may not be the same as the value set.
		        clearLogParamsTable();
		        siX.nTrackPos = siX.nPos; //update the saved position of the trackbar
		        siX.fMask = SIF_POS;
		        SetScrollInfo (logParamsScrollH, SB_CTL, &siX, TRUE);
		        GetScrollInfo (logParamsScrollH, SB_CTL, &siX);
		        logParamsScrollPosX = siX.nPos;
		        drawLogParamsTable(selectLogParams, 210,logParamsHeight, 18 * (logParamsScrollPosX), logParamsScrollPosY);

		        break;

		case WM_VSCROLL: //this case occurs when the vertical window is scrolled
		        // Get all the vertical scroll bar information.
		        siY.cbSize = sizeof (siY);
		        siY.fMask  = SIF_ALL;
		        GetScrollInfo (hwnd, SB_VERT, &siY);

		        switch (LOWORD (wParam))
		        {
		        // User clicked the HOME keyboard key.
		        case SB_TOP:
		            siY.nPos = siY.nMin;
		            break;

		        // User clicked the END keyboard key.
		        case SB_BOTTOM:
		            siY.nPos = siY.nMax;
		            break;

		        // User clicked the top arrow.
		        case SB_LINEUP:
		            siY.nPos -= 10;
		            break;

		        // User clicked the bottom arrow.
		        case SB_LINEDOWN:
		            siY.nPos += 10;
		            break;

		        // User clicked the scroll bar shaft above the scroll box.
		        case SB_PAGEUP:
		            siY.nPos -= siY.nPage;
		            break;

		        // User clicked the scroll bar shaft below the scroll box.
		        case SB_PAGEDOWN:
		            siY.nPos += siY.nPage;
		            break;

		        // User dragged the scroll box.
		        case SB_THUMBTRACK:
		            siY.nPos = HIWORD(wParam);
		            break;

		        default:
		            break;
		        }

		        // Set the position and then retrieve it.  Due to adjustments
		        // by Windows it may not be the same as the value set.
		        clearLogParamsTable();
		        siY.nTrackPos = siY.nPos; //update the saved position of the trackbar
		        siY.fMask = SIF_POS;
		        SetScrollInfo (logParamsScrollV, SB_CTL, &siY, TRUE);
		        GetScrollInfo (logParamsScrollV, SB_CTL, &siY);
		        logParamsScrollPosY = siY.nPos;
		        drawLogParamsTable(selectLogParams, 210,logParamsHeight, 18 * (logParamsScrollPosX), logParamsScrollPosY);

		        break;

		case WM_SIZE:
			//output the window dimensions
			//cout << "case WM_SIZE" << HIWORD (lParam) << "," << LOWORD (lParam) << '\n';

			reDrawAll(hwnd, lParam);

		    // posting WM_PAINT to re-draw graphics in the main window (hwnd)
			InvalidateRect(hwnd, NULL, FALSE);
			PostMessage(hwnd, WM_PAINT, 0, 0);

			break;

		case WM_COMMAND:
			//cout << LOWORD (wParam) << ", " << HIWORD (wParam) << '\n';
			switch(LOWORD (wParam))
			{
			case 1: //log button pressed
				//std::cout << "log now" << '\n';

				//update the stored com port value
				ItemIndex = SendMessage(comSelect, (UINT) CB_GETCURSEL, (WPARAM) 0, (LPARAM) 0);
				//(TCHAR) SendMessage(comSelect, (UINT) CB_GETLBTEXT, (WPARAM) ItemIndex, (LPARAM) ListItem);
				SendMessage(comSelect, (UINT) CB_GETLBTEXT, (WPARAM) ItemIndex, (LPARAM) ListItem);
				//std::cout << ListItem << '\n';

				//update the stored baud rate setting
				GetWindowText(baudrateSelect,baudRateStore,6);
				//std::cout << baudRateStore << '\n';

				//update the stored logging parameters
				saveLogCheckBoxSettings();
				//std::cout << "done saving logCheckBox" << '\n';

				//update the stored button position
				if(logButtonToggle == true)
				{
					//end logging
					logButtonToggle = false;
					CloseHandle(serialPort);
					GetWindowText(status,statusTextStore,50);
					if(statusTextStore[7] == '.')
					{
						SetWindowText(status,"Check Log File");
					}
					initFlag2 = false;
					KillTimer(hwnd, logPollTimer);
					/*for(int j = 0; j < 100; j++)
					{
						std::cout << times[j] << ',' << allData[j][0] << '\n';
					}*/
				}
				else
				{
					logButtonToggle = true;
					initFlag0 = true;
					SetTimer(hwnd, logPollTimer, 100, (TIMERPROC) NULL);
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
		    if(initFlag1 == true)
		    {
		    	std::cout << "painting" << '\n';
		    	initFlagRedraw = true;
		    }
		    EndPaint(hwnd, &ps);
		    break;
		case WM_DESTROY: //this case occurs when the "X" close button is pressed
			PostQuitMessage (0);	//send a WM_QUIT to the message queue
			break;
		case WM_TIMER:
			switch (wParam)
			{
			case logPollTimer:
				//std::cout << "timer triggered event" << '\n';

				//stop the timer from posting messages while we're performing the timer
				//initiated process
				KillTimer(hwnd, logPollTimer);

				if(initFlag0 == true)
				{
					initFlag0 = false;
					SetWindowText(status,"Sending Init");
					initFlag1 = true;
					std::cout << "wait for re-paint" << '\n';
					RedrawWindow(status, NULL, NULL, RDW_UPDATENOW);
					SendMessage(hwnd, WM_PAINT, wParam, lParam);
				}
				else if(initFlag1 == true && initFlagRedraw == true)
				{
					std::cout << "painting complete" << '\n';
					initFlag1 = false;
					initFlagRedraw = false;

					attemptsAtInit = 0;//reset our init attempt counter

					//prepare to save bytes and interpreted data
					std::ofstream writeBytes("daftOBD_byte_log.txt", std::ios::app);//append to the file
					std::ofstream writeData("daftOBD_data_log.txt", std::ios::app);//append to the file

					std::cout << "prep file headers" << '\n';
					//for everything which has a check box, write the header in the files
					populateFileHeaders(writeData, writeBytes);

					std::cout << "done with file headers" << '\n';
					//close the data files
					writeBytes.close();
					writeData.close();

					std::cout << "setting up COM port" << '\n';
					//open the serial port
					serialPort = setupComPort(baudRateStore, ListItem);
					std::cout << "COM port ready" << '\n';

					//perform the OBD-II ISO-9141-2 slow init handshake sequence
					bool initSuccess;
					initSuccess = true;
					std::cout << "sending Init" << '\n';
					serialPort = sendOBDInit(serialPort, ListItem, initSuccess, attemptsAtInit);
					std::cout << "Init attempt complete" << '\n';

					//troubleshooting code without car:
					//initSuccess = true;

					//begin a global timer
					QueryPerformanceFrequency(&systemFrequency);// get ticks per second
					QueryPerformanceCounter(&startTime);// start timer
					lastPollUpdate = startTime;

					if(!initSuccess)
					{
						SetWindowText(status,"Init Fail");
						logButtonToggle = false;
						RedrawWindow(status, NULL, NULL, RDW_UPDATENOW);
					}
					else
					{
						placeInPoll = 0;
						SetWindowText(status,"Polling...");
						initFlag2 = true;//we have successfully initiated
						RedrawWindow(status, NULL, NULL, RDW_UPDATENOW);
					}
				}
				else if(initFlag2)//make sure we've passed initialization
				{
					//if we should log, perform the cycle
					if(logButtonToggle == true)
					{
						//prepare to save bytes and interpreted data
						std::ofstream writeBytes("daftOBD_byte_log.txt", std::ios::app);//append to the file
						std::ofstream writeData("daftOBD_data_log.txt", std::ios::app);//append to the file
						if(numLogParams > 0 && placeInPoll < numLogParams)
						{
							mode = logMode[placeInPoll];
							pid = logPID[placeInPoll];
							uint8_t* dataToSend = logDataSend[placeInPoll];

							//do something special at the start of the set of pollable parameters
							if(placeInPoll == 0)
							{
								//get the current timer value
								QueryPerformanceCounter(&currentTime);//get the loop position time
								double LoggerTimeMS = (currentTime.QuadPart - startTime.QuadPart)*1000 / systemFrequency.QuadPart;// compute the elapsed time in milliseconds
								elapsedLoggerTime = LoggerTimeMS/1000;//this is an attempt to get post-decimal points

								writeBytes << elapsedLoggerTime;
								writeData << elapsedLoggerTime;

								//update the plotting data stored time axis
								for(int j = 0; j < 99; j++)
								{
									times[j] = times[j+1];
								}
								times[99] = elapsedLoggerTime;
							}

							bool pollStatus = serialPollingCycle(serialPort, writeData, writeBytes, mode, pid, dataToSend, dataFromPoll);//if the logging button is pushed, the handle 'serialPort' should be initialized
							if(pollStatus == false)
							{
								SetWindowText(status,"poll fail");
							}

							updatePlottingData(mode, pid, dataFromPoll, numPlottedVariables);

						}

						placeInPoll += 1;

						//un-toggle the log button unless we are supposed to continuously log
						if(!continuousLogging && placeInPoll >= numLogParams)
						{
							GetWindowText(status,statusTextStore,50);
							if(statusTextStore[7] == '.')
							{
								SetWindowText(status,"Check Log File");
							}
							logButtonToggle = false;
							initFlag2 = false;
							CloseHandle(serialPort);
							KillTimer(hwnd, logPollTimer);
							break;
						}

						if(placeInPoll >= numLogParams)
						{
							placeInPoll = 0;
							writeBytes << '\n';
							writeData << '\n';
						}

						//close the data files
						writeBytes.close();
						writeData.close();
						//RedrawWindow(status, NULL, NULL, RDW_UPDATENOW);


						if(numPlottedVariables > 0)
						{
							//check that we don't update the plot too often:
							double plotTimeMS = (currentTime.QuadPart - lastPlotUpdate.QuadPart)*1000 / systemFrequency.QuadPart;// compute the elapsed time in milliseconds
							double elapsedPlotTime = plotTimeMS/1000;//this is an attempt to get post-decimal points
							//std::cout << elapsedPlotTime << '\n';
							if(elapsedPlotTime > 1.0)
							{
								std::cout << "update plot" << '\n';
								lastPlotUpdate = currentTime;
								// posting WM_PAINT to re-draw graphics in the plot portion of the main window (hwnd)
								RECT plotUpdateRegion;
								plotUpdateRegion.left = plotXOrigin;
								plotUpdateRegion.top = plotYOrigin;
								plotUpdateRegion.right = plotXOrigin + plotWidth;
								plotUpdateRegion.bottom = plotYOrigin + plotHeight;
								RECT *update;
								update = &plotUpdateRegion;
								InvalidateRect(hwnd, update, FALSE);
								PostMessage(hwnd, WM_PAINT, 0, 0);
							}
						}
					}
				}
				SetTimer(hwnd, logPollTimer, 10, (TIMERPROC) NULL);
			}

			break;
		default:					//for messages that we don't deal with
			return DefWindowProc(hwnd,message,wParam,lParam);
	}
	return 0;
}

void drawLogParamsTable(HWND parent, int parentWidth, int parentHeight, int scrollPosH, int scrollPosV)
{
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


	int parameterTableWidth = 270;
	int parameterTableHeight = mode3By_offset+mode3BHeight;

	int x_offset = 5 - scrollPosH;
	double fraction = scrollPosV*(parameterTableHeight-parentHeight+20)/3136;
	int y_offset = 5 - fraction;

	if(y_offset > -1*mode1Height)
	{
	//mode 1 parameter table
	mode1 = CreateWindow("BUTTON", "MODE 0x1:Real Time Data",WS_VISIBLE | WS_CHILD|BS_GROUPBOX ,x_offset,y_offset,parameterTableWidth,mode1Height,parent,NULL,NULL,NULL);
	OBD1PID0 = CreateWindow("BUTTON", "PIDs supported 1-20",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,1*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 3,NULL,NULL);
	OBD1PID1 = CreateWindow("BUTTON", "Monitor status since DTC clear",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,2*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 4,NULL,NULL);
	OBD1PID2 = CreateWindow("BUTTON", "Freeze DTC",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,3*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 5,NULL,NULL);
	OBD1PID3 = CreateWindow("BUTTON", "Fuel system status",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,4*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 6,NULL,NULL);
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
	OBD1PID13 = CreateWindow("BUTTON", "Oxygen sensors present",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,15*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 17,NULL,NULL);
	OBD1PID14 = CreateWindow("BUTTON", "Oxygen sensor 1",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,16*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 18,NULL,NULL);
	OBD1PID15 = CreateWindow("BUTTON", "Oxygen sensor 2",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,17*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 19,NULL,NULL);
	OBD1PID1C = CreateWindow("BUTTON", "OBD standard compliance",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,18*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 20,NULL,NULL);
	OBD1PID1F = CreateWindow("BUTTON", "Run time since engine start",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,19*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 21,NULL,NULL);
	OBD1PID20 = CreateWindow("BUTTON", "PIDs supported 21-40",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,20*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 22,NULL,NULL);
	OBD1PID21 = CreateWindow("BUTTON", "Distance traveled with MIL on",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,21*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 23,NULL,NULL);
	OBD1PID2E = CreateWindow("BUTTON", "Command evaporative purge",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,22*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 24,NULL,NULL);
	OBD1PID2F = CreateWindow("BUTTON", "Fuel tank level",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,23*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 25,NULL,NULL);
	OBD1PID33 = CreateWindow("BUTTON", "Absolute barometric pressure",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,24*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 26,NULL,NULL);
	OBD1PID40 = CreateWindow("BUTTON", "PIDs supported 41-60",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,25*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 27,NULL,NULL);
	OBD1PID42 = CreateWindow("BUTTON", "Control module voltage",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,26*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 28,NULL,NULL);
	OBD1PID43 = CreateWindow("BUTTON", "Absolute load value",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,27*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 29,NULL,NULL);
	OBD1PID45 = CreateWindow("BUTTON", "Relative throttle position",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,28*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 30,NULL,NULL);
	}
	if(y_offset+mode2y_offset < parentHeight && y_offset+mode2y_offset+mode2Height > 0)
	{
	mode2 = CreateWindow("BUTTON", "MODE 0x2:Freeze Frame Data",WS_VISIBLE | WS_CHILD|BS_GROUPBOX,x_offset,mode2y_offset+y_offset,parameterTableWidth,mode2Height,parent,NULL,NULL,NULL);
	OBD2PID0 = CreateWindow("BUTTON", "PIDs supported 1-20",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,1*listOccupantHeight,parameterTableWidth-20,18,mode2,(HMENU) 3,NULL,NULL);
	OBD2PID2 = CreateWindow("BUTTON", "DTC that caused freeze frame",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,2*listOccupantHeight,parameterTableWidth-20,18,mode2,(HMENU) 4,NULL,NULL);
	OBD2PID3 = CreateWindow("BUTTON", "Fuel system status",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,3*listOccupantHeight,parameterTableWidth-20,18,mode2,(HMENU) 5,NULL,NULL);
	OBD2PID4 = CreateWindow("BUTTON", "Calc'd engine load",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,4*listOccupantHeight,parameterTableWidth-20,18,mode2,(HMENU) 6,NULL,NULL);
	OBD2PID5 = CreateWindow("BUTTON", "Engine coolant temperature",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,5*listOccupantHeight,parameterTableWidth-20,18,mode2,(HMENU) 7,NULL,NULL);
	OBD2PID6 = CreateWindow("BUTTON", "Short term fuel trim",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,6*listOccupantHeight,parameterTableWidth-20,18,mode2,(HMENU) 8,NULL,NULL);
	OBD2PID7 = CreateWindow("BUTTON", "Long term fuel trim",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,7*listOccupantHeight,parameterTableWidth-20,18,mode2,(HMENU) 9,NULL,NULL);
	OBD2PID12 = CreateWindow("BUTTON", "Engine RPM",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,8*listOccupantHeight,parameterTableWidth-20,18,mode2,(HMENU) 10,NULL,NULL);
	OBD2PID13 = CreateWindow("BUTTON", "Vehicle speed",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,9*listOccupantHeight,parameterTableWidth-20,18,mode2,(HMENU) 11,NULL,NULL);
	OBD2PID16 = CreateWindow("BUTTON", "Mass airflow rate",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,10*listOccupantHeight,parameterTableWidth-20,18,mode2,(HMENU) 12,NULL,NULL);
	OBD2PID17 = CreateWindow("BUTTON", "Throttle position",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,11*listOccupantHeight,parameterTableWidth-20,18,mode2,(HMENU) 13,NULL,NULL);
	}
	if(y_offset+mode3y_offset < parentHeight && y_offset+mode3y_offset+mode3Height > 0)
	{
	mode3 = CreateWindow("BUTTON", "MODE 0x3:Read DTCs",WS_VISIBLE | WS_CHILD|BS_GROUPBOX,x_offset,mode3y_offset+y_offset,parameterTableWidth,mode3Height,parent,NULL,NULL,NULL);
	OBD3PID0 = CreateWindow("BUTTON", "Read DTCs",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,1*listOccupantHeight,parameterTableWidth-20,18,mode3,(HMENU) 3,NULL,NULL);
	}
	if(y_offset+mode4y_offset < parentHeight && y_offset+mode4y_offset+mode4Height > 0)
	{
	mode4 = CreateWindow("BUTTON", "MODE 0x4:Clear DTCs",WS_VISIBLE | WS_CHILD|BS_GROUPBOX,x_offset,mode4y_offset+y_offset,parameterTableWidth,mode4Height,parent,NULL,NULL,NULL);
	OBD4PID0 = CreateWindow("BUTTON", "Clear DTCs",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,1*listOccupantHeight,parameterTableWidth-20,18,mode4,(HMENU) 3,NULL,NULL);
	}
	if(y_offset+mode5y_offset < parentHeight && y_offset+mode5y_offset+mode5Height > 0)
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
	if(y_offset+mode6y_offset < parentHeight && y_offset+mode6y_offset+mode6Height > 0)
	{
	mode6 = CreateWindow("BUTTON", "MODE 0x6:Test results",WS_VISIBLE | WS_CHILD|BS_GROUPBOX,x_offset,mode6y_offset+y_offset,parameterTableWidth,mode6Height,parent,NULL,NULL,NULL);
	OBD6PID0 = CreateWindow("BUTTON", "OBD monitor IDs supported 1-20",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,1*listOccupantHeight,parameterTableWidth-20,18,mode6,(HMENU) 3,NULL,NULL);
	OBD6PID1 = CreateWindow("BUTTON", "Test 1",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,2*listOccupantHeight,parameterTableWidth-20,18,mode6,(HMENU) 4,NULL,NULL);
	OBD6PID2 = CreateWindow("BUTTON", "Test 2",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,3*listOccupantHeight,parameterTableWidth-20,18,mode6,(HMENU) 5,NULL,NULL);
	OBD6PID3 = CreateWindow("BUTTON", "Test 3",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,4*listOccupantHeight,parameterTableWidth-20,18,mode6,(HMENU) 6,NULL,NULL);
	OBD6PID4 = CreateWindow("BUTTON", "Test 4",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,5*listOccupantHeight,parameterTableWidth-20,18,mode6,(HMENU) 7,NULL,NULL);
	OBD6PID5 = CreateWindow("BUTTON", "Test 5",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,6*listOccupantHeight,parameterTableWidth-20,18,mode6,(HMENU) 8,NULL,NULL);
	}
	if(y_offset+mode7y_offset < parentHeight && y_offset+mode7y_offset+mode7Height > 0)
	{
	mode7 = CreateWindow("BUTTON", "MODE 0x7:Pending DTCs",WS_VISIBLE | WS_CHILD|BS_GROUPBOX,x_offset,mode7y_offset+y_offset,parameterTableWidth,mode7Height,parent,NULL,NULL,NULL);
	OBD7PID0 = CreateWindow("BUTTON", "Pending DTCs",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,1*listOccupantHeight,parameterTableWidth-20,18,mode7,(HMENU) 3,NULL,NULL);
	}
	if(y_offset+mode8y_offset < parentHeight && y_offset+mode8y_offset+mode8Height > 0)
	{
	mode8 = CreateWindow("BUTTON", "MODE 0x8:Control components",WS_VISIBLE | WS_CHILD|BS_GROUPBOX,x_offset,mode8y_offset+y_offset,parameterTableWidth,mode8Height,parent,NULL,NULL,NULL);
	OBD8PID0 = CreateWindow("BUTTON", "Component IDs supported 1-20",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,1*listOccupantHeight,parameterTableWidth-20,18,mode8,(HMENU) 3,NULL,NULL);
	OBD8PID1 = CreateWindow("BUTTON", "Component 1",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,2*listOccupantHeight,parameterTableWidth-20,18,mode8,(HMENU) 4,NULL,NULL);
	}
	if(y_offset+mode9y_offset < parentHeight && y_offset+mode9y_offset+mode9Height > 0)
	{
	mode9 = CreateWindow("BUTTON", "MODE 0x9:Request vehicle info",WS_VISIBLE | WS_CHILD|BS_GROUPBOX,x_offset,mode9y_offset+y_offset,parameterTableWidth,mode9Height,parent,NULL,NULL,NULL);
	OBD9PID0 = CreateWindow("BUTTON", "OBD monitor IDs supported 1-20",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,1*listOccupantHeight,parameterTableWidth-20,18,mode9,(HMENU) 3,NULL,NULL);
	OBD9PID1 = CreateWindow("BUTTON", "VIN message count",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,2*listOccupantHeight,parameterTableWidth-20,18,mode9,(HMENU) 4,NULL,NULL);
	OBD9PID2 = CreateWindow("BUTTON", "VIN",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,3*listOccupantHeight,parameterTableWidth-20,18,mode9,(HMENU) 5,NULL,NULL);
	OBD9PID3 = CreateWindow("BUTTON", "Cal ID message count",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,4*listOccupantHeight,parameterTableWidth-20,18,mode9,(HMENU) 6,NULL,NULL);
	OBD9PID4 = CreateWindow("BUTTON", "Calibration ID",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,5*listOccupantHeight,parameterTableWidth-20,18,mode9,(HMENU) 7,NULL,NULL);
	OBD9PID5 = CreateWindow("BUTTON", "CVN message count",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,6*listOccupantHeight,parameterTableWidth-20,18,mode9,(HMENU) 8,NULL,NULL);
	OBD9PID6 = CreateWindow("BUTTON", "Cal Validation No.",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,7*listOccupantHeight,parameterTableWidth-20,18,mode9,(HMENU) 9,NULL,NULL);
	}

	if(y_offset+mode22y_offset < parentHeight && y_offset+mode22y_offset+mode22Height > 0)
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
	OBD22PID526to529 = CreateWindow("BUTTON", "ECU part no.",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,14*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 16,NULL,NULL);
	OBD22PID530 = CreateWindow("BUTTON", "530",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,15*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 17,NULL,NULL);
	OBD22PID531 = CreateWindow("BUTTON", "531",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,16*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 18,NULL,NULL);
	OBD22PID532 = CreateWindow("BUTTON", "532",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,17*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 19,NULL,NULL);
	OBD22PID533 = CreateWindow("BUTTON", "533",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,18*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 20,NULL,NULL);
	OBD22PID534 = CreateWindow("BUTTON", "534",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,19*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 21,NULL,NULL);
	OBD22PID536 = CreateWindow("BUTTON", "536",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,20*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 22,NULL,NULL);
	OBD22PID537 = CreateWindow("BUTTON", "537",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,21*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 23,NULL,NULL);
	OBD22PID538 = CreateWindow("BUTTON", "538",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,22*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 24,NULL,NULL);
	OBD22PID539 = CreateWindow("BUTTON", "539",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,23*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 25,NULL,NULL);
	OBD22PID540to543 = CreateWindow("BUTTON", "Calibration ID",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,24*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 26,NULL,NULL);
	OBD22PID544 = CreateWindow("BUTTON", "544",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,25*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 27,NULL,NULL);
	OBD22PID545to548 = CreateWindow("BUTTOn", "Make, model and year",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,26*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 28,NULL,NULL);
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

	if(y_offset+mode2Fy_offset < parentHeight && y_offset+mode2Fy_offset+mode2FHeight > 0)
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

	if(y_offset+mode3By_offset < parentHeight && y_offset+mode3By_offset+mode3BHeight > 0)
	{
	//OBD MODE 0x3B PID buttons
	mode3B = CreateWindow("BUTTON", "MODE 0x3B:Write VIN",WS_VISIBLE | WS_CHILD|BS_GROUPBOX ,x_offset,mode3By_offset+y_offset,parameterTableWidth,mode3BHeight,parent,NULL,NULL,NULL);
	OBD3BPID0 = CreateWindow("BUTTON", "",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,1*listOccupantHeight,20,18,mode3B,(HMENU) 3,NULL,NULL);
	OBD3BPID1 = CreateWindow("EDIT", mode3BVIN,WS_VISIBLE | WS_CHILD| WS_BORDER |ES_UPPERCASE ,25,1*listOccupantHeight,parameterTableWidth-95,18,mode3B,NULL,NULL,NULL);
	}


	//update the checkbox settings
	restoreLogCheckBoxSettings();

	// Set the vertical scrolling range and page size
    siY.cbSize = sizeof(siY);
    siY.fMask  = SIF_RANGE | SIF_PAGE;
    siY.nMin   = 0;
    siY.nMax   = parameterTableHeight + 20;
    siY.nPage  = (parameterTableHeight + 10) / 10;
    SetScrollInfo(logParamsScrollV, SB_CTL, &siY, TRUE);

    // Set the horizontal scrolling range and page size.
    siX.cbSize = sizeof(siX);
    siX.fMask  = SIF_RANGE | SIF_PAGE;
    siX.nMin   = 0;
    siX.nMax   = (parameterTableWidth)/14;
    siX.nPage  = parameterTableWidth / 18;
    SetScrollInfo(logParamsScrollH, SB_CTL, &siX, TRUE);

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
	if(IsWindowVisible(mode3B))
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


void populateFileHeaders(std::ofstream &dataFile, std::ofstream &byteFile)
{
	numLogParams = 0;
	numPlottedVariables = 0;

	dataFile << '\n' << "Time (s)";
	byteFile << '\n' << "Time (s)";

	//28 1 2
	//mode 1
	std::string MODE1Titles[] = {"PIDs supported 1-20","Monitor Status Since DTCs cleared","DTC which triggered freeze frame","Fuel system status","Calc'd engine load (%)","Coolant Temp (C)","Short term fuel trim - Bank 1(%)","Long term fuel trim - Bank 1 (%)","Engine speed (rpm)","Vehicle speed (km/h)","Timing advance (deg. before TDC)","Intake air temp (C)","Mass air flow (g/sec)","Throttle position (%)","Oxygen sensors present","Oxygen sensor 1 (V or %)","Oxygen sensor 2 (V or %)","OBD standards this vehicle conforms to","Run time since engine start (s)","PIDs supported 21-40","Distance traveled with MIL on (km)","Command evaporative purge (%)","Fuel tank level (%)","Absolute pressure (kPa)","PIDs supported 41-60","Control module voltage (V)","Absolute load (%)","Relative throttle position (%)"};
	for(int a = 0; a < 28; a++)
	{
		//update the table which contains loggable modes and PIDs
		if(mode1checkboxstate[a] != BST_UNCHECKED && numLogParams < 49)
		{
			dataFile << ',' << MODE1Titles[a];
			byteFile << ',' << MODE1Titles[a];
			logMode[numLogParams] = 1;
			logPID[numLogParams] = a;
			numLogParams += 1;
		}
		if(mode1checkboxstate[a] == BST_INDETERMINATE && numPlottedVariables < 9)
		{
			plotModes[numPlottedVariables] = 1;
			plotPIDs[numPlottedVariables] = a;
			numPlottedVariables += 1;
		}
	}

	//mode 2
	std::string MODE2Titles[] = {"PIDs supported 1-20","DTC which triggered freeze frame","Fuel system status","Calc'd engine load (%)","Coolant Temp (C)","Short term fuel trim - Bank 1(%)","Long term fuel trim - Bank 1 (%)","Engine speed (rpm)","Vehicle speed (km/h)","Mass air flow (g/sec)","Throttle position (%)"};
	for(int a = 0; a < 11; a++)
	{
		//update the table which contains loggable modes and PIDs
		if(mode2checkboxstate[a] != BST_UNCHECKED && numLogParams < 49)
		{
			dataFile << ',' << MODE2Titles[a];
			byteFile << ',' << MODE2Titles[a];
			logMode[numLogParams] = 2;
			logPID[numLogParams] = a;
			numLogParams += 1;
		}
		if(mode2checkboxstate[a] == BST_INDETERMINATE && numPlottedVariables < 9)
		{
			plotModes[numPlottedVariables] = 2;
			plotPIDs[numPlottedVariables] = a;
			numPlottedVariables += 1;
		}
	}

	//mode 3
	//update the table which contains loggable modes and PIDs
	if(mode3checkboxstate != BST_UNCHECKED && numLogParams < 49)
	{
		dataFile << ',' << "Diagnostic Trouble Codes";
		byteFile << ',' << "Diagnostic Trouble Codes";
		logMode[numLogParams] = 3;
		logPID[numLogParams] = 0;
		numLogParams += 1;
	}

	//mode 4 has no return values
	//update the table which contains loggable modes and PIDs
	if(mode4checkboxstate != BST_UNCHECKED && numLogParams < 49)
	{
		dataFile << ',' << "Clear Diagnostic Trouble Codes";
		byteFile << ',' << "Clear Diagnostic Trouble Codes";
		logMode[numLogParams] = 4;
		logPID[numLogParams] = 0;
		numLogParams += 1;
	}

	//mode 5
	std::string MODE5Titles[] = {"OBD monitor IDs supported","Oxygen sensor voltage - bank 1 sensor 1","Oxygen sensor voltage - bank 1 sensor 2","Oxygen sensor voltage - bank 1 sensor 3","Oxygen sensor voltage - bank 1 sensor 4","Oxygen sensor voltage - bank 2 sensor 1","Oxygen sensor voltage - bank 2 sensor 2","Oxygen sensor voltage - bank 2 sensor 3","Oxygen sensor voltage - bank 2 sensor 4"};
	for(int a = 0; a < 9; a++)
	{
		//update the table which contains loggable modes and PIDs
		if(mode5checkboxstate[a] != BST_UNCHECKED && numLogParams < 49)
		{
			dataFile << ',' << MODE5Titles[a];
			byteFile << ',' << MODE5Titles[a];
			logMode[numLogParams] = 5;
			logPID[numLogParams] = a;
			numLogParams += 1;
		}
		if(mode5checkboxstate[a] == BST_INDETERMINATE && numPlottedVariables < 9)
		{
			plotModes[numPlottedVariables] = 5;
			plotPIDs[numPlottedVariables] = a;
			numPlottedVariables += 1;
		}
	}

	//mode 6
	std::string MODE6Titles[] = {"PID 0","PID 1","PID 2","PID 3","PID 4","PID 5"};
	for(int a = 0; a < 6; a++)
	{
		//update the table which contains loggable modes and PIDs
		if(mode6checkboxstate[a] != BST_UNCHECKED && numLogParams < 49)
		{
			dataFile << ',' << MODE6Titles[a];
			byteFile << ',' << MODE6Titles[a];
			logMode[numLogParams] = 6;
			logPID[numLogParams] = a;
			numLogParams += 1;
		}
	}

	//mode 7
	//update the table which contains loggable modes and PIDs
	if(mode7checkboxstate != BST_UNCHECKED && numLogParams < 49)
	{
		dataFile << ',' << "Pending Diagnostic Trouble Codes";
		byteFile << ',' << "Pending Diagnostic Trouble Codes";
		logMode[numLogParams] = 7;
		logPID[numLogParams] = 0;
		numLogParams += 1;
	}

	//mode 8
	std::string MODE8Titles[] = {"PID 0","PID 1"};
	for(int a = 0; a < 2; a++)
	{
		//update the table which contains loggable modes and PIDs
		if(mode8checkboxstate[a] != BST_UNCHECKED && numLogParams < 49)
		{
			dataFile << ',' << MODE8Titles[a];
			byteFile << ',' << MODE8Titles[a];
			logMode[numLogParams] = 8;
			logPID[numLogParams] = a;
			numLogParams += 1;
		}
	}

	//mode 9
	std::string MODE9Titles[] = {"Mode 9 supported PIDs","VIN message count","VIN","CAL ID message count","CAL ID","CVN message count","CVN"};
	for(int a = 0; a < 7; a++)
	{
		//update the table which contains loggable modes and PIDs
		if(mode9checkboxstate[a] != BST_UNCHECKED && numLogParams < 49)
		{
			dataFile << ',' << MODE9Titles[a];
			byteFile << ',' << MODE9Titles[a];
			logMode[numLogParams] = 9;
			logPID[numLogParams] = a;
			numLogParams += 1;
		}
	}

	//mode 0x22
	std::string MODE22Titles[] = {"512","513","515","516","517","518","519","520","521","522","523","524","525","ECU part no.","530","531","532","533","534","536","537","538","539","Calibration ID","544","Make, model and year","549","550","551","552","553","554","555","556","557","558","559","560","561","562","563","564","565","566","567","568","569","570","571","572","573","577","583","584","585","586","591","592","593","594","595","596","598","599","601","602","603","605","606","608","609","610","612","613","614","615","616","617","618","619","620","621"};
	for(int a = 0; a < 82; a++)
	{
		//update the table which contains loggable modes and PIDs
		if(mode22checkboxstate[a] != BST_UNCHECKED && numLogParams < 49)
		{
			dataFile << ',' << MODE22Titles[a];
			byteFile << ',' << MODE22Titles[a];
			logMode[numLogParams] = 34;
			logPID[numLogParams] = a;
			numLogParams += 1;
		}
	}

	//mode 2F
	std::string MODE2FTitles[] = {"0x100","0x101","0x102","0x103","0x104","0x120","0x121","0x122","0x125","0x126","0x127","0x12A","0x140","0x141","0x142","0x143","0x144","0x146","0x147","0x148","0x149","0x14A","0x14B","0x160","0x161","0x162","0x163","0x164"};
	for(int a = 0; a < 28; a++)
	{
		//update the table which contains loggable modes and PIDs
		if(mode2Fcheckboxstate[a] != BST_UNCHECKED && numLogParams < 49)
		{
			dataFile << ',' << MODE2FTitles[a];
			byteFile << ',' << MODE2FTitles[a];
			logMode[numLogParams] = 47;
			logPID[numLogParams] = a;
			numLogParams += 1;
		}
	}

	//mode 3B
	//update the table which contains loggable modes and PIDs
	if(mode3Bcheckboxstate != BST_UNCHECKED && numLogParams < 49)
	{
		dataFile << ',' << "VIN write";
		byteFile << ',' << "VIN write";
		logMode[numLogParams] = 59;
		logPID[numLogParams] = 0;
		logDataSend[numLogParams][0] = (uint8_t) mode3BVIN[8];
		logDataSend[numLogParams][1] = (uint8_t) mode3BVIN[11];
		logDataSend[numLogParams][2] = (uint8_t) mode3BVIN[12];
		logDataSend[numLogParams][3] = (uint8_t) mode3BVIN[13];
		logDataSend[numLogParams][4] = (uint8_t) mode3BVIN[14];
		logDataSend[numLogParams][5] = (uint8_t) mode3BVIN[15];
		logDataSend[numLogParams][6] = (uint8_t) mode3BVIN[16];
		numLogParams += 1;
	}

	dataFile << '\n';
	byteFile << '\n';

	//check that we don't have duplicates
	std::cout << "log the following: " << '\n';
	for(int b = 0; b < numLogParams; b++)
	{
		std::cout << "MODE: " << logMode[b] << " PID index: " << logPID[b] << '\n';
	}
}


void updatePlottingData(int mode, int pid, double data, int &numVariablesPlotted)
{
	//update the data stored in the array for plotting

	//the columns are arranged in order of the checkbox list
	//thus, start with '0 variables plotted' and run through
	//the ordered list of possible plottable variables on the way
	//up to the input mode and PID.
	//Increment a counter each time we encounter a variable
	//tagged for plotting. That counter serves as the column
	//index in the stored data array where we will update the
	//value
	int plotDataColumn = 0;
	/*
	numVariablesPlotted = 0;

	//loop through all possible-to-be-plotted variables
	//to determine which vector the data belongs in
	for(int a = 0; a < 28; a ++)
	{
		if(mode1checkboxstate[a] == BST_INDETERMINATE)
		{
			if(mode == 1 && pid == a && numVariablesPlotted < 10)
			{
				plotDataColumn = numVariablesPlotted;
			}
			if(numVariablesPlotted < 9)
			{
				numVariablesPlotted += 1;
			}
		}
	}
	for(int b = 0; b < 11; b ++)
	{
		if(mode2checkboxstate[b] == BST_INDETERMINATE)
		{
			if(mode == 2 && pid == b && numVariablesPlotted < 10)
			{
				plotDataColumn = numVariablesPlotted;
			}
			if(numVariablesPlotted < 9)
			{
				numVariablesPlotted += 1;
			}
		}
	}
	for(int c = 0; c < 9; c ++)
	{
		if(mode5checkboxstate[c] == BST_INDETERMINATE)
		{
			if(mode == 5 && pid == c && numVariablesPlotted < 10)
			{
				plotDataColumn = numVariablesPlotted;
			}
			if(numVariablesPlotted < 9)
			{
				numVariablesPlotted += 1;
			}
		}
	}*/
	if(numVariablesPlotted > 0)
	{
		//loop through the vectors which store the plot modes and PIDs to see which one
		//may match the current mode and PID, then match this to the column in the
		//data storage vector for plotting
		for(int i = 0; i < numVariablesPlotted; i++)
		{
			if(plotModes[i] == mode && plotPIDs[i] == pid)
			{
				plotDataColumn = i;
				break;
			}
		}

		//update the stored data - form: allData[100][10] - a global data array
		for(int k = 0; k < 99; k++)
		{
			allData[k][plotDataColumn] = allData[k+1][plotDataColumn];
		}
		//add the new data in the last place
		allData[99][plotDataColumn] = data;
	}
}

void reDrawAll(HWND hwnd, LPARAM lParam)
{
	//update the stored logging parameters
	saveLogCheckBoxSettings();

	//save the status text
	GetWindowText(status,statusTextStore,50);

	//save the scroll bar positions
	siY.fMask  = SIF_ALL;
	siX.fMask  = SIF_ALL;
	GetScrollInfo (logParamsScrollV, SB_CTL, &siY);
	GetScrollInfo (logParamsScrollH, SB_CTL, &siX);

	//redraw some of the sub windows if the main window is resized:
	//begin by destroying the parts to be re-drawn
	DestroyWindow(plotwindow);
	DestroyWindow(selectLogParams);
	DestroyWindow(logParamsScrollH);
	DestroyWindow(logParamsScrollV);
	/*DestroyWindow(xMinLabel);
				DestroyWindow(xLabel1);
				DestroyWindow(xLabel2);
				DestroyWindow(xLabel3);
				DestroyWindow(xMaxLabel);*/
	DestroyWindow(xTitle);
	DestroyWindow(statusLabel);
	DestroyWindow(status);

	//update stored window dimensions
	logParamsWidth = 210;
	logParamsHeight = (HIWORD (lParam))-(20+logParamsY)-40;
	windowWidth = (LOWORD (lParam));
	windowHeight = (HIWORD (lParam));

	//then re-draw the parts which need to be moved
	plotwindow = CreateWindowEx(0,"STATIC", "Data Plot",WS_VISIBLE | WS_CHILD| WS_BORDER,plotWindowX,plotWindowY,80,18,hwnd,NULL,NULL,NULL);
	/*xMinLabel = CreateWindow("EDIT", "0",WS_VISIBLE | WS_CHILD| WS_BORDER,plotWindowX+80,windowHeight - (plotWindowY + 40),40,20,hwnd,NULL,NULL,NULL);
				xLabel1 = CreateWindow("EDIT", "25",WS_VISIBLE | WS_CHILD| ES_READONLY ,(int) ((plotWindowX+80) + (((windowWidth - 70) - (plotWindowX+80))/4)),windowHeight - (plotWindowY + 40),40,20,hwnd,NULL,NULL,NULL);
				xLabel2 = CreateWindow("EDIT", "50",WS_VISIBLE | WS_CHILD| ES_READONLY ,(int) ((plotWindowX+80) + (((windowWidth - 70) - (plotWindowX+80))/2)),windowHeight - (plotWindowY + 40),40,20,hwnd,NULL,NULL,NULL);
				xLabel3 = CreateWindow("EDIT", "75",WS_VISIBLE | WS_CHILD| ES_READONLY ,(int) ((plotWindowX+80) + 3*(((windowWidth - 70) - (plotWindowX+80))/4)),windowHeight - (plotWindowY + 40),40,20,hwnd,NULL,NULL,NULL);
				xMaxLabel = CreateWindow("EDIT", "100",WS_VISIBLE | WS_CHILD| WS_BORDER,windowWidth - 70,windowHeight - (plotWindowY + 40),40,20,hwnd,NULL,NULL,NULL);
	 */
	xTitle = CreateWindow("STATIC", "Time (s)",WS_VISIBLE | WS_CHILD ,(int) ((plotWindowX+70) + (((windowWidth - 70) - (plotWindowX+80))/2)),windowHeight - (plotWindowY),60,20,hwnd,NULL,NULL,NULL);

	statusLabel = CreateWindow("STATIC", "Status:",WS_VISIBLE | WS_CHILD ,logParamsX,logParamsY+40+logParamsHeight,50,15,hwnd,NULL,NULL,NULL);
	status = CreateWindow("EDIT", statusTextStore,WS_VISIBLE | WS_CHILD |ES_READONLY ,logParamsX+55,logParamsY+40+logParamsHeight,135,15,hwnd,NULL,NULL,NULL);

	logParamsTitle = CreateWindowEx(0,"STATIC", "Select Log/Plot Parameters",WS_VISIBLE | WS_CHILD | WS_BORDER | SS_CENTER ,logParamsX,logParamsY,logParamsWidth-20,20,hwnd,NULL,NULL,NULL);
	selectLogParams = CreateWindowEx(0,"STATIC", "",WS_VISIBLE | WS_CHILD | WS_BORDER,logParamsX,logParamsY+20,logParamsWidth-20,logParamsHeight,hwnd,NULL,NULL,NULL);

	//re-draw scroll bars
	logParamsScrollV = CreateWindowEx(0,"SCROLLBAR", "Vertical scroll bar", WS_VISIBLE | WS_CHILD | SBS_VERT,logParamsX+logParamsWidth-20,logParamsY,20,logParamsHeight+20,hwnd,NULL,NULL,NULL);
	logParamsScrollH = CreateWindowEx(0,"SCROLLBAR", "Horizontal scroll bar", WS_VISIBLE | WS_CHILD | SBS_HORZ,logParamsX,logParamsY + logParamsHeight+20,logParamsWidth-20,20,hwnd,NULL,NULL,NULL);

	//re-draw the table with the check-boxes
	drawLogParamsTable(selectLogParams, 210,logParamsHeight, 18*logParamsScrollPosX, logParamsScrollPosY);

	//restore scroll bar positions
	siY.fMask = SIF_POS;
	siX.fMask = SIF_POS;
	SetScrollInfo (logParamsScrollV, SB_CTL, &siY, TRUE);
	SetScrollInfo (logParamsScrollH, SB_CTL, &siX, TRUE);
}
