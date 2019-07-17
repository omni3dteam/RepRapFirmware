#ifndef SRC_NETWORKING_MIKROTIK_H_
#define SRC_NETWORKING_MIKROTIK_H_

#include "RepRapFirmware.h"
#include "W5500Ethernet/Wiznet/Ethernet/W5500/w5500.h"

#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include "MKTBlock.h"
#include "md5/md5.h"


//#define __SHOW_DEBUG	debugPrintf( "\n%s() -> %d\n\n", __func__, __LINE__ );

#define IP_V4_TEXT_MAXLEN   17
#define MAC_TEXT_MAXLEN     18
#define MIKROTIK_MAX_ANSWER	100
#define MIKROTIK_SOCK_NUM   (uint8_t)6

// Security profile names
#define SP_DEFAULT              "default"
#define SP_MODE_ACCESS_POINT    "omni_AP"
#define SP_MODE_STATION         "omni_station"

// Router interface names
#define IFACE_ETHERNET  "ether1"
#define IFACE_WIFI2G    "wlan1"
#define IFACE_WIFI5G    "wlan2"

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

typedef enum
{
    Sentence_None = 0,
    Sentence_Done,
    Sentence_Trap,
    Sentence_Fatal
} TSentenceRetVal;

#define MAX_WORDS_IN_SENTENCE   32
typedef struct {
    const char *pWord[MAX_WORDS_IN_SENTENCE];
    uint16_t length;
} TMKSentence;


class Mikrotik
{
public:
    Mikrotik();

	void Spin();
	void Activate();

//    bool GetNetworkParams( char *ip, char *mask, char *gw );
    bool GetUpTime( char *buffer );
    bool CreateAP( const char *ssid, const char *pass, TInterface iface );
    bool ConnectToWiFi( const char *ssid, const char *pass, TInterface iface );
    bool EnableWirelessNetwork( TInterface iface );
    bool DisableWirelessNetwork( TInterface iface );
    bool SetDhcpState( TInterface iface, TDhcpMode dhcpMode, TEnableState state );  // @todo Refactor swap 2 and 3
    bool GetDhcpState( TInterface iface, TDhcpMode dhcpMode, TEnableState *pState );

    volatile bool isRequestWaiting;

private:
    volatile bool isLogged = false;
    volatile bool isConnected = false;

    volatile int iLittleEndian;

    MKTBlock *block;

    char answer[MIKROTIK_MAX_ANSWER];

    // Connection
    bool Connect( uint8_t *d_ip, uint16_t d_port );
    void Disconnect();

    // Authorization
    void generateResponse( char *szMD5PasswordToSend, char *szMD5Challenge, const char *password );
    bool try_to_log_in( char *username, char *password );
    bool Login();

    // Request execution
    bool ProcessRequest();  // DON'T CALL MANUALY!!!

    // Checking response
    bool IsRequestSuccessful();
    bool parseAnswer( const char *pReqVal );

    bool getDhcpID( char *pID, TInterface iface, TDhcpMode dhcpMode );
    bool getSecurityProfileID( char *spID, const char *mode );
    bool changeAccessPointPass( const char *pass );
    bool changeWiFiStationPass( const char *pass );

    // Low level communication

    // Send data
    TMKSentence mkSentence;
    static void clear_sentence( TMKSentence *pSentence );
    static void add_word_to_sentence( const char *pWord, TMKSentence *pSentence );
    void write_sentence( TMKSentence *pSentence );
    void write_word( const char *pWord );
    void write_len( int iLen );

    // Receive data
    int read_len();
    bool read_word( char *pWord );
    bool read_sentence();
    void read_block();

    static int isLittleEndian();
    static void md5ToBinary( char *szHex, char *challenge );
    static void md5DigestToHexString( md5_byte_t *binaryDigest, char *szReturn );
    static char hexStringToChar( char *cToConvert );

    int  __write( int __fd, const void *__buf, size_t __nbyte );
    int __read (int __fd, void *__buf, size_t __nbyte);
};

#endif
