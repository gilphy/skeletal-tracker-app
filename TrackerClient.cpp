#include "stdafx.h"
#include "TrackerClient.h"
	 HRESULT STDMETHODCALLTYPE TrackerClient::GetInteractionInfoAtLocation(DWORD skeletonTrackingId,
           NUI_HAND_TYPE handType, FLOAT x, FLOAT y, _Out_ NUI_INTERACTION_INFO *pInteractionInfo)
	{
        pInteractionInfo->IsGripTarget = TRUE;
        return S_OK;
    }