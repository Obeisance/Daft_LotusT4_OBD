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
#include <Unknwn.h>//for windows comm
#include <windows.h>//for windows
#include "ftd2xx.h"//included to directly drive the FTDI com port device
#include <string>
#include <sstream>
#include <objidl.h>
#include <gdiplus.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include "OBDpolling.h"
#include "lotusReflash.h"
using namespace Gdiplus;
//using namespace std;

/* declare Windows procedure */
LRESULT CALLBACK WindowProcedure (HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK reflashWindowProcedure (HWND, UINT, WPARAM, LPARAM);

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
HWND selectLogParams, logParamsTitle, logParamsScrollH, logParamsScrollV, logbutton, continuousButton, plotwindow, plot, plotParam, indicateBaudRate, baudrateSelect,mode1,mode2,mode3,mode4,mode5,mode6,mode7,mode8,mode9,mode22,mode2F,mode3B,reflashModeButton;
HWND comSelect;
HANDLE serialPort, comPortHandle;

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
long filePosition = 0;

//for the reflashing routines
long lastPositionForReflashing = 0;
uint8_t reflashingTimerProcMode = 0;
bool stageIbootloader = false;
bool clearMemorySector = false;
bool readMemory = false;
uint16_t CRC = 0;
uint32_t numpacketsSent = 0;
long bytesToSend = 0;
uint32_t addressToFlash = 0;
bool getNextMessage = true;
uint8_t packetBuffer[300];
uint16_t bufferLength = 0;

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

   xMax = times[0];
   xMin = times[0];
   yMax = allData[0][0];
   yMin = allData[0][0];

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
 	HWND hwnd,reflashModeWindow;		//this are the handles for our windows
	MSG messages;	//here messages to the application window are saved
	WNDCLASSEX wincl,RFMwincl;	//data strcuture for the windowclass
	GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR           gdiplusToken;

	// Initialize GDI+.
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    /*The Windows Structure For The Second Window*/
    RFMwincl.hInstance = hThisInstance;
    RFMwincl.lpszClassName = "ReflashModeClassName";
    RFMwincl.lpfnWndProc = reflashWindowProcedure;
    RFMwincl.style = CS_DBLCLKS;
    RFMwincl.cbSize = sizeof(WNDCLASSEX);
    RFMwincl.hIcon = LoadIcon (NULL, IDI_APPLICATION);
    RFMwincl.hIconSm = LoadIcon (NULL, IDI_APPLICATION);
    RFMwincl.hCursor = LoadCursor (NULL, IDC_ARROW);
    RFMwincl.lpszMenuName = NULL;
    RFMwincl.cbClsExtra = 0;
    RFMwincl.cbWndExtra = 0;
    RFMwincl.hbrBackground = (HBRUSH) COLOR_BACKGROUND;

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

	/* Register the second windows class, and if it fails quit the program
	       returning a 1 so that we know where it failed*/
	ATOM RFMclass = RegisterClassEx(&RFMwincl);
	if(!RFMclass)
		return 1;

	/*the class is registered, lets create the program*/
	hwnd = CreateWindowEx(0,szClassName,"Daft OBD-II Logger v1.5",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,windowWidth,windowHeight,HWND_DESKTOP,NULL,hThisInstance,NULL);
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

	reflashModeWindow = CreateWindowEx( 0,
	                        "ReflashModeClassName",
	                        "Reflash Mode",
	                        WS_OVERLAPPEDWINDOW,
	                        CW_USEDEFAULT,
	                        CW_USEDEFAULT,
	                        windowWidth,
	                        windowHeight,
	                        HWND_DESKTOP,
	                        NULL,
	                        hThisInstance,
	                        NULL);


	//make the window visible on the screen
	ShowWindow(hwnd,nCmdShow);
	ShowWindow(reflashModeWindow,nCmdShow);
	ShowWindow(reflashModeWindow,SW_HIDE);

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

			//prepare to plot some of the logged parameters
			plotwindow = CreateWindowEx(0,"STATIC", "Data Plot",WS_VISIBLE | WS_CHILD| WS_BORDER,plotWindowX,plotWindowY,80,18,hwnd,NULL,NULL,NULL);

			//allow user to set the baud rate for OBD serial comms
			indicateBaudRate = CreateWindow("STATIC", "Baud Rate:",WS_VISIBLE | WS_CHILD|SS_CENTER,285,8,80,19,hwnd,NULL,NULL,NULL);
			baudrateSelect = CreateWindow("EDIT", "10419",WS_VISIBLE | WS_CHILD| WS_BORDER,370,8,80,20,hwnd,NULL,NULL,NULL);

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
					initFlag0 = false;
					initFlag1 = false;
					initFlagRedraw = false;
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
			case 4:
			{
				//reflash mode button pressed
				ShowWindow(hwnd,SW_HIDE);
				HWND windowHandle = FindWindowEx(NULL,NULL,"ReflashModeClassName",NULL);
				ShowWindow(windowHandle,SW_SHOW);
				break;
			}
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

					/*
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
					*/

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

					/*
					//begin a global timer
					QueryPerformanceFrequency(&systemFrequency);// get ticks per second
					QueryPerformanceCounter(&startTime);// start timer
					lastPollUpdate = startTime;
					*/

					if(!initSuccess)
					{
						SetWindowText(status,"Init Fail");
						CloseHandle(serialPort);
						logButtonToggle = false;
						RedrawWindow(status, NULL, NULL, RDW_UPDATENOW);
					}
					else
					{
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

						//begin a global timer
						QueryPerformanceFrequency(&systemFrequency);// get ticks per second
						QueryPerformanceCounter(&startTime);// start timer
						lastPollUpdate = startTime;

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


	int parameterTableWidth = 310;
	int parameterTableHeight = mode3By_offset+mode3BHeight;

	int x_offset = 5 - scrollPosH;
	double fraction = scrollPosV*(parameterTableHeight-parentHeight+20)/3136;
	int y_offset = 5 - fraction;

	if(y_offset > -1*mode1Height)
	{
	//mode 1 parameter table
	mode1 = CreateWindow("BUTTON", "MODE 0x1:Real Time Data",WS_VISIBLE | WS_CHILD|BS_GROUPBOX ,x_offset,y_offset,parameterTableWidth,mode1Height,parent,NULL,NULL,NULL);
	OBD1PID0 = CreateWindow("BUTTON", "0x0 PIDs avail. 1-20",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,1*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 3,NULL,NULL);
	OBD1PID1 = CreateWindow("BUTTON", "0x1 Monitor status since DTC clear",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,2*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 4,NULL,NULL);
	OBD1PID2 = CreateWindow("BUTTON", "0x2 Freeze DTC",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,3*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 5,NULL,NULL);
	OBD1PID3 = CreateWindow("BUTTON", "0x3 Fuel system status",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,4*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 6,NULL,NULL);
	OBD1PID4 = CreateWindow("BUTTON", "0x4 Calc'd engine load",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,5*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 7,NULL,NULL);
	OBD1PID5 = CreateWindow("BUTTON", "0x5 Engine coolant temperature",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,6*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 8,NULL,NULL);
	OBD1PID6 = CreateWindow("BUTTON", "0x6 Short term fuel trim",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,7*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 9,NULL,NULL);
	OBD1PID7 = CreateWindow("BUTTON", "0x7 Long term fuel trim",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,8*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 10,NULL,NULL);
	OBD1PIDC = CreateWindow("BUTTON", "0xC Engine RPM",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,9*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 11,NULL,NULL);
	OBD1PIDD = CreateWindow("BUTTON", "0xD Vehicle speed",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,10*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 12,NULL,NULL);
	OBD1PIDE = CreateWindow("BUTTON", "0xE Timing advance",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,11*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 13,NULL,NULL);
	OBD1PIDF = CreateWindow("BUTTON", "0xF Intake air temperaure",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,12*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 14,NULL,NULL);
	OBD1PID10 = CreateWindow("BUTTON", "0x10 Mass airflow rate",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,13*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 15,NULL,NULL);
	OBD1PID11 = CreateWindow("BUTTON", "0x11 Throttle position",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,14*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 16,NULL,NULL);
	OBD1PID13 = CreateWindow("BUTTON", "0x13 Oxygen sensors present",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,15*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 17,NULL,NULL);
	OBD1PID14 = CreateWindow("BUTTON", "0x14 Oxygen sensor 1",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,16*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 18,NULL,NULL);
	OBD1PID15 = CreateWindow("BUTTON", "0x15 Oxygen sensor 2",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,17*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 19,NULL,NULL);
	OBD1PID1C = CreateWindow("BUTTON", "0x1C OBD standard compliance",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,18*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 20,NULL,NULL);
	OBD1PID1F = CreateWindow("BUTTON", "0x1F Run time since engine start",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,19*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 21,NULL,NULL);
	OBD1PID20 = CreateWindow("BUTTON", "0x20 PIDs avail. 21-40",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,20*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 22,NULL,NULL);
	OBD1PID21 = CreateWindow("BUTTON", "0x21 Distance traveled with MIL on",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,21*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 23,NULL,NULL);
	OBD1PID2E = CreateWindow("BUTTON", "0x2E Command evaporative purge",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,22*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 24,NULL,NULL);
	OBD1PID2F = CreateWindow("BUTTON", "0x2F Fuel tank level",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,23*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 25,NULL,NULL);
	OBD1PID33 = CreateWindow("BUTTON", "0x33 Absolute barometric pressure",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,24*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 26,NULL,NULL);
	OBD1PID40 = CreateWindow("BUTTON", "0x40 PIDs avail. 41-60",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,25*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 27,NULL,NULL);
	OBD1PID42 = CreateWindow("BUTTON", "0x42 Control module voltage",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,26*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 28,NULL,NULL);
	OBD1PID43 = CreateWindow("BUTTON", "0x43 Absolute load value",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,27*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 29,NULL,NULL);
	OBD1PID45 = CreateWindow("BUTTON", "0x45 Relative throttle position",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,28*listOccupantHeight,parameterTableWidth-20,18,mode1,(HMENU) 30,NULL,NULL);
	}
	if(y_offset+mode2y_offset < parentHeight && y_offset+mode2y_offset+mode2Height > 0)
	{
	mode2 = CreateWindow("BUTTON", "MODE 0x2:Freeze Frame Data",WS_VISIBLE | WS_CHILD|BS_GROUPBOX,x_offset,mode2y_offset+y_offset,parameterTableWidth,mode2Height,parent,NULL,NULL,NULL);
	OBD2PID0 = CreateWindow("BUTTON", "0x0 PIDs avail. 1-20",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,1*listOccupantHeight,parameterTableWidth-20,18,mode2,(HMENU) 3,NULL,NULL);
	OBD2PID2 = CreateWindow("BUTTON", "0x2 DTC that caused freeze frame",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,2*listOccupantHeight,parameterTableWidth-20,18,mode2,(HMENU) 4,NULL,NULL);
	OBD2PID3 = CreateWindow("BUTTON", "0x3 Fuel system status",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,3*listOccupantHeight,parameterTableWidth-20,18,mode2,(HMENU) 5,NULL,NULL);
	OBD2PID4 = CreateWindow("BUTTON", "0x4 Calc'd engine load",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,4*listOccupantHeight,parameterTableWidth-20,18,mode2,(HMENU) 6,NULL,NULL);
	OBD2PID5 = CreateWindow("BUTTON", "0x5 Engine coolant temperature",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,5*listOccupantHeight,parameterTableWidth-20,18,mode2,(HMENU) 7,NULL,NULL);
	OBD2PID6 = CreateWindow("BUTTON", "0x6 Short term fuel trim",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,6*listOccupantHeight,parameterTableWidth-20,18,mode2,(HMENU) 8,NULL,NULL);
	OBD2PID7 = CreateWindow("BUTTON", "0x7 Long term fuel trim",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,7*listOccupantHeight,parameterTableWidth-20,18,mode2,(HMENU) 9,NULL,NULL);
	OBD2PID12 = CreateWindow("BUTTON", "0xC Engine RPM",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,8*listOccupantHeight,parameterTableWidth-20,18,mode2,(HMENU) 10,NULL,NULL);
	OBD2PID13 = CreateWindow("BUTTON", "0xD Vehicle speed",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,9*listOccupantHeight,parameterTableWidth-20,18,mode2,(HMENU) 11,NULL,NULL);
	OBD2PID16 = CreateWindow("BUTTON", "0x10 Mass airflow rate",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,10*listOccupantHeight,parameterTableWidth-20,18,mode2,(HMENU) 12,NULL,NULL);
	OBD2PID17 = CreateWindow("BUTTON", "0x11 Throttle position",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,11*listOccupantHeight,parameterTableWidth-20,18,mode2,(HMENU) 13,NULL,NULL);
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
	OBD5PID0 = CreateWindow("BUTTON", "0x0 PIDs avail. 1-20",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,1*listOccupantHeight,parameterTableWidth-20,18,mode5,(HMENU) 3,NULL,NULL);
	OBD5PID1 = CreateWindow("BUTTON", "0x1 STFT O2 rich voltage thresh",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,2*listOccupantHeight,parameterTableWidth-20,18,mode5,(HMENU) 4,NULL,NULL);
	OBD5PID2 = CreateWindow("BUTTON", "0x2 STFT O2 lean voltage thresh",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,3*listOccupantHeight,parameterTableWidth-20,18,mode5,(HMENU) 5,NULL,NULL);
	OBD5PID3 = CreateWindow("BUTTON", "0x3",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,4*listOccupantHeight,parameterTableWidth-20,18,mode5,(HMENU) 6,NULL,NULL);
	OBD5PID4 = CreateWindow("BUTTON", "0x4",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,5*listOccupantHeight,parameterTableWidth-20,18,mode5,(HMENU) 7,NULL,NULL);
	OBD5PID5 = CreateWindow("BUTTON", "0x5",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,6*listOccupantHeight,parameterTableWidth-20,18,mode5,(HMENU) 8,NULL,NULL);
	OBD5PID6 = CreateWindow("BUTTON", "0x6",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,7*listOccupantHeight,parameterTableWidth-20,18,mode5,(HMENU) 9,NULL,NULL);
	OBD5PID7 = CreateWindow("BUTTON", "0x7",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,8*listOccupantHeight,parameterTableWidth-20,18,mode5,(HMENU) 10,NULL,NULL);
	OBD5PID8 = CreateWindow("BUTTON", "0x8",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,9*listOccupantHeight,parameterTableWidth-20,18,mode5,(HMENU) 11,NULL,NULL);
	}
	if(y_offset+mode6y_offset < parentHeight && y_offset+mode6y_offset+mode6Height > 0)
	{
	mode6 = CreateWindow("BUTTON", "MODE 0x6:Test results",WS_VISIBLE | WS_CHILD|BS_GROUPBOX,x_offset,mode6y_offset+y_offset,parameterTableWidth,mode6Height,parent,NULL,NULL,NULL);
	OBD6PID0 = CreateWindow("BUTTON", "0x0, PIDs avail. 1-20",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,1*listOccupantHeight,parameterTableWidth-20,18,mode6,(HMENU) 3,NULL,NULL);
	OBD6PID1 = CreateWindow("BUTTON", "0x1",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,2*listOccupantHeight,parameterTableWidth-20,18,mode6,(HMENU) 4,NULL,NULL);
	OBD6PID2 = CreateWindow("BUTTON", "0x2",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,3*listOccupantHeight,parameterTableWidth-20,18,mode6,(HMENU) 5,NULL,NULL);
	OBD6PID3 = CreateWindow("BUTTON", "0x3",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,4*listOccupantHeight,parameterTableWidth-20,18,mode6,(HMENU) 6,NULL,NULL);
	OBD6PID4 = CreateWindow("BUTTON", "0x4",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,5*listOccupantHeight,parameterTableWidth-20,18,mode6,(HMENU) 7,NULL,NULL);
	OBD6PID5 = CreateWindow("BUTTON", "0x5",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,6*listOccupantHeight,parameterTableWidth-20,18,mode6,(HMENU) 8,NULL,NULL);
	}
	if(y_offset+mode7y_offset < parentHeight && y_offset+mode7y_offset+mode7Height > 0)
	{
	mode7 = CreateWindow("BUTTON", "MODE 0x7:Pending DTCs",WS_VISIBLE | WS_CHILD|BS_GROUPBOX,x_offset,mode7y_offset+y_offset,parameterTableWidth,mode7Height,parent,NULL,NULL,NULL);
	OBD7PID0 = CreateWindow("BUTTON", "Pending DTCs",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,1*listOccupantHeight,parameterTableWidth-20,18,mode7,(HMENU) 3,NULL,NULL);
	}
	if(y_offset+mode8y_offset < parentHeight && y_offset+mode8y_offset+mode8Height > 0)
	{
	mode8 = CreateWindow("BUTTON", "MODE 0x8:Control components",WS_VISIBLE | WS_CHILD|BS_GROUPBOX,x_offset,mode8y_offset+y_offset,parameterTableWidth,mode8Height,parent,NULL,NULL,NULL);
	OBD8PID0 = CreateWindow("BUTTON", "0x0, PIDs avail. 1-20",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,1*listOccupantHeight,parameterTableWidth-20,18,mode8,(HMENU) 3,NULL,NULL);
	OBD8PID1 = CreateWindow("BUTTON", "0x1",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,2*listOccupantHeight,parameterTableWidth-20,18,mode8,(HMENU) 4,NULL,NULL);
	}
	if(y_offset+mode9y_offset < parentHeight && y_offset+mode9y_offset+mode9Height > 0)
	{
	mode9 = CreateWindow("BUTTON", "MODE 0x9:Request vehicle info",WS_VISIBLE | WS_CHILD|BS_GROUPBOX,x_offset,mode9y_offset+y_offset,parameterTableWidth,mode9Height,parent,NULL,NULL,NULL);
	OBD9PID0 = CreateWindow("BUTTON", "0x0, PIDs avail. 1-20",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,1*listOccupantHeight,parameterTableWidth-20,18,mode9,(HMENU) 3,NULL,NULL);
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
	OBD22PID512 = CreateWindow("BUTTON", "0x200 PIDs avail. 201-220",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,1*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 3,NULL,NULL);
	OBD22PID513 = CreateWindow("BUTTON", "0x201",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,2*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 4,NULL,NULL);
	OBD22PID515 = CreateWindow("BUTTON", "0x203",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,3*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 5,NULL,NULL);
	OBD22PID516 = CreateWindow("BUTTON", "0x204",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,4*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 6,NULL,NULL);
	OBD22PID517 = CreateWindow("BUTTON", "0x205 Fuel inj. pulse length",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,5*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 7,NULL,NULL);
	OBD22PID518 = CreateWindow("BUTTON", "0x206",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,6*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 8,NULL,NULL);
	OBD22PID519 = CreateWindow("BUTTON", "0x207 Commanded VVT position",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,7*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 9,NULL,NULL);
	OBD22PID520 = CreateWindow("BUTTON", "0x208 Measured VVT position",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,8*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 10,NULL,NULL);
	OBD22PID521 = CreateWindow("BUTTON", "0x209 Flag high cam engagement",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,9*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 11,NULL,NULL);
	OBD22PID522 = CreateWindow("BUTTON", "0x20A",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,10*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 12,NULL,NULL);
	OBD22PID523 = CreateWindow("BUTTON", "0x20B",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,11*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 13,NULL,NULL);
	OBD22PID524 = CreateWindow("BUTTON", "0x20C Calculated gear",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,12*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 14,NULL,NULL);
	OBD22PID525 = CreateWindow("BUTTON", "0x20D Target idle speed",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,13*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 15,NULL,NULL);
	OBD22PID526to529 = CreateWindow("BUTTON", "0x20E ECU part no.",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,14*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 16,NULL,NULL);
	OBD22PID530 = CreateWindow("BUTTON", "0x212",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,15*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 17,NULL,NULL);
	OBD22PID531 = CreateWindow("BUTTON", "0x213 Commanded A/F ratio",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,16*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 18,NULL,NULL);
	OBD22PID532 = CreateWindow("BUTTON", "0x214",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,17*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 19,NULL,NULL);
	OBD22PID533 = CreateWindow("BUTTON", "0x215 PortC data",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,18*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 20,NULL,NULL);
	OBD22PID534 = CreateWindow("BUTTON", "0x216 PortF0 data",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,19*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 21,NULL,NULL);
	OBD22PID536 = CreateWindow("BUTTON", "0x218",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,20*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 22,NULL,NULL);
	OBD22PID537 = CreateWindow("BUTTON", "0x219",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,21*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 23,NULL,NULL);
	OBD22PID538 = CreateWindow("BUTTON", "0x21A",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,22*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 24,NULL,NULL);
	OBD22PID539 = CreateWindow("BUTTON", "0x21B",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,23*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 25,NULL,NULL);
	OBD22PID540to543 = CreateWindow("BUTTON", "0x21C Calibration ID",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,24*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 26,NULL,NULL);
	OBD22PID544 = CreateWindow("BUTTON", "0x220 PIDs avail. 221-240",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,25*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 27,NULL,NULL);
	OBD22PID545to548 = CreateWindow("BUTTON", "0x221 Make, model and year",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,26*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 28,NULL,NULL);
	OBD22PID549 = CreateWindow("BUTTON", "0x225 Time with 0-1.5% throttle",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,27*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 29,NULL,NULL);
	OBD22PID550 = CreateWindow("BUTTON", "0x226 Time with 1.5-15% throttle",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,28*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 30,NULL,NULL);
	OBD22PID551 = CreateWindow("BUTTON", "0x227 Time with 15-25% throttle",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,29*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 31,NULL,NULL);
	OBD22PID552 = CreateWindow("BUTTON", "0x228 Time with 25-35% throttle",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,30*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 32,NULL,NULL);
	OBD22PID553 = CreateWindow("BUTTON", "0x229 Time with 35-50% throttle",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,31*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 33,NULL,NULL);
	OBD22PID554 = CreateWindow("BUTTON", "0x22A Time with 50-65% throttle",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,32*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 34,NULL,NULL);
	OBD22PID555 = CreateWindow("BUTTON", "0x22B Time with 65-80% throttle",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,33*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 35,NULL,NULL);
	OBD22PID556 = CreateWindow("BUTTON", "0x22C Time with 80-100% throttle",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,34*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 36,NULL,NULL);
	OBD22PID557 = CreateWindow("BUTTON", "0x22D Time with 500-1500 rpm",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,35*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 37,NULL,NULL);
	OBD22PID558 = CreateWindow("BUTTON", "0x22E Time with 1500-2500 rpm",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,36*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 38,NULL,NULL);
	OBD22PID559 = CreateWindow("BUTTON", "0x22F Time with 2500-3500 rpm",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,37*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 39,NULL,NULL);
	OBD22PID560 = CreateWindow("BUTTON", "0x230 Time with 3500-4500 rpm",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,38*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 40,NULL,NULL);
	OBD22PID561 = CreateWindow("BUTTON", "0x231 Time with 4500-5500 rpm",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,39*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 41,NULL,NULL);
	OBD22PID562 = CreateWindow("BUTTON", "0x232 Time with 5500-6500 rpm",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,40*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 42,NULL,NULL);
	OBD22PID563 = CreateWindow("BUTTON", "0x233 Time with 6500-7000 rpm",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,41*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 43,NULL,NULL);
	OBD22PID564 = CreateWindow("BUTTON", "0x234 Time with 7000-7500 rpm",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,42*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 44,NULL,NULL);
	OBD22PID565 = CreateWindow("BUTTON", "0x235 Time at 0-30 km/h",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,43*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 45,NULL,NULL);
	OBD22PID566 = CreateWindow("BUTTON", "0x236 Time at 30-60 km/h",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,44*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 46,NULL,NULL);
	OBD22PID567 = CreateWindow("BUTTON", "0x237 Time at 60-90 km/h",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,45*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 47,NULL,NULL);
	OBD22PID568 = CreateWindow("BUTTON", "0x238 Time at 90-120 km/h",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,46*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 48,NULL,NULL);
	OBD22PID569 = CreateWindow("BUTTON", "0x239 Time at 120-150 km/h",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,47*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 49,NULL,NULL);
	OBD22PID570 = CreateWindow("BUTTON", "0x23A Time at 150-180 km/h",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,48*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 50,NULL,NULL);
	OBD22PID571 = CreateWindow("BUTTON", "0x23B Time at 180-210 km/h",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,49*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 51,NULL,NULL);
	OBD22PID572 = CreateWindow("BUTTON", "0x23C Time at 210-240 km/h",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,50*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 52,NULL,NULL);
	OBD22PID573 = CreateWindow("BUTTON", "0x240 PIDs avail. 241-260",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,51*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 53,NULL,NULL);
	OBD22PID577 = CreateWindow("BUTTON", "0x246 Time with coolant 105-110C",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,52*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 54,NULL,NULL);
	OBD22PID583 = CreateWindow("BUTTON", "0x247 Time with coolant 110-115C",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,53*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 55,NULL,NULL);
	OBD22PID584 = CreateWindow("BUTTON", "0x248 Time with coolant 115-120C",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,54*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 56,NULL,NULL);
	OBD22PID585 = CreateWindow("BUTTON", "0x249 Time with coolant 120+C",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,55*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 57,NULL,NULL);
	OBD22PID586 = CreateWindow("BUTTON", "0x24E High RPM 1",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,56*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 58,NULL,NULL);
	OBD22PID591 = CreateWindow("BUTTON", "0x24F High RPM 2",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,57*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 59,NULL,NULL);
	OBD22PID592 = CreateWindow("BUTTON", "0x250 High RPM 3",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,58*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 60,NULL,NULL);
	OBD22PID593 = CreateWindow("BUTTON", "0x251 High RPM 4",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,59*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 61,NULL,NULL);
	OBD22PID594 = CreateWindow("BUTTON", "0x252 High RPM 5",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,60*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 62,NULL,NULL);
	OBD22PID595 = CreateWindow("BUTTON", "0x253 Engine coolant temp at high rpm 1",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,61*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 63,NULL,NULL);
	OBD22PID596 = CreateWindow("BUTTON", "0x255 Engine hrs at high rpm 1",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,62*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 64,NULL,NULL);
	OBD22PID598 = CreateWindow("BUTTON", "0x256 Engine coolant temp at high rpm 2",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,63*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 65,NULL,NULL);
	OBD22PID599 = CreateWindow("BUTTON", "0x258 Engine hrs at high rpm 2",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,64*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 66,NULL,NULL);
	OBD22PID601 = CreateWindow("BUTTON", "0x259 Engine coolant temp at high rpm 3",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,65*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 67,NULL,NULL);
	OBD22PID602 = CreateWindow("BUTTON", "0x25B Engine hrs at high rpm 3",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,66*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 68,NULL,NULL);
	OBD22PID603 = CreateWindow("BUTTON", "0x25C Engine coolant temp at high rpm 4",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,67*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 69,NULL,NULL);
	OBD22PID605 = CreateWindow("BUTTON", "0x25E Engine hrs at high rpm 4",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,68*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 70,NULL,NULL);
	OBD22PID606 = CreateWindow("BUTTON", "0x25F Engine coolant temp at high rpm 5",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,69*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 71,NULL,NULL);
	OBD22PID608 = CreateWindow("BUTTON", "0x260 PIDs avail. 261-280",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,70*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 72,NULL,NULL);
	OBD22PID609 = CreateWindow("BUTTON", "0x262 Engine hrs at high rpm 5",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,71*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 73,NULL,NULL);
	OBD22PID610 = CreateWindow("BUTTON", "0x263 Max speed (km/h) 1",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,72*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 74,NULL,NULL);
	OBD22PID612 = CreateWindow("BUTTON", "0x264 Max speed (km/h) 2",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,73*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 75,NULL,NULL);
	OBD22PID613 = CreateWindow("BUTTON", "0x265 Max speed (km/h) 3",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,74*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 76,NULL,NULL);
	OBD22PID614 = CreateWindow("BUTTON", "0x266 Max speed (km/h) 4",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,75*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 77,NULL,NULL);
	OBD22PID615 = CreateWindow("BUTTON", "0x267 Max speed (km/h) 5",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,76*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 78,NULL,NULL);
	OBD22PID616 = CreateWindow("BUTTON", "0x268 Fastest 0-100 km/h",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,77*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 79,NULL,NULL);
	OBD22PID617 = CreateWindow("BUTTON", "0x269 Fastest 0-160 km/h",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,78*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 80,NULL,NULL);
	OBD22PID618 = CreateWindow("BUTTON", "0x26A Most recent 0-100 km/h",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,79*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 81,NULL,NULL);
	OBD22PID619 = CreateWindow("BUTTON", "0x26B Most recent 0-160 km/h",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,80*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 82,NULL,NULL);
	OBD22PID620 = CreateWindow("BUTTON", "0x26C Total engine runtime",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,81*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 83,NULL,NULL);
	OBD22PID621 = CreateWindow("BUTTON", "0x26D Number of standing starts",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,82*listOccupantHeight,parameterTableWidth-20,18,mode22,(HMENU) 84,NULL,NULL);
	}

	if(y_offset+mode2Fy_offset < parentHeight && y_offset+mode2Fy_offset+mode2FHeight > 0)
	{
	//OBD MODE 0x2F PID buttons, 28 PIDs
	mode2F = CreateWindow("BUTTON", "MODE 0x2F:Veh.func.~~DANGER~~",WS_VISIBLE | WS_CHILD|BS_GROUPBOX ,x_offset,mode2Fy_offset+y_offset,parameterTableWidth,mode2FHeight,parent,NULL,NULL,NULL);
	OBD2FPID100 = CreateWindow("BUTTON", "0x100 PIDs avail. 101-120",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,1*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 3,NULL,NULL);
	OBD2FPID101 = CreateWindow("BUTTON", "0x101 Fire injector",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,2*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 4,NULL,NULL);
	OBD2FPID102 = CreateWindow("BUTTON", "0x102 Fire injector",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,3*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 5,NULL,NULL);
	OBD2FPID103 = CreateWindow("BUTTON", "0x103 Fire injector",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,4*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 6,NULL,NULL);
	OBD2FPID104 = CreateWindow("BUTTON", "0x104 Fire injector",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,5*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 7,NULL,NULL);
	OBD2FPID120 = CreateWindow("BUTTON", "0x120 PIDs avail. 121-140",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,6*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 8,NULL,NULL);
	OBD2FPID121 = CreateWindow("BUTTON", "0x121 Set VVL solenoid PWM",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,7*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 9,NULL,NULL);
	OBD2FPID122 = CreateWindow("BUTTON", "0x122 Set VVT solenoid PWM",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,8*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 10,NULL,NULL);
	OBD2FPID125 = CreateWindow("BUTTON", "0x125 Test engine speed in cluster",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,9*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 11,NULL,NULL);
	OBD2FPID126 = CreateWindow("BUTTON", "0x126 Test vehicle speed in cluster",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,10*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 12,NULL,NULL);
	OBD2FPID127 = CreateWindow("BUTTON", "0x127 Set Evap solenoid PWM",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,11*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 13,NULL,NULL);
	OBD2FPID12A = CreateWindow("BUTTON", "0x12A",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,12*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 14,NULL,NULL);
	OBD2FPID140 = CreateWindow("BUTTON", "0x140 PIDs avail. 141-160",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,13*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 15,NULL,NULL);
	OBD2FPID141 = CreateWindow("BUTTON", "0x141",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,14*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 16,NULL,NULL);
	OBD2FPID142 = CreateWindow("BUTTON", "0x142",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,15*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 17,NULL,NULL);
	OBD2FPID143 = CreateWindow("BUTTON", "0x143 Adj PORTC0 data",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,16*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 18,NULL,NULL);
	OBD2FPID144 = CreateWindow("BUTTON", "0x144 Adj PORTC0 data",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,17*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 19,NULL,NULL);
	OBD2FPID146 = CreateWindow("BUTTON", "0x146",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,18*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 20,NULL,NULL);
	OBD2FPID147 = CreateWindow("BUTTON", "0x147 Turn on MIL light in cluster",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,19*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 21,NULL,NULL);
	OBD2FPID148 = CreateWindow("BUTTON", "0x148 Turn on oil light in cluster",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,20*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 22,NULL,NULL);
	OBD2FPID149 = CreateWindow("BUTTON", "0x149",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,21*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 23,NULL,NULL);
	OBD2FPID14A = CreateWindow("BUTTON", "0x14A",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,22*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 24,NULL,NULL);
	OBD2FPID14B = CreateWindow("BUTTON", "0x14B",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,23*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 25,NULL,NULL);
	OBD2FPID160 = CreateWindow("BUTTON", "0x160, PIDs avail. 161-180",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,24*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 26,NULL,NULL);
	OBD2FPID161 = CreateWindow("BUTTON", "0x161 Fire spark",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,5,25*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 27,NULL,NULL);
	OBD2FPID162 = CreateWindow("BUTTON", "0x162 Fire spark",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,26*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 28,NULL,NULL);
	OBD2FPID163 = CreateWindow("BUTTON", "0x163 Fire spark",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,27*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 29,NULL,NULL);
	OBD2FPID164 = CreateWindow("BUTTON", "0x164 Fire spark",WS_VISIBLE | WS_CHILD|BS_AUTO3STATE,5,28*listOccupantHeight,parameterTableWidth-20,18,mode2F,(HMENU) 30,NULL,NULL);
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
	std::string MODE1Titles[] = {"0x0 PIDs avail. 1-20","0x1 Monitor Status Since DTCs cleared","0x2 DTC which triggered freeze frame","0x3 Fuel system status","0x4 Calc'd engine load (% at STP)","0x5 Coolant Temp (C)","0x6 Short term fuel trim (%)","0x7 Long term fuel trim (%)","0xC Engine speed (rpm)","0xD Vehicle speed (km/h)","0xE Timing advance (deg. before TDC)","0xF Intake air temp (C)","0x10 Mass air flow (g/sec)","0x11 Throttle position (%)","0x13 Oxygen sensors present","0x14 Oxygen sensor 1 (V),Oxygen sensor 1 short term fuel trim (%)","0x15 Oxygen sensor 2 (V),Oxygen sensor 2 short term fuel trim (%)","0x1C OBD standards this vehicle conforms to","0x1F Run time since engine start (s)","0x20 PIDs supported 21-40","0x21 Distance traveled with MIL on (km)","0x2E Command evaporative purge (%)","0x2F Fuel tank level (%)","0x33 Absolute pressure (kPa)","0x40 PIDs supported 41-60","0x42 Control module voltage (V)","0x43 Absolute load (%)"," 0x45 Relative throttle position (%)"};
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
	std::string MODE2Titles[] = {"0x0 PIDs avail. 1-20","0x2 DTC which triggered freeze frame","0x3 Fuel system status","0x4 Calc'd engine load (% at STP)","0x5 Coolant Temp (C)","0x6 Short term fuel trim (%)","0x7 Long term fuel trim (%)","0xC Engine speed (rpm)","0xD Vehicle speed (km/h)","0x10 Mass air flow (g/sec)","0x11 Throttle position (%)"};
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
	std::string MODE5Titles[] = {"0x0 PIDs avail. 1-20","0x1 STFT O2 rich voltage thresh.","0x2 STFT O2 lean voltage thresh.","0x3","0x4","0x5","0x6","0x7","0x8"};
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
	std::string MODE6Titles[] = {"0x0 PIDs avail. 1-20","0x1","0x2","0x3","0x4","0x5"};
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
	std::string MODE8Titles[] = {"0x0 PIDs avail. 1-20","0x1"};
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
	std::string MODE9Titles[] = {"0x0 PIDs avail. 1-20","0x1 VIN message count","0x2 VIN","0x3 CAL ID message count","0x4 CAL ID","0x5 CVN message count","0x6 CVN"};
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
	std::string MODE22Titles[] = {"0x200 PIDs avail 201-220","0x201","0x203","0x204","0x205 Fuel injection pulse length (microsecond)","0x206","0x207 Commanded VVT position (deg. before TDC)","0x208 Measured VVT position (deg. before TDC)","0x209 Flag for high cam engagement","0x20A","0x20B","0x20C Calculated gear","0x20D Target idle speed (rpm)","0x20E ECU part no.","0x212","0x213 Commanded A/F","0x214","0x215 PORTC data","0x216 PortF0 data","0x218","0x219","0x21A","0x21B","0x21C Calibration ID","0x220 PIDs avail. 221-240","0x221 Make model and year","0x225 Time at 0-1.5% throttle (s)","0x226 Time at 1.5-15% throttle (s)","0x227 Time at 15-25% throttle (s)","0x228 Time at 25-35% throttle (s)","0x229 Time at 35-50% throttle (s)","0x22A Time at 50-65% throttle (s)","0x22B Time at 65-80% throttle (s)","0x22C Time at 80-100% throttle (s)","0x22D Time with engine speed 500-1500 rpm (s)","0x22E Time with engine speed 1500-2500 rpm (s)","0x22F Time with engine speed 2500-3500 rpm (s)","0x230 Time with engine speed 3500-4500 rpm (s)","0x231 Time with engine speed 4500-5500 rpm (s)","0x232 Time with engine speed 5500-6500 rpm (s)","0x233 Time with engine speed 6500-7000 rpm (s)","0x234 Time with engine speed 7000-7500 rpm (s)","0x235 Time at wheel speed 0-30 km/h (s)","0x236 Time at wheel speed 30-60 km/h (s)","0x237 Time at wheel speed 60-90 km/h (s)","0x238 Time at wheel speed 90-120 km/h (s)","0x239 Time at wheel speed 120-150 km/h (s)","0x23A Time at wheel speed 150-180 km/h (s)","0x23B Time at wheel speed 180-210 km/h (s)","0x23C Time at wheel speed 210-240 km/h (s)","0x240 PIDs avail. 241-260","0x246 Time with coolant temperature 105-110C (s)","0x247 Time with coolant temperature 110-115C (s)","0x248 Time with coolant temperature 115-120C (s)","0x249 Time with coolant temperature 120+C (s)","0x24E High RPM 1","0x24F High RPM 2","0x250 High RPM 3","0x251 High RPM 4","0x252 High RPM 5","0x253 Engine coolant temp at high rpm 1 (deg C)","0x255 Engine hrs at high rpm 1 (s)","0x256 Engine coolant temp at high rpm 2 (deg C)","0x258 Engine hrs at high rpm 2 (s)","0x259 Engine coolant temp at high rpm 3 (deg C)","0x25B Engine hrs at high rpm 3 (s)","0x25C Engine coolant temp at high rpm 4 (deg C)","0x25E Engine hrs at high rpm 4 (s)","0x25F Engine coolant temp at high rpm 5 (deg C)","0x260 PIDs avail. 261-280","0x262 Engine hrs at high rpm 5 (s)","0x263 Max wheel speed (km/h) 1","0x264 Max wheel speed (km/h) 2","0x265 Max wheel speed (km/h) 3","0x266 Max wheel speed (km/h) 4","0x267 Max wheel speed (km/h) 5","0x268 Fastest 0-100 km/h (s)","0x269 Fastest 0-160 km/h (s)","0x26A Most recent 0-100 km/h (s)","0x26B Most recent 0-160 km/h (s)","0x26C Total engine runtime (s)","0x26D Number of standing starts (launches)"};
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
	std::string MODE2FTitles[] = {"0x100 PIDs avail. 101-120","0x101 Fire injector","0x102 Fire injector","0x103 Fire injector","0x104 Fire injector","0x120 PIDs avail. 121-140","0x121 Set VVL PWM","0x122 Set VVT PWM","0x125 Test engine speed in gauge cluster","0x126 Test vehicle speed in gauge cluster","0x127 Set evap. purge PWM","0x12A","0x140 PIDs avail. 141-160","0x141","0x142","0x143 Set PORTC data","0x144 Set PORTC data","0x146","0x147 Test MIL light in gauge cluster","0x148 Test oil light in gauge cluster","0x149","0x14A","0x14B","0x160 PIDs avail. 161-180","0x161 Fire ignition","0x162 Fire ignition","0x163 Fire ignition","0x164 Fire ignition"};
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

	int plotDataColumn = -1;

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
		if(plotDataColumn >= 0)
		{
			for(int k = 0; k < 99; k++)
			{
				allData[k][plotDataColumn] = allData[k+1][plotDataColumn];
			}
			//add the new data in the last place
			allData[99][plotDataColumn] = data;
		}
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
	DestroyWindow(reflashModeButton);

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

	//reflashmode button
	reflashModeButton = CreateWindow("BUTTON", "Reflash Mode",WS_VISIBLE | WS_CHILD| WS_BORDER|BS_PUSHBUTTON,logParamsX+55+145,logParamsY+30+logParamsHeight,100,25,hwnd,(HMENU) 4,NULL,NULL);
}


/* CALLBACK for the second window */
LRESULT CALLBACK reflashWindowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_SIZE:
    {
    	//also gets called on the first draw
    	HWND returnButton = CreateWindow("BUTTON", "Return to OBD mode",WS_VISIBLE | WS_CHILD| WS_BORDER|BS_PUSHBUTTON,10,5,150,25,hwnd,(HMENU) 1,NULL,NULL);

    	//initiate com port button
    	//create the combo box dropdown list
    	HWND comPortSelect = CreateWindow("COMBOBOX", TEXT(""), WS_VISIBLE | WS_CHILD| WS_BORDER | CBS_DROPDOWNLIST,520,5,100,200,hwnd,(HMENU) 2,NULL,NULL);
    	checkForAndPopulateComPortList(comPortSelect);//populate the com port dropdown list
    	SendMessage(comPortSelect, CB_SETCURSEL, (WPARAM)0, (LPARAM)0);//set the cursor to the 0th item on the list (WPARAM)
    	SendMessage(comPortSelect, CB_SETITEMHEIGHT, (WPARAM)0, (LPARAM)18);//set the height of the dropdown list items

    	HWND inputFileName = CreateWindow("STATIC", "Input file name of s-record/binary which contains bytes for reflashing:",WS_VISIBLE | WS_CHILD|SS_CENTER| WS_BORDER,5,40,470,19,hwnd,NULL,NULL,NULL);
    	HWND input = CreateWindow("EDIT", "",WS_VISIBLE | WS_CHILD,5,65,600,20,hwnd,NULL,NULL,NULL);

    	HWND inputReflashRange = CreateWindow("STATIC", "Input file name which contains address ranges for reflashing/reading:",WS_VISIBLE | WS_CHILD|SS_CENTER| WS_BORDER,5,90,470,19,hwnd,NULL,NULL,NULL);
    	HWND reflashRange = CreateWindow("EDIT", "",WS_VISIBLE | WS_CHILD,5,115,600,20,hwnd,NULL,NULL,NULL);

    	//initiate encoding bytes button
    	//create the combo box dropdown list
    	HWND encodeByteTypeSelect = CreateWindow("COMBOBOX", TEXT("------"), WS_VISIBLE | WS_CHILD| WS_BORDER | CBS_DROPDOWNLIST,10,140,150,200,hwnd,(HMENU) 2,NULL,NULL);
    	SendMessage(encodeByteTypeSelect,(UINT) CB_ADDSTRING,(WPARAM) 0,(LPARAM) "T4, C121E0001F K0303");
    	SendMessage(encodeByteTypeSelect,(UINT) CB_ADDSTRING,(WPARAM) 0,(LPARAM) "K4, C117M0039F K0205");
    	SendMessage(encodeByteTypeSelect, CB_SETCURSEL, (WPARAM)0, (LPARAM)0);//set the cursor to the 0th item on the list (WPARAM)
    	SendMessage(encodeByteTypeSelect, CB_SETITEMHEIGHT, (WPARAM)0, (LPARAM)18);//set the height of the dropdown list items
    	HWND stageI = CreateWindow("BUTTON", "Use Stage I bootloader",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,170,3,170,18,hwnd,(HMENU) 6,NULL,NULL);
    	HWND clearMemorySector = CreateWindow("BUTTON", "Clear relevant memory sectors when reflashing",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,170,22,330,18,hwnd,(HMENU) 7,NULL,NULL);
    	HWND readMemory = CreateWindow("BUTTON", "Read memory",WS_VISIBLE | WS_CHILD|BS_AUTOCHECKBOX,350,3,120,18,hwnd,(HMENU) 8,NULL,NULL);

    	HWND encodeMessageButton = CreateWindow("BUTTON", "Encode Reflash/Read Message",WS_VISIBLE | WS_CHILD| WS_BORDER|BS_PUSHBUTTON,10+150+10,140,220,25,hwnd,(HMENU) 3,NULL,NULL);
    	HWND decodeMessageButton = CreateWindow("BUTTON", "Test Decode Reflash Message",WS_VISIBLE | WS_CHILD| WS_BORDER|BS_PUSHBUTTON,10+150+10+10+10+220,140,210,25,hwnd,(HMENU) 4,NULL,NULL);

    	HWND Instructions = CreateWindow("STATIC", "1) Write the name of the s-record or binary file which contains rom with the bytes we'll send to the ECU as well as the name of the file which contains hex address ranges line-by-line \n 2) If using the Stage I bootloader, set the check box \n 3) If using the Stage II bootloader, choose the reflash encryption byte set \n 4) Encode the packet-> check the text file logs \n 5) Press 'Reflash ROM' and turn car to key-on",WS_VISIBLE | WS_CHILD|SS_CENTER| WS_BORDER,5,170,600,100,hwnd,NULL,NULL,NULL);

    	HWND reflash = CreateWindow("BUTTON", "Reflash/Read ROM",WS_VISIBLE | WS_CHILD| WS_BORDER|BS_PUSHBUTTON,10,280,130,25,hwnd,(HMENU) 5,NULL,NULL);
    	break;
    }
    case WM_COMMAND:
    {
    	//first, load some of the window handles so that we can collect information from them
    	//these are found in order so if any changes are made then the labels will be incorrect
    	HWND parentWindowHandle = FindWindowEx(NULL,NULL,"ReflashModeClassName",NULL);//find the handle for the reflash mode window
    	HWND returntoobdWindowHandle = FindWindowEx(parentWindowHandle,NULL,NULL,NULL);//find the handle for the return to obd mode button
    	HWND comportSelectWindowHandle = FindWindowEx(parentWindowHandle,returntoobdWindowHandle,NULL,NULL);//find the handle for the comport select drop down
    	HWND inputFileNameWindowHandle = FindWindowEx(parentWindowHandle,comportSelectWindowHandle,NULL,NULL);//find the handle for the input file name title
    	HWND inputWindowHandle = FindWindowEx(parentWindowHandle,inputFileNameWindowHandle,NULL,NULL);//find the handle for the input file name
    	HWND fileRangeTitleWindowHandle = FindWindowEx(parentWindowHandle,inputWindowHandle,NULL,NULL);//find the handle for the address range file name title
    	HWND fileRangeWindowHandle = FindWindowEx(parentWindowHandle,fileRangeTitleWindowHandle,NULL,NULL);//find the handle for the address range file name title
    	HWND encodeBytesWindowHandle = FindWindowEx(parentWindowHandle,fileRangeWindowHandle,NULL,NULL);//find the handle for the window where we select encoding bytes
    	HWND stageIbootloaderCheckboxWindowHandle = FindWindowEx(parentWindowHandle,fileRangeWindowHandle,NULL,NULL);//find the handle for the checkbox that selects the stage I bootloader
    	HWND clearFlashMemorySectorCheckboxWindowHandle = FindWindowEx(parentWindowHandle,stageIbootloaderCheckboxWindowHandle,NULL,NULL);//find the handle for the checkbox that selects 'clear flash memory sector'
    	HWND readMemoryCheckboxWindowHandle = FindWindowEx(parentWindowHandle,clearFlashMemorySectorCheckboxWindowHandle,NULL,NULL);//find the handle for the checkbox that selects 'read memory'




    	switch(LOWORD (wParam))
    	{
    	case 1:
    	{
    		//return to regular operating mode
    		ShowWindow(hwnd,SW_HIDE);//hide this window
    		HWND windowHandle = FindWindowEx(NULL,NULL,szClassName,NULL);//find the handle for the main window
    		ShowWindow(windowHandle,SW_SHOW);//show the main window
    		break;
    	}
    	case 3:
    	{
    		//we now want to encode a reflash packet

    		//extract the file names
    		char windowText[100];
    		//first, get the s-record file name
    		int length = GetWindowText(inputWindowHandle,windowText,100);
    		string srecFile = "";
    		for(int i = 0; i < length; i++)
    		{
    			srecFile.push_back(windowText[i]);
    		}
    		//cout << srecFile << '\n';
    		//next, get the address range file name
    		string addressRangeFile = "";
    		length = GetWindowText(fileRangeWindowHandle,windowText,100);
    		for(int i = 0; i < length; i++)
    		{
    			addressRangeFile.push_back(windowText[i]);
    		}
    		//cout << addressRangeFile << '\n';
    		//next, grab the key byte type from the drop down
    		int keyByteSet = SendMessage(encodeBytesWindowHandle, (UINT) CB_GETCURSEL, (WPARAM) 0, (LPARAM) 0);
    		cout << "Key byte set: " << keyByteSet << '\n';

    		//finally, encode a message to send to the ECU
    		//this takes some time, so the program may act like it hangs for a bit...
    		cout << "Creating reflash packet... please wait" << '\n';
    		if(stageIbootloader)
    		{
    			//prepare stage I bootloader packet
    			reflashingTimerProcMode = 4;
    			CRC = 291;
    			filePosition = 0;
    			numpacketsSent = 0;
    			//ifstream addressRanges(addressRangeFile.c_str());
    			string line = "0x8000-0xFFFE";
    			uint8_t address[3] = {0,0,0};
    			//if(addressRanges.is_open())
    			{
    				//getline(addressRanges, line);
    				bytesToSend = interpretAddressRange(line, address);
    			}
    			//addressRanges.close();
    			addressToFlash = (address[0] << 16) + (address[1] << 8) + (address[2]);
    			ofstream outputPacket("encodedFlashBytes.txt", ios::trunc);
    			outputPacket.close();
    			CRC = stageIbootloaderRomFlashPacketBuilder(addressToFlash, bytesToSend, CRC, numpacketsSent, srecFile, filePosition);
    			if(bytesToSend > 0)
    			{
    				SetTimer(hwnd, logPollTimer, 10, (TIMERPROC) NULL);//10 ms delay (or use 100ms)
    			}
    		}
    		else if(readMemory)
    		{
    			//collect the address range to flash, only read in one line from the file
    			ifstream addressRanges(addressRangeFile.c_str());
    			string line = "";
    			uint8_t address[3] = {0,0,0};
    			if(addressRanges.is_open())
    			{
    				getline(addressRanges, line);
    				bytesToSend = interpretAddressRange(line, address);
    			}
    			addressRanges.close();
    			uint8_t sendBuffer[8] = {6,116,0,0,0,0,0,0x11C};//116 message is to read address data
    			ofstream temp("encodedFlashBytes.txt", ios::trunc);
    			if(keyByteSet == 1)
    			{
    				//K4 encoded first message
    				temp << "59 112 0 0 0 62 0 0 0 0 0 0 0 0 0 0 0 0 135 21 0 135 21 0 135 21 0 135 21 0 135 21 0 191 12 0 140 170 0 218 48 1 110 39 1 86 44 1 200 160 0 91 70 0 182 34 0 52 9 1 57 " << '\n';
    			}
    			else
    			{
    				//T4 encoded first message
    				temp << "59 112 0 0 0 62 0 0 0 0 0 0 0 0 0 0 0 0 26 139 1 26 139 1 26 139 1 26 139 1 26 139 1 206 51 1 206 108 3 140 203 2 63 73 3 169 181 1 55 10 5 162 119 5 60 134 2 6 23 4 242 " << '\n';
    			}
    			for(long i = 0; i < bytesToSend; i+=16)
    			{
    				uint8_t byteSum = 0;
    				sendBuffer[3] = address[0];
    				sendBuffer[4] = address[1];
    				sendBuffer[5] = address[2];
    				sendBuffer[6] = 16;//for my convenience, always read 16 bytes
    				//calculate the checksum
    				//then upload the packet to the temporary document and increment the total byte sum
    				for(uint8_t j = 0; j < 7; j++)
    				{
    					temp << (int) sendBuffer[j] << ' ';
    					byteSum += sendBuffer[j];
    				}
    				sendBuffer[7] = byteSum;
    				temp << (int) sendBuffer[7] << ' ' << '\n';
    				//then increment the address
    				uint8_t prevAddrByte3 = address[2];
    				uint8_t prevAddrByte2 = address[1];
    				address[2] += 16;
    				if(address[2] < prevAddrByte3)
    				{
    					address[1] += 1;
    					if(address[1] < prevAddrByte2)
    					{
    						address[0] += 1;
    					}
    				}
    			}
    			temp << "1 115 116 ";
    			temp.close();
    			cout << "Finished creating read packets" << '\n';
    		}
    		else
    		{
    			//prepare stage II bootloader packet- this takes a long time and the program will look like it 'locks up'
    			createReflashPacket(srecFile, addressRangeFile, keyByteSet, clearMemorySector);
    			cout << "Finished creating reflashing packet" << '\n';
    		}
    		//cout << "Finished creating reflashing packet" << '\n';

    		//try to return the filled in parameters to what they were before the operation
    		SendMessage(encodeBytesWindowHandle, CB_SETCURSEL, (WPARAM)keyByteSet, (LPARAM)0);//set the cursor to the keyByte-th item on the list (WPARAM)
    		RedrawWindow(encodeBytesWindowHandle, NULL, NULL, RDW_UPDATENOW);
    		TCHAR const * psz = srecFile.c_str();
    		SetWindowText(inputWindowHandle,psz);
    		RedrawWindow(inputWindowHandle, NULL, NULL, RDW_UPDATENOW);
    		TCHAR const * psx = addressRangeFile.c_str();
    		SetWindowText(fileRangeWindowHandle,psx);
    		RedrawWindow(fileRangeWindowHandle, NULL, NULL, RDW_UPDATENOW);
    		break;
    	}
    	case 4:
    	{
    		//test decode bootloader packets
    		if(stageIbootloader)
    		{
    			cout << "Nothing to decode for Stage I bootloader - these are not encrypted" << '\n';
    		}
    		else if(readMemory)
    		{
    			//we want to decode the read memory packets - there's not much to do here
    			cout << "Nothing to decode for Stage II bootloader read messages - these are not encrypted" << '\n';
    		}
    		else
    		{
    			//we want to test decode an encoded file
    			//figure out which key byte set we need...
    			int keyByteSet = SendMessage(encodeBytesWindowHandle, (UINT) CB_GETCURSEL, (WPARAM) 0, (LPARAM) 0);
    			//decode the file
    			cout << "Decoding the encoded file... please wait" << '\n';
    			testReflashPacket(keyByteSet);
    			cout << "Finished decoding the packet: please check the text file 'testDecodedFlashPacket.txt' and compare to your s-record" << '\n';
    		}
    		break;
    	}
    	case 5:
    	{
    		//we want to send messages---
    		if(stageIbootloader)
    		{
    			cout << "Stage I bootloader: " << '\n';

    			//we want to use the stage I bootloader to send messages

    			//open a COM port
    			//update the stored com port value
    			int comIndex = SendMessage(comportSelectWindowHandle, (UINT) CB_GETCURSEL, (WPARAM) 0, (LPARAM) 0);
    			SendMessage(comportSelectWindowHandle, (UINT) CB_GETLBTEXT, (WPARAM) comIndex, (LPARAM) ListItem);//global var ListItem will now contain the COM port name

    			char baud[5] = {'0','9','6','1','7'};
    			comPortHandle = setupComPort(baud, ListItem);//open the com port


    			//run the 'pull l-line low' and send '83, 89, 78 while reading a byte between each' routine
    			bool success = true;
    			comPortHandle = initializeStageIReflashMode(comPortHandle, ListItem, success);


    			//begin the timed routines by setting a timer
    			if(success == true)
    			{
    				cout << "reflash mode begin; time to send bytes.." << '\n';
    				//send the first packet
    				uint8_t sendBuffer[12] = {2,48,48,48,56,48,200,3,26,57,101,12};
    				writeToSerialPort(comPortHandle, sendBuffer, 12);
    				uint8_t readBackSendBuffer[12];
    				readFromSerialPort(comPortHandle, readBackSendBuffer, 12, 300);//read back the bytes we sent

    				lastPositionForReflashing = 0;
    				//switch to the mode where we read back bytes sent by the ECU
    				reflashingTimerProcMode = 3;

    				SetTimer(hwnd, logPollTimer, 10, (TIMERPROC) NULL);//10 ms timer
    			}
    			else
    			{
    				CloseHandle(comPortHandle);
    			}

    		}
    		else
    		{
    			cout << "stage II" << '\n';

    			if(readMemory)
    			{
    				//clear the read file to get ready for a new data set
    				ofstream newSREC("ROM read.txt",ios::trunc);
    				newSREC.close();

    				//binary files are daft, so we will fill the file with null characters
    				//in order to fill up space where we will not read in data from the ECU

    				//extract the file names for the range of data we'll read in
    				char windowText[100];
    				string addressRangeFile = "";
    				int length = GetWindowText(fileRangeWindowHandle,windowText,100);
    				for(int i = 0; i < length; i++)
    				{
    					addressRangeFile.push_back(windowText[i]);
    				}

    				//collect the address range to flash, only read in one line from the file
    				ifstream addressRanges(addressRangeFile.c_str(), ios::in);
    				string line = "";
    				uint8_t address[3] = {0,0,0};
    				if(addressRanges.is_open())
    				{
    					getline(addressRanges, line);
    					bytesToSend = interpretAddressRange(line, address);
    				}
    				addressRanges.close();
    				//now we have a 3 byte array which contains the address we'll begin reading in
    				uint32_t addressInt = (address[0] << 16) + (address[1] << 8) + address[2];

    				//also clear the binary file to get ready to read in data.
    				ofstream newBIN("ROM read.bin",ios::trunc);
    				for(uint32_t i = 0; i < addressInt; i ++)
    				{
    					newBIN << 0;//write a null data byte
    				}
    				newBIN.close();//close the binary file- there should be just enough
    				//characters such that the binary will be populated fully, with correct
    				//address per byte accounting by the time we write actual data to it.

    			}
    			//we want to flash the ECU with the stage II bootloader - this is typically used to unlock the stage I bootloader

    			//open a COM port
    			//update the stored com port value
    			int comIndex = SendMessage(comportSelectWindowHandle, (UINT) CB_GETCURSEL, (WPARAM) 0, (LPARAM) 0);
    			SendMessage(comportSelectWindowHandle, (UINT) CB_GETLBTEXT, (WPARAM) comIndex, (LPARAM) ListItem);//global var ListItem will now contain the COM port name

    			char baud[5] = {'2','9','7','6','9'};
    			comPortHandle = setupComPort(baud, ListItem);//open the com port

    			//run the 'pull l-line low' and send [1,113,114], read 0x8D routine
    			bool success = true;
    			comPortHandle = initializeReflashMode(comPortHandle, ListItem, success);

    			//success = false;//set for testing

    			//begin the timed routines by setting a timer
    			if(success == true)
    			{
    				cout << "reflash mode begin; time to send bytes.." << '\n';
    				lastPositionForReflashing = 0;
    				reflashingTimerProcMode = 0;
    				attemptsAtInit = 0;
    				getNextMessage = true;
    				SetTimer(hwnd, logPollTimer, 10, (TIMERPROC) NULL);
    			}
    			else
    			{
    				CloseHandle(comPortHandle);
    			}

    		}
    		break;
    	}
    	case 6:
    	{
    		//user pressed the checkbox for 'stage I bootloader'
    		stageIbootloader = !stageIbootloader;

    		break;
    	}
    	case 7:
    	{
    		//user pressed the checkbox for 'clear memory addresses during reflash'
    		clearMemorySector = !clearMemorySector;
    		//cout << "flag: clear memory sectors where we are reflashing" << '\n';
    		break;
    	}
    	case 8:
    	{
    		//user pressed the checkbox for 'read memory'
    		readMemory = !readMemory;
    		break;
    	}
    	default:
    		break;
    	}
    	break;
    }
    	case WM_DESTROY:
    	{
    		PostQuitMessage (0);       /* send a WM_QUIT to the message queue */
    		break;
    	}
    	case WM_TIMER:
		{
    		KillTimer(hwnd, logPollTimer);

    		//first, load some of the window handles so that we can collect information from them
    		//these are found in order so if any changes are made then the labels will be incorrect
    		HWND parentWindowHandle = FindWindowEx(NULL,NULL,"ReflashModeClassName",NULL);//find the handle for the reflash mode window
    		HWND returntoobdWindowHandle = FindWindowEx(parentWindowHandle,NULL,NULL,NULL);//find the handle for the return to obd mode button
    		HWND comportSelectWindowHandle = FindWindowEx(parentWindowHandle,returntoobdWindowHandle,NULL,NULL);//find the handle for the comport select drop down
    		HWND inputFileNameWindowHandle = FindWindowEx(parentWindowHandle,comportSelectWindowHandle,NULL,NULL);//find the handle for the input file name title
    		HWND inputWindowHandle = FindWindowEx(parentWindowHandle,inputFileNameWindowHandle,NULL,NULL);//find the handle for the input file name

    		//cout << "hello!" << '\n';
    		if(reflashingTimerProcMode == 0)
    		{
    			//stage II bootloader: send-packet to ECU
    			//or
    			//stage I bootloader: main ECU byte send sequence

    			//load a packet
    			if(getNextMessage)
    			{
    				bufferLength = readPacketLineToBuffer(packetBuffer, 300, lastPositionForReflashing);
    			}

    			//send the packet
    			writeToSerialPort(comPortHandle, packetBuffer, bufferLength);
    			uint8_t readBackSendBuffer[bufferLength];
    			readFromSerialPort(comPortHandle, readBackSendBuffer, bufferLength, 1000);//read back the packet we just sent

    			//for debugging, print the packets as we send them- this is redundant with the 'read from serial port' function
    			/*
    			cout << "[ ";
    			for(uint16_t i = 0; i < bufferLength; i++)
    			{
    				cout << (int) packetBuffer[i] << ' ';
    			}
    			cout << "]" << '\n';
    			*/

    			//then, move on to expect the ECU response
    			if(stageIbootloader)
    			{
    				reflashingTimerProcMode = 3;//next, read the ECU response in the stage I bootloader
    			}
    			else
    			{
    				readFromSerialPort(comPortHandle, readBackSendBuffer, 1, 1000);//read back the ECU's inverted check byte response
    				reflashingTimerProcMode = 1;//next, read the ECU response in the stage II bootloader
    			}

    		}
    		if(reflashingTimerProcMode == 1)
    		{
    			//stage II response from ECU - the timing for this process is very tight, so we
    			//may not have the liberty to read this in and reply after a 10ms delay

    			//read response from ECU and reply
    			//we expect 0x02, 0x72, 0xFF, 0x73 -> we're done
    			//or 0x02, 0x72, 0x00, 0x74 -> all is well, expect another packet, or finish
    			//or a 112 message in response to a request to read data
    			if(readMemory && packetBuffer[1] == 116)
    			{
    				//read the response to a read ROM request
    				uint8_t responseBytes[20] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    				int recvd = readFromSerialPort(comPortHandle, responseBytes,19,1000);//read in bytes

    				//finally, we send back the inverted sum byte
    				if(recvd > 0)
    				{
    					uint8_t toSend[1] = {~responseBytes[18]};
    					writeToSerialPort(comPortHandle, toSend, 1);//send
    					uint8_t dumpBytes[1];
    					readFromSerialPort(comPortHandle, dumpBytes,1,300);//read in the byte we sent
    				}

    				//check the packet sum:
    				uint8_t checkSum = 0;
    				for(uint8_t i = 0; i < responseBytes[0]+1; i++)
    				{
    					checkSum += responseBytes[i];
    				}
    				if(checkSum == responseBytes[recvd-1])
    				{
    					//then write the data to an S-record file
    					uint32_t addressToWrite = (packetBuffer[2] << 24) + (packetBuffer[3] << 16) + (packetBuffer[4] << 8) + packetBuffer[5];
    					string HEXdata = "";
    					ofstream newBIN("ROM read.bin",ios::app);
    					for(uint8_t i = 0; i < 16; i++)
    					{
    						string byte = decimal_to_hexString(responseBytes[i + 2]);
    						HEXdata.append(byte);
    						newBIN << responseBytes[i+2];
    					}
    					newBIN.close();
    					buildSREC(addressToWrite, HEXdata);//write s-record data to the s-record file
    					//We also want to write data to the binary file, but we'll do that at the end
    					//so that we can accommodate for data not read in during this read-run
    					getNextMessage = true;
    					//reflashingTimerProcMode = 0;
    				}
    				else
    				{
    					//checksum failed, resend packet request
    					getNextMessage = false;
    					//cout << (int) checkSum << '\n';
    					//reflashingTimerProcMode = 2;
    				}
    				reflashingTimerProcMode = 0;
    			}
    			else
    			{
    				//read the response to the reflash ROM request
    				uint8_t responseBytes[4] = {0,0,0,0};
    				int recvd = readFromSerialPort(comPortHandle, responseBytes,4,10000);//read in bytes

    				//finally, we send back the inverted sum byte
    				if(recvd > 0)
    				{
    					uint8_t toSend[1] = {~responseBytes[3]};
    					writeToSerialPort(comPortHandle, toSend, 1);//send
    					uint8_t dumpBytes[1];
    					readFromSerialPort(comPortHandle, dumpBytes,1,50);//read in the byte we sent
    				}

    				//prepare to send more bytes if things are okay
    				if(recvd == 0 && attemptsAtInit < 0)
    				{
    					//resend message- bypass for now (by looking for attemptsAtInit below 0)
    					getNextMessage = false;
    					reflashingTimerProcMode = 0;
    					attemptsAtInit += 1;
    				}
    				else if(packetBuffer[1] != 115 && responseBytes[0] == 0x2 && responseBytes[1] == 0x72 && responseBytes[2] == 0x0 && responseBytes[3] == 0x74)
    				{
    					getNextMessage = true;
    					reflashingTimerProcMode = 0;
    					attemptsAtInit = 0;
    				}
    				else
    				{
    					cout << "reflash response from ECU indicates we're finished: " << (int) responseBytes[0] << ' ' << (int) responseBytes[1] << ' ' << (int) responseBytes[2] << ' ' << (int) responseBytes[3] << '\n';
    					reflashingTimerProcMode = 2;
    				}
    			}

    		}
    		if(reflashingTimerProcMode == 3)
    		{
    			//stage I bootloader
    			//receive the response of the ECU
    			uint8_t receiveBuffer[30];
    			int numRead = readFromSerialPort(comPortHandle, receiveBuffer, 5, 1000);//read in the byte count set
    			if(numRead == 5)
    			{
    				uint32_t bytesLeft = ((receiveBuffer[1]-48)*1000) + ((receiveBuffer[2]-48)*100) + ((receiveBuffer[3]-48)*10) + (receiveBuffer[4]-48) - 1;
    				uint8_t readTheRest[bytesLeft];
    				numRead = readFromSerialPort(comPortHandle, readTheRest, bytesLeft, 10000);//read in the byte count set
    				for(uint32_t i = 0; i < numRead; i++)
    				{
    					receiveBuffer[5 + i] = readTheRest[i];
    				}

    				if(receiveBuffer[6] != 6 && receiveBuffer[6] != 21)
    				{
    					//send acknowledge packet
    					uint8_t sendBuffer[12] = {2,48,48,48,56,48,6,3,227,245,208,192};// or neg ack: 2,48,48,48,56,48,21,3,100,109,204,15
    					writeToSerialPort(comPortHandle, sendBuffer, 12);
    					uint8_t readBackSendBuffer[12];
    					readFromSerialPort(comPortHandle, readBackSendBuffer, 12, 3000);
    				}

    				//then act based on what message the ECU sent
    				if(receiveBuffer[6] == 202 || receiveBuffer[6] == 204 || receiveBuffer[6] == 208)
    				{
    					//signal to end
    					reflashingTimerProcMode = 2;
    				}
    				else if(receiveBuffer[6] == 203)
    				{
    					//we can go ahead and send reflashing data
    					reflashingTimerProcMode = 0;
    					//then check for the ID of the message that the ECU is expecting:
    					uint32_t IDexpected = ((receiveBuffer[7] - 48)*100000) + ((receiveBuffer[8] - 48)*10000) + ((receiveBuffer[9] - 48)*1000) + ((receiveBuffer[10] - 48)*100) + ((receiveBuffer[11] - 48)*10) + ((receiveBuffer[12] - 48));
    					uint32_t IDreadyToSend = ((packetBuffer[7] - 48)*100000) + ((packetBuffer[8] - 48)*10000) + ((packetBuffer[9] - 48)*1000) + ((packetBuffer[10] - 48)*100) + ((packetBuffer[11] - 48)*10) + ((packetBuffer[12] - 48));
    					if(IDexpected != IDreadyToSend)
    					{
    						//if the ECU shows different data, then we need a new packet
    						getNextMessage = true;
    					}
    					else
    					{
    						//if the ECU shows the same data, then we re-send
    						getNextMessage = false;
    					}
    				}
    				else if(receiveBuffer[6] == 21)
    				{
    					//neg ack means re-send the last message to the ECU
    					getNextMessage = false;
    				}
    				else if(receiveBuffer[6] == 6)
    				{
    					//message ack - carry on - the ECU will send another message next
    					getNextMessage = true;
    				}
    			}
    			else
    			{
    				//if we have a timeout, try resending
    				getNextMessage = false;
    				reflashingTimerProcMode = 0;
    			}

    		}

    		if(reflashingTimerProcMode == 4)
    		{
    			//build the packets for the stage I bootloader

    			//extract the file names
    			char windowText[100];
    			//first, get the s-record file name
    			int length = GetWindowText(inputWindowHandle,windowText,100);
    			string srecFile = "";
    			for(int i = 0; i < length; i++)
    			{
    				srecFile.push_back(windowText[i]);
    			}
    			//build stage I bootloader packet
    			CRC = stageIbootloaderRomFlashPacketBuilder(addressToFlash, bytesToSend, CRC, numpacketsSent, srecFile, filePosition);
    			if(bytesToSend <= 0)
    			{
    				//prepare the CRC message too
    				uint8_t CRCToSendString[5] = {48,48,48,48,48};
    				uint32_t factor = 10000;
    				uint16_t CRCsplit = CRC;
    				for(uint8_t i = 0; i < 5; i++)
    				{
    					uint8_t split = CRCsplit/factor;
    					CRCToSendString[i] = split + 48;
    					CRCsplit -= split*factor;
    					factor = factor/10;
    				}
    				uint8_t CRCmessage[17] = {2,48,48,49,51,48,207,CRCToSendString[0],CRCToSendString[1],CRCToSendString[2],CRCToSendString[3],CRCToSendString[4],3,0,0,0,0};
    				calculateCheckBytes(CRCmessage, 17);

    				ofstream outputPacket("encodedFlashBytes.txt", ios::app);
    				for(uint16_t i = 0; i < 17; i++)
    				{
    					outputPacket << (int) CRCmessage[i] << ' ';
    				}
    				outputPacket.close();
    				reflashingTimerProcMode = 2;
    				cout << "Finished building reflash packet set - check file." << '\n';
    			}
    		}


    		if(reflashingTimerProcMode != 2)
    		{
    			SetTimer(hwnd, logPollTimer, 10, (TIMERPROC) NULL);//10 ms delay (or use 100ms)
    		}
    		else
    		{
    			CloseHandle(comPortHandle);

    			if(readMemory)
    			{
    				//now we should finish the binary file by writing null data up until we reach the correct file
    				//length
    				//first, figure out how large the binary file is already
    				ifstream readBinary("ROM read.bin", ios::in);
    				readBinary.seekg(0, ios::end);//scan to end of file
    				uint32_t size = readBinary.tellg();//read the position
    				readBinary.close();
    				//then open the file and write data to it
    				ofstream newBIN("ROM read.bin",ios::app);
    				if(size < 0x40000)
    				{
    					for(uint32_t i = size; i < 0x40000; i++)
    					{
    						newBIN << 0;
    					}
    				}
    				else
    				{
    					for(uint32_t i = size; i < 0x80000; i++)
    					{
    						newBIN << 0;
    					}
    				}
    				newBIN.close();
    			}
    		}
    		break;
		}
    	default:                      /* for messages that we don't deal with */
    		return DefWindowProc (hwnd, message, wParam, lParam);
    }

    return 0;
}
