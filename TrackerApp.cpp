//------------------------------------------------------------------------------
// <copyright file="tracker.cpp" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
//------------------------------------------------------------------------------

// This module provides sample code used to demonstrate Kinect NUI processing

// Note: 
//     Platform SDK lib path should be added before the VC lib
//     path, because uuid.lib in VC lib path may be older

#include "windef.h"
#include "winbase.h"
#include "shlobj.h"
#include <ShObjIdl.h>
#include <KnownFolders.h>
#include "stdafx.h"
#include <strsafe.h>
#include "trackerApp.h"
#include "resource.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <sstream>
#include <fstream>
#include <iostream>
#include "CommCtrl.h"
#include "TrackerClient.h"

using namespace std;

#pragma comment(lib, "Ws2_32.lib")

// Global Variables:
TrackerApp  g_trackerApp;  // Application class

// for retrieving from edit controls
HWND hCtrl;
char buff[50];

#define INSTANCE_MUTEX_NAME L"trackerInstanceCheck"

/// <summary>
/// Entry point for the application
/// </summary>
/// <param name="hInstance">handle to the application instance</param>
/// <param name="hPrevInstance">always 0</param>
/// <param name="lpCmdLine">command line arguments</param>
/// <param name="nCmdShow">whether to display minimized, maximized, or normally</param>
/// <returns>status</returns>
int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
	MSG       msg;
	WNDCLASS  wc;

	WSADATA wsaData;
	int iResult;

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed: %d\n", iResult);
		return 1;
	}

	// Store the instance handle
	g_trackerApp.m_hInstance = hInstance;

	// Dialog custom window class
	ZeroMemory(&wc,sizeof(wc));
	wc.style=CS_HREDRAW | CS_VREDRAW;
	wc.cbWndExtra=DLGWINDOWEXTRA;
	wc.hInstance=hInstance;
	wc.hCursor=LoadCursor(NULL,IDC_ARROW);
	wc.hIcon=LoadIcon(hInstance,MAKEINTRESOURCE(IDI_TRACKER));
	wc.lpfnWndProc=DefDlgProc;
	wc.lpszClassName=SZ_APPDLG_WINDOW_CLASS;
	if( !RegisterClass(&wc) )
	{
		return 0;
	}

	// Create main application window
	HWND hWndApp = CreateDialogParam(
		hInstance,
		MAKEINTRESOURCE(IDD_APP),
		NULL,
		(DLGPROC) TrackerApp::MessageRouter, 
		reinterpret_cast<LPARAM>(&g_trackerApp));

	// Set display settings
	DEVMODE lpDevMode;
	lpDevMode.dmSize = sizeof(DEVMODE);
	lpDevMode.dmDriverExtra = 0;
	EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &lpDevMode);
	lpDevMode.dmPelsWidth = 1024;
	lpDevMode.dmPelsHeight = 768;
	lpDevMode.dmBitsPerPel = 32;
	lpDevMode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

	// remove all styles from window
	SetWindowLong(hWndApp, GWL_STYLE, 0);

	// unique mutex, if it already exists there is already an instance of this app running
	// in that case we want to show the user an error dialog
	HANDLE hMutex = CreateMutex(NULL, FALSE, INSTANCE_MUTEX_NAME);
	if ( (hMutex != NULL) && (GetLastError() == ERROR_ALREADY_EXISTS) ) 
	{
		//load the app title
		TCHAR szAppTitle[256] = { 0 };
		LoadStringW( hInstance, IDS_APPTITLE, szAppTitle, _countof(szAppTitle) );

		//load the error string
		TCHAR szRes[512] = { 0 };
		LoadStringW( hInstance, IDS_ERROR_APP_INSTANCE, szRes, _countof(szRes) );

		MessageBoxW( NULL, szRes, szAppTitle, MB_OK | MB_ICONHAND );

		CloseHandle(hMutex);
		return -1;
	}

	

	// Show window
	ShowWindow(hWndApp, nCmdShow);
	ChangeDisplaySettings(&lpDevMode, CDS_FULLSCREEN);

	g_trackerApp.LoadFromDisk();

	// Main message loop:
	while(GetMessage(&msg,NULL,0,0)) 
	{
		// If a dialog message will be taken care of by the dialog proc
		if ( (hWndApp != NULL) && IsDialogMessage(hWndApp, &msg) )
		{
			continue;
		}

		// otherwise do our window processing
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	CloseHandle(hMutex);

	WSACleanup();

	return static_cast<int>(msg.wParam);
}

/// <summary>
/// Constructor
/// </summary>
TrackerApp::TrackerApp() : m_hInstance(NULL)
{
	ZeroMemory(m_szAppTitle, sizeof(m_szAppTitle));
	LoadStringW(m_hInstance, IDS_APPTITLE, m_szAppTitle, _countof(m_szAppTitle));
	m_range = Default;
	m_trackedSkeletons = Default;
	m_trackingMode = Default;

	m_fUpdatingUi = false;
	Nui_Zero();

	// Init Direct2D
	D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, &m_pD2DFactory);
}

/// <summary>
/// Destructor
/// </summary>
TrackerApp::~TrackerApp()
{
	// Clean up Direct2D
	SafeRelease(m_pD2DFactory);

	Nui_Zero();
	SysFreeString(m_instanceId);
}



/// <summary>
/// Handles window messages, passes most to the class instance to handle
/// </summary>
/// <param name="hWnd">window message is for</param>
/// <param name="uMsg">message</param>
/// <param name="wParam">message packetData</param>
/// <param name="lParam">additional message packetData</param>
/// <returns>result of message processing</returns>
LRESULT CALLBACK TrackerApp::MessageRouter( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	TrackerApp *pThis = NULL;

	if (WM_INITDIALOG == uMsg)
	{
		pThis = reinterpret_cast<TrackerApp*>(lParam);
		SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
		NuiSetDeviceStatusCallback( &TrackerApp::Nui_StatusProcThunk, pThis );
	}
	else
	{
		pThis = reinterpret_cast<TrackerApp*>(::GetWindowLongPtr(hwnd, GWLP_USERDATA));
	}

	if (NULL != pThis)
	{
		return pThis->WndProc( hwnd, uMsg, wParam, lParam );
	}

	return 0;
}

/// <summary>
/// Handle windows messages for the class instance
/// </summary>
/// <param name="hWnd">window message is for</param>
/// <param name="uMsg">message</param>
/// <param name="wParam">message packetData</param>
/// <param name="lParam">additional message packetData</param>
/// <returns>result of message processing</returns>
LRESULT CALLBACK TrackerApp::WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch(message)
	{
	case WM_INITDIALOG:
		{
			// Clean state the class
			Nui_Zero();

			// Bind application window handle
			m_hWnd = hWnd;

			// Set the font for Frames Per Second display
			LOGFONT lf;
			GetObject( (HFONT)GetStockObject(DEFAULT_GUI_FONT), sizeof(lf), &lf );
			lf.lfHeight *= 4;
			m_hFontFPS = CreateFontIndirect(&lf);
			SendDlgItemMessageW(hWnd, IDC_FPS, WM_SETFONT, (WPARAM)m_hFontFPS, 0);


			SendDlgItemMessageW(m_hWnd, IDC_CAMERAS, CB_SETCURSEL, 0, 0);

			TCHAR szComboText[512] = { 0 };

			// Fill combo box options for tracked skeletons

			LoadStringW(m_hInstance, IDS_TRACKEDSKELETONS_DEFAULT, szComboText, _countof(szComboText));
			SendDlgItemMessageW(m_hWnd, IDC_TRACKEDSKELETONS, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(szComboText));

			LoadStringW(m_hInstance, IDS_TRACKEDSKELETONS_NEAREST1, szComboText, _countof(szComboText));
			SendDlgItemMessageW(m_hWnd, IDC_TRACKEDSKELETONS, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(szComboText));

			LoadStringW(m_hInstance, IDS_TRACKEDSKELETONS_NEAREST2, szComboText, _countof(szComboText));
			SendDlgItemMessageW(m_hWnd, IDC_TRACKEDSKELETONS, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(szComboText));

			LoadStringW(m_hInstance, IDS_TRACKEDSKELETONS_STICKY1, szComboText, _countof(szComboText));
			SendDlgItemMessageW(m_hWnd, IDC_TRACKEDSKELETONS, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(szComboText));

			LoadStringW(m_hInstance, IDS_TRACKEDSKELETONS_STICKY2, szComboText, _countof(szComboText));
			SendDlgItemMessageW(m_hWnd, IDC_TRACKEDSKELETONS, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(szComboText));

			SendDlgItemMessageW(m_hWnd, IDC_TRACKEDSKELETONS, CB_SETCURSEL, 0, 0);
			// Fill combo box options for tracking mode

			LoadStringW(m_hInstance, IDS_TRACKINGMODE_DEFAULT, szComboText, _countof(szComboText));
			SendDlgItemMessageW(m_hWnd, IDC_TRACKINGMODE, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(szComboText));

			LoadStringW(m_hInstance, IDS_TRACKINGMODE_SEATED, szComboText, _countof(szComboText));
			SendDlgItemMessageW(m_hWnd, IDC_TRACKINGMODE, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(szComboText));
			SendDlgItemMessageW(m_hWnd, IDC_TRACKINGMODE, CB_SETCURSEL, 0, 0);

			// Fill combo box options for range

			LoadStringW(m_hInstance, IDS_RANGE_DEFAULT, szComboText, _countof(szComboText));
			SendDlgItemMessageW(m_hWnd, IDC_RANGE, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(szComboText));

			LoadStringW(m_hInstance, IDS_RANGE_NEAR, szComboText, _countof(szComboText));
			SendDlgItemMessageW(m_hWnd, IDC_RANGE, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(szComboText));

			SendDlgItemMessageW(m_hWnd, IDC_RANGE, CB_SETCURSEL, 0, 0);

		}
		break;

	case WM_SHOWWINDOW:
		{
			// Initialize and start NUI processing
			Nui_Init();
		}
		break;

	case WM_USER_UPDATE_FPS:
		{
			::SetDlgItemInt( m_hWnd, static_cast<int>(wParam), static_cast<int>(lParam), FALSE );
		}
		break;

	case WM_COMMAND:
		{
			switch ( LOWORD(wParam))
			{
			case IDC_APPLY:
				{
					if ( HIWORD(wParam) == BN_CLICKED)
					{
						//Get and set kinect angle
						hCtrl = GetDlgItem(m_hWnd, IDC_KINECT_ANGLE);
						GetWindowTextA(hCtrl, buff, 50);
						m_KinectAngle = atof(buff);
						if (m_KinectAngle > 27.0)
							m_KinectAngle = 27.0;
						if (m_KinectAngle < -27.0)
							m_KinectAngle = 27.0;

						NuiCameraElevationSetAngle(m_KinectAngle);

						// Update info from combo boxes
						LRESULT index = ::SendDlgItemMessageW(m_hWnd, IDC_TRACKEDSKELETONS, CB_GETCURSEL, 0, 0);
						UpdateTrackedSkeletonSelection( static_cast<int>(index) );
						LRESULT	index1 = ::SendDlgItemMessageW(m_hWnd, IDC_TRACKINGMODE, CB_GETCURSEL, 0, 0);
						UpdateTrackingMode( static_cast<int>(index1) );
						LRESULT index2 = ::SendDlgItemMessageW(m_hWnd, IDC_RANGE, CB_GETCURSEL, 0, 0);
						UpdateRange( static_cast<int>(index2) );

						// Get kinect position info
						hCtrl = GetDlgItem(m_hWnd, IDC_KINECT_POSITION_X);
						GetWindowTextA(hCtrl, buff, 50);
						m_kinectPosition[0] = atof(buff);
						hCtrl = GetDlgItem(m_hWnd, IDC_KINECT_POSITION_Y);
						GetWindowTextA(hCtrl, buff, 50);
						m_kinectPosition[1] = atof(buff);
						hCtrl = GetDlgItem(m_hWnd, IDC_KINECT_POSITION_Z);
						GetWindowTextA(hCtrl, buff, 50);
						m_kinectPosition[2] = atof(buff);

						// Get IP addresses
						hCtrl = GetDlgItem(m_hWnd, IDC_IPADDRESS1);
						GetWindowTextA(hCtrl, buff, 50);
						m_ipAddress[0] = buff;
						hCtrl = GetDlgItem(m_hWnd, IDC_IPADDRESS2);
						GetWindowTextA(hCtrl, buff, 50);
						m_ipAddress[1] = buff;
						hCtrl = GetDlgItem(m_hWnd, IDC_IPADDRESS3);
						GetWindowTextA(hCtrl, buff, 50);
						m_ipAddress[2] = buff;
						hCtrl = GetDlgItem(m_hWnd, IDC_IPADDRESS4);
						GetWindowTextA(hCtrl, buff, 50);
						m_ipAddress[3] = buff;
						hCtrl = GetDlgItem(m_hWnd, IDC_IPADDRESS5);
						GetWindowTextA(hCtrl, buff, 50);
						m_ipAddress[4] = buff;
						hCtrl = GetDlgItem(m_hWnd, IDC_IPADDRESS6);
						GetWindowTextA(hCtrl, buff, 50);
						m_ipAddress[5] = buff;

						// Get ports
						hCtrl = GetDlgItem(m_hWnd, IDC_PORT1);
						GetWindowTextA(hCtrl, buff, 50);
						m_port[0] = buff;
						hCtrl = GetDlgItem(m_hWnd, IDC_PORT2);
						GetWindowTextA(hCtrl, buff, 50);
						m_port[1] = buff;
						hCtrl = GetDlgItem(m_hWnd, IDC_PORT3);
						GetWindowTextA(hCtrl, buff, 50);
						m_port[2] = buff;
						hCtrl = GetDlgItem(m_hWnd, IDC_PORT4);
						GetWindowTextA(hCtrl, buff, 50);
						m_port[3] = buff;
						hCtrl = GetDlgItem(m_hWnd, IDC_PORT5);
						GetWindowTextA(hCtrl, buff, 50);
						m_port[4] = buff;
						hCtrl = GetDlgItem(m_hWnd, IDC_PORT6);
						GetWindowTextA(hCtrl, buff, 50);
						m_port[5] = buff;

						// get smoothing params
						hCtrl = GetDlgItem(m_hWnd, IDC_SMOOTHING);
						m_smoothParams.fSmoothing = .01*SendMessage(hCtrl, TBM_GETPOS, 0, 0);
						hCtrl = GetDlgItem(m_hWnd, IDC_CORRECTION);
						m_smoothParams.fCorrection = .01*SendMessage(hCtrl, TBM_GETPOS, 0, 0);
						hCtrl = GetDlgItem(m_hWnd, IDC_PREDICTION);
						m_smoothParams.fPrediction = .01*SendMessage(hCtrl, TBM_GETPOS, 0, 0);
						hCtrl = GetDlgItem(m_hWnd, IDC_JITTER_RADIUS);
						GetWindowTextA(hCtrl, buff, 50);
						m_smoothParams.fJitterRadius = atof(buff);
						hCtrl = GetDlgItem(m_hWnd, IDC_MAX_DEVIATION_RADIUS);
						GetWindowTextA(hCtrl, buff, 50);
						m_smoothParams.fMaxDeviationRadius = atof(buff);

						// get listen port
						hCtrl = GetDlgItem(m_hWnd, IDC_SERVPORT);
						GetWindowTextA(hCtrl, buff, 50);
						stringstream ugh;
						ugh << buff;
						ugh >> m_servPort;
					}
				}
				break;
			case IDC_SAVE:
				{
					if ( HIWORD(wParam) == BN_CLICKED)
					{
						ofstream outFile;
						outFile.open("kinectInfo.cfg");
						outFile << m_servPort << endl;
						outFile << m_trackingMode << " " << m_trackedSkeletons << " " << m_range << endl;
						outFile << m_kinectPosition[0] << " " << m_kinectPosition[1] << " " << m_kinectPosition[2] << " " << m_KinectAngle << endl;
						outFile << m_smoothParams.fSmoothing << " " << m_smoothParams.fCorrection << " " << m_smoothParams.fPrediction << " " << 
							m_smoothParams.fJitterRadius << " " << m_smoothParams.fMaxDeviationRadius << endl;
						for (int i = 0; i < MAX_IPS; i++)
							outFile << m_ipAddress[i] << " " << m_port[i] << endl;

						outFile.close();
					}
				}
				break;
			case IDC_LOAD:
				{
					if ( HIWORD(wParam) == BN_CLICKED)
					{
						LoadFromDisk();
					}
				}
				break;
			case IDC_LISTEN_BUTTON:
				{
					if (HIWORD(wParam) == BN_CLICKED)
					{
						hCtrl = GetDlgItem(m_hWnd, IDC_LISTEN_BUTTON);
						if (SendMessage(hCtrl, BM_GETCHECK, 0, 0) == BST_CHECKED)
						{
							m_listening = true;
							SetWindowTextA(hCtrl, "Listening..."); 
						}
						else
						{
							SetWindowTextA(hCtrl, "Listen..."); 
							m_listening = false;
							hCtrl = GetDlgItem(m_hWnd, IDC_IPADDRESS1);
							SetWindowTextA(hCtrl, m_ipAddress[0].c_str());
							hCtrl = GetDlgItem(m_hWnd, IDC_PORT1);
							SetWindowTextA(hCtrl, m_port[0].c_str());
							hCtrl = GetDlgItem(m_hWnd, IDC_IPADDRESS2);
							SetWindowTextA(hCtrl, m_ipAddress[1].c_str());
							hCtrl = GetDlgItem(m_hWnd, IDC_PORT2);
							SetWindowTextA(hCtrl, m_port[1].c_str());
							hCtrl = GetDlgItem(m_hWnd, IDC_IPADDRESS3);
							SetWindowTextA(hCtrl, m_ipAddress[2].c_str());
							hCtrl = GetDlgItem(m_hWnd, IDC_PORT3);
							SetWindowTextA(hCtrl, m_port[2].c_str());
							hCtrl = GetDlgItem(m_hWnd, IDC_IPADDRESS4);
							SetWindowTextA(hCtrl, m_ipAddress[3].c_str());
							hCtrl = GetDlgItem(m_hWnd, IDC_PORT4);
							SetWindowTextA(hCtrl, m_port[3].c_str());
							hCtrl = GetDlgItem(m_hWnd, IDC_IPADDRESS5);
							SetWindowTextA(hCtrl, m_ipAddress[4].c_str());
							hCtrl = GetDlgItem(m_hWnd, IDC_PORT5);
							SetWindowTextA(hCtrl, m_port[4].c_str());
							hCtrl = GetDlgItem(m_hWnd, IDC_IPADDRESS6);
							SetWindowTextA(hCtrl, m_ipAddress[5].c_str());
							hCtrl = GetDlgItem(m_hWnd, IDC_PORT6);
							SetWindowTextA(hCtrl, m_port[5].c_str());
						}
					}

				}
			}
		}
		break;

		// If the titlebar X is clicked destroy app
	case WM_CLOSE:
		DestroyWindow(hWnd);
		break;

	case WM_DESTROY:
		// Uninitialize NUI
		Nui_UnInit();

		// Other cleanup
		DeleteObject(m_hFontFPS);

		// Quit the main message pump
		PostQuitMessage(0);
		break;
	}

	return FALSE;
}

/// <summary>
/// Display a MessageBox with a string table table loaded string
/// </summary>
/// <param name="nID">id of string resource</param>
/// <param name="nType">type of message box</param>
/// <returns>result of MessageBox call</returns>
int TrackerApp::MessageBoxResource( UINT nID, UINT nType )
{
	static TCHAR szRes[512];

	LoadStringW( m_hInstance, nID, szRes, _countof(szRes) );
	return MessageBoxW(m_hWnd, szRes, m_szAppTitle, nType);
}

void TrackerApp::LoadFromDisk() {
	ifstream inFile;
	inFile.open("kinectInfo.cfg");

	// load/apply/update controls for listen port
	inFile >> m_servPort;
	hCtrl = GetDlgItem(m_hWnd, IDC_SERVPORT);
	stringstream ss0;
	ss0 << m_servPort;
	SetWindowTextA(hCtrl, ss0.str().c_str());

	// load/apply calibration/network stuff
	inFile >> m_trackingMode >> m_trackedSkeletons >> m_range;
	inFile >> m_kinectPosition[0] >> m_kinectPosition[1] >> m_kinectPosition[2] >> m_KinectAngle;
	inFile >> m_smoothParams.fSmoothing >> m_smoothParams.fCorrection >> m_smoothParams.fPrediction >> m_smoothParams.fJitterRadius >> m_smoothParams.fMaxDeviationRadius;
	for (int i = 0; i < MAX_IPS; i++)
		inFile >> m_ipAddress[i] >> m_port[i];
	inFile.close();

	NuiCameraElevationSetAngle(m_KinectAngle);

	// update controls for calibration/network stuff
	SendDlgItemMessage(m_hWnd, IDC_TRACKEDSKELETONS, CB_SETCURSEL, m_trackedSkeletons, 0);
	SendDlgItemMessage(m_hWnd, IDC_TRACKINGMODE, CB_SETCURSEL, m_trackingMode, 0);
	SendDlgItemMessage(m_hWnd, IDC_RANGE, CB_SETCURSEL, m_range, 0);

	UpdateTrackingMode( static_cast<int>(m_trackingMode) );
	UpdateTrackedSkeletonSelection( static_cast<int>(m_trackedSkeletons) );
	UpdateRange( static_cast<int>(m_range) );

	stringstream ss; 

	hCtrl = GetDlgItem(m_hWnd, IDC_KINECT_POSITION_X);
	ss << m_kinectPosition[0];
	SetWindowTextA(hCtrl, ss.str().c_str());
	ss.str("");

	hCtrl = GetDlgItem(m_hWnd, IDC_KINECT_POSITION_Y);
	ss << m_kinectPosition[1];
	SetWindowTextA(hCtrl, ss.str().c_str());
	ss.str("");

	hCtrl = GetDlgItem(m_hWnd, IDC_KINECT_POSITION_Z);
	ss << m_kinectPosition[2];
	SetWindowTextA(hCtrl, ss.str().c_str());
	ss.str("");

	hCtrl = GetDlgItem(m_hWnd, IDC_KINECT_ANGLE);
	ss << m_KinectAngle;
	SetWindowTextA(hCtrl, ss.str().c_str());
	ss.str("");

	hCtrl = GetDlgItem(m_hWnd, IDC_SMOOTHING);
	ss << m_smoothParams.fSmoothing;
	SendMessage(hCtrl, TBM_SETPOS, TRUE, (int)(100.0*atof(ss.str().c_str())));
	ss.str("");

	hCtrl = GetDlgItem(m_hWnd, IDC_CORRECTION);
	ss << m_smoothParams.fCorrection;
	SendMessage(hCtrl, TBM_SETPOS, TRUE, (int)(100.0*atof(ss.str().c_str())));
	ss.str("");

	hCtrl = GetDlgItem(m_hWnd, IDC_PREDICTION);
	ss << m_smoothParams.fPrediction;
	SendMessage(hCtrl, TBM_SETPOS, TRUE, (int)(100.0*atof(ss.str().c_str())));
	ss.str("");

	hCtrl = GetDlgItem(m_hWnd, IDC_JITTER_RADIUS);
	ss << m_smoothParams.fJitterRadius;
	SetWindowTextA(hCtrl, ss.str().c_str());
	ss.str("");

	hCtrl = GetDlgItem(m_hWnd, IDC_MAX_DEVIATION_RADIUS);
	ss << m_smoothParams.fMaxDeviationRadius;
	SetWindowTextA(hCtrl, ss.str().c_str());
	ss.str("");

	hCtrl = GetDlgItem(m_hWnd, IDC_IPADDRESS1);
	SetWindowTextA(hCtrl, m_ipAddress[0].c_str());
	hCtrl = GetDlgItem(m_hWnd, IDC_PORT1);
	SetWindowTextA(hCtrl, m_port[0].c_str());
	hCtrl = GetDlgItem(m_hWnd, IDC_IPADDRESS2);
	SetWindowTextA(hCtrl, m_ipAddress[1].c_str());
	hCtrl = GetDlgItem(m_hWnd, IDC_PORT2);
	SetWindowTextA(hCtrl, m_port[1].c_str());
	hCtrl = GetDlgItem(m_hWnd, IDC_IPADDRESS3);
	SetWindowTextA(hCtrl, m_ipAddress[2].c_str());
	hCtrl = GetDlgItem(m_hWnd, IDC_PORT3);
	SetWindowTextA(hCtrl, m_port[2].c_str());
	hCtrl = GetDlgItem(m_hWnd, IDC_IPADDRESS4);
	SetWindowTextA(hCtrl, m_ipAddress[3].c_str());
	hCtrl = GetDlgItem(m_hWnd, IDC_PORT4);
	SetWindowTextA(hCtrl, m_port[3].c_str());
	hCtrl = GetDlgItem(m_hWnd, IDC_IPADDRESS5);
	SetWindowTextA(hCtrl, m_ipAddress[4].c_str());
	hCtrl = GetDlgItem(m_hWnd, IDC_PORT5);
	SetWindowTextA(hCtrl, m_port[4].c_str());
	hCtrl = GetDlgItem(m_hWnd, IDC_IPADDRESS6);
	SetWindowTextA(hCtrl, m_ipAddress[5].c_str());
	hCtrl = GetDlgItem(m_hWnd, IDC_PORT6);
	SetWindowTextA(hCtrl, m_port[5].c_str());
}

int TrackerApp::StartListening()
{
	WSADATA wsa;
	SOCKET s, new_socket = 0;
	struct sockaddr_in server, client;
	int c;

	printf("\nInitialising Winsock...");
	if (WSAStartup(MAKEWORD(2,2),&wsa) != 0)
	{
		printf("Failed. Error Code : %d",WSAGetLastError());
		return 1;
	}

	printf("Initialised.\n");

	//Create a socket
	if((s = socket(AF_INET , SOCK_STREAM , 0 )) == INVALID_SOCKET)
	{
		printf("Could not create socket : %d" , WSAGetLastError());
	}
	u_long nonblocking = 1;
	ioctlsocket(s, FIONBIO, &nonblocking);
	printf("Socket created.\n");

	//Prepare the sockaddr_in structure
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons( m_servPort );

	//Bind
	if (bind(s ,(struct sockaddr *)&server , sizeof(server)) == SOCKET_ERROR)
	{
		printf("Bind failed with error code : %d" , WSAGetLastError());
	}

	puts("Bind done");

	listen(s, MAX_IPS);

	//Accept and incoming connection
	puts("Waiting for incoming connections...");
	int i = 0, prev_socket = new_socket;
	while (i < MAX_IPS-1) {
		c = sizeof(struct sockaddr_in);
		new_socket = accept(s , (struct sockaddr *)&client, &c);
		if ((prev_socket != new_socket) && (new_socket != INVALID_SOCKET)) {
			m_ipAddress[i] = client.sin_addr.s_addr;
			prev_socket = new_socket;
			i++;
		}

	}

	puts("Connection accepted");

	return 0;


}