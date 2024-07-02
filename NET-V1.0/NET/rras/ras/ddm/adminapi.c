/********************************************************************/
/**               Copyright(c) 1995 Microsoft Corporation.	       **/
/********************************************************************/

//***
//
// Filename:    adminapi.c
//
// Description: Contains code to respond to DDM admin. requests.
//
// History:     May 11,1995	    NarenG		Created original version.
//
#include "ddm.h"
#include <lmmsg.h>
#include "objects.h"
#include "handlers.h"
#include "rasapiif.h"
#include "routerif.h"
#include "util.h"
#include <dimsvc.h>     // Generated by MIDL
#include <string.h>
#include <stdlib.h>
#include <mprapip.h>

//**
//
// Call:        DDMAdminInterfaceConnect
//
// Returns:     NO_ERROR - Success
//
// Description:
//
DWORD
DDMAdminInterfaceConnect(
    IN      HANDLE      hDimInterface,
    IN      HANDLE      hEvent,
    IN      BOOL        fBlocking,
    IN      DWORD       dwCallersProcessId
)
{
    HANDLE                      hClientProcess       = NULL;
    DWORD                       dwRetCode            = NO_ERROR;
    ROUTER_INTERFACE_OBJECT*    pIfObject            = NULL;
    HANDLE                      hEventToBeDuplicated = NULL;
    DWORD                       fReturn              = FALSE;

    EnterCriticalSection( &(gblpInterfaceTable->CriticalSection) );

    do
    {
        if ( ( pIfObject = IfObjectGetPointer((HANDLE)hDimInterface) ) == NULL )
        {
            dwRetCode = ERROR_INVALID_HANDLE;
            break;
        }

        if ( pIfObject->State == RISTATE_CONNECTED )
        {
            dwRetCode = NO_ERROR;
            fReturn = TRUE;
            break;
        }

        if ( pIfObject->State == RISTATE_CONNECTING )
        {
            dwRetCode = ERROR_ALREADY_CONNECTING;
            fReturn = TRUE;
            break;
        }

        if ( ( hEvent == NULL ) && ( fBlocking ) )
        {
            //
            // This call is to be synchrnonous, create an event and block on
            // it.
            //

            hEventToBeDuplicated = CreateEvent( NULL, FALSE, FALSE, NULL );

            if ( hEventToBeDuplicated == NULL )
            {
                dwRetCode = GetLastError();

                break;
            }

            dwCallersProcessId = GetCurrentProcessId();
        }
        else
        {
            hEventToBeDuplicated = hEvent;
        }

        if ( hEventToBeDuplicated != NULL )
        {
            //
            //
            // Get process handle of the caller of this API
            //

            hClientProcess = OpenProcess(
                            STANDARD_RIGHTS_REQUIRED | SPECIFIC_RIGHTS_ALL,
                            FALSE,
                            dwCallersProcessId);

            if ( hClientProcess == NULL )
            {
                dwRetCode = GetLastError();

                break;
            }

            //
            // Duplicate the handle to the event
            //

            if ( !DuplicateHandle(
                                hClientProcess,
                                hEventToBeDuplicated,
                                GetCurrentProcess(),
                                &(pIfObject->hEventNotifyCaller),
                                0,
                                FALSE,
                                DUPLICATE_SAME_ACCESS ) )
            {
                CloseHandle( hClientProcess );

                dwRetCode = GetLastError();

                break;
            }

            CloseHandle( hClientProcess );
        }
        else
        {
            pIfObject->hEventNotifyCaller = INVALID_HANDLE_VALUE;
        }

        //
        // Initiate a connection
        //

        dwRetCode = RasConnectionInitiate( pIfObject, FALSE );

        if ( dwRetCode != NO_ERROR )
        {
            CloseHandle( pIfObject->hEventNotifyCaller );

            pIfObject->hEventNotifyCaller = INVALID_HANDLE_VALUE;
        }
        else
        {
            dwRetCode = PENDING;
        }

        DDM_PRINT(  gblDDMConfigInfo.dwTraceId, TRACE_FSM,
	                "RasConnectionInitiate: To %ws dwRetCode=%d",
                    pIfObject->lpwsInterfaceName, dwRetCode );
    }
    while( FALSE );

    LeaveCriticalSection( &(gblpInterfaceTable->CriticalSection) );

    //
    // If we are or already connecting or connected then simply return
    //

    if ( fReturn )
    {
        return( dwRetCode );
    }

    //
    // This is a synchronous call, we need to wait till compeletion
    //

    if ( ( hEvent == NULL ) && ( fBlocking ) )
    {
        if ( dwRetCode == PENDING )
        {
            if ( WaitForSingleObject( hEventToBeDuplicated, INFINITE )
                                                                == WAIT_FAILED )
            {
                CloseHandle( hEventToBeDuplicated );

                return( GetLastError() );
            }

            EnterCriticalSection( &(gblpInterfaceTable->CriticalSection) );

            if ( ( pIfObject = IfObjectGetPointer((HANDLE)hDimInterface) )
                                                                    == NULL )
            {
                dwRetCode = ERROR_INVALID_HANDLE;
            }
            else
            {
                dwRetCode = pIfObject->dwLastError;
            }

            LeaveCriticalSection( &(gblpInterfaceTable->CriticalSection) );
        }

        if ( hEventToBeDuplicated != NULL )
        {
            CloseHandle( hEventToBeDuplicated );
        }
    }

    return( dwRetCode );
}

//**
//
// Call:        DDMAdminInterfaceDisconnect
//
// Returns:     NO_ERROR - Success
//
// Description:
//
DWORD
DDMAdminInterfaceDisconnect(
    IN      HANDLE      hDimInterface
)
{
    DWORD   dwRetCode           = NO_ERROR;
    DWORD   dwTransportIndex    = -1;

    if ( gblDDMConfigInfo.dwNumRouterManagers > 0 )
    {
        for ( dwTransportIndex = 0;
              dwTransportIndex < gblDDMConfigInfo.dwNumRouterManagers;
              dwTransportIndex++ )
        {
            dwRetCode =
              DDMDisconnectInterface(
                 hDimInterface,
                 gblRouterManagers[dwTransportIndex].DdmRouterIf.dwProtocolId );

            if ( dwRetCode != NO_ERROR )
            {
                return( dwRetCode );
            }
        }
    }
    else
    {
        //
        // [old comment] If no router managers are installed then we are a AMB 
        // or NBF only client connection, simply call disconnect interface
        //

        // [new comment]
        //
        // AMB and NBF have been removed from the project but this path is 
        // being kept since logically, you should be able to disconnect an
        // interface regardless of whether any router managers exist.
        // 
        // This philosphy is in spirit with the work we'll do 
        // to merge rasman, dim, and ddm.  Then it will be possible for 
        // code paths like this to execute without any router managers being
        // loaded.
        //

        dwRetCode =  DDMDisconnectInterface( hDimInterface, -1 );
    }

    return( dwRetCode );
}

//**
//
// Call:        DDMAdminServerGetInfo
//
// Returns:     NO_ERROR - Success
//
// Description:
//
DWORD
DDMAdminServerGetInfo(
    IN OUT  PVOID  pServerInfo,
    IN      DWORD  dwLevel
)
{
    MPR_SERVER_0* pServerInfo0;

    if ( dwLevel == 0 )
    {
        pServerInfo0 = (MPR_SERVER_0*)pServerInfo;

        pServerInfo0->fLanOnlyMode = FALSE;
    }
    else
    {
        return( ERROR_NOT_SUPPORTED );
    }

    EnterCriticalSection( &(gblDeviceTable.CriticalSection) );

    //
    // Copy server info
    //

    pServerInfo0->dwTotalPorts = gblDeviceTable.NumDeviceNodes;
    pServerInfo0->dwPortsInUse = gblDeviceTable.NumDevicesInUse;

    LeaveCriticalSection( &(gblDeviceTable.CriticalSection) );

    return( NO_ERROR );
}

//**
//
// Call:        DDMAdminConnectionEnum
//
// Returns:     NO_ERROR - Success
//
// Description:
//
DWORD
DDMAdminConnectionEnum(
    IN OUT  PDIM_INFORMATION_CONTAINER  pInfoStruct,
    IN      DWORD                       dwLevel,
    IN      DWORD                       dwPreferedMaximumLength,
    IN      LPDWORD                     lpdwEntriesRead,
    IN      LPDWORD                     lpdwTotalEntries,
    IN OUT  LPDWORD                     lpdwResumeHandle    OPTIONAL
)
{
    PRASI_CONNECTION_0   pRasConnection0 = NULL;
    PRASI_CONNECTION_1   pRasConnection1 = NULL;
    PRASI_CONNECTION_2   pRasConnection2 = NULL;
    PCONNECTION_OBJECT  pConnObj        = NULL;
    DWORD               dwBucketIndex   = 0;
    DWORD               dwConnObjIndex  = 0;
    DWORD               dwConnInfoSize  = 0;
    DWORD               dwStartIndex    = ( lpdwResumeHandle == NULL )
                                          ? 0
                                          : *lpdwResumeHandle;

    // Calculate the connection info size
    switch (dwLevel) {
        case 0:
            dwConnInfoSize = sizeof( RASI_CONNECTION_0 );
            break;
        case 1:
            dwConnInfoSize = sizeof( RASI_CONNECTION_1 );
            break;
        case 2:
            dwConnInfoSize = sizeof( RASI_CONNECTION_2 );
            break;
        default:
            return ERROR_NOT_SUPPORTED;
    }

    EnterCriticalSection( &(gblDeviceTable.CriticalSection) );

    if ( gblDeviceTable.NumConnectionNodes < dwStartIndex )
    {
        LeaveCriticalSection( &(gblDeviceTable.CriticalSection) );

        return( ERROR_NO_MORE_ITEMS );
    }

    *lpdwTotalEntries = gblDeviceTable.NumConnectionNodes - dwStartIndex;

    if ( dwPreferedMaximumLength != -1 )
    {
        *lpdwEntriesRead = dwPreferedMaximumLength / dwConnInfoSize;

        if ( *lpdwEntriesRead > *lpdwTotalEntries )
        {
            *lpdwEntriesRead = *lpdwTotalEntries;
        }
    }
    else
    {
        *lpdwEntriesRead = *lpdwTotalEntries;
    }

    pInfoStruct->dwBufferSize = *lpdwEntriesRead * dwConnInfoSize;
    pInfoStruct->pBuffer = MIDL_user_allocate( pInfoStruct->dwBufferSize );

    if ( pInfoStruct->pBuffer == NULL )
    {
        LeaveCriticalSection( &(gblDeviceTable.CriticalSection) );

        pInfoStruct->dwBufferSize = 0;

        return( ERROR_NOT_ENOUGH_MEMORY );
    }

    if (dwLevel == 0)
        pRasConnection0 = (PRASI_CONNECTION_0)pInfoStruct->pBuffer;
    else if (dwLevel == 1)
        pRasConnection1 = (PRASI_CONNECTION_1)pInfoStruct->pBuffer;
    else
        pRasConnection2 = (PRASI_CONNECTION_2)pInfoStruct->pBuffer;

    for ( dwBucketIndex = 0;
          dwBucketIndex < gblDeviceTable.NumDeviceBuckets;
          dwBucketIndex++ )
    {
        for( pConnObj = gblDeviceTable.ConnectionBucket[dwBucketIndex];
             pConnObj != (CONNECTION_OBJECT *)NULL;
             pConnObj = pConnObj->pNext )
        {
            //
            // Check if this connection object is within the range we need to
            // copy from.
            //

            if ( ( dwConnObjIndex >= dwStartIndex ) &&
                 ( dwConnObjIndex < (dwStartIndex+*lpdwEntriesRead)))
            {
                //
                // Copy the info
                //

                if (dwLevel == 0) {
                    GetRasiConnection0Data( pConnObj, pRasConnection0 );
                    pRasConnection0++;
                }
                else if (dwLevel == 1) {
                    GetRasiConnection1Data( pConnObj, pRasConnection1 );
                    pRasConnection1++;
                }
                else {
                    GetRasiConnection2Data( pConnObj, pRasConnection2 );
                    pRasConnection2++;
                }

            }
            else if (dwConnObjIndex>=(dwStartIndex+*lpdwEntriesRead))
            {
                //
                // Beyond the range so exit
                //

                if ( lpdwResumeHandle != NULL )
                {
                    *lpdwResumeHandle = dwConnObjIndex;
                }

                LeaveCriticalSection( &(gblDeviceTable.CriticalSection) );

                return( ERROR_MORE_DATA );
            }

            dwConnObjIndex++;
        }
    }

    LeaveCriticalSection( &(gblDeviceTable.CriticalSection) );

    return( NO_ERROR );
}

//**
//
// Call:        DDMAdminConnectionGetInfo
//
// Returns:     NO_ERROR - Success
//
// Description:
//
DWORD
DDMAdminConnectionGetInfo(
    IN      HANDLE                      hConnection,
    IN OUT  PDIM_INFORMATION_CONTAINER  pInfoStruct,
    IN      DWORD                       dwLevel
)
{
    DWORD                       dwRetCode = NO_ERROR;
    ROUTER_INTERFACE_OBJECT *   pIfObject;
    CONNECTION_OBJECT *         pConnObj;

    if ( dwLevel > 2 )
    {
        return( ERROR_NOT_SUPPORTED );
    }

    EnterCriticalSection( &(gblDeviceTable.CriticalSection) );

    switch( dwLevel )
    {
    case 0:

        pInfoStruct->dwBufferSize = sizeof( RASI_CONNECTION_0 );
        break;

    case 1:

        pInfoStruct->dwBufferSize = sizeof( RASI_CONNECTION_1 );
        break;

    case 2:

        pInfoStruct->dwBufferSize = sizeof( RASI_CONNECTION_2 );
        break;
    }

    pInfoStruct->pBuffer = MIDL_user_allocate( pInfoStruct->dwBufferSize );

    if ( pInfoStruct->pBuffer == NULL )
    {
        LeaveCriticalSection( &(gblDeviceTable.CriticalSection) );

        return( ERROR_NOT_ENOUGH_MEMORY );
    }

    //
    // Copy Connection info
    //

    do
    {
        pConnObj = ConnObjGetPointer( (HCONN)hConnection );

        if ( pConnObj == (CONNECTION_OBJECT *)NULL )
        {
            dwRetCode = ERROR_INTERFACE_NOT_CONNECTED;

            break;
        }

        switch( dwLevel )
        {
        case 0:

            dwRetCode = GetRasiConnection0Data(
                                    pConnObj,
                                    (PRASI_CONNECTION_0)pInfoStruct->pBuffer );
            break;

        case 1:

            dwRetCode = GetRasiConnection1Data(
                                    pConnObj,
                                    (PRASI_CONNECTION_1)pInfoStruct->pBuffer );
            break;

        case 2:

            dwRetCode = GetRasiConnection2Data(
                                    pConnObj,
                                    (PRASI_CONNECTION_2)pInfoStruct->pBuffer );
            break;
        }


    }while( FALSE );

    if ( dwRetCode != NO_ERROR )
    {
        MIDL_user_free( pInfoStruct->pBuffer );

        pInfoStruct->pBuffer = NULL;

        pInfoStruct->dwBufferSize = 0;
    }

    LeaveCriticalSection( &(gblDeviceTable.CriticalSection) );

    return( dwRetCode );
}

//**
//
// Call:        DDMAdminConnectionClearStats
//
// Returns:     NO_ERROR - Success
//
// Description:
//
DWORD
DDMAdminConnectionClearStats(
    IN      HANDLE              hConnection
)
{
    return( RasBundleClearStatisticsEx(NULL, (HCONN)hConnection ) );
}

//**
//
// Call:        DDMAdminPortEnum
//
// Returns:     NO_ERROR - Success
//
// Description:
//
DWORD
DDMAdminPortEnum(
    IN OUT  PDIM_INFORMATION_CONTAINER  pInfoStruct,
    IN      HANDLE                      hConnection,
    IN      DWORD                       dwLevel,
    IN      DWORD                       dwPreferedMaximumLength,
    IN      LPDWORD                     lpdwEntriesRead,
    IN      LPDWORD                     lpdwTotalEntries,
    IN OUT  LPDWORD                     lpdwResumeHandle    OPTIONAL
)
{
    PRASI_PORT_0        pRasPort0       = NULL;
    PDEVICE_OBJECT      pDevObj         = NULL;
    PCONNECTION_OBJECT  pConnObj        = NULL;
    DWORD               dwIndex         = 0;
    DWORD               dwBucketIndex   = 0;
    DWORD               dwDevObjIndex   = 0;
    DWORD               dwStartIndex    = ( lpdwResumeHandle == NULL )
                                            ? 0
                                            : *lpdwResumeHandle;

    if ( dwLevel != 0 )
    {
        return( ERROR_NOT_SUPPORTED );
    }

    EnterCriticalSection( &(gblDeviceTable.CriticalSection) );

    if ( hConnection != INVALID_HANDLE_VALUE )
    {
        if ( ( pConnObj = ConnObjGetPointer( (HCONN)hConnection ) ) == NULL )
        {
            LeaveCriticalSection( &(gblDeviceTable.CriticalSection) );

            return( ERROR_INVALID_HANDLE );
        }

        if ( pConnObj->cActiveDevices < dwStartIndex )
        {
            LeaveCriticalSection( &(gblDeviceTable.CriticalSection) );

            return( ERROR_NO_MORE_ITEMS );
        }

        *lpdwTotalEntries = pConnObj->cActiveDevices - dwStartIndex;
    }
    else
    {
        if ( gblDeviceTable.NumDeviceNodes < dwStartIndex )
        {
            LeaveCriticalSection( &(gblDeviceTable.CriticalSection) );

            return( ERROR_NO_MORE_ITEMS );
        }

        *lpdwTotalEntries = gblDeviceTable.NumDeviceNodes - dwStartIndex;
    }

    if ( dwPreferedMaximumLength != -1 )
    {
        *lpdwEntriesRead = dwPreferedMaximumLength / sizeof( RAS_PORT_0 );

        if ( *lpdwEntriesRead > *lpdwTotalEntries )
        {
            *lpdwEntriesRead = *lpdwTotalEntries;
        }
    }
    else
    {
        *lpdwEntriesRead = *lpdwTotalEntries;
    }

    pInfoStruct->dwBufferSize = *lpdwEntriesRead * sizeof( RASI_PORT_0 );
    pInfoStruct->pBuffer      = MIDL_user_allocate( pInfoStruct->dwBufferSize );

    if ( pInfoStruct->pBuffer == NULL )
    {
        LeaveCriticalSection( &(gblDeviceTable.CriticalSection) );

        pInfoStruct->dwBufferSize = 0;

        return( ERROR_NOT_ENOUGH_MEMORY );
    }

    pRasPort0 = (PRASI_PORT_0)pInfoStruct->pBuffer;

    if ( hConnection == INVALID_HANDLE_VALUE )
    {
        for ( dwBucketIndex = 0;
              dwBucketIndex < gblDeviceTable.NumDeviceBuckets;
              dwBucketIndex++ )
        {
            for( pDevObj = gblDeviceTable.DeviceBucket[dwBucketIndex];
                 pDevObj != (DEVICE_OBJECT *)NULL;
                 pDevObj = pDevObj->pNext )
            {
                //
                // Check if this port is within the range we need to copy
                // from.
                //

                if ( ( dwDevObjIndex >= dwStartIndex ) &&
                     ( dwDevObjIndex < (dwStartIndex+*lpdwEntriesRead)))
                {
                    //
                    // Copy the info
                    //

                    GetRasiPort0Data( pDevObj, pRasPort0 );

                    pRasPort0++;
                }
                else if (dwDevObjIndex>=(dwStartIndex+*lpdwEntriesRead))
                {
                    //
                    // Beyond the range so exit
                    //

                    if ( lpdwResumeHandle != NULL )
                    {
                        *lpdwResumeHandle = dwDevObjIndex;
                    }

                    LeaveCriticalSection( &(gblDeviceTable.CriticalSection) );

                    return( ERROR_MORE_DATA );
                }

                dwDevObjIndex++;
            }
        }
    }
    else
    {
        for ( dwIndex = 0; dwIndex < pConnObj->cDeviceListSize; dwIndex++ )
        {
            if ( pConnObj->pDeviceList[dwIndex] != NULL )
            {
                //
                // Check if this port is within the range we need to copy
                // from.
                //

                if ( ( dwDevObjIndex >= dwStartIndex ) &&
                     ( dwDevObjIndex < (dwStartIndex+*lpdwEntriesRead)))
                {
                    //
                    // Copy the info
                    //

                    GetRasiPort0Data(pConnObj->pDeviceList[dwIndex], pRasPort0);

                    pRasPort0++;
                }
                else if (dwDevObjIndex>=(dwStartIndex+*lpdwEntriesRead))
                {
                    //
                    // Beyond the range so exit
                    //

                    if ( lpdwResumeHandle != NULL )
                    {
                        *lpdwResumeHandle = dwDevObjIndex;
                    }

                    LeaveCriticalSection( &(gblDeviceTable.CriticalSection) );

                    return( NO_ERROR );
                }

                dwDevObjIndex++;
            }
        }
    }

    LeaveCriticalSection( &(gblDeviceTable.CriticalSection) );

    return( NO_ERROR );
}

//**
//
// Call:        DDMAdminPortGetInfo
//
// Returns:     NO_ERROR - Success
//
// Description:
//
DWORD
DDMAdminPortGetInfo(
    IN      HANDLE                      hPort,
    IN OUT  PDIM_INFORMATION_CONTAINER  pInfoStruct,
    IN      DWORD                       dwLevel
)
{
    DEVICE_OBJECT * pDevObj;
    DWORD           dwRetCode;

    if ( dwLevel > 1 )
    {
        return( ERROR_NOT_SUPPORTED );
    }

    EnterCriticalSection( &(gblDeviceTable.CriticalSection) );

    pInfoStruct->dwBufferSize = ( dwLevel == 0 )
                                ? sizeof( RAS_PORT_0 )
                                : sizeof( RAS_PORT_1 );

    pInfoStruct->pBuffer = MIDL_user_allocate( pInfoStruct->dwBufferSize );

    if ( pInfoStruct->pBuffer == NULL )
    {
        LeaveCriticalSection( &(gblDeviceTable.CriticalSection) );

        return( ERROR_NOT_ENOUGH_MEMORY );
    }

    //
    // Copy port info
    //

    do
    {
        pDevObj = DeviceObjGetPointer( (HPORT)hPort );

        if ( pDevObj == (HPORT)NULL )
        {
            dwRetCode = ERROR_INVALID_PORT_HANDLE;

            break;
        }

        if ( dwLevel == 0 )
        {
            dwRetCode = GetRasiPort0Data( pDevObj,
                                        (PRASI_PORT_0)pInfoStruct->pBuffer );
        }
        else
        {
            dwRetCode = GetRasiPort1Data( pDevObj,
                                        (PRASI_PORT_1)pInfoStruct->pBuffer );
        }
    }
    while( FALSE );

    if ( dwRetCode != NO_ERROR )
    {
        MIDL_user_free( pInfoStruct->pBuffer );

        pInfoStruct->pBuffer = NULL;

        pInfoStruct->dwBufferSize = 0;
    }

    LeaveCriticalSection( &(gblDeviceTable.CriticalSection) );

    return( dwRetCode );
}

//**
//
// Call:        DDMAdminPortClearStats
//
// Returns:     NO_ERROR - Success
//
// Description:
//
DWORD
DDMAdminPortClearStats(
    IN      HANDLE          hPort
)
{
    PDEVICE_OBJECT pDevObj = NULL;

    EnterCriticalSection( &(gblDeviceTable.CriticalSection) );

    if ( ( pDevObj = DeviceObjGetPointer( (HPORT)hPort ) ) == NULL )
    {
        LeaveCriticalSection( &(gblDeviceTable.CriticalSection) );

        return( ERROR_INVALID_HANDLE );
    }

    LeaveCriticalSection( &(gblDeviceTable.CriticalSection) );

    return( RasPortClearStatistics(NULL, (HPORT)hPort ) );
}

//**
//
// Call:        DDMAdminPortReset
//
// Returns:     NO_ERROR - Success
//
// Description:
//
DWORD
DDMAdminPortReset(
    IN      HANDLE          hPort
)
{
    return( NO_ERROR );
}

//**
//
// Call:        DDMAdminPortDisconnect
//
// Returns:     NO_ERROR - Success
//
// Description: Disconnect the client port.
//
DWORD
DDMAdminPortDisconnect(
    IN      HANDLE          hPort
)
{
    DEVICE_OBJECT * pDevObj;
    DWORD           dwRetCode = NO_ERROR;

    EnterCriticalSection( &(gblDeviceTable.CriticalSection) );

    do
    {
        if ( ( pDevObj = DeviceObjGetPointer( (HPORT)hPort ) ) == NULL )
        {
            dwRetCode = ERROR_INVALID_HANDLE;

            break;
        }

        if ( pDevObj->fFlags & DEV_OBJ_OPENED_FOR_DIALOUT )
        {
            RasApiCleanUpPort( pDevObj );
        }
        else
        {
            if ( pDevObj->fFlags & DEV_OBJ_PPP_IS_ACTIVE )
            {
                PppDdmStop( (HPORT)pDevObj->hPort, NO_ERROR );
            }
            else
            {
                DevStartClosing( pDevObj );
            }
        }
    }
    while( FALSE );

    LeaveCriticalSection( &(gblDeviceTable.CriticalSection) );

    return( dwRetCode );
}

//**
//
// Call:        DDMRegisterConnectionNotification
//
// Returns:     NO_ERROR         - Success
//              Non-zero returns - Failure
//
// Description: Will insert or remove and event from the notification list
//
DWORD
DDMRegisterConnectionNotification(
    IN BOOL     fRegister,
    IN HANDLE   hEventClient,
    IN HANDLE   hEventRouter
)
{
    DWORD                   dwRetCode           = NO_ERROR;
    NOTIFICATION_EVENT *    pNotificationEvent  = NULL;

    EnterCriticalSection( &(gblpInterfaceTable->CriticalSection) );

    if ( fRegister )
    {
        //
        // Insert event in notification list
        //

        pNotificationEvent = (NOTIFICATION_EVENT *)
                                        LOCAL_ALLOC(
                                                LPTR,
                                                sizeof(NOTIFICATION_EVENT) );
        if ( pNotificationEvent == NULL )
        {
            dwRetCode = GetLastError();
        }
        else
        {
            pNotificationEvent->hEventClient = hEventClient;
            pNotificationEvent->hEventRouter = hEventRouter;

            InsertHeadList(
                (LIST_ENTRY *)&(gblDDMConfigInfo.NotificationEventListHead),
                (LIST_ENTRY*)pNotificationEvent );
        }
    }
    else
    {
        //
        // Remove event from notification list
        //

        for( pNotificationEvent = (NOTIFICATION_EVENT *)
                            (gblDDMConfigInfo.NotificationEventListHead.Flink);
             pNotificationEvent != (NOTIFICATION_EVENT *)
                            &(gblDDMConfigInfo.NotificationEventListHead);
             pNotificationEvent = (NOTIFICATION_EVENT *)
                            (pNotificationEvent->ListEntry.Flink) )
        {
            if ( pNotificationEvent->hEventClient == hEventClient )
            {
                RemoveEntryList( (LIST_ENTRY *)pNotificationEvent );

                CloseHandle( pNotificationEvent->hEventClient );

                CloseHandle( pNotificationEvent->hEventRouter );

                LOCAL_FREE( pNotificationEvent );

                break;
            }
        }
    }

    LeaveCriticalSection( &(gblpInterfaceTable->CriticalSection) );

    return( dwRetCode );
}

//**
//
// Call:        DDMSendUserMessage
//
// Returns:     NO_ERROR - Success
//
// Description:
//
DWORD
DDMSendUserMessage(
    IN  HANDLE      hConnection,
    IN  LPWSTR      lpwszMessage
)
{

    PCONNECTION_OBJECT  pConnObj            = NULL;
    DWORD               dwRetCode           = NO_ERROR;

    EnterCriticalSection( &(gblDeviceTable.CriticalSection) );

    do
    {
        pConnObj = ConnObjGetPointer( (HCONN)hConnection );

        if ( pConnObj == (CONNECTION_OBJECT *)NULL )
        {
            dwRetCode = ERROR_INTERFACE_NOT_CONNECTED;

            break;
        }

        if ( pConnObj->fFlags & CONN_OBJ_MESSENGER_PRESENT )
        {
            WCHAR wszRemoteComputer[CNLEN+1];

            MultiByteToWideChar( CP_ACP,
                                 0,
                                 pConnObj->bComputerName,
                                 -1,
                                 wszRemoteComputer,
                                 CNLEN+1 );

            dwRetCode = NetMessageBufferSend(
                                NULL,
                                wszRemoteComputer,
                                NULL,
                                (BYTE*)lpwszMessage,
                                (wcslen(lpwszMessage)+1) * sizeof(WCHAR));
        }

    } while( FALSE );

    LeaveCriticalSection( &(gblDeviceTable.CriticalSection) );

    return(dwRetCode);
}

DWORD
DDMAdminRemoveQuarantine(
    IN HANDLE hConnection,
    IN BOOL fIsIpAddress)
{
    DWORD dwBucketIndex;
    CONNECTION_OBJECT *pConnObj = NULL;
    DWORD dwErr = ERROR_SUCCESS;
    BOOL fFound = FALSE;

    EnterCriticalSection(&gblDeviceTable.CriticalSection);

    if(fIsIpAddress)
    {
        for ( dwBucketIndex = 0;
              dwBucketIndex < gblDeviceTable.NumDeviceBuckets;
              dwBucketIndex++ )
        {
            for( pConnObj = gblDeviceTable.ConnectionBucket[dwBucketIndex];
                 pConnObj != (CONNECTION_OBJECT *)NULL;
                 pConnObj = pConnObj->pNext )
            {
                if(pConnObj->PppProjectionResult.ip.dwRemoteAddress == 
                                         HandleToUlong(hConnection))
                {
                    fFound = TRUE;
                    break;
                }
            }

            if(fFound)
            {
                break;
            }
        }
    }
    else
    {
        pConnObj = ConnObjGetPointer((HCONN) hConnection);
    }

    if(NULL != pConnObj)
    {
        //
        // If we have a valid connection object, Remove quarantine
        // on the connection object.
        //
        dwErr = RemoveQuarantineOnConnection(pConnObj);
    }
    else
    {
        dwErr = ERROR_INTERFACE_NOT_CONNECTED;
    }

    LeaveCriticalSection(&gblDeviceTable.CriticalSection);

    return dwErr;    

}
    