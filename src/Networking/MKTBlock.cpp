#include <cstring>
#include <cstdio>
#include "MKTBlock.h"

#ifndef __LINUX_DBG
    #include "RepRapFirmware.h"
#else
    #define debugPrintf printf
#endif

MKTBlock::MKTBlock()
{
    spaceLeft = MKT_BLOCK_SIZE;
    pNextFreeAddr = heap;
    sentenceCount = 0;
    memset( heap, 0, MKT_BLOCK_SIZE );
}

MKTBlock::~MKTBlock()
{
    memset( heap, 0, MKT_BLOCK_SIZE );
}


void MKTBlock::ReInit()
{
    spaceLeft = MKT_BLOCK_SIZE;
    pNextFreeAddr = heap;
    sentenceCount = 0;
    memset( heap, 0, MKT_BLOCK_SIZE );
}


uint16_t MKTBlock::GetSentenceCount()
{
    return sentenceCount;
}


bool MKTBlock::AddWordToSentence( const char *pWord )
{
    if ( ( !pWord ) || ( strlen( pWord ) == 0 ) )
        return AddEndOfSentence();

    size_t reqSize = strlen( pWord ) + 1;
    if ( spaceLeft < reqSize )
        return false;

    strcpy( pNextFreeAddr, pWord );
    pNextFreeAddr += reqSize;

    // update free space
    spaceLeft -= reqSize;

    return true;
}


void MKTBlock::Print()
{
    debugPrintf( "\n\nReceived:\n" );

    const char* pWord = GetFirstWord();
    do
    {
        debugPrintf( "%s\n", pWord );
        pWord = GetNextWord( pWord );
    } while ( pWord );
}


bool MKTBlock::AddEndOfSentence()
{
    if ( !spaceLeft )
        return false;

    *pNextFreeAddr = 0;
    pNextFreeAddr++;
    spaceLeft--;
    return true;
}


const char *MKTBlock::GetFirstWord()
{
    return reinterpret_cast<const char *>(heap);
}


const char *MKTBlock::GetNextWord( const char *pCurrent )
{
    const char *pRes = pCurrent + strlen( pCurrent ) + 1;

    if ( ( *pRes == 0 ) && ( *(pRes+1) == 0 ) )
        return nullptr;
    else
        return pRes;
}

