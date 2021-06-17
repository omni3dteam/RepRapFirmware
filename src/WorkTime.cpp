/*
 * WorkTime.cpp
 *
 *  Created on: 22 maj 2019
 *      Author: Dawid
 */

#include "WorkTime.h"
#include "Platform.h"

void WorkTime::Init()
{
	uint64_t tempVal = *reinterpret_cast<uint64_t *>(timeAddress);
	SetSeconds(tempVal);

	tempVal = *reinterpret_cast<uint64_t *>(printTimeAddress);
	SetPrintSeconds(tempVal);
}

WriteStatus WorkTime::WriteToFlash(uint64_t t, uint64_t p)
{
	cpu_irq_disable();

	if (flash_unlock(startAddress, endAddress, 0, 0) != FLASH_RC_OK)
	{
		return WriteStatus::WRITE_ERR_UNLOCK;
	}
	if (flash_erase_page(timeAddress, IFLASH_ERASE_PAGES_8) != FLASH_RC_OK)
	{
		return WriteStatus::WRITE_ERR_ERASE;
	}
	if (SafeFlashWrite(timeAddress, &t, sizeof(t)) != FLASH_RC_OK)
	{
		return WriteStatus::WRITE_ERR_TIME;
	}
	if (SafeFlashWrite(printTimeAddress, &p, sizeof(p)) != FLASH_RC_OK)
	{
		return WriteStatus::WRITE_ERR_TIME;
	}
	if (flash_lock(startAddress, endAddress, 0, 0) != FLASH_RC_OK)
	{
		return WriteStatus::WRITE_ERR_LOCK;
	}

	cpu_irq_enable();

	return WriteStatus::WRITE_OK;
}

bool WorkTime::SafeFlashWrite(uint32_t address, const void *buffer, uint32_t size)
{
	bool status;
	cpu_irq_disable();
	status = flash_write(address, buffer, size, 0) != FLASH_RC_OK;
	cpu_irq_enable();

	return status;
}

void WorkTime::Write()
{
	WriteStatus status = WriteToFlash(GetSeconds(), GetPrintSeconds());

	if (status != WriteStatus::WRITE_OK)
	{
		reprap.GetPlatform().MessageF(LogMessage, "Cannot write time to flash (%d)\n", (int)status);
	}
}
