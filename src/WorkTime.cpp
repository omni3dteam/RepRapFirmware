/*
 * WorkTime.cpp
 *
 *  Created on: 22 maj 2019
 *      Author: Dawid
 */

#include "WorkTime.h"

void WorkTime::Init()
{
	uint64_t tempVal = *reinterpret_cast<uint64_t *>(timeAddress);
	SetSeconds(tempVal);
}

void WorkTime::WriteToFlash(uint64_t val)
{
	cpu_irq_disable();
	flash_unlock(startAddress, endAddress, 0, 0);
	flash_erase_page(timeAddress, IFLASH_ERASE_PAGES_8);
	flash_write(timeAddress, &val, sizeof(val), 0);
	flash_lock(startAddress, endAddress, 0, 0);
	cpu_irq_enable();
}

void WorkTime::Write()
{
	WriteToFlash(GetSeconds());
}
