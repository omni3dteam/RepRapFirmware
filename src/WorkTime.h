/*
 * WorkTime.h
 *
 *  Created on: 22 maj 2019
 *      Author: Dawid
 */

#ifndef SRC_WORKTIME_H_
#define SRC_WORKTIME_H_

#include "RepRap.h"

enum class WriteStatus {
    	WRITE_OK = 0,
		WRITE_ERR_UNLOCK,
		WRITE_ERR_LOCK,
		WRITE_ERR_ERASE,
		WRITE_ERR_TIME
    };

class WorkTime
{
private:
	uint64_t secondsSaved;
	uint64_t secondsPrint;

    void SetPrintSeconds(uint64_t s)
    {
    	secondsPrint = s;
    }

    bool SafeFlashWrite(uint32_t address, const void *buffer, uint32_t size);

public:
	static constexpr uint32_t startAddress = 0x0047F800;
	static constexpr uint32_t endAddress   = 0x0047FFFF;
	static constexpr uint32_t timeAddress  = 0x0047F800;
	static constexpr uint32_t printTimeAddress  = 0x0047F808;

	void Init();
    void Write();
    WriteStatus WriteToFlash(uint64_t t, uint64_t p);

    uint64_t GetSeconds()
    {
    	return secondsSaved + millis() / 1000;
    }

    void SetSeconds(uint64_t h)
    {
    	secondsSaved = h;
    }

    uint16_t GetHours()
    {
    	return GetSeconds() / 3600;
    }

    void SetHours(uint16_t h)
    {
    	secondsSaved = h * 3600;
    }

    void SetPrintHours(uint16_t h)
	{
    	secondsPrint = h * 3600;
	}


    uint64_t GetPrintSeconds()
    {
    	return secondsPrint;
    }

    uint16_t GetPrintHours()
    {
    	return GetPrintSeconds() / 3600;
    }

    void IncrementPrintTime()
    {
    	secondsPrint++;
    }
};

#endif /* SRC_WORKTIME_H_ */
