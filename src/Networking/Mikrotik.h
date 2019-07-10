#ifndef SRC_NETWORKING_MIKROTIK_H_
#define SRC_NETWORKING_MIKROTIK_H_

#include "RepRapFirmware.h"
#include "W5500Ethernet/Wiznet/Ethernet/W5500/w5500.h"

extern "C"
{
#include "W5500Ethernet/Wiznet/Ethernet/Mikrotik/mikrotik-api.h"
}

#define IP_V4_TEXT_MAXLEN	17
#define MAC_TEXT_MAXLEN		18
#define MIKROTIK_MAX_ANSWER	100
#define MIKROTIK_SOCK_NUM	(uint8_t)5

// Security profile names
#define SP_DEFAULT			    "default"
#define SP_MODE_ACCESS_POINT	"omni_AP"
#define SP_MODE_STATION			"omni_station"

class Mikrotik
{
public:
	Mikrotik();
	void Spin();
	void Activate();

	bool GetNetworkParams( char *ip, char *mask, char *gw );
	bool GetUpTime( char *buffer );

	bool CreateAP( const char *ssid, const char *pass, bool is5G );
	bool ConnectToWiFi( const char *ssid, const char *pass, bool is5G );

	bool EnableWirelessNetwork( bool is5G );
	bool DisableWirelessNetwork( bool is5G );

	volatile bool isRequestWaiting;

private:
	bool isLogged    = false;
	bool isConnected = false;

	// Mikrotik variables
	struct Sentence stSentence;
	struct Block    stBlock;

	char answer[MIKROTIK_MAX_ANSWER];

	bool Connect( uint8_t *d_ip, uint16_t d_port );
	void Disconnect();
	bool Login();
	bool ProcessRequest();

	bool parseAnswer( const char *pReqVal );

	bool getSecurityProfileID( char *spID, const char *mode );
	bool changeAccessPointPass( const char *pass );
	bool changeWiFiStationPass( const char *pass );
};

#endif
