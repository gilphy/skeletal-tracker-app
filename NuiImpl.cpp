//------------------------------------------------------------------------------
// <copyright file="NuiImpl.cpp" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
//------------------------------------------------------------------------------

// Implementation of TrackerApp methods dealing with NUI processing


#include "stdafx.h"
#include "trackerApp.h"
#include "resource.h"
#include <mmsystem.h>
#include <assert.h>
#include <strsafe.h>

//lookups for color tinting based on player index
static const int g_IntensityShiftByPlayerR[] = { 1, 2, 0, 2, 0, 0, 2, 0 };
static const int g_IntensityShiftByPlayerG[] = { 1, 2, 2, 0, 2, 0, 0, 1 };
static const int g_IntensityShiftByPlayerB[] = { 1, 0, 2, 2, 0, 2, 0, 2 };

static const float g_JointThickness = 6.0f;
static const float g_TrackedBoneThickness = 6.0f;
static const float g_InferredBoneThickness = 1.0f;

const int g_BytesPerPixel = 4;

const int g_ScreenWidth = 320;
const int g_ScreenHeight = 240;


enum _SV_TRACKED_SKELETONS
{
	SV_TRACKED_SKELETONS_DEFAULT = 0,
	SV_TRACKED_SKELETONS_NEAREST1,
	SV_TRACKED_SKELETONS_NEAREST2,
	SV_TRACKED_SKELETONS_STICKY1,
	SV_TRACKED_SKELETONS_STICKY2
} SV_TRACKED_SKELETONS;

enum _SV_TRACKING_MODE
{
	SV_TRACKING_MODE_DEFAULT = 0,
	SV_TRACKING_MODE_SEATED
} SV_TRACKING_MODE;

enum _SV_RANGE
{
	SV_RANGE_DEFAULT = 0,
	SV_RANGE_NEAR,
} SV_RANGE;
/// <summary>
/// Zero out member variables
/// </summary>
void TrackerApp::Nui_Zero()
{
	SafeRelease( m_pNuiSensor );

	m_pRenderTarget = NULL;
	m_pBrushJointTracked = NULL;
	m_pBrushJointInferred = NULL;
	m_pBrushBoneTracked = NULL;
	m_pBrushBoneInferred = NULL;
	ZeroMemory(m_Points,sizeof(m_Points));

	m_hNextDepthFrameEvent = NULL;
	m_hNextColorFrameEvent = NULL;
	m_hNextSkeletonEvent = NULL;
	m_pDepthStreamHandle = NULL;
	m_pVideoStreamHandle = NULL;
	m_hThNuiProcess = NULL;
	m_hEvNuiProcessStop = NULL;
	m_LastSkeletonFoundTime = 0;
	m_bScreenBlanked = false;
	m_DepthFramesTotal = 0;
	m_LastDepthFPStime = 0;
	m_LastDepthFramesTotal = 0;
	m_pDrawDepth = NULL;
	m_pDrawColor = NULL;
	m_TrackedSkeletons = 0;
	m_SkeletonTrackingFlags = NUI_SKELETON_TRACKING_FLAG_ENABLE_IN_NEAR_RANGE;
	m_DepthStreamFlags = 0;
	ZeroMemory(m_StickySkeletonIds,sizeof(m_StickySkeletonIds));

	m_smoothParams.fSmoothing = 0.5f;
	m_smoothParams.fCorrection = 0.5f;
	m_smoothParams.fPrediction = 0.5f;
	m_smoothParams.fJitterRadius = 0.5f;
	m_smoothParams.fMaxDeviationRadius = 0.04f;

	m_activeUser = -1;
	m_secondaryUser = -1;

	m_listening = false;
}

/// <summary>
/// Callback to handle Kinect status changes, redirects to the class callback handler
/// </summary>
/// <param name="hrStatus">current status</param>
/// <param name="instanceName">instance name of Kinect the status change is for</param>
/// <param name="uniqueDeviceName">unique device name of Kinect the status change is for</param>
/// <param name="pUserData">additional packetData</param>
void CALLBACK TrackerApp::Nui_StatusProcThunk( HRESULT hrStatus, const OLECHAR* instanceName, const OLECHAR* uniqueDeviceName, void * pUserData )
{
	reinterpret_cast<TrackerApp *>(pUserData)->Nui_StatusProc( hrStatus, instanceName, uniqueDeviceName );
}

/// <summary>
/// Callback to handle Kinect status changes
/// </summary>
/// <param name="hrStatus">current status</param>
/// <param name="instanceName">instance name of Kinect the status change is for</param>
/// <param name="uniqueDeviceName">unique device name of Kinect the status change is for</param>
void CALLBACK TrackerApp::Nui_StatusProc( HRESULT hrStatus, const OLECHAR* instanceName, const OLECHAR* uniqueDeviceName )
{
	// Update UI
	PostMessageW( m_hWnd, WM_USER_UPDATE_COMBO, 0, 0 );

	if( SUCCEEDED(hrStatus) )
	{
		if ( S_OK == hrStatus )
		{
			if ( m_instanceId && 0 == wcscmp(instanceName, m_instanceId) )
			{
				Nui_Init(m_instanceId);
			}
			else if ( !m_pNuiSensor )
			{
				Nui_Init();
			}
		}
	}
	else
	{
		if ( m_instanceId && 0 == wcscmp(instanceName, m_instanceId) )
		{
			Nui_UnInit();
			Nui_Zero();
		}
	}
}

/// <summary>
/// Initialize Kinect by instance name
/// </summary>
/// <param name="instanceName">instance name of Kinect to initialize</param>
/// <returns>S_OK if successful, otherwise an error code</returns>
HRESULT TrackerApp::Nui_Init( OLECHAR *instanceName )
{
	// Generic creation failure
	if ( NULL == instanceName )
	{
		MessageBoxResource( IDS_ERROR_NUICREATE, MB_OK | MB_ICONHAND );
		return E_FAIL;
	}

	HRESULT hr = NuiCreateSensorById( instanceName, &m_pNuiSensor );

	// Generic creation failure
	if ( FAILED(hr) )
	{
		MessageBoxResource( IDS_ERROR_NUICREATE, MB_OK | MB_ICONHAND );
		return hr;
	}

	SysFreeString(m_instanceId);

	m_instanceId = m_pNuiSensor->NuiDeviceConnectionId();

	return Nui_Init();
}

/// <summary>
/// Initialize Kinect
/// </summary>
/// <returns>S_OK if successful, otherwise an error code</returns>
HRESULT TrackerApp::Nui_Init( )
{
	HRESULT  hr;
	bool     result;

	if ( !m_pNuiSensor )
	{
		hr = NuiCreateSensorByIndex(0, &m_pNuiSensor);

		if ( FAILED(hr) )
		{
			return hr;
		}

		SysFreeString(m_instanceId);

		m_instanceId = m_pNuiSensor->NuiDeviceConnectionId();
	}

	m_hNextDepthFrameEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
	m_hNextColorFrameEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
	m_hNextSkeletonEvent = CreateEvent( NULL, TRUE, FALSE, NULL );

	// reset the tracked skeletons, range, and tracking mode
	SendDlgItemMessage(m_hWnd, IDC_TRACKEDSKELETONS, CB_SETCURSEL, 0, 0);
	SendDlgItemMessage(m_hWnd, IDC_TRACKINGMODE, CB_SETCURSEL, 0, 0);
	SendDlgItemMessage(m_hWnd, IDC_RANGE, CB_SETCURSEL, 0, 0);

	EnsureDirect2DResources();

	m_pDrawDepth = new DrawDevice( );
	result = m_pDrawDepth->Initialize( GetDlgItem( m_hWnd, IDC_DEPTHVIEWER ), m_pD2DFactory, 320, 240, 320 * 4 );
	if ( !result )
	{
		MessageBoxResource( IDS_ERROR_DRAWDEVICE, MB_OK | MB_ICONHAND );
		return E_FAIL;
	}
	

	DWORD nuiFlags = NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX | NUI_INITIALIZE_FLAG_USES_SKELETON |  NUI_INITIALIZE_FLAG_USES_COLOR | NUI_INITIALIZE_FLAG_USES_AUDIO;

	hr = m_pNuiSensor->NuiInitialize(nuiFlags);
	if ( E_NUI_SKELETAL_ENGINE_BUSY == hr )
	{
		nuiFlags = NUI_INITIALIZE_FLAG_USES_DEPTH |  NUI_INITIALIZE_FLAG_USES_COLOR;
		hr = m_pNuiSensor->NuiInitialize( nuiFlags) ;
	}

	if ( FAILED( hr ) )
	{
		if ( E_NUI_DEVICE_IN_USE == hr )
		{
			MessageBoxResource( IDS_ERROR_IN_USE, MB_OK | MB_ICONHAND );
		}
		else
		{
			MessageBoxResource( IDS_ERROR_NUIINIT, MB_OK | MB_ICONHAND );
		}
		return hr;
	}


	if ( HasSkeletalEngine( m_pNuiSensor ) )
	{
		hr = m_pNuiSensor->NuiSkeletonTrackingEnable( m_hNextSkeletonEvent, m_SkeletonTrackingFlags );
		if( FAILED( hr ) )
		{
			MessageBoxResource( IDS_ERROR_SKELETONTRACKING, MB_OK | MB_ICONHAND );
			return hr;
		}
	}


	hr = m_pNuiSensor->NuiImageStreamOpen(
		HasSkeletalEngine(m_pNuiSensor) ? NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX : NUI_IMAGE_TYPE_DEPTH,
		NUI_IMAGE_RESOLUTION_320x240,
		m_DepthStreamFlags,
		2,
		m_hNextDepthFrameEvent,
		&m_pDepthStreamHandle );

	if ( FAILED( hr ) )
	{
		MessageBoxResource(IDS_ERROR_DEPTHSTREAM, MB_OK | MB_ICONHAND);
		return hr;
	}

	// Start the Nui processing thread
	m_hEvNuiProcessStop = CreateEvent( NULL, FALSE, FALSE, NULL );
	m_hThNuiProcess = CreateThread( NULL, 0, Nui_ProcessThread, this, 0, NULL );

	return hr;
}

/// <summary>
/// Uninitialize Kinect
/// </summary>
void TrackerApp::Nui_UnInit( )
{
	// Stop the Nui processing thread
	if ( NULL != m_hEvNuiProcessStop )
	{
		// Signal the thread
		SetEvent(m_hEvNuiProcessStop);

		// Wait for thread to stop
		if ( NULL != m_hThNuiProcess )
		{
			WaitForSingleObject( m_hThNuiProcess, INFINITE );
			CloseHandle( m_hThNuiProcess );
		}
		CloseHandle( m_hEvNuiProcessStop );
	}

	if ( m_pNuiSensor )
	{
		m_pNuiSensor->NuiShutdown( );
	}
	if ( m_hNextSkeletonEvent && ( m_hNextSkeletonEvent != INVALID_HANDLE_VALUE ) )
	{
		CloseHandle( m_hNextSkeletonEvent );
		m_hNextSkeletonEvent = NULL;
	}
	if ( m_hNextDepthFrameEvent && ( m_hNextDepthFrameEvent != INVALID_HANDLE_VALUE ) )
	{
		CloseHandle( m_hNextDepthFrameEvent );
		m_hNextDepthFrameEvent = NULL;
	}
	if ( m_hNextColorFrameEvent && ( m_hNextColorFrameEvent != INVALID_HANDLE_VALUE ) )
	{
		CloseHandle( m_hNextColorFrameEvent );
		m_hNextColorFrameEvent = NULL;
	}

	SafeRelease( m_pNuiSensor );

	// clean up Direct2D graphics
	delete m_pDrawDepth;
	m_pDrawDepth = NULL;

	delete m_pDrawColor;
	m_pDrawColor = NULL;

	DiscardDirect2DResources();
}

/// <summary>
/// Thread to handle Kinect processing, calls class instance thread processor
/// </summary>
/// <param name="pParam">instance pointer</param>
/// <returns>always 0</returns>
DWORD WINAPI TrackerApp::Nui_ProcessThread( LPVOID pParam )
{
	TrackerApp *pthis = (TrackerApp *)pParam;
	return pthis->Nui_ProcessThread( );
}

/// <summary>
/// Thread to handle Kinect processing
/// </summary>
/// <returns>always 0</returns>
DWORD WINAPI TrackerApp::Nui_ProcessThread( )
{
	const int numEvents = 4;
	HANDLE hEvents[numEvents] = { m_hEvNuiProcessStop, m_hNextDepthFrameEvent, m_hNextColorFrameEvent, m_hNextSkeletonEvent };
	int    nEventIdx;
	DWORD  t;

	m_LastDepthFPStime = timeGetTime( );

	// Blank the skeleton display on startup
	m_LastSkeletonFoundTime = 0;

	// Main thread loop
	bool continueProcessing = true;
	while ( continueProcessing )
	{
		// Wait for any of the events to be signalled
		nEventIdx = WaitForMultipleObjects( numEvents, hEvents, FALSE, 1000 );

		// Timed out, continue
		if ( nEventIdx == WAIT_TIMEOUT )
		{
			continue;
		}

		// stop event was signalled 
		if ( WAIT_OBJECT_0 == nEventIdx )
		{
			continueProcessing = false;
			break;
		}

		// Wait for each object individually with a 0 timeout to make sure to
		// process all signalled objects if multiple objects were signalled
		// this loop iteration

		// In situations where perfect correspondance between color/depth/skeleton
		// is essential, a priority queue should be used to service the item
		// which has been updated the longest ago

		if ( WAIT_OBJECT_0 == WaitForSingleObject( m_hNextDepthFrameEvent, 0 ) )
		{
			//only increment frame count if a frame was successfully drawn
			if ( Nui_GotDepthAlert() )
			{
				++m_DepthFramesTotal;
			}
		}



		// Once per second, display the depth FPS
		t = timeGetTime( );
		if ( (t - m_LastDepthFPStime) > 1000 )
		{
			int fps = ((m_DepthFramesTotal - m_LastDepthFramesTotal) * 1000 + 500) / (t - m_LastDepthFPStime);
			PostMessageW( m_hWnd, WM_USER_UPDATE_FPS, IDC_FPS, fps );
			m_LastDepthFramesTotal = m_DepthFramesTotal;
			m_LastDepthFPStime = t;
		}


	}

	return 0;
}

/// <summary>
/// Handle new depth data
/// </summary>
/// <returns>true if a frame was processed, false otherwise</returns>
bool TrackerApp::Nui_GotDepthAlert( )
{
	NUI_IMAGE_FRAME imageFrame;
	bool processedFrame = true;

	HRESULT hr = m_pNuiSensor->NuiImageStreamGetNextFrame(
		m_pDepthStreamHandle,
		0,
		&imageFrame );

	if ( FAILED( hr ) )
	{
		return false;
	}

	INuiFrameTexture * pTexture = imageFrame.pFrameTexture;
	NUI_LOCKED_RECT LockedRect;
	pTexture->LockRect( 0, &LockedRect, NULL, 0 );
	if ( 0 != LockedRect.Pitch )
	{
		DWORD frameWidth, frameHeight;

		NuiImageResolutionToSize( imageFrame.eResolution, frameWidth, frameHeight );

		// draw the bits to the bitmap
		BYTE * rgbrun = m_depthRGBX;
		const USHORT * pBufferRun = (const USHORT *)LockedRect.pBits;

		// end pixel is start + width*height - 1
		const USHORT * pBufferEnd = pBufferRun + (frameWidth * frameHeight);

		assert( frameWidth * frameHeight * g_BytesPerPixel <= ARRAYSIZE(m_depthRGBX) );

		while ( pBufferRun < pBufferEnd )
		{
			USHORT depth     = *pBufferRun;
			USHORT realDepth = NuiDepthPixelToDepth(depth);
			USHORT player    = NuiDepthPixelToPlayerIndex(depth);

			// transform 13-bit depth information into an 8-bit intensity appropriate
			// for display (we disregard information in most significant bit)
			BYTE intensity = static_cast<BYTE>(~(realDepth >> 4));

			// 0 means no player
			if (player == 0) {
				*(rgbrun++) = intensity >> g_IntensityShiftByPlayerB[0];
				*(rgbrun++) = intensity >> g_IntensityShiftByPlayerG[0];
				*(rgbrun++) = intensity >> g_IntensityShiftByPlayerR[0];
				// no alpha information, skip the last byte
				++rgbrun;
			}
			// tint the active user green
			else if (player-1 == m_activeUser) {
				*(rgbrun++) = intensity >> g_IntensityShiftByPlayerB[3];
				*(rgbrun++) = intensity >> g_IntensityShiftByPlayerG[3];
				*(rgbrun++) = intensity >> g_IntensityShiftByPlayerR[3];
				// no alpha information, skip the last byte
				++rgbrun;
			}
			// tint everyone else red
			else { 
				*(rgbrun++) = intensity >> g_IntensityShiftByPlayerB[2];
				*(rgbrun++) = intensity >> g_IntensityShiftByPlayerG[2];
				*(rgbrun++) = intensity >> g_IntensityShiftByPlayerR[2];
				// no alpha information, skip the last byte
				++rgbrun;
			}
			++pBufferRun;
		}

		NUI_SKELETON_FRAME SkeletonFrame;
		hr = m_pNuiSensor->NuiSkeletonGetNextFrame( 0, &SkeletonFrame );

		// smooth out the skeleton data
		HRESULT hr = m_pNuiSensor->NuiTransformSmooth(&SkeletonFrame,&m_smoothParams);

		RECT rct;
		GetClientRect( GetDlgItem( m_hWnd, IDC_DEPTHVIEWER ), &rct);
		int width = rct.right;
		int height = rct.bottom;

		m_pDrawDepth->ProcessSkeletonFrame( m_depthRGBX, frameWidth * frameHeight * g_BytesPerPixel, SkeletonFrame, m_pNuiSensor, 640, 480, m_ipAddress, m_port);
	}

	else
	{
		processedFrame = false;
		OutputDebugString( L"Buffer length of received texture is bogus\r\n" );
	}

	pTexture->UnlockRect( 0 );

	m_pNuiSensor->NuiImageStreamReleaseFrame( m_pDepthStreamHandle, &imageFrame );

	return processedFrame;
}





/// <summary>
/// Converts a skeleton point to screen space
/// </summary>
/// <param name="skeletonPoint">skeleton point to tranform</param>
/// <param name="width">width (in pixels) of output buffer</param>
/// <param name="height">height (in pixels) of output buffer</param>
/// <returns>point in screen-space</returns>
D2D1_POINT_2F TrackerApp::SkeletonToScreen( Vector4 skeletonPoint, int width, int height )
{
	LONG x, y;
	USHORT depth;

	// calculate the skeleton's position on the screen
	// NuiTransformSkeletonToDepthImage returns coordinates in NUI_IMAGE_RESOLUTION_320x240 space
	NuiTransformSkeletonToDepthImage( skeletonPoint, &x, &y, &depth );

	float screenPointX = static_cast<float>(x * width) / g_ScreenWidth;
	float screenPointY = static_cast<float>(y * height) / g_ScreenHeight;

	return D2D1::Point2F(screenPointX, screenPointY);
}


/// <summary>
/// Invoked when the user changes the selection of tracked skeletons
/// </summary>
/// <param name="mode">skelton tracking mode to switch to</param>
void TrackerApp::UpdateTrackedSkeletonSelection( int mode )
{
	m_TrackedSkeletons = mode;
	m_trackedSkeletons = mode;
	UpdateSkeletonTrackingFlag(
		NUI_SKELETON_TRACKING_FLAG_TITLE_SETS_TRACKED_SKELETONS,
		(mode != SV_TRACKED_SKELETONS_DEFAULT));
}

/// <summary>
/// Invoked when the user changes the tracking mode
/// </summary>
/// <param name="mode">tracking mode to switch to</param>
void TrackerApp::UpdateTrackingMode( int mode )
{
	m_trackingMode = mode;
	UpdateSkeletonTrackingFlag(
		NUI_SKELETON_TRACKING_FLAG_ENABLE_SEATED_SUPPORT,
		(mode == SV_TRACKING_MODE_SEATED) );
}

/// <summary>
/// Invoked when the user changes the range
/// </summary>
/// <param name="mode">range to switch to</param>
void TrackerApp::UpdateRange( int mode )
{
	m_range = mode;
	UpdateDepthStreamFlag(
		NUI_IMAGE_STREAM_FLAG_ENABLE_NEAR_MODE,
		(mode != SV_RANGE_DEFAULT) );
}

/// <summary>
/// Sets or clears the specified skeleton tracking flag
/// </summary>
/// <param name="flag">flag to set or clear</param>
/// <param name="value">true to set, false to clear</param>
void TrackerApp::UpdateSkeletonTrackingFlag( DWORD flag, bool value )
{
	DWORD newFlags = m_SkeletonTrackingFlags;

	if (value)
	{
		newFlags |= flag;
	}
	else
	{
		newFlags &= ~flag;
	}

	if (NULL != m_pNuiSensor && newFlags != m_SkeletonTrackingFlags)
	{
		if ( !HasSkeletalEngine(m_pNuiSensor) )
		{
			MessageBoxResource(IDS_ERROR_SKELETONTRACKING, MB_OK | MB_ICONHAND);
		}

		m_SkeletonTrackingFlags = newFlags;

		HRESULT hr = m_pNuiSensor->NuiSkeletonTrackingEnable( m_hNextSkeletonEvent, m_SkeletonTrackingFlags );

		if ( FAILED( hr ) )
		{
			MessageBoxResource(IDS_ERROR_SKELETONTRACKING, MB_OK | MB_ICONHAND);
		}
	}
}

/// <summary>
/// Sets or clears the specified depth stream flag
/// </summary>
/// <param name="flag">flag to set or clear</param>
/// <param name="value">true to set, false to clear</param>
void TrackerApp::UpdateDepthStreamFlag( DWORD flag, bool value )
{
	DWORD newFlags = m_DepthStreamFlags;

	if (value)
	{
		newFlags |= flag;
	}
	else
	{
		newFlags &= ~flag;
	}

	if (NULL != m_pNuiSensor && newFlags != m_DepthStreamFlags)
	{
		m_DepthStreamFlags = newFlags;
		m_pNuiSensor->NuiImageStreamSetImageFrameFlags( m_pDepthStreamHandle, m_DepthStreamFlags );
	}
}

/// <summary>
/// Ensure necessary Direct2d resources are created
/// </summary>
/// <returns>S_OK if successful, otherwise an error code</returns>
HRESULT TrackerApp::EnsureDirect2DResources()
{
	HRESULT hr = S_OK;

	if ( !m_pRenderTarget )
	{
		RECT rc;
		GetWindowRect( GetDlgItem( m_hWnd, IDC_SKELETALVIEW ), &rc );  

		int width = rc.right - rc.left;
		int height = rc.bottom - rc.top;
		D2D1_SIZE_U size = D2D1::SizeU( width, height );
		D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties();
		rtProps.pixelFormat = D2D1::PixelFormat( DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE);
		rtProps.usage = D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE;

		// Create a Hwnd render target, in order to render to the window set in initialize
		hr = m_pD2DFactory->CreateHwndRenderTarget(
			rtProps,
			D2D1::HwndRenderTargetProperties(GetDlgItem( m_hWnd, IDC_SKELETALVIEW), size),
			&m_pRenderTarget
			);
		if ( FAILED( hr ) )
		{
			MessageBoxResource( IDS_ERROR_DRAWDEVICE, MB_OK | MB_ICONHAND );
			return E_FAIL;
		}

		//light green
		m_pRenderTarget->CreateSolidColorBrush( D2D1::ColorF( 68, 192, 68 ), &m_pBrushJointTracked );

		//yellow
		m_pRenderTarget->CreateSolidColorBrush( D2D1::ColorF( 255, 255, 0 ), &m_pBrushJointInferred );

		//green
		m_pRenderTarget->CreateSolidColorBrush( D2D1::ColorF( 0, 128, 0 ), &m_pBrushBoneTracked );

		//gray
		m_pRenderTarget->CreateSolidColorBrush( D2D1::ColorF( 128, 128, 128 ), &m_pBrushBoneInferred );
	}

	return hr;
}

/// <summary>
/// Dispose Direct2d resources 
/// </summary>
void TrackerApp::DiscardDirect2DResources( )
{
	SafeRelease(m_pRenderTarget);

	SafeRelease( m_pBrushJointTracked );
	SafeRelease( m_pBrushJointInferred );
	SafeRelease( m_pBrushBoneTracked );
	SafeRelease( m_pBrushBoneInferred );
}
