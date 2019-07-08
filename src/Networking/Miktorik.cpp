#include "Mikrotik.h"
#include "Platform.h"
#include "RepRap.h"
#include "W5500Ethernet/Wiznet/Ethernet/socketlib.h"

constexpr size_t ClientStackWords = 550;
static Task<ClientStackWords> clientTask;


Mikrotik::Mikrotik() : isRequestWaiting(false)
{
	memset( answer, 0, 1024 );
	answSize = 0;
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
	strcpy( buffer, answer );

	// 3. PROCESS ANSWER

	//printBlock( &stBlock );
	for ( int i = 0; i < stBlock.iLength; i++ )
	{
		struct Sentence *pSentence = stBlock.stSentence[i];

		if ( strstr( pSentence->szSentence[0], "!re" ) )
		{
			for ( int j=1; j<pSentence->iLength; j++ )
			{
				if ( strstr( pSentence->szSentence[j], "uptime" ) )
				{
					debugPrintf( "%s\n", pSentence->szSentence[j] );
					sprintf( answer, &pSentence->szSentence[j][8] );
				}
			}
		}
	}

	// FINISH
	// clear the sentence
	clearSentence( &stSentence );

	return true;
}


bool Mikrotik::Connect( uint8_t *d_ip, uint16_t d_port )
{
	if ( isConnected )
		Disconnect();

	const uint16_t s_port  = 1234;
	const uint8_t protocol = 0x01;  //Sn_MR_TCP

	int se = __socket( socket, protocol, s_port, 0 );

	if ( se != socket )
	{
		debugPrintf( "Socket error %i\n", se );
		return false;
	}

	// Set keep-alive
	// Should be placed between socket() and connect()
	setSn_KPALVTR( socket, 2 );

	debugPrintf( "Connecting to: %d.%d.%d.%d:%d... ", d_ip[0], d_ip[1], d_ip[2], d_ip[3], d_port );
	int ce = __connect( socket, d_ip, d_port );

	if ( ce != 1 /* SOCK_OK */ )
	{
		debugPrintf( "connection error %i\n", ce );
		__close( socket );
		return false;
	}

	debugPrintf( "CONNECTED!\n" );

	isConnected = true;
	return isConnected;
}


void Mikrotik::Disconnect()
{
	__disconnect( socket );
	__close( socket );

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
		int iLoginResult = login( socket, default_username, default_password );
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
	debugPrintf( "\nTEST REQ PROC\n\n" );

	if ( !isLogged )
		if ( !Login() )
		{
			SafeSnprintf( answer, MIKROTIK_MAX_ANSWER, "MIKROTIK: can't log in" );
			return false;
		}

	writeSentence( socket, &stSentence );

	// receive and print response block from the API
	stBlock = readBlock( socket );

	return true;
}


void Mikrotik::TestN()
{
	if ( !isLogged )
		for( int i = 0; i < 5; i++ )
			if ( Login() )
				break;

	if ( !isLogged )
		return;

	// initialize first sentence
	initializeSentence( &stSentence );

	addWordToSentence( &stSentence, "/system/resource/print" );
	addWordToSentence( &stSentence, "=.proplist=uptime" );
	writeSentence( socket, &stSentence );

	// receive and print response block from the API
	stBlock = readBlock( socket );
	//printBlock( &stBlock );

    for ( int i = 0; i < stBlock.iLength; i++ )
    {
        struct Sentence *pSentence = stBlock.stSentence[i];

        if ( strstr( pSentence->szSentence[0], "!re" ) )
        {
            for ( int j=1; j<pSentence->iLength; j++ )
            {
                if ( strstr( pSentence->szSentence[j], "uptime" ) )
                    debugPrintf( "%s\n", pSentence->szSentence[j] );
            }
        }
    }

	// FINISH
	// clear the sentence
	clearSentence( &stSentence );
}
