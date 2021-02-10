#ifndef LCDUPDATER_H_
#define LCDUPDATER_H_

#include "RepRapFirmware.h"
#include "crc32_lcd.h"
#include "MessageType.h"

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
		errorTimeoutLcd,
		errorTimeoutFw,
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
	void MessageF(MessageType type, const char *fmt, ...);
	errorId readPacket(uint32_t msTimeout);
	UARTClass& uploadPort;
	FileStore *uploadFile;
	UploadState state;
	uint32_t lastAttemptTime;
	FilePosition fileSize;

	static constexpr char lcdFrameBegin = 0xAA;
	static constexpr uint8_t framePayloadLength = 96;
	static constexpr uint32_t firstAckTimeout = 7000; 		// 7s is enough. We need this time to wait for erase lcd flash space.

	uint32_t packageAckTimeout;
	uint32_t fileOffset;
    uint16_t filePayload;
    uint16_t idPackage;
    uint8_t retransmissionTry;
	uint8_t payloadBuffer[framePayloadLength];
	bool retransmission;
};

#endif /* LCDUPDATER_H_ */
