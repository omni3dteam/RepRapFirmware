#include "Mikrotik.h"
#include "Platform.h"
#include "RepRap.h"
#include "W5500Ethernet/Wiznet/Ethernet/socketlib.h"

constexpr size_t ClientStackWords = 550;
static Task<ClientStackWords> clientTask;

static const char *IFACE_NAME_TABLE[] = { IFACE_ETHERNET, IFACE_WIFI2G, IFACE_WIFI5G };


Mikrotik::Mikrotik()
{
	memset( answer, 0, 1024 );
	block = new MKTBlock();
}


void Mikrotik::Spin()
{
	if( !isRequestWaiting )
		return;

	ProcessRequest();
	isRequestWaiting = false;
}


bool Mikrotik::GetUpTime( char *buffer )
{
    // 1. PREPARE REQUEST
    const char *cmd[] = { "/system/resource/print", "=.proplist=uptime" };

    clear_sentence( &mkSentence );
    add_word_to_sentence( cmd[0], &mkSentence );
    add_word_to_sentence( cmd[1], &mkSentence );

	// 2. WAIT FOR EXECUTION
	ExecuteRequest();

    // 3. PROCESS ANSWER
    if ( !IsRequestSuccessful() )
        return false;

    // Parse answer
    if ( !parseAnswer( "uptime" ) )
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
	if ( !EnableInterface( ether1 ) )
		return false;

	return true;
}


// https://wiki.mikrotik.com/wiki/Manual:Making_a_simple_wireless_AP
bool Mikrotik::CreateAP( const char *ssid, const char *pass, TInterface iface )
{
	// 0. PREPARE ROUTER CONFIGURATION
    // Only WiFi allowed here
    if ( iface == ether1 )
        return false;

    if ( !SetDhcpState( ether1, DhcpClient, Disabled ) )
		return false;

    DisableInterface( ether1 );
    DisableInterface( iface == wifi5g ? wifi2g : wifi5g );

    if ( !SetDhcpState( iface, DhcpClient, Disabled ) )
        return false;

    if ( !SetDhcpState( iface, DhcpServer, Enabled ) )
        return false;

    if ( pass != NULL )
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
    SafeSnprintf( wlanID, sizeof( wlanID ), "=.id=%s", IFACE_NAME_TABLE[iface] );

    char band[50];
    if ( iface == wifi2g )
    	SafeSnprintf( band, sizeof( band ), "%s", "=band=2ghz-b/g/n" );
    else
    	SafeSnprintf( band, sizeof( band ), "%s", "=band=5ghz-a/n/ac" );

    char apName[MIKROTIK_MAX_ANSWER];
    SafeSnprintf( apName, sizeof( apName ), "=ssid=%s", ssid );

    const char *cmd[] = { "/interface/wireless/set", "=frequency=auto", "=mode=ap-bridge", "=disabled=false" };

    clear_sentence( &mkSentence );
    add_word_to_sentence( cmd[0], &mkSentence );
    add_word_to_sentence( wlanID, &mkSentence );
    add_word_to_sentence( apName, &mkSentence );
    add_word_to_sentence( cmd[1], &mkSentence );
    add_word_to_sentence( cmd[2], &mkSentence );
    add_word_to_sentence( cmd[3], &mkSentence );

    char sp[50] = "=security-profile=";
    if ( pass == nullptr )
        strcat( sp, SP_DEFAULT );
    else
        strcat( sp, SP_MODE_ACCESS_POINT );

    add_word_to_sentence( sp, &mkSentence );

	// 2. WAIT FOR EXECUTION
    ExecuteRequest();

    // 3. PROCESS ANSWER
    if ( !IsRequestSuccessful() )
        return false;

    SafeSnprintf( answer, MIKROTIK_MAX_ANSWER, "done" );
    return true;
}


// https://wiki.mikrotik.com/wiki/Manual:Wireless_AP_Client#Additional_Station_Configuration
bool Mikrotik::ConnectToWiFi( const char *ssid, const char *pass, TInterface iface )
{
	// 0. PRECONFIG ROUTER
    // Only WiFi allowed here
    if ( iface == ether1 )
        return false;

    if ( !SetDhcpState( ether1, DhcpClient, Disabled ) )
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
    SafeSnprintf( wlanID, sizeof( wlanID ), "=.id=%s", IFACE_NAME_TABLE[iface] );

    char networkName[MIKROTIK_MAX_ANSWER];
    SafeSnprintf( networkName, sizeof( networkName ), "=ssid=%s", ssid );

    const char *cmd[] = { "/interface/wireless/set", "=mode=station", "=disabled=false" };

    clear_sentence( &mkSentence );
    add_word_to_sentence( cmd[0],      &mkSentence );
    add_word_to_sentence( wlanID,      &mkSentence );
    add_word_to_sentence( networkName, &mkSentence );
    add_word_to_sentence( cmd[1],      &mkSentence );
    add_word_to_sentence( cmd[2],      &mkSentence );

    char sp[50] = "=security-profile=";
    if ( pass == NULL )
        strcat( sp, SP_DEFAULT );
    else
        strcat( sp, SP_MODE_STATION );

    add_word_to_sentence( sp, &mkSentence );

	// 2. WAIT FOR EXECUTION
    ExecuteRequest();

    // 3. PROCESS ANSWER
    if ( !IsRequestSuccessful() )
        return false;

    SafeSnprintf( answer, MIKROTIK_MAX_ANSWER, "done" );
    return true;
}


bool Mikrotik::EnableInterface( TInterface iface )
{
    // 1. PREPARE REQUEST
    const char cmdWiFi[] = "/interface/wireless/enable";
    const char cmdEht[]  = "/interface/ethernet/enable";

    const char *cmd = ( iface == ether1 ) ? cmdEht : cmdWiFi;

    char id[15];
    SafeSnprintf( id, sizeof( id ), "=.id=%s", IFACE_NAME_TABLE[iface] );

    clear_sentence( &mkSentence );
    add_word_to_sentence( cmd, &mkSentence );
    add_word_to_sentence( id,  &mkSentence );

	// 2. WAIT FOR EXECUTION
    ExecuteRequest();

    // 3. PROCESS ANSWER
    if ( !IsRequestSuccessful() )
        return false;

    return true;
}


bool Mikrotik::DisableInterface( TInterface iface )
{
	// 0. Disable secondary iface services
    if ( !SetDhcpState( iface, DhcpClient, Disabled ) )
        return false;

    if ( iface != ether1 )  // ether1 not supports dhcp-server mode
		if ( !SetDhcpState( iface, DhcpServer, Disabled ) )
			return false;

    // 1. PREPARE REQUEST
	const char cmdWiFi[] = "/interface/wireless/disable";
	const char cmdEht[]  = "/interface/ethernet/disable";

	const char *cmd = ( iface == ether1 ) ? cmdEht : cmdWiFi;

    char id[15];
    SafeSnprintf( id, sizeof( id ), "=.id=%s", IFACE_NAME_TABLE[iface] );

    clear_sentence( &mkSentence );
    add_word_to_sentence( cmd, &mkSentence );
    add_word_to_sentence( id,  &mkSentence );

	// 2. WAIT FOR EXECUTION
    ExecuteRequest();

    // 3. PROCESS ANSWER
    if ( !IsRequestSuccessful() )
        return false;

    return true;
}


// Router has few preconfigured DHCP-client params for ethernet and wifi ifaces
bool Mikrotik::SetDhcpState( TInterface iface, TDhcpMode dhcpMode, TEnableState state )
{
    // ether1 can be client only
    if ( ( iface == ether1 ) && ( dhcpMode == DhcpServer ) )
        return false;

    char id[10] = {0};
    if ( !getDhcpID( id, iface, dhcpMode ) )
        return false;

    // 1. PREPARE REQUEST;
    const char *cmdServer = "/ip/dhcp-server/set";
    const char *cmdClient = "/ip/dhcp-client/set";

    char cmdWlanId[30];
    SafeSnprintf( cmdWlanId, sizeof( cmdWlanId ), "=.id=%s", id );

    char cmdState[30];
    SafeSnprintf( cmdState, sizeof( cmdState ), "=disabled=%s", state == Enabled ? "false" : "true" );

    clear_sentence( &mkSentence );
    add_word_to_sentence( dhcpMode == DhcpServer ? cmdServer : cmdClient, &mkSentence );
    add_word_to_sentence( cmdWlanId, &mkSentence );
    add_word_to_sentence( cmdState,  &mkSentence );

	// 2. WAIT FOR EXECUTION
    ExecuteRequest();

    // 3. PROCESS ANSWER
    if ( !IsRequestSuccessful() )
        return false;

    return true;
}


bool Mikrotik::GetDhcpState( TInterface iface, TDhcpMode dhcpMode, TEnableState *pState )
{
    // ether1 can be client only
    if ( ( iface == ether1 ) && ( dhcpMode == DhcpServer ) )
        return false;

    // 1. PREPARE REQUEST
    const char *cmdServer = "/ip/dhcp-server/print";
    const char *cmdClient = "/ip/dhcp-client/print";

    char cmdWlan[30];
    SafeSnprintf( cmdWlan, sizeof( cmdWlan ), "?interface=%s", IFACE_NAME_TABLE[iface] );

    char cmdOpt[30];
    SafeSnprintf( cmdOpt, sizeof( cmdOpt ), "=.proplist=disabled" );

    clear_sentence( &mkSentence );
    add_word_to_sentence( dhcpMode == DhcpServer ? cmdServer : cmdClient, &mkSentence );
    add_word_to_sentence( cmdWlan, &mkSentence );
    add_word_to_sentence( cmdOpt,  &mkSentence );

	// 2. WAIT FOR EXECUTION
    ExecuteRequest();

    // 3. PROCESS ANSWER
    if ( !IsRequestSuccessful() )
        return false;

    if ( !parseAnswer( "disabled" ) )
        return false;

    if ( strstr( answer, "false" ) )
        *pState = Enabled;
    else if ( strstr( answer, "true" ) )
        *pState = Disabled;
    else
        return false;

    return true;
}


void Mikrotik::ExecuteRequest()
{
	isRequestWaiting = true;
	while( isRequestWaiting );
}


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

	return isConnected;
}


void Mikrotik::Disconnect()
{
	__disconnect( MIKROTIK_SOCK_NUM );
	__close( MIKROTIK_SOCK_NUM );

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
    char cNull[1] = {0};
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
    char *szMD5Challenge;

    write_word( "/login" );
    write_word( "" );

    block->ReInit();
    read_sentence();
    block->Print();

    char tmp[100] = { 0 };
    strncpy( tmp, block->GetNextWord( block->GetFirstWord() ), sizeof( tmp ) - 1 );

    // extract md5 string from the challenge sentence
    szMD5Challenge = strtok( tmp,  "=" );
    szMD5Challenge = strtok( NULL, "=" );

    printf( "MD5 of challenge = %s\n", szMD5Challenge );

    char szMD5PasswordToSend[33] = { 0 };
    generateResponse( szMD5PasswordToSend, szMD5Challenge, password );

    TMKSentence sentence;
    clear_sentence( &sentence );
    add_word_to_sentence( "/login", &sentence );

    char name[100];
    SafeSnprintf( name, sizeof( name ), "=name=%s", username );
    add_word_to_sentence( name, &sentence );

    char resp[100];
    SafeSnprintf( resp, sizeof( resp ), "=response=00%s", szMD5PasswordToSend );
    add_word_to_sentence( resp, &sentence );

    write_sentence( &sentence );

    block->ReInit();
    read_sentence();
    block->Print();

    return ( strstr( block->GetFirstWord(), "!done" ) != NULL );
}


bool Mikrotik::Login()
{
	// CONNECTION
	uint8_t d_ip[] = { 192, 168, 60, 1 };
	const uint16_t d_port = 8728;

	//#warning "ACHTUNG! Default credentials!"
	char default_username[] = "admin";
	char default_password[] = "";

    const int NUM_OF_LOGIN_TRY = 5;
    for ( int i = 0; i < NUM_OF_LOGIN_TRY; i++ )
    {
		if ( isConnected )
			Disconnect();

		isLogged = false;

		if ( !Connect( d_ip, d_port ) )
			return false;

        debugPrintf( "Login attempt %i of %i... ", i + 1, NUM_OF_LOGIN_TRY );
        if ( try_to_log_in( default_username, default_password ) )
        {
        	debugPrintf( "success\n" );
            isLogged = true;
            break;
        }
        else
        {
            debugPrintf( "FAILED!\n" );
            Disconnect();
        }
    }

    block->ReInit();
    return isLogged;
}


// @warning Should be called INDIRECTLY from Mikrotik::Spin() <- Network::Spin() <- NetworkLoop() [RTOS TASK]
bool Mikrotik::ProcessRequest()
{
	if ( !isLogged )
		if ( !Login() )
		{
			SafeSnprintf( answer, MIKROTIK_MAX_ANSWER, "MIKROTIK: can't log in" );
			return false;
		}

	write_sentence( &mkSentence );
	clear_sentence( &mkSentence );

	read_block();
	block->Print();

	return true;
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

    debugPrintf( "FAILED to parse answer!\nCan't find %s\n", key );
    return false;
}


// Text "!done" in router answer means successful execution of request
bool Mikrotik::IsRequestSuccessful()
{
    const char* pWord = block->GetFirstWord();
    do
    {
        if ( strstr( pWord, "!done" ) == pWord )
            return true;

        pWord = block->GetNextWord( pWord );
    } while ( pWord );

    return false;
}


bool Mikrotik::getDhcpID( char *pID, TInterface iface, TDhcpMode dhcpMode )
{
    // 1. PREPARE REQUEST
    char cmdIface[20];
    SafeSnprintf( cmdIface, sizeof( cmdIface ), "?interface=%s", IFACE_NAME_TABLE[iface] );

    const char *cmdServer = "/ip/dhcp-server/print";
    const char *cmdClient = "/ip/dhcp-client/print";
    const char *cmdOpt    = "=.proplist=.id";

    clear_sentence( &mkSentence );
    add_word_to_sentence( dhcpMode == DhcpServer ? cmdServer : cmdClient, &mkSentence );
    add_word_to_sentence( cmdIface, &mkSentence );
    add_word_to_sentence( cmdOpt,   &mkSentence );

	// 2. WAIT FOR EXECUTION
    ExecuteRequest();

    // 3. PROCESS ANSWER
    if ( !IsRequestSuccessful() )
        return false;

    // Extract security profile ID from answer
    bool success = parseAnswer( ".id" );
    if ( success )
        strcpy( pID, answer );

    return success;
}


bool Mikrotik::getSecurityProfileID( char *spID, const char *mode )
{
    // 1. PREPARE REQUEST
    char reqSP[20];
    SafeSnprintf( reqSP, sizeof( reqSP ), "?name=%s", mode );

    const char *cmd[] = { "/interface/wireless/security-profiles/print", "=.proplist=.id" };

    clear_sentence( &mkSentence );
    add_word_to_sentence( cmd[0], &mkSentence );
    add_word_to_sentence( reqSP,  &mkSentence );
    add_word_to_sentence( cmd[1], &mkSentence );

	// 2. WAIT FOR EXECUTION
    ExecuteRequest();

    // 3. PROCESS ANSWER
    if ( !IsRequestSuccessful() )
        return false;

    // Extract security profile ID from answer
    bool success = parseAnswer( ".id" );
    if ( success )
        strcpy( spID, answer );

    return success;
}


bool Mikrotik::changeAccessPointPass( const char *pass )
{
    char spID[10] = {0};
    if ( !getSecurityProfileID( spID, SP_MODE_ACCESS_POINT ) )
        return false;

    const char cmd[] = "/interface/wireless/security-profiles/set";

    char id[25];
    SafeSnprintf( id, sizeof( id ), "=.id=%s", spID );

    char newPassWpa2[100];
    SafeSnprintf( newPassWpa2, sizeof( newPassWpa2 ), "=wpa2-pre-shared-key=%s", pass );

    char newPassSupplicant[100];
    SafeSnprintf( newPassSupplicant, sizeof( newPassSupplicant ), "=supplicant-identity=%s", pass );

    clear_sentence( &mkSentence );
    add_word_to_sentence( cmd,               &mkSentence );
    add_word_to_sentence( id,                &mkSentence );
    add_word_to_sentence( newPassWpa2,       &mkSentence );
    add_word_to_sentence( newPassSupplicant, &mkSentence );

	// 2. WAIT FOR EXECUTION
    ExecuteRequest();

    // 3. PROCESS ANSWER
    if ( !IsRequestSuccessful() )
        return false;

    return true;
}


bool Mikrotik::changeWiFiStationPass( const char *pass )
{
    char spID[10] = {0};
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

    clear_sentence( &mkSentence );
    add_word_to_sentence( cmd,            &mkSentence );
    add_word_to_sentence( id,             &mkSentence );
    add_word_to_sentence( wpaPass,        &mkSentence );
    add_word_to_sentence( wpa2Pass,       &mkSentence );
    add_word_to_sentence( supplicantPass, &mkSentence );

	// 2. WAIT FOR EXECUTION
    ExecuteRequest();

    // 3. PROCESS ANSWER
    if ( !IsRequestSuccessful() )
        return false;

    return true;
}


void Mikrotik::clear_sentence( TMKSentence *pSentence )
{
    memset( pSentence, 0, sizeof( TMKSentence ) );
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
    debugPrintf( "Write: '%s'\n", pWord );
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

            if ( iLen )
            	debugPrintf( "received %i byte(s)\nlefts - %i byte(s)\n", iBytesRead, iLen );

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
    }

    // if any errors, get the next sentence
    if ( retval == Sentence_Trap || retval == Sentence_Fatal )
        read_sentence();

    block->AddEndOfSentence();

    if ( retval == Sentence_None )
    	return true;
    else
    	return false;
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
