/********************************************************************
 * Some definitions
 * Word = piece of API code
 * Sentence = multiple words
 * Block = multiple sentences (usually in response to a sentence request)
 *

	int fdSock;
	int iLoginResult;
	struct Sentence stSentence;
	struct Block stBlock;

	fdSock = apiConnect("10.0.0.1", 8728);

	// attempt login
	iLoginResult = login(fdSock, "admin", "adminPassword");

	if (!iLoginResult)
	{
		apiDisconnect(fdSock);
		//Printf("Invalid username or password.\n");
		exit(1);
	}

	// initialize, fill and send sentence to the API
	initializeSentence(&stSentence);
	addWordToSentence(&stSentence, "/interface/getall");
	writeSentence(fdSock, &stSentence);

	// receive and print block from the API
	stBlock = readBlock(fdSock);
	printBlock(&stBlock);

	apiDisconnect(fdSock);

 ********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "md5.h"
#include "mikrotik-api.h"
#include "RepRapFirmware.h"
#include "../W5500/w5500.h"
#include "../socketlib.h"

// endianness variable...global
int iLittleEndian;

extern void debugPrintf(const char *, ...);


int  __write( int __fd, const void *__buf, size_t __nbyte )
{
	return __send( __fd, (uint8_t *)__buf, (uint16_t)__nbyte );
}

int __read (int __fd, void *__buf, size_t __nbyte)
{
	return __recv( __fd, (uint8_t *)__buf, (uint16_t)__nbyte );
}


/********************************************************************
 * Connect to API
 * Returns a socket descriptor
 ********************************************************************/
int apiConnect( uint8_t *d_ip, uint16_t d_port )
{
	const int sock_num = 5;
	__close( sock_num );

	const uint16_t s_port  = 1234;
	const uint8_t protocol = 0x01;  //Sn_MR_TCP

	int se = __socket( sock_num, protocol, s_port, 0 );

	if ( se != sock_num )
	{
		//debugPrintf( "Socket error %i\n", se );
		return -1;
	}

	// Set keep-alive
	// Should be placed between socket() and connect()
	setSn_KPALVTR( sock_num, 2);

	debugPrintf( "Connecting to: %d.%d.%d.%d:%d... ", d_ip[0], d_ip[1], d_ip[2], d_ip[3], d_port );
	int ce = __connect( sock_num, d_ip, d_port );

	if ( ce != 1 /* SOCK_OK */ )
	{
		debugPrintf( "connection error %i\n", ce );
		__close( sock_num );
		//return -2;
		return ce;
	}

	debugPrintf( "CONNECTED!\n" );

	return sock_num;
}


/********************************************************************
 * Disconnect from API
 * Close the API socket
 ********************************************************************/
void apiDisconnect( int fdSock )
{
	debugPrintf( "%s()\n", __func__ );
    //Printf( "Closing socket\n" );
	__disconnect( fdSock );
    __close( fdSock );
}


/********************************************************************
 * Login to the API
 * 1 is returned on successful login
 * 0 is returned on unsuccessful login
 ********************************************************************/
int login( int fdSock, char *username, char *password )
{
	debugPrintf( "%s()\n", __func__ );
    struct Sentence stReadSentence;
    struct Sentence stWriteSentence;
    char *szMD5Challenge;
    char *szMD5ChallengeBinary;
    char *szMD5PasswordToSend;
    md5_state_t state;
    md5_byte_t digest[16];
    char cNull[1] = {0};

    writeWord( fdSock, "/login" );
    writeWord( fdSock, "" );

    stReadSentence = readSentence( fdSock );
    printSentence( &stReadSentence );

    if ( stReadSentence.iReturnValue != DONE )
    {
    	debugPrintf( "error read sentence\n" );
        return 0;
    }


    // extract md5 string from the challenge sentence
    szMD5Challenge = strtok( stReadSentence.szSentence[1], "=" );
    szMD5Challenge = strtok( NULL, "=" );

    debugPrintf( "MD5 of challenge = %s\n", szMD5Challenge );

    // convert szMD5Challenge to binary
    szMD5ChallengeBinary = md5ToBinary( szMD5Challenge );

    // get md5 of the password + challenge concatenation
    md5_init( &state );
    md5_append( &state, (md5_byte_t*)cNull, 1 );
    md5_append( &state, (const md5_byte_t *) password, strlen( password ));
    md5_append( &state, (const md5_byte_t *) szMD5ChallengeBinary, strlen( szMD5ChallengeBinary ));
    md5_finish( &state, digest );

    // convert this digest to a string representation of the hex values
    // digest is the binary format of what we want to send
    // szMD5PasswordToSend is the "string" hex format
    szMD5PasswordToSend = md5DigestToHexString( digest );

    clearSentence( &stReadSentence );

    debugPrintf( "szPasswordToSend = %s\n", szMD5PasswordToSend );

    // put together the login sentence
    initializeSentence( &stWriteSentence );

    addWordToSentence( &stWriteSentence, "/login" );
    addWordToSentence( &stWriteSentence, "=name=" );
    addPartWordToSentence( &stWriteSentence, username );
    addWordToSentence( &stWriteSentence, "=response=00" );
    addPartWordToSentence( &stWriteSentence, szMD5PasswordToSend );

    printSentence( &stWriteSentence );
    writeSentence( fdSock, &stWriteSentence );


    stReadSentence = readSentence( fdSock );
    printSentence( &stReadSentence );

    if ( stReadSentence.iReturnValue == DONE )
    {
        clearSentence( &stReadSentence );
        return 1;
    }
    else
    {
        clearSentence( &stReadSentence );
        return 0;
    }
}


/********************************************************************
 * Encode message length and write it out to the socket
 ********************************************************************/
void writeLen( int fdSock, int iLen )
{
    char *cEncodedLength;  // encoded length to send to the api socket
    char *cLength;         // exactly what is in memory at &iLen integer

    cLength = (char*)calloc( sizeof( int ), 1 );
    cEncodedLength = (char*)calloc( sizeof( int ), 1 );

    // set cLength address to be same as iLen
    cLength = (char *) &iLen;

    // ? //Printf( "length of word is %d\n", iLen ) : 0;

    // write 1 byte
    if ( iLen < 0x80 )
    {
        cEncodedLength[0] = (char) iLen;
        __write( fdSock, cEncodedLength, 1 );
    }

        // write 2 bytes
    else if ( iLen < 0x4000 )
    {
        // ? //Printf( "iLen < 0x4000.\n" ) : 0;

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

        __write( fdSock, cEncodedLength, 2 );
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

        __write( fdSock, cEncodedLength, 3 );
    }

        // write 4 bytes
        // this code SHOULD work, but is untested...
    else if ( iLen < 0x10000000 )
    {
        // ? //Printf( "iLen < 0x10000000.\n" ) : 0;

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

        __write( fdSock, cEncodedLength, 4 );
    }
    else  // this should never happen
    {
    	//Printf( "length of word is %d\n", iLen );
    	//Printf( "word is too long.\n" );
        debugPrintf( "FATAL ERROR!!!!!!!!!!!\n" );
    	exit( 1 );
    }
}


/********************************************************************
 * Write a word to the socket
 ********************************************************************/
void writeWord( int fdSock, char *szWord )
{
    debugPrintf( "Write: '%s'\n", szWord );
    writeLen( fdSock, strlen( szWord ) );
    for ( int i = 0; i < (int)strlen( szWord ); i++ )
    {
    	char c = szWord[i];
    	//debugPrintf( "Send: 0x%02X - %c\n", c, c );
    	__write( fdSock, &c, 1 );
    }
}


/********************************************************************
 * Write a sentence (multiple words) to the socket
 ********************************************************************/
void writeSentence( int fdSock, struct Sentence *stWriteSentence )
{
    int iIndex;

    if ( stWriteSentence->iLength == 0 )
    {
        return;
    }

//    // ? //Printf( "Writing sentence\n" ) : 0;
//    // ? printSentence( stWriteSentence ) : 0;

    for ( iIndex = 0; iIndex < stWriteSentence->iLength; iIndex++ )
    {
        writeWord( fdSock, stWriteSentence->szSentence[iIndex] );
    }

    writeWord( fdSock, "" );
}


/********************************************************************
 * Read a message length from the socket
 *
 * 80 = 10000000 (2 character encoded length)
 * C0 = 11000000 (3 character encoded length)
 * E0 = 11100000 (4 character encoded length)
 *
 * Message length is returned
 ********************************************************************/
int readLen( int fdSock )
{
    char cFirstChar; // first character read from socket
    char *cLength;   // length of next message to read...will be cast to int at the end
    int *iLen;       // calculated length of next message (Cast to int)

    cLength = (char*)calloc( sizeof( int ), 1 );

    delay( 5 );
    __read( fdSock, &cFirstChar, 1 );

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
            __read( fdSock, &cLength[2], 1 );
            __read( fdSock, &cLength[1], 1 );
            __read( fdSock, &cLength[0], 1 );
        }
        else
        {
            cLength[0] = cFirstChar;
            cLength[0] &= 0x1f;        // mask out the 1st 3 bits
            __read( fdSock, &cLength[1], 1 );
            __read( fdSock, &cLength[2], 1 );
            __read( fdSock, &cLength[3], 1 );
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
            __read( fdSock, &cLength[1], 1 );
            __read( fdSock, &cLength[0], 1 );
        }
        else
        {
            cLength[1] = cFirstChar;
            cLength[1] &= 0x3f;        // mask out the 1st 2 bits
            __read( fdSock, &cLength[2], 1 );
            __read( fdSock, &cLength[3], 1 );
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
            __read( fdSock, &cLength[0], 1 );
        }
        else
        {
            cLength[2] = cFirstChar;
            cLength[2] &= 0x7f;        // mask out the 1st bit
            __read( fdSock, &cLength[3], 1 );
        }

        iLen = (int *) cLength;
    }

        // assume 1-byte encoded length...same on both LE and BE systems
    else
    {
        //debugPrintf( "1-byte encoded length\n" );
        iLen = (int*)malloc( sizeof( int ));
        *iLen = (int) cFirstChar;
    }

    return *iLen;
}


/********************************************************************
 * Read a word from the socket
 * The word that was read is returned as a string
 ********************************************************************/
char *readWord( int fdSock )
{
    int iLen = readLen( fdSock );
    int iBytesToRead = 0;
    int iBytesRead = 0;
    char *szRetWord;
    char *szTmpWord;

    //debugPrintf( "Expected msg size is %i byte(s)\n", iLen );

    if ( iLen > 0 )
    {
        // allocate memory for strings
        szRetWord = (char*)calloc( sizeof( char ), iLen + 1 );
        szTmpWord = (char*)calloc( sizeof( char ), 1024 + 1 );

        int numOfTry = 0;
        while ( iLen > 0 )
        {
            // determine number of bytes to read this time around
            // lesser of 1024 or the number of byes left to read
            // in this word
            iBytesToRead = iLen > 1024 ? 1024 : iLen;

            // read iBytesToRead from the socket

            iBytesRead = __read( fdSock, szTmpWord, iBytesToRead );
            if ( iBytesRead < 0 )
            {
            	if ( numOfTry++ > 7 )
            		return NULL;

            	debugPrintf( "Can't read data - %i\n", iBytesRead );
            	delay( 1000 );
            	continue;
            }

            // terminate szTmpWord
            szTmpWord[iBytesRead] = 0;

            // concatenate szTmpWord to szRetWord
            strcat( szRetWord, szTmpWord );

            // subtract the number of bytes we just read from iLen
            iLen -= iBytesRead;

            if ( iLen )
            	debugPrintf( "received %i byte(s)\nlefts - %i byte(s)\n", iBytesRead, iLen );
            numOfTry = 0;
        }

        // deallocate szTmpWord
        free( szTmpWord );

        // ? //Printf( "word = %s\n", szRetWord ) : 0;
        return szRetWord;
    }
    else
    {
        return NULL;
    }
}


/********************************************************************
 * Read a sentence from the socket
 * A Sentence struct is returned
 ********************************************************************/
struct Sentence readSentence( int fdSock )
{
    struct Sentence stReturnSentence;
    char *szWord;

    // ? //Printf( "readSentence\n" ) : 0;

    initializeSentence( &stReturnSentence );

    while (( szWord = readWord( fdSock )))
    {
        addWordToSentence( &stReturnSentence, szWord );

        // check to see if we can get a return value from the API
        if ( strstr( szWord, "!done" ) != NULL)
        {
            //debugPrintf( "return sentence contains !done\n" );
            stReturnSentence.iReturnValue = DONE;
        }
        else if ( strstr( szWord, "!trap" ) != NULL)
        {
            //debugPrintf( "return sentence contains !trap\n" );
            stReturnSentence.iReturnValue = TRAP;
        }
        else if ( strstr( szWord, "!fatal" ) != NULL)
        {
            //debugPrintf( "return sentence contains !fatal\n" );
            stReturnSentence.iReturnValue = FATAL;
        }

    }

    // if any errors, get the next sentence
    if ( stReturnSentence.iReturnValue == TRAP || stReturnSentence.iReturnValue == FATAL )
    {
        readSentence( fdSock );
    }


//	for ( int i = 0; i < stReturnSentence.iLength; i++ )
//	{
//		debugPrintf( "stReturnSentence.szSentence[%d] = %s\n", i, stReturnSentence.szSentence[i] );
//	}

    return stReturnSentence;
}


/********************************************************************
 * Read sentence block from the socket...keeps reading sentences
 * until it encounters !done, !trap or !fatal from the socket
 ********************************************************************/
struct Block readBlock( int fdSock )
{
    struct Sentence stSentence;
    struct Block stBlock;
    initializeBlock( &stBlock );

    // ? //Printf( "readBlock\n" ) : 0;

    do
    {
        stSentence = readSentence( fdSock );
        // ? //Printf( "readSentence succeeded.\n" ) : 0;

        addSentenceToBlock( &stBlock, &stSentence );
        // ? //Printf( "addSentenceToBlock succeeded\n" ) : 0;

    }
    while ( stSentence.iReturnValue == 0 );


    // ? //Printf( "readBlock completed successfully\n" ) : 0;

    return stBlock;
}


/********************************************************************
 * Initialize a new block
 * Set iLength to 0.
 ********************************************************************/
void initializeBlock( struct Block *stBlock )
{
    // ? //Printf( "initializeBlock\n" ) : 0;

    stBlock->iLength = 0;
}


/********************************************************************
 * Clear an existing block
 * Free all sentences in the Block struct and set iLength to 0.
 ********************************************************************/
void clearBlock( struct Block *stBlock )
{
    // ? //Printf( "clearBlock\n" ) : 0;

    free( stBlock->stSentence );
    initializeBlock( stBlock );
}


/********************************************************************
 * Print a block.
 * Output a Block with //Printf.
 ********************************************************************/
void printBlock( struct Block *stBlock )
{
    int i;

//    // ? //Printf( "printBlock\n" ) : 0;
//    // ? //Printf( "block iLength = %d\n", stBlock->iLength ) : 0;

    for ( i = 0; i < stBlock->iLength; i++ )
    {
        printSentence( stBlock->stSentence[i] );
    }
}


void printBlockData( struct Block *stBlock )
{
    int i;

    for ( i = 0; i < stBlock->iLength; i++ )
    {
        char *ptr = *stBlock->stSentence[i]->szSentence;
        if ( strcmp( "!re", ptr ) != 0 )
            continue;

        int len = stBlock->stSentence[i]->iLength;

        for ( int j = 1; j < len; j++ )
        {
            char *str = stBlock->stSentence[i]->szSentence[j];
            debugPrintf( "%s\n", str );
        }
        debugPrintf( "\n" );
    }

}


/********************************************************************
 * Add a sentence to a block
 * Allocate memory and add a sentence to a Block.
 ********************************************************************/
void addSentenceToBlock( struct Block *stBlock, struct Sentence *stSentence )
{
    int iNewLength;
    iNewLength = stBlock->iLength + 1;

    // ? //Printf( "addSentenceToBlock iNewLength=%d\n", iNewLength ) : 0;

    // allocate mem for the new Sentence position
    if ( stBlock->iLength == 0 )
    {
        stBlock->stSentence = (Sentence **)malloc( 1 * sizeof stBlock->stSentence );
    }
    else
    {
        stBlock->stSentence = (Sentence **)realloc( stBlock->stSentence, iNewLength * sizeof stBlock->stSentence + 1 );
    }

    // allocate mem for the full sentence struct
    stBlock->stSentence[stBlock->iLength] = (Sentence *)malloc( sizeof *stSentence );

    // copy actual sentence struct to the block position
    memcpy( stBlock->stSentence[stBlock->iLength], stSentence, sizeof *stSentence );

    // update iLength
    stBlock->iLength = iNewLength;

    // ? //Printf( "addSentenceToBlock stBlock->iLength=%d\n", stBlock->iLength ) : 0;
}


/********************************************************************
 * Initialize a new sentence
 ********************************************************************/
void initializeSentence( struct Sentence *stSentence )
{
    // ? //Printf( "initializeSentence\n" ) : 0;

    stSentence->iLength = 0;
    stSentence->iReturnValue = 0;
}


/********************************************************************
 * Clear an existing sentence
 ********************************************************************/
void clearSentence( struct Sentence *stSentence )
{
    // ? //Printf( "initializeSentence\n" ) : 0;

    free( stSentence->szSentence );
    initializeSentence( stSentence );
}


/********************************************************************
 * Add a word to a sentence struct
 ********************************************************************/
void addWordToSentence( struct Sentence *stSentence, const char *szWordToAdd )
{
    int iNewLength;
    iNewLength = stSentence->iLength + 1;

    // allocate mem for the new word position
    if ( stSentence->iLength == 0 )
    {
        stSentence->szSentence = (char**)malloc( 1 * sizeof stSentence->szSentence );
    }
    else
    {
        stSentence->szSentence =(char**) realloc( stSentence->szSentence, iNewLength * sizeof stSentence->szSentence + 1 );
    }


    // allocate mem for the full word string
    stSentence->szSentence[stSentence->iLength] = (char*)malloc( strlen( szWordToAdd ) + 1 );

    // copy word string to the sentence
    strcpy( stSentence->szSentence[stSentence->iLength], szWordToAdd );

    // update iLength
    stSentence->iLength = iNewLength;
}


/********************************************************************
 * Add a partial word to a sentence struct...useful for concatenation
 ********************************************************************/
void addPartWordToSentence( struct Sentence *stSentence, char *szWordToAdd )
{
    int iIndex;
    iIndex = stSentence->iLength - 1;

    // reallocate memory for the new partial word
    stSentence->szSentence[iIndex] = (char*)realloc( stSentence->szSentence[iIndex],
                                              strlen( stSentence->szSentence[iIndex] ) + strlen( szWordToAdd ) + 1 );

    // concatenate the partial word to the existing sentence
    strcat( stSentence->szSentence[iIndex], szWordToAdd );
}


/********************************************************************
 * Print a Sentence struct
 ********************************************************************/
void printSentence( struct Sentence *stSentence )
{
    int i;

//    // ? //Printf( "Sentence iLength = %d\n", stSentence->iLength ) : 0;
//    // ? //Printf( "Sentence iReturnValue = %d\n", stSentence->iReturnValue ) : 0;

//    //Printf( "Sentence iLength = %d\n", stSentence->iLength );
//    //Printf( "Sentence iReturnValue = %d\n", stSentence->iReturnValue );

    debugPrintf( "\nReceived %i sentences\n", stSentence->iLength );
    for ( i = 0; i < stSentence->iLength; i++ )
    {
        debugPrintf( "%s\n", stSentence->szSentence[i] );
    }

    debugPrintf( "\n" );
}


/********************************************************************
 * MD5 helper function to convert an md5 hex char representation to
 * binary representation.
 ********************************************************************/
char *md5ToBinary( char *szHex )
{
    int di;
    char cBinWork[3];
    char *szReturn;

    // allocate 16 + 1 bytes for our return string
    szReturn = (char*)malloc(( 16 + 1 ) * sizeof *szReturn );

    // 32 bytes in szHex?
    if ( strlen( szHex ) != 32 )
    {
        return NULL;
    }

    for ( di = 0; di < 32; di += 2 )
    {
        cBinWork[0] = szHex[di];
        cBinWork[1] = szHex[di + 1];
        cBinWork[2] = 0;

        // ? //Printf( "cBinWork = %s\n", cBinWork ) : 0;

        szReturn[di / 2] = hexStringToChar( cBinWork );
    }

    return szReturn;
}


/********************************************************************
 * MD5 helper function to calculate and return hex representation
 * of an MD5 digest stored in binary.
 ********************************************************************/
char *md5DigestToHexString( md5_byte_t *binaryDigest )
{
    int di;
    char *szReturn;

    // allocate 32 + 1 bytes for our return string
    szReturn = (char*)malloc(( 32 + 1 ) * sizeof *szReturn );


    for ( di = 0; di < 16; ++di )
    {
    	sprintf( szReturn + di * 2, "%02x", binaryDigest[di] );
    }

    return szReturn;
}


/********************************************************************
 * Quick and dirty function to convert hex string to char...
 * the toConvert string MUST BE 2 characters + null terminated.
 ********************************************************************/
char hexStringToChar( char *cToConvert )
{
    unsigned int iAccumulated = 0;
    char cString0[2] = {cToConvert[0], 0};
    char cString1[2] = {cToConvert[1], 0};

    // look @ first char in the 16^1 place
    if ( cToConvert[0] == 'f' || cToConvert[0] == 'F' )
    {
        iAccumulated += 16 * 15;
    }
    else if ( cToConvert[0] == 'e' || cToConvert[0] == 'E' )
    {
        iAccumulated += 16 * 14;
    }
    else if ( cToConvert[0] == 'd' || cToConvert[0] == 'D' )
    {
        iAccumulated += 16 * 13;
    }
    else if ( cToConvert[0] == 'c' || cToConvert[0] == 'C' )
    {
        iAccumulated += 16 * 12;
    }
    else if ( cToConvert[0] == 'b' || cToConvert[0] == 'B' )
    {
        iAccumulated += 16 * 11;
    }
    else if ( cToConvert[0] == 'a' || cToConvert[0] == 'A' )
    {
        iAccumulated += 16 * 10;
    }
    else
    {
        iAccumulated += 16 * atoi( cString0 );
    }


    // now look @ the second car in the 16^0 place
    if ( cToConvert[1] == 'f' || cToConvert[1] == 'F' )
    {
        iAccumulated += 15;
    }
    else if ( cToConvert[1] == 'e' || cToConvert[1] == 'E' )
    {
        iAccumulated += 14;
    }
    else if ( cToConvert[1] == 'd' || cToConvert[1] == 'D' )
    {
        iAccumulated += 13;
    }
    else if ( cToConvert[1] == 'c' || cToConvert[1] == 'C' )
    {
        iAccumulated += 12;
    }
    else if ( cToConvert[1] == 'b' || cToConvert[1] == 'B' )
    {
        iAccumulated += 11;
    }
    else if ( cToConvert[1] == 'a' || cToConvert[1] == 'A' )
    {
        iAccumulated += 10;
    }
    else
    {
        iAccumulated += atoi( cString1 );
    }

    // ? Printf( "%d\n", iAccumulated ) : 0;
    return (char) iAccumulated;
}


/********************************************************************
 * Test whether or not this system is little endian at RUNTIME
 * Courtesy: http://download.osgeo.org/grass/grass6_progman/endian_8c_source.html
 ********************************************************************/
int isLittleEndian( void )
{
    union
    {
        int testWord;
        char testByte[sizeof( int )];
    } endianTest;

    endianTest.testWord = 1;

    if ( endianTest.testByte[0] == 1 )
    {
        return 1;
    }               /* true: little endian */

    return 0;                   /* false: big endian */
}

