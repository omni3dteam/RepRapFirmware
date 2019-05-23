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
	uint32_t hours;
public:
	static constexpr uint32_t startAddress = 0x0047F800;
	static constexpr uint32_t endAddress   = 0x0047FFFF;
	static constexpr uint32_t timeAddress  = 0x0047F800;

	uint32_t workTimeLocal = 0;

	void Init();
    void Spin();
    void WriteDuringUpload();
    void Write(uint32_t);
    void SetAndWrite(uint32_t);
    uint32_t Read();

    uint32_t GetHours()
    {
    	return hours;
    }

    void SetHours(uint32_t h)
    {
    	Write(h);
    	hours = h;
    }

};



#endif /* SRC_WORKTIME_H_ */
