#ifndef LCDUPDATER_H_
#define LCDUPDATER_H_

#include "RepRapFirmware.h"
#include "crc32_lcd.h"

class LcdUpdater
{
public:
	bool UpdateLcdModule();
	LcdUpdater(UARTClass& port);
	bool OpenLcdFirmware();
	bool isLcdUploader {false};

private:

	 enum class errorId{
		NO_ERROR = 0,
		FRAME_WRONG_LENGTH,
		FRAME_MUST_START_AT_0XAA,
		WRONG_FRAME_CRC,
		WRONG_INIT_PKG_CRC,
		WRONG_INIT_PKG_LENGTH_NEW_FIRMWARE,
		WRONG_INIT_PKG_MAGIC_CODE, // omni3d
		FLASH_ERROR_INIT,
		FLASH_ERROR_UNLOCK,
		FLASH_ERROR_ERASE,
		FLASH_ERROR_WRITE,
		ERROR_WRONG_HASH,
		ERROR_RECEIVE_NEW_FIRMWARE_WITHOUT_VALID_INIT_PKG,
		ERROR_TIMEOUT,
		FRAME_HASH_OK=99 //<= to nie b³¹d tylko wszystko jest ok po ca³ej zabawie
	};

	enum class LcdUploadResult
	{
		success = 0,
		timeout,
		connect,
		badReply,
		fileRead,
	};


	enum class UploadState
	{
		idle,
		initPacket,
		uploading,
		done
	};

	void writePacketRaw(uint8_t *data, size_t dataLen);
	void prepareFrameToLcd();
	void flushInput();
	int readPacket(uint32_t msTimeout);
	void MessageF(const char *fmt, ...);
	UARTClass& uploadPort;
	FileStore *uploadFile;
	UploadState state;
	uint32_t lastAttemptTime;
	FilePosition fileSize;
	LcdUploadResult uploadResult;

	uint32_t fileOffset {0};
    uint16_t filePayload {96};
	const char lcdFrameBegin {0xAA};

	static const uint32_t initWriteInterval {7000}; 		// 7s is enough. We need this time to wait for erase lcd flash space.
	static const uint32_t blockWriteInterval {50};

};

#endif /* LCDUPDATER_H_ */
