#include "LcdUpdater.h"

#include "RepRapFirmware.h"
#include "Platform.h"
#include "RepRap.h"
#include "Storage/FileStore.h"

// LCD command codes
const uint8_t LCD_FLASH_BEGIN = 0xAA;

const unsigned int WifiFirmwareModule = 4;  // LCD module

LcdUpdater::LcdUpdater(UARTClass& port)
	: uploadPort(port), uploadFile(nullptr), state(UploadState::idle)
{
}

bool LcdUpdater::UpdateLcdModule()
{
	switch(state)
	{
	case UploadState::initPacket:
	case UploadState::uploading:
		if (millis() - lastAttemptTime >= blockWriteInterval)
		{
/*			const uint32_t blkCnt = (fileSize + EspFlashBlockSize - 1) / EspFlashBlockSize;
			if (uploadBlockNumber < blkCnt)
			{
				uploadResult = flashWriteBlock(0, 0);
				lastAttemptTime = millis();
				if (uploadResult != LcdUploadResult::success)
				{
					MessageF("Flash block upload failed\n");
					state = UploadState::done;
				}
			}
			else
			{
				state = UploadState::done;
			}*/
			debugPrintf("send..\n");
		}
		break;
	case UploadState::done:
		break;
	case UploadState::idle:
	default:
		break;
	}

	return true;
}

bool LcdUpdater::OpenLcdFirmware()
{
	Platform& platform = reprap.GetPlatform();
	uploadFile = platform.OpenFile(DEFAULT_SYS_DIR, LCD_FIRMWARE_FILE, OpenMode::read);
	if (uploadFile == nullptr)
	{
		MessageF("Failed to open file %s\n", LCD_FIRMWARE_FILE);
		state = UploadState::idle;
		return false;
	}

	fileSize = uploadFile->Length();
	if (fileSize == 0)
	{
		uploadFile->Close();
		MessageF("Upload file is empty %s\n", LCD_FIRMWARE_FILE);
		state = UploadState::idle;
		return false;
	}
	MessageF("Open file ok!\n");
	state = UploadState::initPacket;
	return true;
}

void LcdUpdater::MessageF(const char *fmt, ...)
{
	va_list vargs;
	va_start(vargs, fmt);
	reprap.GetPlatform().MessageF(FirmwareUpdateMessage, fmt, vargs);
	va_end(vargs);
}

// End
