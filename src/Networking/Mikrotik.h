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

// Router interface names
#define IFACE_ETHERNET	"ether1"
#define IFACE_WIFI2G	"wlan1"
#define IFACE_WIFI5G	"wlan2"

typedef enum
{
	ether1 = 0,
	wifi2g,
	wifi5g
} TInterface;

typedef enum
{
	DhcpServer = 0,
	DhcpClient
} TDhcpMode;

typedef enum
{
	Disabled = 0,
	Enabled
} TEnableState;


// @todo Refactor ---> 'Singleton'
class Mikrotik
{
public:
	Mikrotik();
	void Spin();
	void Activate();

	bool GetNetworkParams( char *ip, char *mask, char *gw );
	bool GetUpTime( char *buffer );

	bool CreateAP( const char *ssid, const char *pass, TInterface iface );
	bool ConnectToWiFi( const char *ssid, const char *pass, TInterface iface );

	bool EnableWirelessNetwork( TInterface iface );
	bool DisableWirelessNetwork( TInterface iface );

	bool SetDhcpState( TInterface iface, TEnableState state, TDhcpMode dhcpMode );
	bool GetDhcpState( TInterface iface, TDhcpMode dhcpMode, TEnableState *pState );

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

	bool getDhcpID( char *pID, TInterface iface, TDhcpMode dhcpMode );
	bool getSecurityProfileID( char *spID, const char *mode );

	bool changeAccessPointPass( const char *pass );
	bool changeWiFiStationPass( const char *pass );
};

#endif
