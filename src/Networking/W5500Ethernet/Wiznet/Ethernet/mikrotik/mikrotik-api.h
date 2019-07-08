#ifdef __cplusplus
extern "C" {
#endif

#include "md5.h"

#define DEBUG 0

#define DONE 1
#define TRAP 2
#define FATAL 3

struct Sentence {
    char **szSentence;    // array of strings representing individual words
    int iLength;          // length of szSentence (number of array elements)
    int iReturnValue;     // return value of sentence reads from API
};

struct Block {
    struct Sentence **stSentence;
    int iLength;
};

// API specific functions
int apiConnect( uint8_t *d_ip, uint16_t d_port );
void apiDisconnect(int);

int login(int, char *, char *);
void writeLen(int, int);
int readLen(int);
void writeWord(int, char *);
char *readWord(int);

// API helper functions to make things a little bit easier
void initializeSentence(struct Sentence *);
void clearSentence(struct Sentence *);
void writeSentence(int, struct Sentence *);
struct Sentence readSentence(int);
void printSentence(struct Sentence *);
void addWordToSentence(struct Sentence *, const char *);
void addPartWordToSentence(struct Sentence *, char *);

void initializeBlock(struct Block *);
void clearBlock(struct Block *);
struct Block readBlock(int);
void addSentenceToBlock(struct Block *, struct Sentence *);
void printBlock(struct Block *);
void printBlockData( struct Block *stBlock );

// MD5 helper functions
char *md5DigestToHexString(md5_byte_t *);
char *md5ToBinary(char *);
char hexStringToChar(char *);

// Endian tests
int isLittleEndian(void);

#ifdef __cplusplus
}
#endif

