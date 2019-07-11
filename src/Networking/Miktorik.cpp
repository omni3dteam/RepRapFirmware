#include "Mikrotik.h"
#include "Platform.h"
#include "RepRap.h"
#include "W5500Ethernet/Wiznet/Ethernet/socketlib.h"

constexpr size_t ClientStackWords = 550;
static Task<ClientStackWords> clientTask;

static const char *IFACE_NAME_TABLE[] = { IFACE_ETHERNET, IFACE_WIFI2G, IFACE_WIFI5G };


Mikrotik::Mikrotik() : isRequestWaiting(false)
{
	memset( answer, 0, 1024 );
}


extern "C" void ClientLoop(void *)
{
	for (;;)
	{
		//reprap.GetClient().Spin();
		RTOSIface::Yield();
	}
}


void Mikrotik::Activate()
{
#ifdef RTOS
	clientTask.Create(ClientLoop, "MIKROTIK", nullptr, TaskPriority::SpinPriority);
#endif
}


void Mikrotik::Spin()
{
	if( isRequestWaiting )
	{
		ProcessRequest();
		isRequestWaiting = false;
	}
}


bool Mikrotik::GetNetworkParams( char *ip, char *mask, char *gw )
{
	//! @todo ADD SUPPORT

	return true;
}


bool Mikrotik::GetUpTime( char *buffer )
{
	// 1. PREPARE REQUEST

	// initialize first sentence
	initializeSentence( &stSentence );

	const char *cmd[] = { "/system/resource/print", "=.proplist=uptime" };
	addWordToSentence( &stSentence, cmd[0] );
	addWordToSentence( &stSentence, cmd[1] );

	// 2. WAIT FOR EXECUTION

	isRequestWaiting = true;
	while( isRequestWaiting );

	// 3. PROCESS ANSWER

	// Parse answer

	if ( !parseAnswer( "uptime" ) )
		SafeSnprintf( buffer, MIKROTIK_MAX_ANSWER, "no answer found" );
	else
		strcpy( buffer, answer );

	// FINISH
	clearSentence( &stSentence );

	return true;
}


// https://wiki.mikrotik.com/wiki/Manual:Making_a_simple_wireless_AP
bool Mikrotik::CreateAP( const char *ssid, const char *pass, TInterface iface )
{
	// Only WiFi allowed here
	if ( iface == ether1 )
		return false;

	DisableWirelessNetwork( iface == wifi5g ? wifi2g : wifi5g );

	if( !SetDhcpState( iface, Disabled, DhcpClient ) )
		return false;

	if( !SetDhcpState( iface, Enabled, DhcpServer ) )
		return false;

	// 0. PRECONFIG ROUTER

	if ( pass != NULL )
		if ( !changeAccessPointPass( pass ) )
		{
			SafeSnprintf( answer, MIKROTIK_MAX_ANSWER, "can't configure security profile" );
			debugPrintf( "ERROR: %s\n", answer );
			return false;
		}

	// 1. PREPARE REQUEST

	// initialize first sentence
	initializeSentence( &stSentence );

	char wlanID[20];
	SafeSnprintf( wlanID, sizeof( wlanID ), "=.id=%s", IFACE_NAME_TABLE[iface] );

	char band[50];
	if ( iface == wifi2g )
		SafeSnprintf( band, sizeof( band ), "%s", "=band=2ghz-b/g/n" );
	else
		SafeSnprintf( band, sizeof( band ), "%s", "=band=5ghz-a/n/ac" );

	char apName[MIKROTIK_MAX_ANSWER];
	SafeSnprintf( apName, sizeof( apName ), "=ssid=%s", ssid );

	const char *cmd[] = { "/interface/wireless/set", "=frequency=auto", "=mode=ap-bridge", "=disabled=false" };

	addWordToSentence( &stSentence, cmd[0] );
	addWordToSentence( &stSentence, wlanID );
	addWordToSentence( &stSentence, apName );
	addWordToSentence( &stSentence, cmd[1] );
	addWordToSentence( &stSentence, cmd[2] );
	addWordToSentence( &stSentence, cmd[3] );

	char sp[50] = "=security-profile=";
	if ( pass == NULL )
		strcat( sp, SP_DEFAULT );
	else
		strcat( sp, SP_MODE_ACCESS_POINT );

	addWordToSentence( &stSentence, sp );

	// 2. WAIT FOR EXECUTION

	isRequestWaiting = true;
	while( isRequestWaiting );

	// 3. TODO Analyze router response
	clearSentence( &stSentence );

	SafeSnprintf( answer, MIKROTIK_MAX_ANSWER, "done" );
	return true;
}


// https://wiki.mikrotik.com/wiki/Manual:Wireless_AP_Client#Additional_Station_Configuration
bool Mikrotik::ConnectToWiFi( const char *ssid, const char *pass, TInterface iface )
{
	// Only WiFi allowed here
	if ( iface == ether1 )
		return false;

	DisableWirelessNetwork( iface == wifi5g ? wifi2g : wifi5g );

	if( !SetDhcpState( iface, Disabled, DhcpServer ) )
		return false;

	if( !SetDhcpState( iface, Enabled, DhcpClient ) )
		return false;

	// PRECONFIG ROUTER

	if ( pass != NULL )
		if ( !changeWiFiStationPass( pass ) )
		{
			SafeSnprintf( answer, MIKROTIK_MAX_ANSWER, "can't configure security profile" );
			debugPrintf( "ERROR: %s\n", answer );
			return false;
		}

	// 1. PREPARE REQUEST

	// initialize first sentence
	initializeSentence( &stSentence );

	char wlanID[20];
	SafeSnprintf( wlanID, sizeof( wlanID ), "=.id=%s", IFACE_NAME_TABLE[iface] );

	char networkName[MIKROTIK_MAX_ANSWER];
	SafeSnprintf( networkName, sizeof( networkName ), "=ssid=%s", ssid );

	const char *cmd[] = { "/interface/wireless/set", "=mode=station", "=disabled=false" };

	addWordToSentence( &stSentence, cmd[0] );
	addWordToSentence( &stSentence, wlanID );
	addWordToSentence( &stSentence, networkName );
	addWordToSentence( &stSentence, cmd[1] );
	addWordToSentence( &stSentence, cmd[2] );

	char sp[50] = "=security-profile=";
	if ( pass == NULL )
		strcat( sp, SP_DEFAULT );
	else
		strcat( sp, SP_MODE_STATION );

	addWordToSentence( &stSentence, sp );

	// 2. WAIT FOR EXECUTION

	isRequestWaiting = true;
	while( isRequestWaiting );

	// 3. TODO Analyze router response

	clearSentence( &stSentence );

	SafeSnprintf( answer, MIKROTIK_MAX_ANSWER, "done" );
	return true;
}


bool Mikrotik::EnableWirelessNetwork( TInterface iface )
{
	if ( iface == ether1 )
		return false;

	// 1. PREPARE REQUEST

	initializeSentence( &stSentence );

	const char cmd[] = "/interface/wireless/enable";

	char wlanID[15];
	SafeSnprintf( wlanID, sizeof( wlanID ), "=.id=%s", IFACE_NAME_TABLE[iface] );

	addWordToSentence( &stSentence, cmd );
	addWordToSentence( &stSentence, wlanID );

	// 2. WAIT FOR EXECUTION

	isRequestWaiting = true;
	while( isRequestWaiting );

	// 3. TODO Analyze router response
	clearSentence( &stSentence );

	return true;
}


bool Mikrotik::DisableWirelessNetwork( TInterface iface )
{
	// Only WiFi allowed
	if ( iface == ether1 )
		return false;

	if( !SetDhcpState( iface, Disabled, DhcpServer ) )
		return false;

	if( !SetDhcpState( iface, Disabled, DhcpClient ) )
		return false;

	// 1. PREPARE REQUEST

	initializeSentence( &stSentence );

	const char cmd[] = "/interface/wireless/disable";

	// wlan1 is 2G, wlan2 - 5G
	char wlanID[15];
	SafeSnprintf( wlanID, sizeof( wlanID ), "=.id=%s", IFACE_NAME_TABLE[iface] );

	addWordToSentence( &stSentence, cmd );
	addWordToSentence( &stSentence, wlanID );

	// 2. WAIT FOR EXECUTION

	isRequestWaiting = true;
	while( isRequestWaiting );

	// 3. TODO Analyze router response
	clearSentence( &stSentence );
	return true;
}


// Router has few preconfigured DHCP-client params for ethernet and wifi ifaces
bool Mikrotik::SetDhcpState( TInterface iface, TEnableState state, TDhcpMode dhcpMode )
{
	// Only WiFi allowed here
	if ( iface == ether1 )
		return false;

	char id[10] = { 0 };
	if ( !getDhcpID( id, iface, dhcpMode ) )
		return false;

	// 1. PREPARE REQUEST

	initializeSentence( &stSentence );

	const char *cmdServer = "/ip/dhcp-server/set";
	const char *cmdClient = "/ip/dhcp-client/set";

	char cmdWlanId[30];
	SafeSnprintf( cmdWlanId, sizeof( cmdWlanId ), "=.id=%s", id );

	char cmdState[30];
	SafeSnprintf( cmdState, sizeof( cmdState ), "=disabled=%s", state == Enabled ? "false" : "true" );

	addWordToSentence( &stSentence, dhcpMode == DhcpServer ? cmdServer : cmdClient );
	addWordToSentence( &stSentence, cmdWlanId );
	addWordToSentence( &stSentence, cmdState );

	// 2. WAIT FOR EXECUTION

	isRequestWaiting = true;
	while( isRequestWaiting );

	// 3. TODO Analyze router response
	clearSentence( &stSentence );

	return true;
}


bool Mikrotik::GetDhcpState( TInterface iface, TDhcpMode dhcpMode, TEnableState *pState )
{
	// Only WiFi allowed here
	if ( iface == ether1 )
		return false;

	// 1. PREPARE REQUEST

	initializeSentence( &stSentence );

	const char *cmdServer = "/ip/dhcp-server/print";
	const char *cmdClient = "/ip/dhcp-client/print";

	char cmdWlan[30];
	SafeSnprintf( cmdWlan, sizeof( cmdWlan ), "?interface=%s", IFACE_NAME_TABLE[iface] );

	char cmdOpt[30];
	SafeSnprintf( cmdOpt, sizeof( cmdOpt ), "=.proplist=disabled" );

	addWordToSentence( &stSentence, dhcpMode == DhcpServer ? cmdServer : cmdClient );
	addWordToSentence( &stSentence, cmdWlan );
	addWordToSentence( &stSentence, cmdOpt );

	// 2. WAIT FOR EXECUTION

	isRequestWaiting = true;
	while( isRequestWaiting );


	// 3. PROCESS ANSWER

	bool success = parseAnswer( "disabled" );
	clearSentence( &stSentence );

	if ( !success )
		return false;

	if ( strstr( answer, "false" ) )
		*pState = Enabled;
	else if ( strstr( answer, "true" ) )
		*pState = Disabled;
	else
		return false;

	return true;
}


bool Mikrotik::Connect( uint8_t *d_ip, uint16_t d_port )
{
	if ( isConnected )
		Disconnect();

	const uint16_t s_port  = 1234;
	const uint8_t protocol = 0x01;  //Sn_MR_TCP

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

	if ( ce != 1 /* SOCK_OK */ )
	{
		debugPrintf( "connection error %i\n", ce );
		__close( MIKROTIK_SOCK_NUM );
		return false;
	}

	debugPrintf( "CONNECTED!\n" );

	isConnected = true;
	return isConnected;
}


void Mikrotik::Disconnect()
{
	__disconnect( MIKROTIK_SOCK_NUM );
	__close( MIKROTIK_SOCK_NUM );

	isConnected = false;
	isLogged    = false;
}


bool Mikrotik::Login()
{
	if( isConnected )
		Disconnect();

	isLogged = false;

	// CONNECTION
	uint8_t d_ip[] = { 192, 168, 60, 1 };
	const uint16_t d_port = 8728;

	if ( !Connect( d_ip, d_port ) )
		return false;

	// LOGIN
	#warning "ACHTUNG! Default credentials!"
	char default_username[] = "admin";
	char default_password[] = "";

	const int NUM_OF_LOGIN_TRY = 5;
	for( int i = 0; i < NUM_OF_LOGIN_TRY; i++ )
	{
		debugPrintf( "Login attempt %i of %i... ", i+1, NUM_OF_LOGIN_TRY );
		int iLoginResult = login( MIKROTIK_SOCK_NUM, default_username, default_password );
		if ( iLoginResult )
		{
			debugPrintf( "success\n" );
			isLogged = true;
			break;
		}
		else
		{
			Disconnect();
			debugPrintf( "FAILED!\n\Err: %i\n", iLoginResult );
		}
	}

	return isLogged;
}


bool Mikrotik::ProcessRequest()
{
	if ( !isLogged )
		if ( !Login() )
		{
			SafeSnprintf( answer, MIKROTIK_MAX_ANSWER, "MIKROTIK: can't log in" );
			return false;
		}

	writeSentence( MIKROTIK_SOCK_NUM, &stSentence );

	// receive and print response block from the API
	stBlock = readBlock( MIKROTIK_SOCK_NUM );

	return true;
}


/* Success router response will looks like this
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
	// Each sentence (exclude first) is =KEY=VALUE pair
	char key[MIKROTIK_MAX_ANSWER];
	SafeSnprintf( key, sizeof( key ), "=%s=", pSearchText );

	debugPrintf( "\n%s() - searching for %s\n\n", __func__, key );
	for ( int i = 0; i < stBlock.iLength; i++ )
	{
		struct Sentence *pSentence = stBlock.stSentence[i];

		if ( strstr( pSentence->szSentence[0], "!re" ) )
		{
			for ( int j=1; j<pSentence->iLength; j++ )
			{
				// Our search '=KEY=' should be located at the begin of sentence
				if ( strstr( pSentence->szSentence[j], key ) == pSentence->szSentence[j] )
				{
					debugPrintf( "FOUND: %s\n", pSentence->szSentence[j] );
					// And 'VALUE' is the rest of this sentence
					SafeSnprintf( answer, sizeof( answer ), &pSentence->szSentence[j][strlen(key)] );
					return true;
				}
			}
		}
	}

	return false;
}


bool Mikrotik::getDhcpID( char *pID, TInterface iface, TDhcpMode dhcpMode )
{
	// 1. PREPARE REQUEST
	char cmdIface[20];
	SafeSnprintf( cmdIface, sizeof( cmdIface ), "?interface=%s", IFACE_NAME_TABLE[iface] );

	// initialize first sentence
	initializeSentence( &stSentence );

	const char *cmdServer = "/ip/dhcp-server/print";
	const char *cmdClient = "/ip/dhcp-client/print";
	const char *cmdOpt = "=.proplist=.id";

	addWordToSentence( &stSentence, dhcpMode == DhcpServer ? cmdServer : cmdClient );
	addWordToSentence( &stSentence, cmdIface );
	addWordToSentence( &stSentence, cmdOpt );

	// 2. WAIT FOR EXECUTION

	isRequestWaiting = true;
	while( isRequestWaiting );

	// 3. PROCESS ANSWER

	// Extract security profile ID from answer

	bool success = parseAnswer( ".id" );

	if ( success )
	{
		strcpy( pID, answer );
		debugPrintf( "%s() - id is %s\n", __func__, pID );
	}
	else
		debugPrintf( "%s() - id not found\n", __func__ );

	clearSentence( &stSentence );
	return success;
}


bool Mikrotik::getSecurityProfileID( char *spID, const char *mode )
{
	// 1. PREPARE REQUEST
	char reqSP[20];
	SafeSnprintf( reqSP, sizeof( reqSP ), "?name=%s", mode );

	// initialize first sentence
	initializeSentence( &stSentence );

	const char *cmd[] = { "/interface/wireless/security-profiles/print", "=.proplist=.id" };
	addWordToSentence( &stSentence, cmd[0] );
	addWordToSentence( &stSentence, reqSP );
	addWordToSentence( &stSentence, cmd[1] );

	// 2. WAIT FOR EXECUTION

	isRequestWaiting = true;
	while( isRequestWaiting );

	// 3. PROCESS ANSWER

	// Extract security profile ID from answer

	bool success = parseAnswer( ".id" );

	if ( success )
		strcpy( spID, answer );
	else
		debugPrintf( "%s() - id not found\n", __func__ );

	clearSentence( &stSentence );
	return success;
}


bool Mikrotik::changeAccessPointPass( const char *pass )
{
	char spID[10] = { 0 };
	if ( !getSecurityProfileID( spID, SP_MODE_ACCESS_POINT ) )
		return false;

	const char cmd[] = "/interface/wireless/security-profiles/set";

	char id[25];
	SafeSnprintf( id, sizeof( id ), "=.id=%s", spID );

	char newPassWpa2[100];
	SafeSnprintf( newPassWpa2, sizeof( newPassWpa2 ), "=wpa2-pre-shared-key=%s", pass );

	char newPassSupplicant[100];
	SafeSnprintf( newPassSupplicant, sizeof( newPassSupplicant ), "=supplicant-identity=%s", pass );

	addWordToSentence( &stSentence, cmd );
	addWordToSentence( &stSentence, id );
	addWordToSentence( &stSentence, newPassWpa2 );
	addWordToSentence( &stSentence, newPassSupplicant );

	// 2. WAIT FOR EXECUTION

	isRequestWaiting = true;
	while( isRequestWaiting );

	// 3. TODO Analyze router response
	clearSentence( &stSentence );

	return true;
}


bool Mikrotik::changeWiFiStationPass( const char *pass )
{
	char spID[10] = { 0 };
	if ( !getSecurityProfileID( spID, SP_MODE_STATION ) )
		return false;

	const char cmd[] = "/interface/wireless/security-profiles/set";

	char id[25];
	SafeSnprintf( id, sizeof( id ), "=.id=%s", spID );

	// We have no idea about encryption type
	// Thats why we're going to set all pass types in security profile

	char wpaPass[100];
	SafeSnprintf( wpaPass, sizeof( wpaPass ), "=wpa2-pre-shared-key=%s", pass );

	char wpa2Pass[100];
	SafeSnprintf( wpa2Pass, sizeof( wpa2Pass ), "=wpa2-pre-shared-key=%s", pass );

	char supplicantPass[100];
	SafeSnprintf( supplicantPass, sizeof( supplicantPass ), "=supplicant-identity=%s", pass );

	addWordToSentence( &stSentence, cmd );
	addWordToSentence( &stSentence, id );
	addWordToSentence( &stSentence, wpaPass );
	addWordToSentence( &stSentence, wpa2Pass );
	addWordToSentence( &stSentence, supplicantPass );

	// 2. WAIT FOR EXECUTION

	isRequestWaiting = true;
	while( isRequestWaiting );

	// 3. TODO Analyze router response
	clearSentence( &stSentence );

	return true;
}
