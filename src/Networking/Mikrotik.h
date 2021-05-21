#ifndef SRC_NETWORKING_MIKROTIK_H_
#define SRC_NETWORKING_MIKROTIK_H_

#ifndef __LINUX_DBG
    #include "RepRapFirmware.h"
	#include "GCodes/GCodeResult.h"
    #include "W5500Ethernet/Wiznet/Ethernet/W5500/w5500.h"
    #define MIKROTIK_SOCK_NUM   (uint8_t)5
#endif

#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include "MKTBlock.h"
#include "md5/md5.h"

#define MAX_WORDS_IN_SENTENCE   32
#define MIKROTIK_MAX_ANSWER     100

// Security profile names
#define SP_DEFAULT              "default"
#define SP_MODE_ACCESS_POINT    "omni_AP"
#define SP_MODE_STATION         "omni_station"

// Router interface names
#define IFACE_ETHERNET  "ether1"
#define IFACE_WIFI2G    "wlan1"
#define IFACE_WIFI5G    "wlan2"


#define MIKROTIK_IP_WIFI_2G "192.168.24.1/24"
#define MIKROTIK_IP_WIFI_5G "192.168.50.1/24"
#define MIKROTIK_DST_ADDRESS "0.0.0.0/0"

#define MIKROTIK_WIFI_2G_DEF_BAND "=band=2ghz-b/g/n"
#define MIKROTIK_WIFI_5G_DEF_BAND "=band=5ghz-a/n/ac"

#define MIKROTIK_SUCCESS_ANSWER "!done"


                            //******************//
                            // API REQUESTS CMD //
                            //******************//

#define CMD_LOGIN                               "/login"

#define CMD_INTERFACE                           "/interface"

    #define CMD_INTERFACE_ETHERNET              CMD_INTERFACE "/ethernet"

        #define CMD_INTERFACE_ETHERNET_PRINT    CMD_INTERFACE_ETHERNET "/print"
        #define CMD_INTERFACE_ETHERNET_ENABLE   CMD_INTERFACE_ETHERNET "/enable"
        #define CMD_INTERFACE_ETHERNET_DISABLE  CMD_INTERFACE_ETHERNET "/disable"

    #define CMD_INTERFACE_WIRELESS              CMD_INTERFACE "/wireless"

        #define CMD_INTERFACE_WIRELESS_SET      CMD_INTERFACE_WIRELESS "/set"
        #define CMD_INTERFACE_WIRELESS_SCAN     CMD_INTERFACE_WIRELESS "/scan"
        #define CMD_INTERFACE_WIRELESS_PRINT    CMD_INTERFACE_WIRELESS "/print"
        #define CMD_INTERFACE_WIRELESS_ENABLE   CMD_INTERFACE_WIRELESS "/enable"
        #define CMD_INTERFACE_WIRELESS_DISABLE  CMD_INTERFACE_WIRELESS "/disable"

        #define CMD_INTERFACE_WIRELESS_SEC_PROF CMD_INTERFACE_WIRELESS "/security-profiles"

            #define CMD_INTERFACE_WIRELESS_SEC_PROF_SET     CMD_INTERFACE_WIRELESS_SEC_PROF "/set"
            #define CMD_INTERFACE_WIRELESS_SEC_PROF_PRINT   CMD_INTERFACE_WIRELESS_SEC_PROF "/print"

#define CMD_SYSTEM                              "/system"

    #define CMD_SYSTEM_RESOURCE                 CMD_SYSTEM "/resource"

        #define CMD_SYSTEM_RESOURCE_PRINT       CMD_SYSTEM_RESOURCE "/print"

#define CMD_IP                                  "/ip"

    #define CMD_IP_ADDRESS                      CMD_IP  "/address"

        #define CMD_IP_ADDRESS_SET              CMD_IP_ADDRESS  "/set"
        #define CMD_IP_ADDRESS_PRINT            CMD_IP_ADDRESS  "/print"
        #define CMD_IP_ADDRESS_DISABLE          CMD_IP_ADDRESS  "/disable"

    #define CMD_IP_DHCP_CLIENT                  CMD_IP "/dhcp-client"

        #define CMD_IP_DHCP_CLIENT_SET          CMD_IP_DHCP_CLIENT "/set"
        #define CMD_IP_DHCP_CLIENT_PRINT        CMD_IP_DHCP_CLIENT "/print"

    #define CMD_IP_DHCP_SERVER                  CMD_IP "/dhcp-server"

        #define CMD_IP_DHCP_SERVER_SET          CMD_IP_DHCP_SERVER "/set"
        #define CMD_IP_DHCP_SERVER_PRINT        CMD_IP_DHCP_SERVER "/print"

	#define CMD_IP_ROUTE						CMD_IP "/route"

		#define CMD_IP_ROUTE_ADD				CMD_IP_ROUTE "/add"
		#define CMD_IP_ROUTE_PRINT				CMD_IP_ROUTE "/print"
		#define CMD_IP_ROUTE_REMOVE				CMD_IP_ROUTE "/remove"
		#define CMD_IP_ROUTE_GATEWAY			"/gateway"
		#define CMD_IP_ROUTE_DST				"/dst-address"


                            //*********************//
                            // API REQUESTS PARAMS //
                            //*********************//

#define SET_SIGN    "="
#define GET_SIGN    "?"

#define SET_PARAM( param )          SET_SIGN param SET_SIGN
#define SET_PARAM_V( param, value ) SET_SIGN param SET_SIGN value

#define REQ_PARAM( param )          GET_SIGN param SET_SIGN
#define REQ_PARAM_V( param, value ) GET_SIGN param SET_SIGN value

#define GREP_OPT( param )           SET_PARAM_V( ".proplist", param )

// params
#define P_ID            ".id"
#define P_SSID          "ssid"
#define P_NAME          "name"
#define P_MODE          "mode"
#define P_UPTIME        "uptime"
#define P_RUNNING       "running"
#define P_ADDRESS       "address"
#define P_DYNAMIC       "dynamic"
#define P_PASSWORD		"password"
#define P_DURATION      "duration"
#define P_DISABLED      "disabled"
#define P_RESPONSE      "response"
#define P_FREQUENCY     "frequency"
#define P_INTERFACE     "interface"
#define P_GATEWAY		"gateway"
#define P_DST_ADDRESS	"dst-address"
// /ip route add dst-address=0.0.0.0/0 gateway=yyy.zzz.xxx.yyy

#define P_SECURITY_PROFILE      "security-profile"
#define P_SUPPLICANT_IDENTITY   "supplicant-identity"
#define P_WPA2_PRE_SHARED_KEY   "wpa2-pre-shared-key"

// values
#define V_AUTO      "auto"
#define V_5180MHz   "5180"
#define V_TRUE      "true"
#define V_FALSE     "false"
#define V_STATION   "station"
#define V_AP_BRIDGE "ap-bridge"


typedef enum
{
    none = 0,
    ether1,
    wifi2g,
    wifi5g
} TInterface;

typedef enum
{
    invalid = 0,
    AccessPoint,
    Station
} TWifiMode;

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

typedef struct {
    const char *pWord[MAX_WORDS_IN_SENTENCE];
    uint16_t length;
} TMKSentence;

typedef enum
{
	Booting = 0,
	Connected,
	Disconnected,
	Connecting
} TStatus;

class Mikrotik
{
public:
    Mikrotik();

    void Spin();

    bool GetUpTime( char *buffer );

    bool ConnectToEthernet();
    bool CreateAP( const char *ssid, const char *pass, TInterface iface );
    bool ConnectToWiFi( const char *ssid, const char *pass, TInterface iface );

    bool EnableInterface( TInterface iface );
    bool SetGateway( const char *gateway );
    bool RefreshGateway();
    bool RemoveGateway();
    bool DisableInterface( TInterface iface );
    bool GetCurrentInterface( TInterface *iface );
    bool GetWifiMode( TInterface iface, TWifiMode *pMode );
    bool GetSSID( TInterface iface, char *ssid );
    bool IsNetworkAvailable( TInterface iface );

    bool SetDhcpState( TInterface iface, TDhcpMode dhcpMode, TEnableState state );
    bool GetDhcpState( TInterface iface, TDhcpMode dhcpMode, TEnableState *pState );

    bool GetInterfaceIP( TInterface iface, char *ip );
    bool GetInterfaceIP( TInterface iface, char *ip, bool isStatic );
    bool SetStaticIP( TInterface iface, const char *ip );
    bool RemoveStaticIP( TInterface iface );
    bool IsRouterAvailable();
    void SendNetworkStatus();
    void DisableInterface();
    void Check();
    GCodeResult Configure(GCodeBuffer& gb, const StringRef& reply);
    GCodeResult SearchWiFiNetworks(GCodeBuffer& gb, const StringRef& reply);
    GCodeResult StaticIP(GCodeBuffer& gb, const StringRef& reply);
    GCodeResult DHCPState(GCodeBuffer& gb, const StringRef& reply);

    uint16_t ScanWiFiNetworks( TInterface iface, uint8_t duration, char *pBuffer, uint32_t MAX_BUF_SIZE );

    char ip[20];
    char mask[20];
    char gateway[20];
    char ssid[40];
    char password[40];
    uint8_t maskBit;
    bool isStatic;

    TInterface interface;
    TWifiMode mode;
    TStatus status;

private:
    volatile bool isRequestWaiting = false;

    volatile bool isLogged    = false;
    volatile bool isConnected = false;
    volatile bool notResponse  = false;

#ifdef __LINUX_DBG
    int MIKROTIK_SOCK_NUM = 0;
#endif

    volatile int iLittleEndian;

    MKTBlock *block;

    char answer[MIKROTIK_MAX_ANSWER];
    char gatewayId[MAX_WORDS_IN_SENTENCE];

    // Connection
#ifdef __LINUX_DBG
    bool Connect( const char *d_ip, uint16_t d_port );
#else
    bool Connect( uint8_t *d_ip, uint16_t d_port );
#endif
    void Disconnect();

    // Authorization
    void generateResponse( char *szMD5PasswordToSend, char *szMD5Challenge, const char *password );
    bool try_to_log_in( char *username, char *password );
    bool Login(uint8_t numLoginTry);

    // Request execution
    bool ProcessRequest();  // DON'T CALL MANUALLY on DUET3D!!!
    bool LoginRequest();

    // Checking response
    bool IsRequestSuccessful();
    bool parseAnswer( const char *pReqVal );

    bool isInterfaceActive( TInterface iface );
    bool getStaticIpId( char *pID, TInterface iface );
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
