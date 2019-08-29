#ifndef LCDUPDATER_H_
#define LCDUPDATER_H_

#include "RepRapFirmware.h"
#include "crc32_lcd.h"

class LcdUpdater
{
public:
	bool UpdateLcdModule();
	LcdUpdater(UARTClass& port);
	bool isLcdUploader {false};
	bool OpenLcdFirmware();

private:

	 enum class errorId{
		noError = 0,
		frameWrongLength,
		frameMustStartAt_0xAA,
		wrongFrameCrc,
		wrongInitPackageCrc,
		wrongInitPackageLengthNewFirmware,
		wrongInitPackageMagicCode,
		flashErrorInit,
		flashErrorUnlock,
		flashErrorErase,
		flashErrorWrite,
		errorWrongHash,
		errorReceiveNewFirmwareWithoutValidInitPackage,
		errorTimeout,
		frameHashOk = 99
	};


	enum class UploadState
	{
		idle,
		initPacket,
		uploading,
		done
	};

	void write(const uint8_t c);
	void writePacketRaw(uint8_t *data, size_t dataLen);
	void prepareFrameToLcd();
	void flushInput();
	void MessageF(const char *fmt, ...);
	errorId readPacket(uint32_t msTimeout);
	UARTClass& uploadPort;
	FileStore *uploadFile;
	UploadState state;
	uint32_t lastAttemptTime;
	FilePosition fileSize;

	uint32_t fileOffset {0};
    uint16_t filePayload {96};
	uint8_t payloadBuffer[96];
	bool retransmission {false};
	uint16_t idPackage {0};
	uint8_t retransmissionTry {0};
	const uint8_t framePayload {96};

	uint32_t blockWriteInterval {30};
	static const uint32_t initWriteInterval {7000}; 		// 7s is enough. We need this time to wait for erase lcd flash space.

};

#endif /* LCDUPDATER_H_ */
