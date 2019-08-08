//
// Created by zlyt0_vcp on 16.07.2019.
//

#ifndef MIKROTIK_MKTBLOCK_H
#define MIKROTIK_MKTBLOCK_H


#include <cstddef>
#include <stdint-gcc.h>


#define MKT_BLOCK_SIZE 2048

class MKTBlock
{
public:
    MKTBlock();
    ~MKTBlock();

    void ReInit();

    uint16_t GetSentenceCount();
    bool AddWordToSentence( const char *pWord );
    bool AddEndOfSentence();

    void Print();
    const char* GetFirstWord();
    const char* GetNextWord( const char *pCurrent );

private:
    char heap[MKT_BLOCK_SIZE];
    char *pNextFreeAddr = NULL;
    size_t   spaceLeft;
    uint16_t sentenceCount;
};


#endif //MIKROTIK_MKTBLOCK_H
