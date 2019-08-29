#include "LcdUpdater.h"

#include "RepRapFirmware.h"
#include "Platform.h"
#include "RepRap.h"
#include "Storage/FileStore.h"

const char lcdFrameBegin = 0xAA;

LcdUpdater::LcdUpdater(UARTClass& port)
	: uploadPort(port), uploadFile(nullptr), state(UploadState::idle), fileOffset(0)
{
}

bool LcdUpdater::UpdateLcdModule()
{
	switch(state)
	{
	case UploadState::initPacket:
	case UploadState::uploading:
		if (millis() - lastAttemptTime >= (state == UploadState::initPacket ? initWriteInterval : blockWriteInterval))
		{
			prepareFrameToLcd();
			lastAttemptTime = millis();
		}
		break;
	case UploadState::done:
	case UploadState::idle:
		uploadFile->Close();
		isLcdUploader = false;
		return true;
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
	retransmission = false;
	state = UploadState::initPacket;
	fileOffset = idPackage = retransmissionTry = 0;
	filePayload = framePayload;
	blockWriteInterval = 25;

	memset(payloadBuffer, 0, framePayload);
	return true;
}


LcdUpdater::errorId LcdUpdater::readPacket(uint32_t msTimeout)
{
	uint32_t startTime = millis();

	while(1)
	{
		if (millis() - startTime > msTimeout)
		{
			return errorId::errorTimeout;
		}

		if (uploadPort.available() < 1)
		{
			continue;
		}

		return (errorId)uploadPort.read();
	}
}

void LcdUpdater::prepareFrameToLcd()
{
	if(fileOffset + framePayload > uploadFile->Length())
	{
		filePayload = uploadFile->Length() - fileOffset;

		// give more time for last frame because LCD need to compute hash
		blockWriteInterval = 1000;
		state = UploadState::done;
	}

	if(!retransmission)
	{
		uploadFile->Read(reinterpret_cast<char *>(payloadBuffer), framePayload);
	}

	flushInput();
	writePacketRaw(payloadBuffer, framePayload);

	errorId stat = readPacket(state == UploadState::initPacket ? initWriteInterval : blockWriteInterval);

	if(stat == errorId::noError)
	{
		memset(payloadBuffer, 0, framePayload);
		fileOffset += framePayload;
		retransmission = false;
		idPackage++;

		if(state == UploadState::initPacket)
		{
			state = UploadState::uploading;
		}
	}
	else if(stat == errorId::frameHashOk)
	{
		state = UploadState::done;
	}
	else
	{
		debugPrintf("Error code: %d \r\n", (int)stat);
		retransmission = true;
		if(++retransmissionTry > 3)
		{
			MessageF("Internal problem. Can't upload firmware to LCD. Reason: %d\n\r", (int)stat);
			state = UploadState::done;
		}
	}

}

void LcdUpdater::writePacketRaw(uint8_t *data, size_t dataLen)
{
	write(lcdFrameBegin);				// send the packet start character

	const uint8_t *payload = (uint8_t *)&filePayload;
	write(payload[0]);
	write(payload[1]);

	uint8_t *offset = (uint8_t *)&fileOffset;
	write(offset[0]);
	write(offset[1]);
	write(offset[2]);
	write(offset[3]);

	for(size_t i = 0; i < dataLen; ++i)
	{
		write(data[i]);
	}

	//calc CRC32
	unsigned int crc32Value = 0;
	crc32Value = xcrc32(&lcdFrameBegin, 1, crc32Value);
	crc32Value = xcrc32(reinterpret_cast<const char *>(payload), 2, crc32Value);
	crc32Value = xcrc32(reinterpret_cast<const char *>(offset), 4, crc32Value);
	crc32Value = xcrc32(reinterpret_cast<const char *>(data), dataLen, crc32Value);

	uint8_t *crc32 = (uint8_t *)&crc32Value;
	write(crc32[0]);
	write(crc32[1]);
	write(crc32[2]);
	write(crc32[3]);

	//debugPrintf("CRC32: 0x%x - ", crc32Value);
}

void LcdUpdater::flushInput()
{
	while (uploadPort.available() != 0)
	{
		(void)uploadPort.read();
	}
}

void LcdUpdater::write(const uint8_t c)
{
	uploadPort.write(c);
}

void LcdUpdater::MessageF(const char *fmt, ...)
{
	va_list vargs;
	va_start(vargs, fmt);
	reprap.GetPlatform().MessageF(FirmwareUpdateMessage, fmt, vargs);
	va_end(vargs);
}

// End
