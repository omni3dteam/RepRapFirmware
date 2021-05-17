#include "LcdUpdater.h"

#include "RepRapFirmware.h"
#include "Platform.h"
#include "RepRap.h"
#include "Storage/FileStore.h"

LcdUpdater::LcdUpdater(UARTClass& port)
	: uploadPort(port), uploadFile(nullptr), state(UploadState::idle), fileOffset(0)
{
	idPackage = 0;
	fileOffset = 0;
	retransmissionTry = 0;
	filePayload = framePayloadLength;
	retransmission = false;
	packageAckTimeout = 30;

	memset(payloadBuffer, 0, sizeof(payloadBuffer));
}

bool LcdUpdater::UpdateLcdModule()
{
	switch(state)
	{
	case UploadState::initPacket:
	case UploadState::uploading:
		if (millis() - lastAttemptTime >= (state == UploadState::initPacket ? firstAckTimeout : packageAckTimeout))
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
		MessageF(ErrorMessage, "[E001] Failed to open file %s\n", LCD_FIRMWARE_FILE);
		state = UploadState::idle;
		return false;
	}

	fileSize = uploadFile->Length();
	if (fileSize == 0)
	{
		uploadFile->Close();
		MessageF(ErrorMessage, "[E002] Upload file is empty %s\n", LCD_FIRMWARE_FILE);
		state = UploadState::idle;
		return false;
	}

	isLcdUploader = true;
	retransmission = false;
	state = UploadState::initPacket;
	fileOffset = idPackage = retransmissionTry = 0;
	filePayload = framePayloadLength;
	packageAckTimeout = 25;

	memset(payloadBuffer, 0, framePayloadLength);
	return true;
}


LcdUpdater::errorId LcdUpdater::readPacket(uint32_t msTimeout)
{
	uint32_t startTime = millis();

	while(true)
	{
		if (millis() - startTime > msTimeout)
		{
			return errorId::errorTimeoutFw;
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
	if(fileOffset + framePayloadLength > uploadFile->Length())
	{
		filePayload = uploadFile->Length() - fileOffset;

		// give more time for last frame because LCD need to compute hash
		packageAckTimeout = 1000;
		state = UploadState::done;
	}

	if(!retransmission)
	{
		uploadFile->Read(reinterpret_cast<char *>(payloadBuffer), framePayloadLength);
	}

	flushInput();
	writePacketRaw(payloadBuffer, framePayloadLength);

	errorId stat = readPacket(state == UploadState::initPacket ? firstAckTimeout : packageAckTimeout);

	if(stat == errorId::noError)
	{
		memset(payloadBuffer, 0, framePayloadLength);
		fileOffset += framePayloadLength;
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
		//debugPrintf("Error code: %d \n", (int)stat);
		retransmission = true;
		if(++retransmissionTry > 3)
		{
			MessageF(ErrorMessage, "[E003] Internal problem. Can't upload firmware to LCD. Reason: %d\n", (int)stat);
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

void LcdUpdater::MessageF(MessageType type, const char *fmt, ...)
{
	va_list vargs;
	va_start(vargs, fmt);
	reprap.GetPlatform().MessageF(type, fmt, vargs);
	va_end(vargs);
}

// End
