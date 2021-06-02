#ifndef VIRTUALSTORAGE_H_
#define VIRTUALSTORAGE_H_

#include "Platform.h"
#include "RepRap.h"
//#include "GCodes/GCodes.h"
#include "GCodes/GCodeBuffer.h"

#define FILES_LIST_DIR  "0:/usb/"
#define FILES_LIST 		"files.usb"
#define FILES_INFO		"filesinfo.usb"

#define UPDATE_FIRMWARE_FILE "usbFirmware.bin"

#define MAX_FILE_LENGTH 128


class VirtualStorage
{
public:
	VirtualStorage();
	void Init();
	bool Mount(size_t card);
	bool IsDriveMounted(size_t card);
	bool GetVirtualFileList(const char* dir, OutputBuffer *response, bool label);
	bool GetVirtualFileInfo(const char* filename, OutputBuffer *response);
	void EndPrinting();
	void UploadRequest();
	bool SelectFileToPrint(const char *file);
	void SetFileToDownload(const char *name);
	void RequestStartPrinting();
	void RequestResumePrinting();
	void RequestPausePrinting();
	void RequestStopPrinting();
	bool IsUsbPrinting();
	GCodeResult Configure(GCodeBuffer& gb, const StringRef& reply);

private:
	void SendBasicCommand(const char* cmd);
	int getDirectoryOffset(const char* dir);
	const char *GET_FILES_DIR() { return FILES_LIST_DIR; };
	const char *GET_FILES_LIST() { return FILES_LIST; };
	const char *GET_FILES_INFO() { return FILES_INFO; };
	char virtualDir;
	bool enabled;
	bool uploadFirmware;
	bool isUsbPrinting;

	char filename[MAX_FILE_LENGTH];

};

#endif
// VIRTUALSTORAGE_H_
