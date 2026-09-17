#pragma once

//   1) TryConnectDriver()      — opens \\.\{UeDmp GUID} if loaded.
//                                No PID required.  Prints a driver-
//                                connected banner before we even
//                                know which process to attach to.
//   2) (wait for the game)
//   3) ReaderInit(pid)         — binds the reader to that PID.
//                                Uses the driver connection from
//                                step 1 if it succeeded, otherwise
//                                falls back to OpenProcess + RPM.


#include <windows.h>
#include <cstdint>

extern uint64_t Base;

bool Read(void* address, void* buffer, size_t size);

template<typename T>
T Read(void* address)
{
	T buffer{};
	Read(address, &buffer, sizeof(T));
	return buffer;
}


bool TryConnectDriver();
bool ReaderInit(uint32_t pid);
bool DriverConnected();

const char* ReaderModeName();

bool ResolveModule(uint32_t pid, const wchar_t* moduleName,
                   void*& base, uint32_t& size);
