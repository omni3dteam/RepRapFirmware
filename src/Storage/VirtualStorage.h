#ifndef VIRTUALSTORAGE_H_
#define VIRTUALSTORAGE_H_

#include "Platform.h"
#include "RepRap.h"
//#include "GCodes/GCodes.h"
#include "GCodes/GCodeBuffer.h"

#define FILES_LIST_DIR  "0:/"
#define FILES_LIST 		"files.usb"

class VirtualStorage
{
public:
	VirtualStorage();
	void Init();
	bool Mount(size_t card);
	bool IsDriveMounted(size_t card);
	bool GetVirtualFileList(char dir, OutputBuffer *response, bool label);
	void SendDownloadRequest(OutputBuffer *r);
	void StopDownloadRequest();
	bool SelectFileToPrint(const char *file);
	void SetFileToDownload(const char *name);
	GCodeResult Configure(GCodeBuffer& gb, const StringRef& reply);

private:
	const char *GET_FILES_DIR() { return FILES_LIST_DIR; };
	const char *GET_FILES_LIST() { return FILES_LIST; };
	char virtualDir;
	bool enabled;

	char filename[128];
};

#endif
// VIRTUALSTORAGE_H_
