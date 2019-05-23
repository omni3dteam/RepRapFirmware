/*
 * WorkTime.cpp
 *
 *  Created on: 22 maj 2019
 *      Author: Dawid
 */

#include "WorkTime.h"

void WorkTime::Init()
{
	uint32_t tmpHours = Read();
	SetHours(tmpHours);
}

uint32_t WorkTime::Read()
{
	uint32_t tempVal = *reinterpret_cast<uint32_t *>(timeAddress);
	SetHours(tempVal);
	return tempVal;
}

void WorkTime::Write(uint32_t val)
{
	cpu_irq_disable();
	flash_unlock(startAddress, endAddress, 0, 0);
	flash_erase_page(timeAddress, IFLASH_ERASE_PAGES_8);
	flash_write(timeAddress, &val, sizeof(val), 0);
	flash_lock(startAddress, endAddress, 0, 0);
	cpu_irq_enable();
}

void WorkTime::WriteDuringUpload()
{
	Write(GetHours());
}

void WorkTime::SetAndWrite(uint32_t h)
{
	Write(h);
	SetHours(h);
}

void WorkTime::Spin()
{
	// 3600000ms = 1h
	if(millis() - workTimeLocal > 3600000)
	{
		workTimeLocal = millis();
		Write(GetHours() + 1);
	}
}
