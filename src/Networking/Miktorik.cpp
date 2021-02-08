#include "Mikrotik.h"
#include "MikrotikCredentials.h"

#ifndef __LINUX_DBG
    #include "Platform.h"
    #include "RepRap.h"
	#include "GCodes/GCodeBuffer.h"
    #include "W5500Ethernet/Wiznet/Ethernet/socketlib.h"
	#include "Network.h"
#else
    #include <cstdio>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <cstring>
    #include <cstdlib>
#endif

static uint8_t isRouterAvailable;
static const char *IFACE_NAME_TABLE[] = { nullptr, IFACE_ETHERNET, IFACE_WIFI2G, IFACE_WIFI5G };
const char statusStr[][16] = { "Booting", "Connected", "Disconnected", "Connecting" };

#ifndef __LINUX_DBG
    #define ExecuteRequest()    isRequestWaiting = true;   \
                                while( isRequestWaiting ); \
                                __NOP();
#else
    #define ExecuteRequest ProcessRequest
    #define SafeSnprintf snprintf
    #define debugPrintf printf
    #define __send write
    #define __recv( sock, buf, size ) recv( sock, buf, size, 0 )
#endif


Mikrotik::Mikrotik() : isRequestWaiting(false)
{
    memset( answer, 0, 1024 );
    block = new MKTBlock();

    interface = none;
    status = Booting;
    mode = invalid;
    gateway[0] = ssid[0] = mask[0] = ip[0] = 0;
}


void Mikrotik::Spin()
{
    if ( isRequestWaiting )
    {
    	if(isRouterAvailable)
    	{
    		LoginRequest();
    	}
    	else
    	{
    		ProcessRequest();
    	}
        isRequestWaiting = false;
    }
}


bool Mikrotik::GetUpTime( char *buffer )
{
    // 1. PREPARE REQUEST
    const char *cmd     = CMD_SYSTEM_RESOURCE_PRINT;
    const char *cmdGrep = GREP_OPT( P_UPTIME );

    clear_sentence( &mkSentence );
    add_word_to_sentence( cmd,     &mkSentence );
    add_word_to_sentence( cmdGrep, &mkSentence );

    // 2. WAIT FOR EXECUTION
    ExecuteRequest();

    // 3. PROCESS ANSWER
    if ( !IsRequestSuccessful() )
        return false;

    // Parse answer
    if ( !parseAnswer( P_UPTIME ) )
    {
        SafeSnprintf( buffer, MIKROTIK_MAX_ANSWER, "no answer found" );
        return false;
    }

    strcpy( buffer, answer );
    return true;
}


// Use ehter1 interface with dynamic IP
bool Mikrotik::ConnectToEthernet()
{
    // Disable secondary interfaces
    if ( !DisableInterface( wifi2g ) )
        return false;

    if ( !DisableInterface( wifi5g ) )
        return false;

    // Prepare DHCP client
    if ( !SetDhcpState( ether1, DhcpClient, Enabled ) )
        return false;

    // Enable interface
    return EnableInterface( ether1 );
}


// https://wiki.mikrotik.com/wiki/Manual:Making_a_simple_wireless_AP
bool Mikrotik::CreateAP( const char *ssid, const char *pass, TInterface iface )
{
    // 0. PREPARE ROUTER CONFIGURATION
    // Only WiFi allowed here
    if ( iface <= ether1 )
        return false;

    if ( !SetDhcpState( ether1, DhcpClient, Disabled ) )
        return false;

    DisableInterface( ether1 );
    DisableInterface( iface == wifi5g ? wifi2g : wifi5g );

    if ( !SetDhcpState( iface, DhcpClient, Disabled ) )
        return false;

    if ( !SetDhcpState( iface, DhcpServer, Enabled ) )
        return false;

    const char ip2g[] = MIKROTIK_IP_WIFI_2G;
    const char ip5g[] = MIKROTIK_IP_WIFI_5G;

    const char *ip = ( iface == wifi2g ) ? ip2g : ip5g;
    if ( !SetStaticIP( iface, ip ) )
        return false;

    if ( pass != nullptr )
    {
        if ( !changeAccessPointPass( pass ) )
        {
            SafeSnprintf( answer, MIKROTIK_MAX_ANSWER, "can't configure security profile" );
            debugPrintf( "ERROR: %s\n", answer );
            return false;
        }
    }

    // 1. PREPARE REQUEST
    char wlanID[20];
    SafeSnprintf( wlanID, sizeof( wlanID ), SET_PARAM( P_ID ) "%s", IFACE_NAME_TABLE[iface] );

    char apName[MIKROTIK_MAX_ANSWER];
    SafeSnprintf( apName, sizeof( apName ), SET_PARAM( P_SSID ) "%s", ssid );

    const char *cmd[] = { CMD_INTERFACE_WIRELESS_SET,
                          SET_PARAM_V( P_FREQUENCY, V_AUTO ),
                          SET_PARAM_V( P_MODE, V_AP_BRIDGE),
                          SET_PARAM_V( P_DISABLED, V_FALSE )
                        };

    clear_sentence( &mkSentence );
    add_word_to_sentence( cmd[0], &mkSentence );
    add_word_to_sentence( wlanID, &mkSentence );
    add_word_to_sentence( apName, &mkSentence );
    add_word_to_sentence( iface == wifi5g ? SET_PARAM_V( P_FREQUENCY, V_5180MHz ) : cmd[1], &mkSentence );
    add_word_to_sentence( cmd[2], &mkSentence );
    add_word_to_sentence( cmd[3], &mkSentence );

    char sp[50] = SET_PARAM( P_SECURITY_PROFILE );
    if ( pass == nullptr )
        strcat( sp, SP_DEFAULT );
    else
        strcat( sp, SP_MODE_ACCESS_POINT );

    add_word_to_sentence( sp, &mkSentence );

    // 2. WAIT FOR EXECUTION
    ExecuteRequest();

    // 3. PROCESS ANSWER
    return IsRequestSuccessful();
}


// https://wiki.mikrotik.com/wiki/Manual:Wireless_AP_Client#Additional_Station_Configuration
bool Mikrotik::ConnectToWiFi( const char *ssid, const char *pass, TInterface iface )
{
    // 0. PRECONFIG ROUTER
    // Only WiFi allowed here
    if ( iface <= ether1 )
        return false;

    DisableInterface( ether1 );
    DisableInterface( iface == wifi5g ? wifi2g : wifi5g );

    if ( !SetDhcpState( iface, DhcpServer, Disabled ) )
        return false;

    if ( !SetDhcpState( iface, DhcpClient, Enabled ) )
        return false;

    if ( pass != nullptr )
    {
        if ( !changeWiFiStationPass( pass ) )
        {
            SafeSnprintf( answer, MIKROTIK_MAX_ANSWER, "can't configure security profile" );
            debugPrintf( "ERROR: %s\n", answer );
            return false;
        }
    }

    // 1. PREPARE REQUEST
    char wlanID[20];
    SafeSnprintf( wlanID, sizeof( wlanID ), SET_PARAM( P_ID ) "%s", IFACE_NAME_TABLE[iface] );

    char networkName[MIKROTIK_MAX_ANSWER];
    SafeSnprintf( networkName, sizeof( networkName ), SET_PARAM( P_SSID ) "%s", ssid );

    const char *cmd[] = { CMD_INTERFACE_WIRELESS_SET,
                          SET_PARAM_V( P_MODE, V_STATION ),
                          SET_PARAM_V( P_DISABLED, V_FALSE )
                        };

    clear_sentence( &mkSentence );
    add_word_to_sentence( cmd[0],      &mkSentence );
    add_word_to_sentence( wlanID,      &mkSentence );
    add_word_to_sentence( networkName, &mkSentence );
    add_word_to_sentence( cmd[1],      &mkSentence );
    add_word_to_sentence( cmd[2],      &mkSentence );

    char sp[50] = SET_PARAM( P_SECURITY_PROFILE );
    if ( pass == nullptr )
        strcat( sp, SP_DEFAULT );
    else
        strcat( sp, SP_MODE_STATION );

    add_word_to_sentence( sp, &mkSentence );

    // 2. WAIT FOR EXECUTION
    ExecuteRequest();

    // 3. PROCESS ANSWER
    return IsRequestSuccessful();
}


bool Mikrotik::EnableInterface( TInterface iface )
{
    if ( !iface )
        return false;

    // 1. PREPARE REQUEST
    const char *cmdWiFi = CMD_INTERFACE_WIRELESS_ENABLE;
    const char *cmdEht  = CMD_INTERFACE_ETHERNET_ENABLE;

    const char *cmd = ( iface == ether1 ) ? cmdEht : cmdWiFi;

    char id[15];
    SafeSnprintf( id, sizeof( id ), SET_PARAM( P_ID ) "%s", IFACE_NAME_TABLE[iface] );

    clear_sentence( &mkSentence );
    add_word_to_sentence( cmd, &mkSentence );
    add_word_to_sentence( id,  &mkSentence );

    // 2. WAIT FOR EXECUTION
    ExecuteRequest();

    // 3. PROCESS ANSWER
    return IsRequestSuccessful();
}

bool Mikrotik::SetGateway( const char *gateway )
{
	// 1. PREPARE REQUEST
	const char *cmd = CMD_IP_ROUTE_ADD;

	char addr1[28], addr2[28];
	SafeSnprintf( addr1, sizeof( addr1 ), SET_PARAM( P_DST_ADDRESS ) "%s", MIKROTIK_DST_ADDRESS );
	SafeSnprintf( addr2, sizeof( addr2 ), SET_PARAM( P_GATEWAY ) "%s", gateway );

    clear_sentence( &mkSentence );
    add_word_to_sentence( cmd,  &mkSentence );
    add_word_to_sentence( addr1,  &mkSentence );
    add_word_to_sentence( addr2,  &mkSentence );

    // 2. WAIT FOR EXECUTION
    notResponse = true;
    ExecuteRequest();
    notResponse = false;

    // 3. PROCESS ANSWER
    return IsRequestSuccessful();
}

bool Mikrotik::RefreshGateway()
{
	// 1. PREPARE REQUEST
	const char *cmd = CMD_IP_ROUTE_PRINT;

    clear_sentence( &mkSentence );
    add_word_to_sentence( cmd,  &mkSentence );

    // 2. WAIT FOR EXECUTION
    ExecuteRequest();

    // 3. PROCESS ANSWER
	if ( !IsRequestSuccessful() )
		return false;

	// Extract security profile ID from answer
	bool success = parseAnswer( P_ID );
	if ( success )
		strcpy( gatewayId, answer );

	return success;
}

bool Mikrotik::RemoveGateway()
{
	// 1. PREPARE REQUEST
	const char *cmd = CMD_IP_ROUTE_REMOVE;

	char addr[28];
	SafeSnprintf( addr, sizeof( addr ), SET_PARAM( P_ID ) "%s", gatewayId );
	debugPrintf("Frame: %s\n", gatewayId);

    clear_sentence( &mkSentence );
    add_word_to_sentence( cmd,  &mkSentence );
    add_word_to_sentence( addr,  &mkSentence );

    // 2. WAIT FOR EXECUTION
    ExecuteRequest();

    // 3. PROCESS ANSWER
    return IsRequestSuccessful();
}


bool Mikrotik::DisableInterface( TInterface iface )
{
    if ( !iface )
        return false;

    // 0. Disable secondary iface services
    if ( !SetDhcpState( iface, DhcpClient, Disabled ) )
        return false;

    if ( !RemoveStaticIP( iface ) )
        return false;

    if ( iface != ether1 )  // ether1 not supports dhcp-server mode
        if ( !SetDhcpState( iface, DhcpServer, Disabled ) )
            return false;

    // 1. PREPARE REQUEST
    const char *cmdWiFi = CMD_INTERFACE_WIRELESS_DISABLE;
    const char *cmdEht  = CMD_INTERFACE_ETHERNET_DISABLE;

    const char *cmd = ( iface == ether1 ) ? cmdEht : cmdWiFi;

    char id[15];
    SafeSnprintf( id, sizeof( id ), SET_PARAM( P_ID ) "%s", IFACE_NAME_TABLE[iface] );

    clear_sentence( &mkSentence );
    add_word_to_sentence( cmd, &mkSentence );
    add_word_to_sentence( id,  &mkSentence );

    // 2. WAIT FOR EXECUTION
    ExecuteRequest();

    // 3. PROCESS ANSWER
    return IsRequestSuccessful();
}


bool Mikrotik::GetCurrentInterface( TInterface *iface )
{
    if ( isInterfaceActive( ether1 ) )
        *iface = ether1;
    else if ( isInterfaceActive( wifi2g ) )
        *iface = wifi2g;
    else if ( isInterfaceActive( wifi5g ) )
        *iface = wifi5g;
    else
        return false;

    return true;
}


bool Mikrotik::GetWifiMode( TInterface iface, TWifiMode *pMode )
{
    // ether1 is not wifi iface
    if ( iface <= ether1 )
        return false;

    // 1. PREPARE REQUEST
    char cmdIface[50] = { 0 };
    SafeSnprintf( cmdIface, sizeof( cmdIface ), REQ_PARAM( P_NAME ) "%s", IFACE_NAME_TABLE[iface] );

    const char *cmd         = CMD_INTERFACE_WIRELESS_PRINT;
    const char cmdEnabled[] = REQ_PARAM_V( P_DISABLED, V_FALSE );
    const char cmdOpt[]     = GREP_OPT( P_MODE );

    clear_sentence( &mkSentence );
    add_word_to_sentence( cmd,        &mkSentence );
    add_word_to_sentence( cmdIface,   &mkSentence );
    add_word_to_sentence( cmdEnabled, &mkSentence );
    add_word_to_sentence( cmdOpt,     &mkSentence );

    // 2. WAIT FOR EXECUTION
    ExecuteRequest();

    // 3. PROCESS ANSWER
    if ( !IsRequestSuccessful() )
        return false;  // ERROR

    if ( !parseAnswer( P_MODE ) )
        return false;  // ERROR

    const char modeAP[]     = V_AP_BRIDGE;
    const char modeClient[] = V_STATION;

    if ( strstr( answer, modeAP ) != nullptr )
        *pMode = AccessPoint;
    else if ( strstr( answer, modeClient ) != nullptr )
        *pMode = Station;
    else
        *pMode = invalid;

    return true;
}


bool Mikrotik::GetSSID( TInterface iface, char *ssid )
{
    // ether1 is not wifi iface
    if ( iface <= ether1 )
        return false;

    // 1. PREPARE REQUEST
    char cmdIface[50] = { 0 };
    SafeSnprintf( cmdIface, sizeof( cmdIface ), REQ_PARAM( P_NAME ) "%s", IFACE_NAME_TABLE[iface] );

    const char *cmd     = CMD_INTERFACE_WIRELESS_PRINT;
    const char cmdOpt[] = GREP_OPT( P_SSID );

    clear_sentence( &mkSentence );
    add_word_to_sentence( cmd,        &mkSentence );
    add_word_to_sentence( cmdIface,   &mkSentence );
    add_word_to_sentence( cmdOpt,     &mkSentence );

    // 2. WAIT FOR EXECUTION
    ExecuteRequest();

    // 3. PROCESS ANSWER
    if ( !IsRequestSuccessful() )
        return false;  // ERROR

    if ( !parseAnswer( P_SSID ) )
        return false;  // ERROR

    strcpy( ssid, answer );
    return true;
}


/**
 * Returns "true" when successfully connected to WiFi or ethernet network
 *
 * In case of "Access point" mode returns "true" when at least one client connected
 */
bool Mikrotik::IsNetworkAvailable( TInterface iface )
{
    if ( !iface )
        return false;

    // 1. PREPARE REQUEST
    char cmdIface[50] = { 0 };
    SafeSnprintf( cmdIface, sizeof( cmdIface ), REQ_PARAM( P_NAME ) "%s", IFACE_NAME_TABLE[iface] );

    const char *cmdWiFi = CMD_INTERFACE_WIRELESS_PRINT;
    const char *cmdEht  = CMD_INTERFACE_ETHERNET_PRINT;

    const char *cmd = ( iface == ether1 ) ? cmdEht : cmdWiFi;
    const char cmdEnabled[] = REQ_PARAM_V( P_DISABLED, V_FALSE );
    const char cmdOpt[]     = GREP_OPT( P_RUNNING );

    clear_sentence( &mkSentence );
    add_word_to_sentence( cmd,        &mkSentence );
    add_word_to_sentence( cmdIface,   &mkSentence );
    add_word_to_sentence( cmdEnabled, &mkSentence );
    add_word_to_sentence( cmdOpt,     &mkSentence );

    // 2. WAIT FOR EXECUTION
    ExecuteRequest();

    // 3. PROCESS ANSWER
    if ( !IsRequestSuccessful() )
        return false;  // ERROR

    if ( !parseAnswer( P_RUNNING ) )
        return false;  // ERROR

    return ( strstr( answer, V_TRUE ) != nullptr );
}


//! Router has few preconfigured DHCP-client params for ethernet and wifi ifaces
bool Mikrotik::SetDhcpState( TInterface iface, TDhcpMode dhcpMode, TEnableState state )
{
    if ( !iface )
        return false;

    // ether1 can be client only
    if ( ( iface == ether1 ) && ( dhcpMode == DhcpServer ) )
        return false;

    if ( ( dhcpMode == DhcpClient ) && ( state == Enabled ) )
        RemoveStaticIP( iface );

    char id[10] = { 0 };
    if ( !getDhcpID( id, iface, dhcpMode ) )
        return false;

    // 1. PREPARE REQUEST;
    const char *cmdServer = CMD_IP_DHCP_SERVER_SET;
    const char *cmdClient = CMD_IP_DHCP_CLIENT_SET;

    char cmdWlanId[30];
    SafeSnprintf( cmdWlanId, sizeof( cmdWlanId ), SET_PARAM( P_ID ) "%s", id );

    char cmdState[30];
    SafeSnprintf( cmdState, sizeof( cmdState ), SET_PARAM( P_DISABLED ) "%s", state == Enabled ? V_FALSE : V_TRUE );

    clear_sentence( &mkSentence );
    add_word_to_sentence( dhcpMode == DhcpServer ? cmdServer : cmdClient, &mkSentence );
    add_word_to_sentence( cmdWlanId, &mkSentence );
    add_word_to_sentence( cmdState,  &mkSentence );

    // 2. WAIT FOR EXECUTION
    ExecuteRequest();

    // 3. PROCESS ANSWER
    return IsRequestSuccessful();
}


bool Mikrotik::GetDhcpState( TInterface iface, TDhcpMode dhcpMode, TEnableState *pState )
{
    if ( !iface )
        return false;

    // ether1 can be client only
    if ( ( iface == ether1 ) && ( dhcpMode == DhcpServer ) )
        return false;

    // 1. PREPARE REQUEST
    const char *cmdServer = CMD_IP_DHCP_SERVER_PRINT;
    const char *cmdClient = CMD_IP_DHCP_CLIENT_PRINT;

    char cmdWlan[30];
    SafeSnprintf( cmdWlan, sizeof( cmdWlan ), REQ_PARAM( P_INTERFACE ) "%s", IFACE_NAME_TABLE[iface] );

    const char *cmdGrep = GREP_OPT( P_DISABLED );

    clear_sentence( &mkSentence );
    add_word_to_sentence( dhcpMode == DhcpServer ? cmdServer : cmdClient, &mkSentence );
    add_word_to_sentence( cmdWlan, &mkSentence );
    add_word_to_sentence( cmdGrep, &mkSentence );

    // 2. WAIT FOR EXECUTION
    ExecuteRequest();

    // 3. PROCESS ANSWER
    if ( !IsRequestSuccessful() )
        return false;

    if ( !parseAnswer( P_DISABLED ) )
        return false;

    if ( strstr( answer, V_FALSE ) )
        *pState = Enabled;
    else if ( strstr( answer, V_TRUE ) )
        *pState = Disabled;
    else
        return false;

    return true;
}


bool Mikrotik::GetInterfaceIP( TInterface iface, char *ip )
{
    if ( !iface )
        return false;

    if ( GetInterfaceIP( iface, ip, true ) )
        return true;

    return ( GetInterfaceIP( iface, ip, false ) );
}


bool Mikrotik::GetInterfaceIP( TInterface iface, char *ip, bool isStatic )
{
    if ( !iface )
        return false;

    // 1. PREPARE REQUEST
    char cmdIface[20];
    SafeSnprintf( cmdIface, sizeof( cmdIface ), REQ_PARAM( P_INTERFACE ) "%s", IFACE_NAME_TABLE[iface] );

    const char *cmd        = CMD_IP_ADDRESS_PRINT;
    const char *cmdEnabled = REQ_PARAM_V( P_DISABLED, V_FALSE );
    const char *cmdGrep    = GREP_OPT( P_ADDRESS );

    const char *cmdDynamic = REQ_PARAM_V( P_DYNAMIC, V_TRUE );
    const char *cmdStatic  = REQ_PARAM_V( P_DYNAMIC, V_FALSE );

    clear_sentence( &mkSentence );
    add_word_to_sentence( cmd, &mkSentence );
    add_word_to_sentence( cmdIface, &mkSentence );
    add_word_to_sentence( isStatic ? cmdStatic : cmdDynamic, &mkSentence );
    add_word_to_sentence( cmdEnabled, &mkSentence );
    add_word_to_sentence( cmdGrep, &mkSentence );

    // 2. WAIT FOR EXECUTION
    ExecuteRequest();

    // 3. PROCESS ANSWER
    if ( !IsRequestSuccessful() )
        return false;

    // Extract security profile ID from answer
    bool success = parseAnswer( P_ADDRESS );
    if ( success )
        strcpy( ip, answer );

    return success;
}


bool Mikrotik::SetStaticIP( TInterface iface, const char *ip )
{
    if ( !iface )
        return false;

    if ( !SetDhcpState( iface, DhcpClient, Disabled ) )
        return false;

    // Find interface static IP identifier
    char sipID[15] = { 0 };
    if ( !getStaticIpId( sipID, iface ) )
        return false;

    // 1. Prepare request
    char idCmd[25] = { 0 };
    SafeSnprintf( idCmd, sizeof( idCmd ), SET_PARAM( P_ID ) "%s", sipID );

    char addrCmd[MIKROTIK_MAX_ANSWER] = { 0 };
    SafeSnprintf( addrCmd, sizeof( addrCmd ), SET_PARAM( P_ADDRESS ) "%s", ip );

    const char *cmd      = CMD_IP_ADDRESS_SET;
    const char *stateCmd = SET_PARAM_V( P_DISABLED, V_FALSE );

    clear_sentence( &mkSentence );
    add_word_to_sentence( cmd,      &mkSentence );
    add_word_to_sentence( idCmd,    &mkSentence );
    add_word_to_sentence( addrCmd,  &mkSentence );
    add_word_to_sentence( stateCmd, &mkSentence );

    // 2. WAIT FOR EXECUTION
    ExecuteRequest();

    // 3. PROCESS ANSWER
    return IsRequestSuccessful();
}


bool Mikrotik::RemoveStaticIP( TInterface iface )
{
    if ( !iface )
        return false;

    // Find interface static IP identifier
    char sipID[30] = { 0 };
    if ( !getStaticIpId( sipID, iface ) )
        return false;

    // Prepare request
    const char *cmd = CMD_IP_ADDRESS_DISABLE;
    char id[50];
    SafeSnprintf( id, sizeof( id ), SET_PARAM( P_ID ) "%s", sipID );

    clear_sentence( &mkSentence );
    add_word_to_sentence( cmd, &mkSentence );
    add_word_to_sentence( id,  &mkSentence );

    // 2. WAIT FOR EXECUTION
    ExecuteRequest();

    // 3. CHECK ANSWER
    return IsRequestSuccessful();
}


static bool isStrInBuf( const char *pStr, const char *pBuf, uint32_t count )
{
    if ( !count )
        return false;

    const char *pNext = pBuf;

    while ( count-- )
    {
        if ( strcmp( pStr, pNext ) == 0 )
            return true;

        pNext += strlen( pNext ) + 1;
    }

    return false;
}


uint16_t Mikrotik::ScanWiFiNetworks( TInterface iface, uint8_t duration, char *pBuffer, uint32_t MAX_BUF_SIZE )
{
    if ( iface <= ether1 )
        return 0;

    // 1. PREPARE REQUEST
    char cmdIface[50] = { 0 };
    SafeSnprintf( cmdIface, sizeof( cmdIface ), SET_PARAM( P_ID ) "%s", IFACE_NAME_TABLE[iface] );

    char cmdDuration[50] = { 0 };
    SafeSnprintf( cmdDuration, sizeof( cmdDuration ), SET_PARAM( P_DURATION ) "%u", duration  );

    const char *cmd     = CMD_INTERFACE_WIRELESS_SCAN;
    const char cmdOpt[] = GREP_OPT( P_SSID );

    clear_sentence( &mkSentence );
    add_word_to_sentence( cmd,         &mkSentence );
    add_word_to_sentence( cmdIface,    &mkSentence );
    add_word_to_sentence( cmdDuration, &mkSentence );
    add_word_to_sentence( cmdOpt,      &mkSentence );

    // 2. WAIT FOR EXECUTION
    ExecuteRequest();

    // 3. PROCESS ANSWER
    if ( !IsRequestSuccessful() )
        return 0;  // ERROR

    char key[MIKROTIK_MAX_ANSWER];
    SafeSnprintf( key, sizeof( key ), "=%s=", P_SSID );

    const char* pWord = block->GetFirstWord();
    char *pNext = pBuffer;
    uint16_t count  = 0;
    uint32_t length = 0;
    do
    {
        // Our search '=KEY=' should be located at the begin of word
        if ( strstr( pWord, key ) == pWord )
        {
            const char *pStr = &pWord[strlen(key)];
            if ( isStrInBuf( pStr, pBuffer, count ) )
            {
                pWord = block->GetNextWord( pWord );
                continue;
            }

            length += strlen( pStr ) + 1;
            if ( length >= MAX_BUF_SIZE )
                break;

            if (strlen(pStr) > 0)
            {
            	strcpy( pNext, pStr );
            	pNext += strlen( pStr ) + 1;
            	count++;
            }
        }

        pWord = block->GetNextWord( pWord );
    } while ( pWord );

    return count;
}


#ifndef __LINUX_DBG
bool Mikrotik::Connect( uint8_t *d_ip, uint16_t d_port )
{
    if ( isConnected )
        Disconnect();

    const uint16_t s_port  = 1234;
    const uint8_t protocol = Sn_MR_TCP;

    int se = __socket( MIKROTIK_SOCK_NUM, protocol, s_port, 0 );

    if ( se != MIKROTIK_SOCK_NUM )
    {
        debugPrintf( "Socket error %i\n", se );
        return false;
    }

    // Set keep-alive
    // Should be placed between socket() and connect()
    setSn_KPALVTR( MIKROTIK_SOCK_NUM, 2 );

    debugPrintf( "Connecting to: %d.%d.%d.%d:%d... ", d_ip[0], d_ip[1], d_ip[2], d_ip[3], d_port );
    int ce = __connect( MIKROTIK_SOCK_NUM, d_ip, d_port );

    if ( ce != SOCK_OK )
    {
        debugPrintf( "connection error %i\n", ce );
        __close( MIKROTIK_SOCK_NUM );
    }
    else
    {
        debugPrintf( "CONNECTED!\n" );
        isConnected = true;
    }
#else
bool Mikrotik::Connect( const char *d_ip, uint16_t d_port )
{
    if ( isConnected )
        Disconnect();

    int fdSock;
    struct sockaddr_in address{};
    int iConnectResult;
    int iLen;

    fdSock = socket( AF_INET, SOCK_STREAM, 0 );

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr( d_ip );
    address.sin_port = htons( d_port );
    iLen = sizeof( address );

    debugPrintf( "Connecting to %s\n", d_ip );

    iConnectResult = connect( fdSock, (struct sockaddr *) &address, iLen );

    if ( iConnectResult == -1 )
    {
        perror( "Connection problem" );
        exit( 1 );
    }
    else
    {
        debugPrintf( "Successfully connected to %s\n", d_ip );
    }

    // determine endianness of this machine
    // iLittleEndian will be set to 1 if we are
    // on a little endian machine...otherwise
    // we are assumed to be on a big endian processor
    iLittleEndian = isLittleEndian();

    MIKROTIK_SOCK_NUM = fdSock;
    debugPrintf( "CONNECTED!\n" );

    isConnected = true;
#endif

    return isConnected;
}


void Mikrotik::Disconnect()
{
#ifndef __LINUX_DBG
    __disconnect( MIKROTIK_SOCK_NUM );
    __close( MIKROTIK_SOCK_NUM );
#else
    close( MIKROTIK_SOCK_NUM );
#endif

    isConnected = false;
    isLogged    = false;

    debugPrintf( "\n***DISCONNECTED***\n\n" );
}


void Mikrotik::generateResponse( char *szMD5PasswordToSend, char *szMD5Challenge, const char *password )
{
    // convert szMD5Challenge to binary
    char szMD5ChallengeBinary[17] = { 0 };
    md5ToBinary( szMD5Challenge, szMD5ChallengeBinary );

    md5_state_t state;
    char cNull[1] = { 0 };
    md5_byte_t digest[16] = { 0 };

    // get md5 of the password + challenge concatenation
    md5_init( &state );
    md5_append( &state, (md5_byte_t *) cNull, 1 );
    md5_append( &state, (const md5_byte_t *) password, strlen( password ) );
    md5_append( &state, (const md5_byte_t *) szMD5ChallengeBinary, strlen( szMD5ChallengeBinary ) );
    md5_finish( &state, digest );

    // convert this digest to a string representation of the hex values
    // digest is the binary format of what we want to send
    // szMD5PasswordToSend is the "string" hex format
    md5DigestToHexString( digest, szMD5PasswordToSend );
}


/********************************************************************
 * Login to the API
 * true  is returned on successful login
 * false is returned on unsuccessful login
 ********************************************************************/
bool Mikrotik::try_to_log_in( char *username, char *password )
{
    TMKSentence sentence;
    clear_sentence( &sentence );
    add_word_to_sentence( CMD_LOGIN, &sentence );

    char name[100];
    SafeSnprintf( name, sizeof( name ), SET_PARAM( P_NAME ) "%s", username );
    add_word_to_sentence( name, &sentence );

    char pass[100];
    SafeSnprintf( pass, sizeof( pass ), SET_PARAM( P_PASSWORD ) "%s", password );
    add_word_to_sentence( pass, &sentence );

    write_sentence( &sentence );

    block->ReInit();
    read_sentence();
    block->Print();

    return ( strstr( block->GetFirstWord(), MIKROTIK_SUCCESS_ANSWER ) != nullptr );
}


bool Mikrotik::Login(uint8_t numLoginTry)
{
#ifdef __LINUX_DBG
    const char d_ip[] = "192.168.60.1";
#else
    uint8_t d_ip[] = { 192, 168, 60, 1 };
#endif
    const uint16_t d_port = 8728;

    static bool isKnownPass = false;
    static uint8_t nbrPass = 0;

    for ( int i = 0; i < numLoginTry; i++ )
    {
        if ( isConnected )
            Disconnect();

        isLogged = false;

        if ( !Connect( d_ip, d_port ) )
            return false;

        debugPrintf( "Login attempt %i of %i... [%s] ", i + 1, numLoginTry, isKnownPass ? default_password[nbrPass] : default_password[i % 2] );
        if ( try_to_log_in( default_username, isKnownPass ? default_password[nbrPass] : default_password[i % 2] ) )
        {
        	nbrPass = i % 2;
        	debugPrintf( "success default credentials [%d]\n", nbrPass );
            isKnownPass = true;
            isLogged = true;
            break;
        }
		else
		{
			debugPrintf( "FAILED!\n" );
			isKnownPass = false;
			Disconnect();
		}
    }

    block->ReInit();
    return isLogged;
}


// @warning On DUET3D board it should be called INDIRECTLY from
// Mikrotik::Spin() <- Network::Spin() <- NetworkLoop() [RTOS TASK]
bool Mikrotik::ProcessRequest()
{
    if ( !isLogged )
        if ( !Login(5) )
        {
            SafeSnprintf( answer, MIKROTIK_MAX_ANSWER, "MIKROTIK: can't log in" );
            return false;
        }

    write_sentence( &mkSentence );
    clear_sentence( &mkSentence );


    if (!notResponse)
    {
    	read_block();
    }

    return true;
}

// it checks router connection
bool Mikrotik::LoginRequest()
{
    if ( !isLogged )
        if ( !Login(5) )
        {
            return false;
        }

    return true;
}

bool Mikrotik::IsRouterAvailable()
{
	if ( isLogged )
		return true;

	isRouterAvailable = true;
	ExecuteRequest();

	bool ret = LoginRequest();
	isRouterAvailable = false;

	return ret;
}


/* Success router response should looks like this
!re
=.id=*7
=name=omni
=mode=dynamic-keys
=authentication-types=wpa2-eap
=unicast-ciphers=aes-ccm
=mschapv2-username=
=mschapv2-password=
=disable-pmkid=false
=default=false

!done
 */
bool Mikrotik::parseAnswer( const char *pSearchText )
{
    // Each word in sentence (exclude first) is =KEY=VALUE pair
    char key[MIKROTIK_MAX_ANSWER];
    SafeSnprintf( key, sizeof( key ), "=%s=", pSearchText );

    const char* pWord = block->GetFirstWord();
    do
    {
        // Our search '=KEY=' should be located at the begin of word
        if ( strstr( pWord, key ) == pWord )
        {
            // And 'VALUE' is the rest of this word
            strncpy( answer, &pWord[strlen(key)], sizeof( answer ) - 1 );
            return true;
        }

        pWord = block->GetNextWord( pWord );
    } while ( pWord );

    //debugPrintf( "FAILED to parse answer!\nCan't find %s\n", key );
    return false;
}


// Text "!done" in router answer means successful execution of request
bool Mikrotik::IsRequestSuccessful()
{
    const char* pWord = block->GetFirstWord();

    while ( pWord )
    {
        if ( strstr( pWord, MIKROTIK_SUCCESS_ANSWER ) == pWord )
            return true;

        pWord = block->GetNextWord( pWord );
    }

    block->Print();
    return false;
}


bool Mikrotik::isInterfaceActive( TInterface iface )
{
    if ( !iface )
        return false;

    // 1. PREPARE REQUEST
    const char cmdWiFi[] = CMD_INTERFACE_WIRELESS_PRINT;
    const char cmdEht[]  = CMD_INTERFACE_ETHERNET_PRINT;

    const char *cmd = ( iface == ether1 ) ? cmdEht : cmdWiFi;

    char cmdIface[50] = { 0 };
    SafeSnprintf( cmdIface, sizeof( cmdIface ), REQ_PARAM( P_NAME ) "%s", IFACE_NAME_TABLE[iface] );

    const char cmdOpt[] = GREP_OPT( P_DISABLED );

    clear_sentence( &mkSentence );
    add_word_to_sentence( cmd,      &mkSentence );
    add_word_to_sentence( cmdIface, &mkSentence );
    add_word_to_sentence( cmdOpt,   &mkSentence );

    // 2. WAIT FOR EXECUTION
    ExecuteRequest();

    // 3. PROCESS ANSWER
    if ( !IsRequestSuccessful() )
        return false;  // ERROR

    if ( !parseAnswer( P_DISABLED ) )
        return false;  // ERROR

    if ( strstr( answer, V_FALSE ) )
        return true;
    else if ( strstr( answer, V_TRUE ) )
        return false;
    else
        return false;  // ERROR
}


bool Mikrotik::getStaticIpId( char *pID, TInterface iface )
{
    if ( !iface )
        return false;

    // 1. PREPARE REQUEST
    char cmdIface[50] = { 0 };
    SafeSnprintf( cmdIface, sizeof( cmdIface ), REQ_PARAM( P_INTERFACE ) "%s", IFACE_NAME_TABLE[iface] );

    const char *cmd     = CMD_IP_ADDRESS_PRINT;
    const char *cmdDyn  = REQ_PARAM_V( P_DYNAMIC, V_FALSE );
    const char *cmdGrep = GREP_OPT( P_ID );

    clear_sentence( &mkSentence );
    add_word_to_sentence( cmd,      &mkSentence );
    add_word_to_sentence( cmdIface, &mkSentence );
    add_word_to_sentence( cmdDyn,   &mkSentence );
    add_word_to_sentence( cmdGrep,  &mkSentence );

    // 2. WAIT FOR EXECUTION
    ExecuteRequest();

    // 3. PROCESS ANSWER
    if ( !IsRequestSuccessful() )
        return false;

    // Extract security profile ID from answer
    bool success = parseAnswer( P_ID );
    if ( success )
        strcpy( pID, answer );

    return success;
}


bool Mikrotik::getDhcpID( char *pID, TInterface iface, TDhcpMode dhcpMode )
{
    if ( !iface )
        return false;

    // 1. PREPARE REQUEST
    char cmdIface[20];
    SafeSnprintf( cmdIface, sizeof( cmdIface ), REQ_PARAM( P_INTERFACE ) "%s", IFACE_NAME_TABLE[iface] );

    const char *cmdServer = CMD_IP_DHCP_SERVER_PRINT;
    const char *cmdClient = CMD_IP_DHCP_CLIENT_PRINT;
    const char *cmdGrep   = GREP_OPT( P_ID );

    clear_sentence( &mkSentence );
    add_word_to_sentence( dhcpMode == DhcpServer ? cmdServer : cmdClient, &mkSentence );
    add_word_to_sentence( cmdIface, &mkSentence );
    add_word_to_sentence( cmdGrep,  &mkSentence );

    // 2. WAIT FOR EXECUTION
    ExecuteRequest();

    // 3. PROCESS ANSWER
    if ( !IsRequestSuccessful() )
        return false;

    // Extract security profile ID from answer
    bool success = parseAnswer( P_ID );
    if ( success )
        strcpy( pID, answer );

    return success;
}


bool Mikrotik::getSecurityProfileID( char *spID, const char *mode )
{
    // 1. PREPARE REQUEST
    char reqSP[20];
    SafeSnprintf( reqSP, sizeof( reqSP ), REQ_PARAM( P_NAME ) "%s", mode );

    const char *cmd     = CMD_INTERFACE_WIRELESS_SEC_PROF_PRINT ;
    const char *cmdGrep = GREP_OPT( P_ID );

    clear_sentence( &mkSentence );
    add_word_to_sentence( cmd,     &mkSentence );
    add_word_to_sentence( reqSP,   &mkSentence );
    add_word_to_sentence( cmdGrep, &mkSentence );

    // 2. WAIT FOR EXECUTION
    ExecuteRequest();

    // 3. PROCESS ANSWER
    if ( !IsRequestSuccessful() )
        return false;

    // Extract security profile ID from answer
    bool success = parseAnswer( P_ID );
    if ( success )
        strcpy( spID, answer );

    return success;
}


bool Mikrotik::changeAccessPointPass( const char *pass )
{
    char spID[10] = { 0 };
    if ( !getSecurityProfileID( spID, SP_MODE_ACCESS_POINT ) )
        return false;

    const char *cmd = CMD_INTERFACE_WIRELESS_SEC_PROF_SET;

    char id[25];
    SafeSnprintf( id, sizeof( id ), SET_PARAM( P_ID ) "%s", spID );

    char newPassWpa2[100];
    SafeSnprintf( newPassWpa2, sizeof( newPassWpa2 ), SET_PARAM( P_WPA2_PRE_SHARED_KEY ) "%s", pass );

    char newPassSupplicant[100];
    SafeSnprintf( newPassSupplicant, sizeof( newPassSupplicant ), SET_PARAM( P_SUPPLICANT_IDENTITY ) "%s", pass );

    clear_sentence( &mkSentence );
    add_word_to_sentence( cmd,               &mkSentence );
    add_word_to_sentence( id,                &mkSentence );
    add_word_to_sentence( newPassWpa2,       &mkSentence );
    add_word_to_sentence( newPassSupplicant, &mkSentence );

    // 2. WAIT FOR EXECUTION
    ExecuteRequest();

    // 3. PROCESS ANSWER
    return IsRequestSuccessful();
}


bool Mikrotik::changeWiFiStationPass( const char *pass )
{
    char spID[10] = { 0 };
    if ( !getSecurityProfileID( spID, SP_MODE_STATION ) )
        return false;

    const char *cmd = CMD_INTERFACE_WIRELESS_SEC_PROF_SET;

    char id[25];
    SafeSnprintf( id, sizeof( id ), SET_PARAM( P_ID ) "%s", spID );

    // We have no idea about encryption type
    // Thats why we're going to set all pass types in security profile

    char wpa2Pass[100];
    SafeSnprintf( wpa2Pass, sizeof( wpa2Pass ), SET_PARAM( P_WPA2_PRE_SHARED_KEY ) "%s", pass );

    char supplicantPass[100];
    SafeSnprintf( supplicantPass, sizeof( supplicantPass ), SET_PARAM( P_SUPPLICANT_IDENTITY ) "%s", pass );

    clear_sentence( &mkSentence );
    add_word_to_sentence( cmd,            &mkSentence );
    add_word_to_sentence( id,             &mkSentence );
    add_word_to_sentence( wpa2Pass,       &mkSentence );
    add_word_to_sentence( supplicantPass, &mkSentence );

    // 2. WAIT FOR EXECUTION
    ExecuteRequest();

    // 3. PROCESS ANSWER
    return IsRequestSuccessful();
}


void Mikrotik::clear_sentence( TMKSentence *pSentence )
{
    memset( pSentence, 0, sizeof( TMKSentence ) );
}


void Mikrotik::print_sentence( TMKSentence *pSentence )
{
    if ( pSentence->length == 0 )
        return;

    for ( int i = 0; i < pSentence->length; i++ )
        debugPrintf( "sentence[%i]: len(%i), %s\n", i, (int)strlen(pSentence->pWord[i]), pSentence->pWord[i] );

    debugPrintf( "\n" );
}


void Mikrotik::add_word_to_sentence( const char *pWord, TMKSentence *pSentence )
{
    pSentence->pWord[pSentence->length++] = pWord;
}


void Mikrotik::write_sentence( TMKSentence *pSentence )
{
    if ( pSentence->length == 0 )
        return;

    for ( int i = 0; i < pSentence->length; i++ )
        write_word( pSentence->pWord[i] );

    write_word( "" );
}


void Mikrotik::write_word( const char *pWord )
{
    //debugPrintf( "Write: '%s'\n", pWord );
    write_len( strlen( pWord ) );
    for ( int i = 0; i < (int)strlen( pWord ); i++ )
    {
        char c = pWord[i];
        //debugPrintf( "Send: 0x%02X - %c\n", c, c );
        __write( MIKROTIK_SOCK_NUM, &c, 1 );
    }
}


/********************************************************************
 * Encode message length and write it out to the socket
 ********************************************************************/
void Mikrotik::write_len( int iLen )
{
    char cEncodedLength[4];  // encoded length to send to the api socket
    char *cLength;           // exactly what is in memory at &iLen integer

    // set cLength address to be same as iLen
    cLength = (char *) &iLen;

    // write 1 byte
    if ( iLen < 0x80 )
    {
        cEncodedLength[0] = (char) iLen;
        __write( MIKROTIK_SOCK_NUM, cEncodedLength, 1 );
    }
    // write 2 bytes
    else if ( iLen < 0x4000 )
    {
        if ( iLittleEndian )
        {
            cEncodedLength[0] = cLength[1] | 0x80;
            cEncodedLength[1] = cLength[0];
        }
        else
        {
            cEncodedLength[0] = cLength[2] | 0x80;
            cEncodedLength[1] = cLength[3];
        }

        __write( MIKROTIK_SOCK_NUM, cEncodedLength, 2 );
    }
    // write 3 bytes
    else if ( iLen < 0x200000 )
    {
        // ? //Printf( "iLen < 0x200000.\n" ) : 0;

        if ( iLittleEndian )
        {
            cEncodedLength[0] = cLength[2] | 0xc0;
            cEncodedLength[1] = cLength[1];
            cEncodedLength[2] = cLength[0];
        }
        else
        {
            cEncodedLength[0] = cLength[1] | 0xc0;
            cEncodedLength[1] = cLength[2];
            cEncodedLength[2] = cLength[3];
        }

        __write( MIKROTIK_SOCK_NUM, cEncodedLength, 3 );
    }
    // write 4 bytes
    // this code SHOULD work, but is untested...
    else if ( iLen < 0x10000000 )
    {
        if ( iLittleEndian )
        {
            cEncodedLength[0] = cLength[3] | 0xe0;
            cEncodedLength[1] = cLength[2];
            cEncodedLength[2] = cLength[1];
            cEncodedLength[3] = cLength[0];
        }
        else
        {
            cEncodedLength[0] = cLength[0] | 0xe0;
            cEncodedLength[1] = cLength[1];
            cEncodedLength[2] = cLength[2];
            cEncodedLength[3] = cLength[3];
        }

        __write( MIKROTIK_SOCK_NUM, cEncodedLength, 4 );
    }
    else  // this should never happen
    {
        debugPrintf( "length of word is %d\n", iLen );
        debugPrintf( "word is too long.\n" );
        debugPrintf( "FATAL ERROR!!!!!!!!!!!\n" );
        return;
    }
}


int Mikrotik::read_len()
{
    char cFirstChar;          // first character read from socket
    char cLength[4] = { 0 };  // length of next message to read...will be cast to int at the end
    int *iLen;                // calculated length of next message (Cast to int)

    int tmp;
    __read( MIKROTIK_SOCK_NUM, &cFirstChar, 1 );
    //debugPrintf( "byte[1] = 0x%X\n", cFirstChar );

    // read 4 bytes
    // this code SHOULD work, but is untested...
    if (( cFirstChar & 0xE0 ) == 0xE0 )
    {
        //debugPrintf( "4-byte encoded length\n" );
        if ( iLittleEndian )
        {
            cLength[3] = cFirstChar;
            cLength[3] &= 0x1f;        // mask out the 1st 3 bits
            __read( MIKROTIK_SOCK_NUM, &cLength[2], 1 );
            __read( MIKROTIK_SOCK_NUM, &cLength[1], 1 );
            __read( MIKROTIK_SOCK_NUM, &cLength[0], 1 );
        }
        else
        {
            cLength[0] = cFirstChar;
            cLength[0] &= 0x1f;        // mask out the 1st 3 bits
            __read( MIKROTIK_SOCK_NUM, &cLength[1], 1 );
            __read( MIKROTIK_SOCK_NUM, &cLength[2], 1 );
            __read( MIKROTIK_SOCK_NUM, &cLength[3], 1 );
        }

        iLen = (int *) cLength;
    }
    // read 3 bytes
    else if (( cFirstChar & 0xC0 ) == 0xC0 )
    {
        //debugPrintf( "3-byte encoded length\n" );
        if ( iLittleEndian )
        {
            cLength[2] = cFirstChar;
            cLength[2] &= 0x3f;        // mask out the 1st 2 bits
            __read( MIKROTIK_SOCK_NUM, &cLength[1], 1 );
            __read( MIKROTIK_SOCK_NUM, &cLength[0], 1 );
        }
        else
        {
            cLength[1] = cFirstChar;
            cLength[1] &= 0x3f;        // mask out the 1st 2 bits
            __read( MIKROTIK_SOCK_NUM, &cLength[2], 1 );
            __read( MIKROTIK_SOCK_NUM, &cLength[3], 1 );
        }

        iLen = (int *) cLength;
    }
    // read 2 bytes
    else if (( cFirstChar & 0x80 ) == 0x80 )
    {
        //debugPrintf( "2-byte encoded length\n" );
        if ( iLittleEndian )
        {
            cLength[1] = cFirstChar;
            cLength[1] &= 0x7f;        // mask out the 1st bit
            __read( MIKROTIK_SOCK_NUM, &cLength[0], 1 );
        }
        else
        {
            cLength[2] = cFirstChar;
            cLength[2] &= 0x7f;        // mask out the 1st bit
            __read( MIKROTIK_SOCK_NUM, &cLength[3], 1 );
        }

        iLen = (int *) cLength;
    }
    // assume 1-byte encoded length...same on both LE and BE systems
    else
    {
        //debugPrintf( "1-byte encoded length\n" );
        iLen = &tmp;
        *iLen = (int)cFirstChar;
    }

    return *iLen;
}


//! @warning overflow is possible
bool Mikrotik::read_word( char *pWord )
{
    int iLen = read_len();
    int iBytesToRead = 0;
    int iBytesRead = 0;

    //debugPrintf( "Expected msg size is %i byte(s)\n", iLen );

    if ( iLen > 0 )
    {
        pWord[0] = 0;
        char tmpBuf[512];

        int numOfTry = 0;
        while ( iLen > 0 )
        {
            // determine number of bytes to read this time around
            // lesser of 1024 or the number of byes left to read
            // in this word
            iBytesToRead = iLen > 511 ? 511 : iLen;

            // read iBytesToRead from the socket

            //debugPrintf( "Ready to receive %i byte(s)... ", iBytesToRead );
            iBytesRead = __read( MIKROTIK_SOCK_NUM, tmpBuf, iBytesToRead );
            //debugPrintf( "Received %i byte(s)\n", iBytesRead );

            if ( iBytesRead < 0 )
            {
                if ( numOfTry++ > 7 )
                    return false;

                debugPrintf( "Can't read data - %i\n", iBytesRead );
                //usleep( 1000000 );
                continue;
            }

            // terminate received data...
            tmpBuf[iBytesRead] = 0;
            // ...and concatenate with main buffer
            strcat( pWord, tmpBuf );

            // subtract the number of bytes we just read from iLen
            iLen -= iBytesRead;

//            if ( iLen )
//                debugPrintf( "received %i byte(s)\nlefts - %i byte(s)\n", iBytesRead, iLen );

            numOfTry = 0;
        }

        return true;
    }
    else
        return false;
}


bool Mikrotik::read_sentence()
{
    char word[512] = { 0 };
    TSentenceRetVal retval = Sentence_None;

    uint32_t readTime = millis();

    while ( read_word( word ) )
    {
        if ( !block->AddWordToSentence( word ) )
        {
            debugPrintf( "\n\nFAILED TO ADD WORD TO SENTENSE!!!\n\n" );
            return false;
        }

        // check to see if we can get a return value from the API
        if ( strstr( word, "!done" ) != NULL )
            retval = Sentence_Done;
        else if ( strstr( word, "!trap" ) != NULL )
            retval = Sentence_Trap;
        else if ( strstr( word, "!fatal" ) != NULL )
            retval = Sentence_Fatal;
        else
            retval = Sentence_None;

        // if we cannot read whole sentence we need to return false
        if (millis() - readTime > 15000)
        {
        	debugPrintf( "Timeout read sentence!\n" );
        	return false;
        }
    }

    // if any errors, get the next sentence
    if ( retval == Sentence_Trap || retval == Sentence_Fatal )
        read_sentence();

    block->AddEndOfSentence();

    return ( retval == Sentence_None );
}


void Mikrotik::read_block()
{
    block->ReInit();
    while( read_sentence() );
}


/********************************************************************
 * Test whether or not this system is little endian at RUNTIME
 * Courtesy: http://download.osgeo.org/grass/grass6_progman/endian_8c_source.html
 ********************************************************************/
int Mikrotik::isLittleEndian()
{
    union
    {
        int testWord;
        char testByte[sizeof( int )];
    } endianTest{};

    endianTest.testWord = 1;

    if ( endianTest.testByte[0] == 1 )
        return 1;  /* true: little endian */
    else
        return 0;  /* false: big endian */
}

/********************************************************************
 * MD5 helper function to convert an md5 hex char representation to
 * binary representation.
 ********************************************************************/
void Mikrotik::md5ToBinary( char *szHex, char *challenge )
{
    int di;
    char cBinWork[3];

    // 32 bytes in szHex?
    if ( strlen( szHex ) != 32 )
        return;

    for ( di = 0; di < 32; di += 2 )
    {
        cBinWork[0] = szHex[di];
        cBinWork[1] = szHex[di + 1];
        cBinWork[2] = 0;

        // debugPrintf( "cBinWork = %s\n", cBinWork );
        challenge[di / 2] = hexStringToChar( cBinWork );
    }
}


/********************************************************************
 * MD5 helper function to calculate and return hex representation
 * of an MD5 digest stored in binary.
 ********************************************************************/
void Mikrotik::md5DigestToHexString( md5_byte_t *binaryDigest, char *szReturn )
{
    for ( int di = 0; di < 16; ++di )
        sprintf( szReturn + di * 2, "%02x", binaryDigest[di] );
}


/********************************************************************
 * Quick and dirty function to convert hex string to char...
 * the toConvert string MUST BE 2 characters + null terminated.
 ********************************************************************/
char Mikrotik::hexStringToChar( char *cToConvert )
{
    unsigned int iAccumulated = 0;
    char cString0[2] = {cToConvert[0], 0};
    char cString1[2] = {cToConvert[1], 0};

    // look @ first char in the 16^1 place
    if ( cToConvert[0] == 'f' || cToConvert[0] == 'F' )
        iAccumulated += 16 * 15;
    else if ( cToConvert[0] == 'e' || cToConvert[0] == 'E' )
        iAccumulated += 16 * 14;
    else if ( cToConvert[0] == 'd' || cToConvert[0] == 'D' )
        iAccumulated += 16 * 13;
    else if ( cToConvert[0] == 'c' || cToConvert[0] == 'C' )
        iAccumulated += 16 * 12;
    else if ( cToConvert[0] == 'b' || cToConvert[0] == 'B' )
        iAccumulated += 16 * 11;
    else if ( cToConvert[0] == 'a' || cToConvert[0] == 'A' )
        iAccumulated += 16 * 10;
    else
        iAccumulated += 16 * atoi( cString0 );

    // now look @ the second car in the 16^0 place
    if ( cToConvert[1] == 'f' || cToConvert[1] == 'F' )
        iAccumulated += 15;
    else if ( cToConvert[1] == 'e' || cToConvert[1] == 'E' )
        iAccumulated += 14;
    else if ( cToConvert[1] == 'd' || cToConvert[1] == 'D' )
        iAccumulated += 13;
    else if ( cToConvert[1] == 'c' || cToConvert[1] == 'C' )
        iAccumulated += 12;
    else if ( cToConvert[1] == 'b' || cToConvert[1] == 'B' )
        iAccumulated += 11;
    else if ( cToConvert[1] == 'a' || cToConvert[1] == 'A' )
        iAccumulated += 10;
    else
        iAccumulated += atoi( cString1 );

    // debugPrintf( "%d\n", iAccumulated );
    return (char) iAccumulated;
}


int  Mikrotik::__write( int __fd, const void *__buf, size_t __nbyte )
{
    return __send( __fd, (uint8_t *)__buf, (uint16_t)__nbyte );
}


int Mikrotik::__read (int __fd, void *__buf, size_t __nbyte)
{
    return __recv( __fd, (uint8_t *)__buf, (uint16_t)__nbyte );
}

GCodeResult Mikrotik::Configure(GCodeBuffer& gb, const StringRef& reply)
{
	bool seen = false;
	int cParam;

	String<StringLength40> tSsid, tPass;

	if (gb.Seen('C'))
	{
		cParam = gb.GetIValue();

		if(cParam == 2)
		{
			ConnectToEthernet();
			return GCodeResult::ok;
		}
	}
	else
	{
		reply.copy("C parameter is needed");
		return GCodeResult::error;
	}

	if (gb.Seen('I'))
	{
		interface = gb.GetIValue() == 2 ? wifi2g : wifi5g;
	}
	else
	{
		reply.copy("Interface parameter is needed");
		return GCodeResult::error;
	}

	seen = false;
	gb.TryGetPossiblyQuotedString('S', tSsid.GetRef(), seen);

	if (seen)
	{
		seen = false;
		gb.TryGetPossiblyQuotedString('P', tPass.GetRef(), seen);

		// Password must have at least 8 characters
		if (tPass.strlen() < 8)
		{
			reply.copy("Password must have at least 8 characters");
			return GCodeResult::error;
		}

		if (seen)
		{
			if (cParam)
			{
				CreateAP(tSsid.c_str(), tPass.c_str(), interface);
				mode = AccessPoint;
			}
			else
			{
				ConnectToWiFi(tSsid.c_str(), tPass.c_str(), interface);
				mode = Station;
			}
			strcpy(ssid, tSsid.c_str());
			strcpy(password, tPass.c_str());
		}
		else
		{
			reply.copy("Password is needed");
			return GCodeResult::error;
		}
	}
	else
	{
		reply.copy("SSID is needed");
		return GCodeResult::error;
	}

	return GCodeResult::ok;
}

// Push status network to LCD
void Mikrotik::SendNetworkStatus()
{
	char outputBuffer[256] = { 0 };
	char md[4] = { 0 };
	char typeIp = { 0 };

	if (interface == ether1)
	{
		strncpy(md, "E", sizeof(md));
	}
	else
	{
		switch (mode)
		{
			case AccessPoint:
				strncpy(md, interface == wifi2g ? "A2" : "A5", sizeof( md ));
				break;
			case Station:
				strncpy(md, interface == wifi2g ? "W2" : "W5", sizeof( md ));
				break;
			default:
				strncpy(md, "X", sizeof( md ));
				break;
		}
	}
	typeIp = mode == AccessPoint ? 'Y' : isStatic ? 'S' : 'D';

	char tempIp[32];

	strcpy(tempIp, ip);
	char* pos = strstr(tempIp, "/");

	size_t charPtr = pos - tempIp;

	if (charPtr > 0 && charPtr < strlen(tempIp))
	{
		tempIp[charPtr] = 0;
	}

	if (status == Disconnected || status == Booting)
	{
		strncpy(md, "X", sizeof( md ));
		typeIp = 'Y';
		tempIp[0] = mask[0] = gateway[0] = ssid[0] = 0;
	}

	SafeSnprintf(outputBuffer, sizeof(outputBuffer),"{\"networkStatus\":[\"%s\",\"%s\",\"%c\",\"%s\",\"%s\",\"%s\",\"%s\"]}",
				md, statusStr[status], typeIp, tempIp, mask, gateway,
				interface == ether1 ? "" : ssid);
	reprap.GetPlatform().MessageF(LcdMessage, outputBuffer);
	debugPrintf(outputBuffer);
}

void Mikrotik::Check()
{
	bool isNetworkRunning = false;
	status = Booting;

	if (IsRouterAvailable())
	{
		if (!GetCurrentInterface(&interface))
		{
			status = Disconnected;
			debugPrintf("ERROR! Can't get current interface\n");
		}
		else
		{
			//debugPrintf("Current interface: %d\n", (uint8_t)interface);

			isNetworkRunning = IsNetworkAvailable(interface);
			//debugPrintf("Running: %s\n", isNetworkRunning ? "YES" : "NO");

			if(isNetworkRunning)
			{
				status = Connected;
				TEnableState state;
				if (!GetDhcpState(interface, DhcpClient, &state))
				{
					debugPrintf("ERROR! Can't get DHCP client state\n");
					return;
				}

				isStatic = state == Enabled ? false : true;

				if (!GetInterfaceIP(interface, ip, isStatic))
				{
					strncpy(ip, "Obtaining", sizeof(ip));
				}

				//debugPrintf("IP addr: %s\n", ip);
				//debugPrintf("IP type: %s\n", isStatic ? "static" : "dynamic");

				//debugPrintf("Mode: ");
				if (interface != ether1)
				{
					GetWifiMode(interface, &mode);
					if (!GetSSID(interface, ssid))
					{
						SafeSnprintf(ssid, sizeof( ssid ), "Can't get SSID\n");
					}
				}
			}
			else
			{
				strncpy(ip, "Obtaining", sizeof( ip ));
				status = Connecting;
			}
		}
	}
}

void Mikrotik::DisableInterface()
{
	if (GetCurrentInterface(&interface))
	{
		DisableInterface(interface);
	}
	else
	{
		debugPrintf("Cannot find current interface\n");
	}
	interface = none;

	SendNetworkStatus();
}

GCodeResult Mikrotik::SearchWiFiNetworks(GCodeBuffer& gb, const StringRef& reply)
{
	const uint32_t SIZE = 1024;
	uint8_t searchTime = 5;
	char list[SIZE];
	char outBuffer[SIZE] = "{\"ssidList\":[";
	char minBuffer[64] = { 0 };

	TInterface interface = wifi2g;

	if (gb.Seen('I'))
	{
		interface = gb.GetIValue() == 2 ? wifi2g : wifi5g;
	}
	if (gb.Seen('T'))
	{
		searchTime = gb.GetUIValue();
	}

	uint16_t count = ScanWiFiNetworks( interface, searchTime, list, SIZE );

	debugPrintf( "Available networks count: %u\n\n", count );
	char *pNext = list;
	debugPrintf( "%s", list );

	if ( count )
	{
		while ( count-- )
		{
			SafeSnprintf(minBuffer, sizeof(minBuffer), "\"%s\"%s", pNext, count ? "," : "]}");
			strcat(outBuffer, minBuffer);
			memset(minBuffer, 0, sizeof(minBuffer));
			debugPrintf( "SSID: %s\n", pNext );
			pNext += strlen( pNext ) + 1;
		}
		reprap.GetPlatform().MessageF(LcdMessage, outBuffer);
	}

	return GCodeResult::ok;
}

GCodeResult Mikrotik::StaticIP(GCodeBuffer& gb, const StringRef& reply)
{
	uint8_t maskBits = 24;

	if (gb.Seen('I'))
	{
		uint32_t interVal = gb.GetUIValue();
		interface = interVal == ether1 ? ether1 : interVal == wifi2g ? wifi2g : wifi5g;
	}

	if (gb.Seen('D'))
	{
		if (gb.GetIValue())
		{
			if(GetCurrentInterface(&interface))
			{
				RemoveStaticIP(interface);
				isStatic = false;
			}
		}
	}

	if (gb.Seen('R'))
	{
		IPAddress maskIP;
		if (gb.GetIPAddress(maskIP))
		{
			String<StringLength20> maskStr;
			maskStr.printf("%d.%d.%d.%d", maskIP.GetQuad(0), maskIP.GetQuad(1), maskIP.GetQuad(2), maskIP.GetQuad(3));
			strcpy(mask, maskStr.c_str());

			uint8_t i;
			uint32_t ipMask = maskIP.GetV4BigEndian();

			maskBits = 0;
			for(i = 32; i > 0; --i)
			{
				if(ipMask & (1u << (i - 1)))
				{
					maskBits++;
				}
				else
				{
					break;
				}
			}

			maskBit = maskBits;
		}
		else
		{
			reply.copy("Can't set net mask");
			return GCodeResult::error;
		}
	}


	if (gb.Seen('P'))
	{
		IPAddress ipC;
		if (gb.GetIPAddress(ipC))
		{
			String<StringLength20> ipAddress;

			ipAddress.printf("%d.%d.%d.%d/%d", ipC.GetQuad(0), ipC.GetQuad(1), ipC.GetQuad(2), ipC.GetQuad(3), maskBits);

			if (!SetStaticIP(interface, ipAddress.c_str()))
			{
				reply.copy("Can't set static IP address");
				return GCodeResult::error;
			}
			strcpy(ip, ipAddress.c_str());
			isStatic = true;
			reprap.GetNetwork().ReinitSockets();
		}
		else
		{
			reply.copy("Can't set IP address");
			return GCodeResult::error;
		}
	}

	if (gb.Seen('A'))
	{
		IPAddress gatewayAddress;
		if (gb.GetIPAddress(gatewayAddress))
		{
			String<StringLength20> ipGateway;

			ipGateway.printf("%d.%d.%d.%d", gatewayAddress.GetQuad(0), gatewayAddress.GetQuad(1), gatewayAddress.GetQuad(2), gatewayAddress.GetQuad(3));

			if (RefreshGateway())
			{
				RemoveGateway();

				SetGateway(ipGateway.c_str());
				strcpy(gateway, ipGateway.c_str());
			}
		}
		else
		{
			reply.copy("Can't set gateway address");
			return GCodeResult::error;
		}
		//reprap.GetNetwork().ReinitSockets();
	}

	return GCodeResult::ok;
}

GCodeResult Mikrotik::DHCPState(GCodeBuffer& gb, const StringRef& reply)
{
	TDhcpMode dhcpMode = DhcpServer;
	TEnableState dhcp = Disabled;
	TInterface tInterface = wifi2g;

	if (gb.Seen('P'))
	{
		dhcpMode = gb.GetIValue() ? DhcpClient : DhcpServer;
	}
	else
	{
		reply.copy("P parameter is needed");
		return GCodeResult::error;
	}

	if (gb.Seen('I'))
	{
		tInterface = gb.GetIValue() == 2 ? wifi2g : wifi5g;
	}
	else
	{
		reply.copy("I parameter is needed");
		return GCodeResult::error;
	}

	if (GetDhcpState(tInterface, dhcpMode, &dhcp))
	{
		reply.printf("DHCP %s %s %s\n", mode ? "client" : "server", interface == 2 ? "wifi2g" : "wifi5g", dhcp ? "enabled" : "disabled");
	}
	else
	{
		reply.cat("Failed to get dhcp state\n");
	}

	return GCodeResult::ok;
}
