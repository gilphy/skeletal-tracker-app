//------------------------------------------------------------------------------
// <copyright file="DrawDevice.cpp" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
//------------------------------------------------------------------------------


#include "stdafx.h"
#include "DrawDevice.h"
#include "NuiApi.h"
#include "resource.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <sstream>
#include <MMSystem.h>
#include "trackerApp.h"
#include <string>

using namespace std;

#pragma comment(lib, "Ws2_32.lib")

extern TrackerApp  g_trackerApp;

const int g_ScreenWidth = 640;
const int g_ScreenHeight = 480;
const float BONE_THICKNESS = 3.0;
const float JOINT_THICKNESS = 2.5;

DWORD lastSkelFoundTime;

float packetData[18];

//---------------------------------------------------------------------------------------

int SendDataUDP(std::string ipAddress, std::string port)

{		int sockfd;
struct addrinfo hints, *servinfo, *p;
int rv;
int numbytes;
char *ip_addr = NULL;
memset(&hints, 0, sizeof hints);
hints.ai_family = AF_INET;
hints.ai_socktype = SOCK_DGRAM;


if ((rv = getaddrinfo(ipAddress.c_str(), port.c_str(), &hints, &servinfo)) != 0) 
{	fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
return 1;
}
// loop through all the results and make a socket
for(p = servinfo; p != NULL; p = p->ai_next) 
{
	if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) 
	{
		perror("talker: socket");
		continue;
	}
	break;
}
if (p == NULL) 
{	fprintf(stderr, "talker: failed to bind socket\n");
return 2;
}
if ((numbytes = sendto(sockfd, reinterpret_cast<const char *>(packetData), sizeof(float)*6, 0, p->ai_addr, p->ai_addrlen)) == -1)
{	perror("talker: sendto");
exit(1);
}

freeaddrinfo(servinfo);
printf("talker: sent %d bytes to %s\n", numbytes, ip_addr);
closesocket(sockfd);

return 0;
}

bool Listen() {
	for (int i = 0; i < MAX_IPS; i++)
	{
		g_trackerApp.m_ipAddress[i] = "";
		g_trackerApp.m_port[i] = "";
	}

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
	server.sin_port = htons( (g_trackerApp.m_servPort) );

	//Bind
	if( bind(s ,(struct sockaddr *)&server , sizeof(server)) == SOCKET_ERROR)
	{
		printf("Bind failed with error code : %d" , WSAGetLastError());
	}

	puts("Bind done");

	listen(s, MAX_IPS);

	//Accept and incoming connection
	puts("Waiting for incoming connections...");
	int i = 0, prev_socket = new_socket;
	while (i < MAX_IPS) {
		stringstream ipBuffer, portBuffer;
		c = sizeof(struct sockaddr_in);
		if (g_trackerApp.m_listening == false) break;
		new_socket = accept(s , (struct sockaddr *)&client, &c);
		if ((prev_socket != new_socket) && (new_socket != INVALID_SOCKET)) {
			ipBuffer << (int)client.sin_addr.S_un.S_un_b.s_b1 << "." << (int)client.sin_addr.S_un.S_un_b.s_b2 << "." << (int)client.sin_addr.S_un.S_un_b.s_b3 << "." << (int)client.sin_addr.S_un.S_un_b.s_b4;
			portBuffer << (int)client.sin_port;
			g_trackerApp.m_ipAddress[i] = ipBuffer.str();
			g_trackerApp.m_port[i] = portBuffer.str();
			prev_socket = new_socket;
			i++;
		}

	}

	closesocket(s);
	closesocket(new_socket);
}

/// <summary>
/// Constructor
/// </summary>
DrawDevice::DrawDevice() : 
m_hWnd(0),
	m_sourceWidth(0),
	m_sourceHeight(0),
	m_sourceStride(0),
	m_pD2DFactory(NULL), 
	m_pRenderTarget(NULL),
	m_pBitmap(0)
{
}

/// <summary>
/// Destructor
/// </summary>
DrawDevice::~DrawDevice()
{
	DiscardResources();
	SafeRelease(m_pD2DFactory);
}

/// <summary>
/// Ensure necessary Direct2d resources are created
/// </summary>
/// <returns>S_OK if successful, otherwise an error code</returns>
HRESULT DrawDevice::EnsureResources()
{
	HRESULT hr = S_OK;

	if ( !m_pRenderTarget )
	{
		D2D1_SIZE_U size = D2D1::SizeU( m_sourceWidth, m_sourceHeight );

		D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties();
		rtProps.pixelFormat = D2D1::PixelFormat( DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE);
		rtProps.usage = D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE;

		// Create a Hwnd render target, in order to render to the window set in initialize
		hr = m_pD2DFactory->CreateHwndRenderTarget(
			rtProps,
			D2D1::HwndRenderTargetProperties(m_hWnd, size),
			&m_pRenderTarget
			);

		if ( FAILED( hr ) )
		{
			return hr;
		}

		// Create a bitmap that we can copy image packetData into and then render to the target
		hr = m_pRenderTarget->CreateBitmap(
			size, 
			D2D1::BitmapProperties( D2D1::PixelFormat( DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE) ),
			&m_pBitmap 
			);

		m_pRenderTarget->CreateSolidColorBrush( D2D1::ColorF( 68, 192, 68 ), &m_pBrush );

		if ( FAILED( hr ) )
		{
			SafeRelease( m_pRenderTarget );
			return hr;
		}
	}

	return hr;
}

/// <summary>
/// Dispose of Direct2d resources 
/// </summary>
void DrawDevice::DiscardResources( )
{
	SafeRelease(m_pRenderTarget);
	SafeRelease(m_pBitmap);
}

/// <summary>
/// Set the window to draw to as well as the video format
/// Implied bits per pixel is 32
/// </summary>
/// <param name="hWnd">window to draw to</param>
/// <param name="pD2DFactory">already created D2D factory object</param>
/// <param name="sourceWidth">width (in pixels) of image packetData to be drawn</param>
/// <param name="sourceHeight">height (in pixels) of image packetData to be drawn</param>
/// <param name="sourceStride">length (in bytes) of a single scanline</param>
/// <returns>true if successful, false otherwise</returns>
bool DrawDevice::Initialize( HWND hWnd, ID2D1Factory * pD2DFactory, int sourceWidth, int sourceHeight, int sourceStride )
{
	if ( NULL == pD2DFactory )
	{
		return false;
	}

	m_hWnd = hWnd;

	// One factory for the entire application so save a pointer here
	m_pD2DFactory = pD2DFactory;

	m_pD2DFactory->AddRef( );

	// Get the frame size
	m_sourceWidth  = sourceWidth;
	m_sourceHeight = sourceHeight;
	m_sourceStride = sourceStride;

	return true;
}

D2D1_POINT_2F SkeletonToScreen( Vector4 skeletonPoint, int width, int height )
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

void DrawDevice::DrawBone( const NUI_SKELETON_DATA & skel, NUI_SKELETON_POSITION_INDEX bone0, NUI_SKELETON_POSITION_INDEX bone1 )
{
	NUI_SKELETON_POSITION_TRACKING_STATE bone0State = skel.eSkeletonPositionTrackingState[bone0];
	NUI_SKELETON_POSITION_TRACKING_STATE bone1State = skel.eSkeletonPositionTrackingState[bone1];

	if (bone0State == NUI_SKELETON_POSITION_TRACKED || bone1State == NUI_SKELETON_POSITION_TRACKED)
		m_pRenderTarget->DrawLine( m_Points[bone0], m_Points[bone1], m_pBrush, BONE_THICKNESS );

}


bool DrawDevice::ProcessSkeletonFrame( BYTE * pImage, unsigned long cbImage, NUI_SKELETON_FRAME SkeletonFrame, 
	INuiSensor *m_pNuiSensor, int width, int height, std::string ipAddress[MAX_IPS], std::string port[MAX_IPS])
{
	if (g_trackerApp.m_listening == true)
		Listen();

	// incorrectly sized image packetData passed in
	if ( cbImage < ((m_sourceHeight - 1) * m_sourceStride) + (m_sourceWidth * 4) )
		return false;

	// create the resources for this draw device
	// they will be recreated if previously lost
	HRESULT hr = EnsureResources( );
	if ( FAILED( hr ) )
		return false;
	hr = EnsureDirect2DResources();
	if ( FAILED( hr ) )
		return false;

	// Copy the image that was passed in into the direct2d bitmap
	hr = m_pBitmap->CopyFromMemory( NULL, pImage, m_sourceStride );
	if ( FAILED( hr ) )
		return false;

	m_pRenderTarget->BeginDraw();

	// Draw the bitmap stretched to the size of the window
	m_pRenderTarget->DrawBitmap( m_pBitmap );

	LONG x, y;
	USHORT depth;
	DWORD nearestIDs[2] = { 0, 0 };
	USHORT nearestDepths[2] = { NUI_IMAGE_DEPTH_MAXIMUM, NUI_IMAGE_DEPTH_MAXIMUM };

	// Determine which users to track by seeing who is closest
	for ( int i = 0 ; i < NUI_SKELETON_COUNT; i++ )
	{
		NUI_SKELETON_TRACKING_STATE trackingState = SkeletonFrame.SkeletonData[i].eTrackingState;

		// calculate the skeleton's position on the screen
		NuiTransformSkeletonToDepthImage( SkeletonFrame.SkeletonData[i].Position, &x, &y, &depth );

		if ( trackingState == NUI_SKELETON_TRACKED || trackingState == NUI_SKELETON_POSITION_ONLY ) {

			if ( depth < nearestDepths[0] )
			{
				nearestDepths[1] = nearestDepths[0];
				nearestIDs[1] = nearestIDs[0];
				g_trackerApp.m_secondaryUser = g_trackerApp.m_activeUser;

				nearestDepths[0] = depth;
				nearestIDs[0] = SkeletonFrame.SkeletonData[i].dwTrackingID;
				g_trackerApp.m_activeUser = i;
			}
			else if ( depth < nearestDepths[1] )
			{
				nearestDepths[1] = depth;
				nearestIDs[1] = SkeletonFrame.SkeletonData[i].dwTrackingID;
				g_trackerApp.m_secondaryUser = i;
			}
		}
	}

	// apply
	m_pNuiSensor->NuiSkeletonSetTrackedSkeletons(nearestIDs);

	for ( int i = 0 ; i < NUI_SKELETON_COUNT; i++ )
	{
		NUI_SKELETON_TRACKING_STATE trackingState = SkeletonFrame.SkeletonData[i].eTrackingState;
		// Draw tracked skeletons
		if ( trackingState == NUI_SKELETON_TRACKED )
		{
			// Get joints in screen space
			for (int j = 0; j < NUI_SKELETON_POSITION_COUNT; j++)
				m_Points[j] = SkeletonToScreen( SkeletonFrame.SkeletonData[i].SkeletonPositions[j], width, height);

			// only send data of active user
			if (g_trackerApp.m_activeUser == i) {
				// convert the relevant coordinates to target coordinate system, in inches
				float angleRad = -(g_trackerApp.m_KinectAngle)*.01745;
				float head_x = SkeletonFrame.SkeletonData[i].SkeletonPositions[3].x*39.37 + g_trackerApp.m_kinectPosition[0];
				float head_y = (SkeletonFrame.SkeletonData[i].SkeletonPositions[3].y*cos(angleRad) 
					- SkeletonFrame.SkeletonData[i].SkeletonPositions[3].z*sin(angleRad))*39.37 + g_trackerApp.m_kinectPosition[1];
				float head_z = (SkeletonFrame.SkeletonData[i].SkeletonPositions[3].z*cos(angleRad)
					+ SkeletonFrame.SkeletonData[i].SkeletonPositions[3].y*sin(angleRad))*39.37 + g_trackerApp.m_kinectPosition[2];
				float should_x = SkeletonFrame.SkeletonData[i].SkeletonPositions[2].x*39.37 + g_trackerApp.m_kinectPosition[0];
				float should_y = (SkeletonFrame.SkeletonData[i].SkeletonPositions[2].y*cos(angleRad) 
					- SkeletonFrame.SkeletonData[i].SkeletonPositions[2].z*sin(angleRad))*39.37 + g_trackerApp.m_kinectPosition[1];
				float should_z = (SkeletonFrame.SkeletonData[i].SkeletonPositions[2].z*cos(angleRad)
					+ SkeletonFrame.SkeletonData[i].SkeletonPositions[2].y*sin(angleRad))*39.37 + g_trackerApp.m_kinectPosition[2];
				float right_elbow_x = SkeletonFrame.SkeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_RIGHT].x*39.37 + g_trackerApp.m_kinectPosition[0];
				float right_elbow_y = (SkeletonFrame.SkeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_RIGHT].y*cos(angleRad) 
					- SkeletonFrame.SkeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_RIGHT].z*sin(angleRad))*39.37 + g_trackerApp.m_kinectPosition[1];
				float right_elbow_z = (SkeletonFrame.SkeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_RIGHT].z*cos(angleRad)
					+ SkeletonFrame.SkeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_RIGHT].y*sin(angleRad))*39.37 + g_trackerApp.m_kinectPosition[2];
				float right_hand_x = SkeletonFrame.SkeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT].x*39.37 + g_trackerApp.m_kinectPosition[0];
				float right_hand_y = (SkeletonFrame.SkeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT].y*cos(angleRad) 
					- SkeletonFrame.SkeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT].z*sin(angleRad))*39.37 + g_trackerApp.m_kinectPosition[1];
				float right_hand_z = (SkeletonFrame.SkeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT].z*cos(angleRad)
					+ SkeletonFrame.SkeletonData[i].SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT].y*sin(angleRad))*39.37 + g_trackerApp.m_kinectPosition[2];

				float headTilt = abs(atan((should_x - head_x)/(should_y - head_y)));

				// left eye
				packetData[0] = head_x;
				packetData[1] = head_y;
				packetData[2] = head_z;

				// right eye
				packetData[3] = head_x;
				packetData[4] = head_y;
				packetData[5] = head_z;

				// correct eye positions for head tilt
				if (head_x < should_x) {
					packetData[0] -= cos(headTilt)*1.25;
					packetData[1] -= sin(headTilt)*1.25;
					packetData[3] += cos(headTilt)*1.25;
					packetData[4] += sin(headTilt)*1.25;
				}
				else {
					packetData[3] += cos(headTilt)*1.25;
					packetData[4] -= sin(headTilt)*1.25;
					packetData[0] -= cos(headTilt)*1.25;
					packetData[1] += sin(headTilt)*1.25;
				}

				// right elbow
				packetData[6] = right_elbow_x;
				packetData[7] = right_elbow_y;
				packetData[8] = right_elbow_z;

				// right hand
				packetData[9] = right_hand_x;
				packetData[10] = right_hand_y;
				packetData[11] = right_hand_z;
			}

			// draw only torso if we are globally in seated mode
			if (g_trackerApp.m_trackingMode == Seated)
			{
				DrawBone( SkeletonFrame.SkeletonData[i], NUI_SKELETON_POSITION_HEAD, NUI_SKELETON_POSITION_SHOULDER_CENTER );
				DrawBone( SkeletonFrame.SkeletonData[i], NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_LEFT );
				DrawBone( SkeletonFrame.SkeletonData[i], NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_RIGHT );
				DrawBone( SkeletonFrame.SkeletonData[i], NUI_SKELETON_POSITION_SHOULDER_LEFT, NUI_SKELETON_POSITION_ELBOW_LEFT );
				DrawBone( SkeletonFrame.SkeletonData[i], NUI_SKELETON_POSITION_ELBOW_LEFT, NUI_SKELETON_POSITION_WRIST_LEFT );
				DrawBone( SkeletonFrame.SkeletonData[i], NUI_SKELETON_POSITION_WRIST_LEFT, NUI_SKELETON_POSITION_HAND_LEFT );
				DrawBone( SkeletonFrame.SkeletonData[i], NUI_SKELETON_POSITION_SHOULDER_RIGHT, NUI_SKELETON_POSITION_ELBOW_RIGHT );
				DrawBone( SkeletonFrame.SkeletonData[i], NUI_SKELETON_POSITION_ELBOW_RIGHT, NUI_SKELETON_POSITION_WRIST_RIGHT );
				DrawBone( SkeletonFrame.SkeletonData[i], NUI_SKELETON_POSITION_WRIST_RIGHT, NUI_SKELETON_POSITION_HAND_RIGHT );
			}
			// otherwise draw the whole thing
			else {
				// Render Torso
				DrawBone( SkeletonFrame.SkeletonData[i], NUI_SKELETON_POSITION_HEAD, NUI_SKELETON_POSITION_SHOULDER_CENTER );
				DrawBone( SkeletonFrame.SkeletonData[i], NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_LEFT );
				DrawBone( SkeletonFrame.SkeletonData[i], NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_RIGHT );
				DrawBone( SkeletonFrame.SkeletonData[i], NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SPINE );
				DrawBone( SkeletonFrame.SkeletonData[i], NUI_SKELETON_POSITION_SPINE, NUI_SKELETON_POSITION_HIP_CENTER );
				DrawBone( SkeletonFrame.SkeletonData[i], NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_LEFT );
				DrawBone( SkeletonFrame.SkeletonData[i], NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_RIGHT );

				// Left Arm
				DrawBone( SkeletonFrame.SkeletonData[i], NUI_SKELETON_POSITION_SHOULDER_LEFT, NUI_SKELETON_POSITION_ELBOW_LEFT );
				DrawBone( SkeletonFrame.SkeletonData[i], NUI_SKELETON_POSITION_ELBOW_LEFT, NUI_SKELETON_POSITION_WRIST_LEFT );
				DrawBone( SkeletonFrame.SkeletonData[i], NUI_SKELETON_POSITION_WRIST_LEFT, NUI_SKELETON_POSITION_HAND_LEFT );

				// Right Arm
				DrawBone( SkeletonFrame.SkeletonData[i], NUI_SKELETON_POSITION_SHOULDER_RIGHT, NUI_SKELETON_POSITION_ELBOW_RIGHT );
				DrawBone( SkeletonFrame.SkeletonData[i], NUI_SKELETON_POSITION_ELBOW_RIGHT, NUI_SKELETON_POSITION_WRIST_RIGHT );
				DrawBone( SkeletonFrame.SkeletonData[i], NUI_SKELETON_POSITION_WRIST_RIGHT, NUI_SKELETON_POSITION_HAND_RIGHT );

				// Left Leg
				DrawBone( SkeletonFrame.SkeletonData[i], NUI_SKELETON_POSITION_HIP_LEFT, NUI_SKELETON_POSITION_KNEE_LEFT );
				DrawBone( SkeletonFrame.SkeletonData[i], NUI_SKELETON_POSITION_KNEE_LEFT, NUI_SKELETON_POSITION_ANKLE_LEFT );
				DrawBone( SkeletonFrame.SkeletonData[i], NUI_SKELETON_POSITION_ANKLE_LEFT, NUI_SKELETON_POSITION_FOOT_LEFT );

				// Right Leg
				DrawBone( SkeletonFrame.SkeletonData[i], NUI_SKELETON_POSITION_HIP_RIGHT, NUI_SKELETON_POSITION_KNEE_RIGHT );
				DrawBone( SkeletonFrame.SkeletonData[i], NUI_SKELETON_POSITION_KNEE_RIGHT, NUI_SKELETON_POSITION_ANKLE_RIGHT );
				DrawBone( SkeletonFrame.SkeletonData[i], NUI_SKELETON_POSITION_ANKLE_RIGHT, NUI_SKELETON_POSITION_FOOT_RIGHT );
			}
			// Draw joints
			for (int j = 0; j < NUI_SKELETON_POSITION_COUNT; j++ )
			{
				D2D1_ELLIPSE ellipse = D2D1::Ellipse( m_Points[j], JOINT_THICKNESS, JOINT_THICKNESS );
				if (SkeletonFrame.SkeletonData[i].eSkeletonPositionTrackingState[j] == NUI_SKELETON_POSITION_INFERRED ||
					SkeletonFrame.SkeletonData[i].eSkeletonPositionTrackingState[j] == NUI_SKELETON_POSITION_TRACKED )
					m_pRenderTarget->FillEllipse(ellipse, m_pBrush);
			}
		}
		else if ( trackingState == NUI_SKELETON_POSITION_ONLY )
		{
			// draw the center of mass of skeleton
			D2D1_ELLIPSE ellipse = D2D1::Ellipse( SkeletonToScreen(SkeletonFrame.SkeletonData[i].Position, width, height), JOINT_THICKNESS, JOINT_THICKNESS);
			m_pRenderTarget->FillEllipse(ellipse, m_pBrush);
		}


	}
	// send to each of 6 IPs
	for (int i = 0; i < MAX_IPS; i++)
		SendDataUDP(ipAddress[i], port[i]);

	hr = m_pRenderTarget->EndDraw();

	// Device lost, need to recreate the render target
	// We'll dispose it now and retry drawing
	if ( hr == D2DERR_RECREATE_TARGET )
	{
		hr = S_OK;
		DiscardResources();
		DiscardDirect2DResources();
	}

	return SUCCEEDED( hr );
}

/// <summary>
/// Ensure necessary Direct2d resources are created
/// </summary>
/// <returns>S_OK if successful, otherwise an error code</returns>
HRESULT DrawDevice::EnsureDirect2DResources()
{
	HRESULT hr = S_OK;

	if ( !m_pRenderTarget )
	{
		m_pRenderTarget->CreateSolidColorBrush( D2D1::ColorF(D2D1::ColorF::White), &m_pBrush );
	}

	return hr;
}

/// <summary>
/// Dispose Direct2d resources 
/// </summary>
void DrawDevice::DiscardDirect2DResources( )
{
	SafeRelease(m_pRenderTarget);
	SafeRelease(m_pBrush);
}
