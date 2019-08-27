#include "LcdUpdater.h"

#include "RepRapFirmware.h"
#include "Platform.h"
#include "RepRap.h"
#include "Storage/FileStore.h"

const unsigned int WifiFirmwareModule = 4;  // LCD module

LcdUpdater::LcdUpdater(UARTClass& port)
	: uploadPort(port), uploadFile(nullptr), state(UploadState::idle), fileOffset(0)
{
}

bool LcdUpdater::UpdateLcdModule()
{
	switch(state)
	{
	case UploadState::initPacket:
		if (millis() - lastAttemptTime >= initWriteInterval)
		{
			prepareFrameToLcd();
			lastAttemptTime = millis();
		}
		break;
	case UploadState::uploading:
		if (millis() - lastAttemptTime >= blockWriteInterval)
		{

			prepareFrameToLcd();


			lastAttemptTime = millis();
		}
		break;
	case UploadState::done:
		isLcdUploader = false;
		return true;
		break;
	case UploadState::idle:
	default:
		break;
	}

	return false;
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

	isLcdUploader = true;
	state = UploadState::initPacket;
	return true;
}


int LcdUpdater::readPacket(uint32_t msTimeout)
{
	uint32_t startTime = millis();

	// wait for the response
	uint16_t needBytes = 1;

	while(1)
	{
		if (millis() - startTime > msTimeout)
		{
			return 21;
			//return(errorId::ERROR_TIMEOUT);
		}

		if (uploadPort.available() < needBytes)
		{
			// insufficient data available
			// preferably, return to Spin() here
			continue;
		}

		int c = uploadPort.read();

		return c;

/*		switch(c)
		{
		case errorId::NO_ERROR:
		case errorId::FRAME_HASH_OK:
			return c;
			break;
		case errorId::FRAME_WRONG_LENGTH:
		case errorId::FRAME_MUST_START_AT_0XAA:
		case errorId::WRONG_FRAME_CRC:
		case errorId::WRONG_INIT_PKG_CRC:
		case errorId::WRONG_INIT_PKG_LENGTH_NEW_FIRMWARE:
		case errorId::WRONG_INIT_PKG_MAGIC_CODE:
		case errorId::FLASH_ERROR_INIT:
		case errorId::FLASH_ERROR_UNLOCK:
		case errorId::FLASH_ERROR_ERASE:
		case errorId::FLASH_ERROR_WRITE:
		case errorId::ERROR_WRONG_HASH:
		case errorId::ERROR_RECEIVE_NEW_FIRMWARE_WITHOUT_VALID_INIT_PKG:
		default:
			return c;
		}*/
	}
}

void LcdUpdater::prepareFrameToLcd()
{
	uint8_t payloadBuffer[96];
	flushInput();

	if(fileOffset + 96 > uploadFile->Length())
	{
		filePayload = uploadFile->Length() - fileOffset;
		debugPrintf("payload: %d\r\n", filePayload);
		state = UploadState::done;
	}

	memset(payloadBuffer, 0, 96);

	// Get the data for the block
	uploadFile->Read(reinterpret_cast<char *>(payloadBuffer), 96);

	writePacketRaw(payloadBuffer, 96);

	int stat = readPacket(state == UploadState::initPacket ? initWriteInterval : blockWriteInterval);

	if(stat == 0)
	{
		static int inc = 1;
		debugPrintf("send packet: %d (offset: %lu, file lenght: %lu)\n", inc++, fileOffset, uploadFile->Length());
		fileOffset += 96;

		if(state == UploadState::initPacket)
		{
			state = UploadState::uploading;
		}
	}
	else
	{
		debugPrintf("Error code: %d \r\n", stat);
		state = UploadState::idle;
	}

}


void LcdUpdater::MessageF(const char *fmt, ...)
{
	va_list vargs;
	va_start(vargs, fmt);
	reprap.GetPlatform().MessageF(FirmwareUpdateMessage, fmt, vargs);
	va_end(vargs);
}

void LcdUpdater::flushInput()
{
	while (uploadPort.available() != 0)
	{
		(void)uploadPort.read();
	}
}

void LcdUpdater::writePacketRaw(uint8_t *data, size_t dataLen)
{
	uploadPort.write(lcdFrameBegin);				// send the packet start character

	const uint8_t *payload = (uint8_t *)&filePayload;
	uploadPort.write(payload[0]);
	uploadPort.write(payload[1]);

	uint8_t *offset = (uint8_t *)&fileOffset;
	uploadPort.write(offset[0]);
	uploadPort.write(offset[1]);
	uploadPort.write(offset[2]);
	uploadPort.write(offset[3]);

	for(size_t i = 0; i < dataLen; ++i)
	{
		uploadPort.write(data[i]);
	}

	//calc CRC32
	unsigned int crc32Value = 0;
	crc32Value = xcrc32(&lcdFrameBegin, 1, crc32Value);
	crc32Value = xcrc32(reinterpret_cast<const char *>(payload), 2, crc32Value);
	crc32Value = xcrc32(reinterpret_cast<const char *>(offset), 4, crc32Value);
	crc32Value = xcrc32(reinterpret_cast<const char *>(data), dataLen, crc32Value);

	uint8_t *crc32 = (uint8_t *)&crc32Value;
	uploadPort.write(crc32[0]);
	uploadPort.write(crc32[1]);
	uploadPort.write(crc32[2]);
	uploadPort.write(crc32[3]);

	debugPrintf("CRC32: 0x%x - ", crc32Value);
}

// End
