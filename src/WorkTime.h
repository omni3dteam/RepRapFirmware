/*
 * WorkTime.h
 *
 *  Created on: 22 maj 2019
 *      Author: Dawid
 */

#ifndef SRC_WORKTIME_H_
#define SRC_WORKTIME_H_

#include "RepRap.h"

class WorkTime
{
private:
	uint64_t secondsSaved;
public:
	static constexpr uint32_t startAddress = 0x0047F800;
	static constexpr uint32_t endAddress   = 0x0047FFFF;
	static constexpr uint32_t timeAddress  = 0x0047F800;

	void Init();
    void Write();
    void WriteToFlash(uint64_t);

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
};

#endif /* SRC_WORKTIME_H_ */
