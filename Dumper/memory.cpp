#include "memory.h"
#include "../Driver/km_shared.h"
#include <vector>
#include <cstring>
#include <tlhelp32.h>


enum class Mode { Uninit, Driver, Usermode };
static Mode      s_mode  = Mode::Uninit;
static HANDLE    s_hProc = nullptr;                  // usermode fallback
static HANDLE    s_hDrv  = INVALID_HANDLE_VALUE;     // driver device
static uint32_t  s_pid   = 0;
static bool      s_drvConnected = false;

uint64_t Base;

// Driver read 
static bool DriverRead(void* address, void* buffer, size_t size)
{
	const uint32_t CHUNK = UEDMP_MAX_READ;
	std::vector<uint8_t> ioBuf(sizeof(IO_READ_REQ) + CHUNK);

	size_t total = 0;
	while (total < size) {
		uint32_t want = (uint32_t)((size - total) > CHUNK
		                           ? CHUNK : (size - total));
		std::memset(ioBuf.data(), 0, sizeof(IO_READ_REQ));
		IO_READ_REQ* req = (IO_READ_REQ*)ioBuf.data();
		req->pid     = s_pid;
		req->address = (unsigned long long)((uintptr_t)address + total);
		req->size    = want;

		DWORD returned = 0;
		BOOL ok = DeviceIoControl(
			s_hDrv, IOCTL_UEDMP_READ_MEM,
			ioBuf.data(), sizeof(IO_READ_REQ),
			ioBuf.data(), (DWORD)(sizeof(IO_READ_REQ) + want),
			&returned, nullptr);
		if (!ok || returned < sizeof(IO_READ_REQ))
			return total > 0;

		uint32_t got = req->bytesRead;
		if (got == 0)
			return total > 0;

		std::memcpy((uint8_t*)buffer + total,
		            ioBuf.data() + sizeof(IO_READ_REQ),
		            got);
		total += got;
		if (got < want)
			return true;  
	}
	return total == size;
}

bool TryConnectDriver()
{
	if (s_hDrv != INVALID_HANDLE_VALUE) {
		s_drvConnected = true;
		return true;   
	}
	s_hDrv = CreateFileW(UEDMP_USERMODE_PATH,
	                     GENERIC_READ | GENERIC_WRITE,
	                     0, nullptr, OPEN_EXISTING,
	                     FILE_ATTRIBUTE_NORMAL, nullptr);
	s_drvConnected = (s_hDrv != INVALID_HANDLE_VALUE);
	return s_drvConnected;
}

bool DriverConnected() { return s_drvConnected; }


bool ReaderInit(uint32_t pid)
{
	s_pid = pid;

	if (s_drvConnected) {
		s_mode = Mode::Driver;
		return true;
	}

	if (TryConnectDriver()) {
		s_mode = Mode::Driver;
		return true;
	}
	s_hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
	if (s_hProc) {
		s_mode = Mode::Usermode;
		return true;
	}

	s_mode = Mode::Uninit;
	return false;
}

bool Read(void* address, void* buffer, size_t size)
{
	switch (s_mode) {
	case Mode::Driver:   return DriverRead(address, buffer, size);
	case Mode::Usermode: return ReadProcessMemory(s_hProc, address, buffer, size, nullptr);
	default:             return false;
	}
}

const char* ReaderModeName()
{
	switch (s_mode) {
	case Mode::Driver:   return "kernel driver (MmCopyVirtualMemory)";
	case Mode::Usermode: return "usermode (OpenProcess + ReadProcessMemory)";
	default:             return "uninitialized";
	}
}

static bool DriverResolveModule(uint32_t pid, const wchar_t* moduleName,
                                void*& base, uint32_t& size)
{
	if (s_hDrv == INVALID_HANDLE_VALUE) return false;

	for (uint32_t i = 0; i < 1024; ++i) {
		IO_MODULE_REQ req{};
		req.pid         = pid;
		req.moduleIndex = i;
		DWORD returned = 0;
		BOOL ok = DeviceIoControl(
			s_hDrv, IOCTL_UEDMP_MODULE_INFO,
			&req, sizeof(req),
			&req, sizeof(req),
			&returned, nullptr);
		if (!ok || returned < sizeof(req))
			break;   
		if (_wcsicmp(req.moduleName, moduleName) != 0)
			continue;

		base = (void*)(uintptr_t)req.moduleBase;
		size = req.moduleSize;
		return true;
	}
	return false;
}

static bool ToolhelpResolveModule(uint32_t pid, const wchar_t* moduleName,
                                  void*& base, uint32_t& size)
{
	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
	if (snap == INVALID_HANDLE_VALUE) return false;

	bool found = false;
	MODULEENTRY32W mod{};
	mod.dwSize = sizeof(mod);
	if (Module32FirstW(snap, &mod)) {
		do {
			if (_wcsicmp(mod.szModule, moduleName) == 0) {
				base  = mod.modBaseAddr;
				size  = mod.modBaseSize;
				found = true;
				break;
			}
		} while (Module32NextW(snap, &mod));
	}
	CloseHandle(snap);
	return found;
}

bool ResolveModule(uint32_t pid, const wchar_t* moduleName,
                   void*& base, uint32_t& size)
{
	if (s_drvConnected && DriverResolveModule(pid, moduleName, base, size))
		return true;

	return ToolhelpResolveModule(pid, moduleName, base, size);
}
