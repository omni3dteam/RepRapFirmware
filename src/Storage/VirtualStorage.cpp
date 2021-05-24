#include "VirtualStorage.h"


VirtualStorage::VirtualStorage()
{
	Init();
}

void VirtualStorage::Init()
{
	virtualDir = '1';
	enabled = false;
	uploadFirmware = false;
	filename[0] = 0;
}

bool VirtualStorage::GetVirtualFileList(char dir, OutputBuffer *response, bool label)
{
	bool status = false;

	if (virtualDir == dir && enabled)
	{
		FileStore * const f = reprap.GetPlatform().OpenSysFile(FILES_LIST_DIR FILES_LIST, OpenMode::read);
		if (f == nullptr)
		{
			reprap.GetPlatform().MessageF(ErrorMessage, "Failed to open file %s\n", GET_FILES_LIST());
		}
		else
		{
			char byte;
			size_t len = f->Length();

			if (label)
			{
				for (size_t i = 0; i < len; ++i)
				{
					f->Read(byte);
					const char b = byte;
					if (b != '\n')
					{
						response->cat(&b, 1);
					}
				}
			}
			else
			{
				const size_t sz = 1024;
				char buff[sz];

				for (;;)
				{
					memset(buff, 0, sz);
					if (f->ReadLine(buff, sz) < 0)
					{
						break;
					}

					char *pch = buff;

					for (;;)
					{
						pch = strstr(pch, "\"name\":");
						if (pch)
						{
							const char sep = ',';

							while (*(pch++) != ':')
								;
							while (*pch != sep)
							{
								while (strchr(" \t=:", *pch) != 0)	// skip the possible separators
								{
									pch++;
								}
								response->cat(pch, 1);

								pch++;
							}
							response->cat(&sep, 1);
						}
						else
						{
							break;
						}
					}
				}
			}

			response->catf("],\"next\":%u,\"err\":%u}", 0, 0);
		}
		f->Close();
		status = true;
	}

	return status;
}

void VirtualStorage::SendDownloadRequest(OutputBuffer *r)
{
	if (filename[0] != 0)
	{
		if (uploadFirmware)
		{
			r->catf(",\"usbFirmware\":%d", uploadFirmware);
			//debugPrintf("UPLOAD!\n");
		}
		r->cat(",\"getFile\":");
		r->EncodeString(filename, false);
	}
}

void VirtualStorage::SetFileToDownload(const char *name)
{
	memcpy(filename, name, 128);
}

void VirtualStorage::UploadRequest()
{
	uploadFirmware = true;

	const char *firmware = UPDATE_FIRMWARE_FILE;
	memcpy(filename, firmware, strlen(firmware));
}

void VirtualStorage::StopDownloadRequest()
{
	filename[0] = 0;
	uploadFirmware = false;
}

bool VirtualStorage::SelectFileToPrint(const char *file)
{
	bool status = false;

	if (virtualDir == file[0] && enabled)
	{
		if (file[1] == ':')
		{
			const char *p = file + 3;
			memcpy(filename, p, 128);
			//debugPrintf("File to print: %s", filename);
			status = true;
		}
	}

	return status;
}

GCodeResult VirtualStorage::Configure(GCodeBuffer& gb, const StringRef& reply)
{
	bool seen = false;

	if (gb.Seen('S'))
	{
		enabled = gb.GetUIValue();
		seen = true;
	}

	if (gb.Seen('D'))
	{
		virtualDir = '0' + gb.GetUIValue();
		seen = true;
	}

	if (!seen)
	{
		reply.printf("Virtual Storage is %s, dir number: %c", enabled ? "enabled" : "disabled", virtualDir);
	}

	return GCodeResult::ok;
}

bool VirtualStorage::Mount(size_t card)
{
	return ((card == (size_t)(virtualDir - '0')) && enabled);
}

bool VirtualStorage::IsDriveMounted(size_t card)
{
	return Mount(card);
}
