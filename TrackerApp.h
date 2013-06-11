//------------------------------------------------------------------------------
// <copyright file="trackerApp.h" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
//------------------------------------------------------------------------------

// Declares of TrackerApp class

#pragma once

#include "resource.h"
#include "NuiApi.h"
#include "DrawDevice.h"
#include "propsys.h"
#include <dmo.h>
#include <wmcodecdsp.h>
#include <mmreg.h>
#include <uuids.h>
#include <string>
#include "TrackerClient.h"

#define Default 0
#define Closest1 1
#define Closest2 2
#define Sticky1  3
#define Sticky2  4
#define Near 1
#define Seated 1
#define SZ_APPDLG_WINDOW_CLASS          _T("trackerAppDlgWndClass")
#define WM_USER_UPDATE_FPS              WM_USER
#define WM_USER_UPDATE_COMBO            WM_USER+1
#define WM_USER_UPDATE_TRACKING_COMBO   WM_USER+2

class TrackerApp
{
public:
	/// <summary>
	/// Constructor
	/// </summary>
	TrackerApp();

	/// <summary>
	/// Destructor
	/// </summary>
	~TrackerApp();

	/// <summary>
	/// Initialize Kinect
	/// </summary>
	/// <returns>S_OK if successful, otherwise an error code</returns>
	HRESULT                 Nui_Init( );

	/// <summary>
	/// Initialize Kinect by instance name
	/// </summary>
	/// <param name="instanceName">instance name of Kinect to initialize</param>
	/// <returns>S_OK if successful, otherwise an error code</returns>
	HRESULT                 Nui_Init( OLECHAR * instanceName );

	/// <summary>
	/// Uninitialize Kinect
	/// </summary>
	void                    Nui_UnInit( );

	/// <summary>
	/// Zero out member variables
	/// </summary>
	void                    Nui_Zero( );

	/// <summary>
	/// Handle new depth packetData
	/// </summary>
	/// <returns>true if a frame was processed, false otherwise</returns>
	bool                    Nui_GotDepthAlert( );

	/// <summary>
	/// Handle new skeleton packetData
	/// </summary>
	bool                    Nui_GotSkeletonAlert( );

	/// <summary>
	/// Handles window messages, passes most to the class instance to handle
	/// </summary>
	/// <param name="hWnd">window message is for</param>
	/// <param name="uMsg">message</param>
	/// <param name="wParam">message packetData</param>
	/// <param name="lParam">additional message packetData</param>
	/// <returns>result of message processing</returns>
	static LRESULT CALLBACK MessageRouter(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	/// <summary>
	/// Handle windows messages for the class instance
	/// </summary>
	/// <param name="hWnd">window message is for</param>
	/// <param name="uMsg">message</param>
	/// <param name="wParam">message packetData</param>
	/// <param name="lParam">additional message packetData</param>
	/// <returns>result of message processing</returns>
	LRESULT CALLBACK        WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	/// <summary>
	/// Callback to handle Kinect status changes, redirects to the class callback handler
	/// </summary>
	/// <param name="hrStatus">current status</param>
	/// <param name="instanceName">instance name of Kinect the status change is for</param>
	/// <param name="uniqueDeviceName">unique device name of Kinect the status change is for</param>
	/// <param name="pUserData">additional packetData</param>
	static void CALLBACK    Nui_StatusProcThunk(HRESULT hrStatus, const OLECHAR* instanceName, const OLECHAR* uniqueDeviceName, void* pUserData);

	/// <summary>
	/// Callback to handle Kinect status changes
	/// </summary>
	/// <param name="hrStatus">current status</param>
	/// <param name="instanceName">instance name of Kinect the status change is for</param>
	/// <param name="uniqueDeviceName">unique device name of Kinect the status change is for</param>
	void CALLBACK           Nui_StatusProc( HRESULT hrStatus, const OLECHAR* instanceName, const OLECHAR* uniqueDeviceName );

	HWND                    m_hWnd;
	HINSTANCE               m_hInstance;

	int MessageBoxResource(UINT nID, UINT nType);

	/// <summary>
	/// Invoked when the user changes the tracking mode
	/// </summary>
	/// <param name="mode">tracking mode to switch to</param>
	void                    UpdateTrackingMode( int mode );

	/// <summary>
	/// Invoked when the user changes the range
	/// </summary>
	/// <param name="mode">range to switch to</param>
	void                    UpdateRange( int mode );

	/// <summary>
	/// Invoked when the user changes the selection of tracked skeletons
	/// </summary>
	/// <param name="mode">skelton tracking mode to switch to</param>
	void                    UpdateTrackedSkeletonSelection( int mode );

	/// <summary>
	/// Sets or clears the specified skeleton tracking flag
	/// </summary>
	/// <param name="flag">flag to set or clear</param>
	/// <param name="value">true to set, false to clear</param>
	void                    UpdateSkeletonTrackingFlag( DWORD flag, bool value );

	/// <summary>
	/// Sets or clears the specified depth stream flag
	/// </summary>
	/// <param name="flag">flag to set or clear</param>
	/// <param name="value">true to set, false to clear</param>
	void                    UpdateDepthStreamFlag( DWORD flag, bool value );

	/// <summary>
	/// Determines which skeletons to track and tracks them
	/// </summary>
	/// <param name="skel">skeleton frame information</param>
	void                    UpdateTrackedSkeletons( const NUI_SKELETON_FRAME & skel );

	/// <summary>
	/// Ensure necessary Direct2d resources are created
	/// </summary>
	/// <returns>S_OK if successful, otherwise an error code</returns>
	HRESULT                 EnsureDirect2DResources( );

	/// <summary>
	/// Dispose Direct2d resources 
	/// </summary>
	void                    DiscardDirect2DResources( );

	int StartListening();
	void LoadFromDisk();

	/// <summary>
	/// Converts a skeleton point to screen space
	/// </summary>
	/// <param name="skeletonPoint">skeleton point to tranform</param>
	/// <param name="width">width (in pixels) of output buffer</param>
	/// <param name="height">height (in pixels) of output buffer</param>
	/// <returns>point in screen-space</returns>
	D2D1_POINT_2F           SkeletonToScreen(Vector4 skeletonPoint, int width, int height);

	bool                    m_fUpdatingUi;
	TCHAR                   m_szAppTitle[256];    // Application title

	// Current Kinect sensor
	INuiSensor*             m_pNuiSensor;

	/// <summary>
	/// Thread to handle Kinect processing, calls class instance thread processor
	/// </summary>
	/// <param name="pParam">instance pointer</param>
	/// <returns>always 0</returns>
	static DWORD WINAPI     Nui_ProcessThread( LPVOID pParam );

	/// <summary>
	/// Thread to handle Kinect processing
	/// </summary>
	/// <returns>always 0</returns>
	DWORD WINAPI            Nui_ProcessThread( );

	// Current kinect
	BSTR                    m_instanceId;

	// Kinect calibration
	float m_kinectPosition[3];
	std::string m_ipAddress[MAX_IPS];
	std::string m_port[MAX_IPS];
	int m_trackingMode;
	int m_trackedSkeletons;
	int m_range;
	int m_activeUser;
	int m_secondaryUser;
	short m_servPort;
	bool m_reevalGestureTriggered;
	LONG m_KinectAngle;
	NUI_TRANSFORM_SMOOTH_PARAMETERS m_smoothParams;
	bool m_listening;

	TrackerClient m_interactionClient;

	// Skeletal drawing
	ID2D1HwndRenderTarget *  m_pRenderTarget;
	ID2D1SolidColorBrush *   m_pBrushJointTracked;
	ID2D1SolidColorBrush *   m_pBrushJointInferred;
	ID2D1SolidColorBrush *   m_pBrushBoneTracked;
	ID2D1SolidColorBrush *   m_pBrushBoneInferred;
	D2D1_POINT_2F            m_Points[NUI_SKELETON_POSITION_COUNT];
	NUI_SKELETON_BONE_ORIENTATION m_boneOrientations[20];

	// Draw devices
	DrawDevice *            m_pDrawDepth;
	DrawDevice *            m_pDrawColor;
	ID2D1Factory *          m_pD2DFactory;

	// thread handling
	HANDLE        m_hThNuiProcess;
	HANDLE        m_hEvNuiProcessStop;

	HANDLE        m_hNextDepthFrameEvent;
	HANDLE        m_hNextColorFrameEvent;
	HANDLE        m_hNextSkeletonEvent;
	HANDLE        m_pDepthStreamHandle;
	HANDLE        m_pVideoStreamHandle;

	HFONT         m_hFontFPS;
	BYTE          m_depthRGBX[640*480*4];
	DWORD         m_LastSkeletonFoundTime;
	bool          m_bScreenBlanked;
	int           m_DepthFramesTotal;
	DWORD         m_LastDepthFPStime;
	int           m_LastDepthFramesTotal;
	int           m_TrackedSkeletons;
	DWORD         m_SkeletonTrackingFlags;
	DWORD         m_DepthStreamFlags;

	DWORD         m_StickySkeletonIds[NUI_SKELETON_MAX_TRACKED_COUNT];
};

