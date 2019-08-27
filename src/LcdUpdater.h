#ifndef LCDUPDATER_H_
#define LCDUPDATER_H_

#include "RepRapFirmware.h"

class LcdUpdater
{
public:
	bool UpdateLcdModule();
	LcdUpdater(UARTClass& port);
	bool OpenLcdFirmware();

private:
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

	void MessageF(const char *fmt, ...);
	UARTClass& uploadPort;
	FileStore *uploadFile;
	UploadState state;
	static const uint32_t blockWriteInterval = 15;
	uint32_t lastAttemptTime;
	FilePosition fileSize;
	LcdUploadResult uploadResult;

};

#endif /* LCDUPDATER_H_ */
