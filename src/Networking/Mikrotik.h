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

class Mikrotik
{
public:
	Mikrotik();
	void Spin();
	void Activate();

	bool GetNetworkParams( char *ip, char *mask, char *gw );
	bool GetUpTime( char *buffer );

	volatile bool isRequestWaiting;

private:
	uint8_t socket = 5;
	bool isLogged    = false;
	bool isConnected = false;

	// Mikrotik variables
	struct Sentence stSentence;
	struct Block stBlock;

	char answer[MIKROTIK_MAX_ANSWER];
	uint16_t answSize;

	bool Connect( uint8_t *d_ip, uint16_t d_port );
	void Disconnect();
	bool Login();
	bool ProcessRequest();

	void TestN();
};

#endif
