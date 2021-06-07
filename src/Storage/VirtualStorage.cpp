#include "VirtualStorage.h"


VirtualStorage::VirtualStorage()
{
	Init();
}

void VirtualStorage::Init()
{
	virtualDir = '2';
	filename[0] = 0;

	enabled = false;
	isUsbPrinting = false;
	uploadFirmware = false;
}



bool VirtualStorage::GetVirtualFileList(const char* dir, OutputBuffer *response, bool label)
{
	bool status = false;

	if (virtualDir == dir[0] && dir[1] == ':' && enabled)
	{
		String<FormatStringLength> path;
		path.printf("%s%s/%s", FILES_LIST_DIR, dir[2] == '/' ? &dir[3] : "", FILES_LIST);

		FileStore * const f = reprap.GetPlatform().OpenSysFile(path.c_str(), OpenMode::read);
		if (f == nullptr)
		{
			response->cat("{\"err\":1}");
			reprap.GetPlatform().MessageF(ErrorMessage, "Failed to open list file %s\n", path.c_str());
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
				// Is 256 sufficient?
				const size_t sz = 256;
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

// Use this function in order to find last '/' which helps to find path and file
int VirtualStorage::getDirectoryOffset(const char* dir)
{
	const char sep = '/';
    char* ptr = nullptr;
    char* lastSlash = nullptr;

    ptr = strchr(dir, sep);
    while(ptr != nullptr)
    {
        lastSlash = ptr;
        ptr = strchr(ptr + 1, sep);
    }

    return lastSlash - dir;
}


bool VirtualStorage::GetVirtualFileInfo(const char* filename, OutputBuffer *response)
{
	bool status = false;

	if (enabled && filename[0] == virtualDir && filename[1] == ':' && filename[2] == '/')
	{
		String<FormatStringLength> path;

		int offset = getDirectoryOffset(filename);

		if (offset > 0)
		{
			path.printf("%s%s", FILES_LIST_DIR, &filename[3]);	// create path based on internal usb address
			path[offset + 5] = 0;								// remove real filename
			path.cat(FILES_LIST);								// and file with json data
		}
		else
		{
			return status;
		}

		const char* fileWithExtension = &filename[offset + 1];
		int i = 0;

		FileStore * const f = reprap.GetPlatform().OpenSysFile(path.c_str(), OpenMode::read);
		if (f == nullptr)
		{
			reprap.GetPlatform().MessageF(ErrorMessage, "Failed to open file info %s\n", path.c_str());
		}
		else
		{
			const size_t sz = 256;
			char buff[sz];

			for (;;)
			{
				char* file = nullptr;

				if (f->ReadLine(buff, sz) > 0)
				{
					file = strstr(buff, fileWithExtension);

					if (file != nullptr)
					{
						break;
					}
					++i;
				}
				else
				{
					f->Close();
					return false;
				}
			}
			f->Close();

			FileStore * const info = reprap.GetPlatform().OpenSysFile(path.c_str(), OpenMode::read);
			if (info == nullptr)
			{
				reprap.GetPlatform().MessageF(ErrorMessage, "Failed to open file %s\n", GET_FILES_INFO());
			}
			else
			{
				int j = 0;

				for (;;)
				{
					if (f->ReadLine(buff, sz) > 0)
					{
						if (i == j)
						{
							response->cat(buff, strlen(buff));
							status = true;
							break;
						}
						j++;
					}
					else
					{
						info->Close();
						response->cat("{\"err\":1}");
						return false;
					}
				}
			}
			info->Close();
		}
	}

	return status;
}

void VirtualStorage::SetFileToDownload(const char *name)
{
	memcpy(filename, name, MAX_FILE_LENGTH);
}

void VirtualStorage::UploadRequest()
{
	uploadFirmware = true;

	const char *firmware = UPDATE_FIRMWARE_FILE;
	memcpy(filename, firmware, strlen(firmware));
}

void VirtualStorage::EndPrinting()
{
	filename[0] = 0;
	isUsbPrinting = false;
}

bool VirtualStorage::SelectFileToPrint(const char *file)
{
	bool status = false;

	if (virtualDir == file[0] && enabled && !isUsbPrinting)
	{
		if (file[1] == ':')
		{
			const char *p = file + 3;
			memcpy(filename, p, MAX_FILE_LENGTH);
			debugPrintf("File to print: %s\n", filename);

			String<FormatStringLength> commandSelect;
			commandSelect.printf("select \"%s\"\n", filename);
			reprap.GetPlatform().Message(TelnetMessage, commandSelect.c_str());
			debugPrintf("Select cmd: %s\n", commandSelect.c_str());
			isUsbPrinting = status = true;
		}
	}

	return status;
}

void VirtualStorage::RequestStartPrinting()
{
	if (enabled && isUsbPrinting)
	{
		SendBasicCommand("start");
		isUsbPrinting = true;
	}
}

void VirtualStorage::RequestResumePrinting()
{
	RequestStartPrinting();
}

void VirtualStorage::RequestPausePrinting()
{
	if (enabled && isUsbPrinting)
	{
		SendBasicCommand("pause");
		isUsbPrinting = true;
	}
}

void VirtualStorage::RequestStopPrinting()
{
	if (enabled && isUsbPrinting)
	{
		SendBasicCommand("stop");
		isUsbPrinting = false;
	}
}

bool VirtualStorage::IsUsbPrinting()
{
	return isUsbPrinting;
}

void VirtualStorage::SendBasicCommand(const char* cmd)
{
	String<FormatStringLength> commandSelect;
	commandSelect.printf("%s\n", cmd);
	reprap.GetPlatform().Message(TelnetMessage, commandSelect.c_str());
	//debugPrintf("Select cmd: %s\n", commandSelect.c_str());
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
