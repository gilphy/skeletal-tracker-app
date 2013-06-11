#include "stdafx.h"
#include <NuiApi.h>
#include <KinectInteraction.h>

#pragma once

class TrackerClient : public INuiInteractionClient
{
public:
    TrackerClient()                                                 {}
    ~TrackerClient()                                                {}
    STDMETHODIMP_(ULONG)    AddRef()                                    { return 2;     }
    STDMETHODIMP_(ULONG)    Release()                                   { return 1;     }
    STDMETHODIMP            QueryInterface(REFIID riid, void **ppv)     { return S_OK;  }
    HRESULT STDMETHODCALLTYPE GetInteractionInfoAtLocation(DWORD skeletonTrackingId,
           NUI_HAND_TYPE handType, FLOAT x, FLOAT y, _Out_ NUI_INTERACTION_INFO *pInteractionInfo);
};