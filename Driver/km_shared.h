#pragma once

#ifdef _KERNEL_MODE
#include <ntddk.h>
#else
#include <windows.h>
#include <winioctl.h>   // CTL_CODE, METHOD_BUFFERED, FILE_DEVICE_UNKNOWN, FILE_SPECIAL_ACCESS
#endif

// Device names — GUID-style for stealth
#define UEDMP_DEVICE_NAME     L"\\Device\\{7F3A9C82-4E1D-4B65-B824-9F1E3D5A8C21}"
#define UEDMP_DOS_NAME        L"\\DosDevices\\{7F3A9C82-4E1D-4B65-B824-9F1E3D5A8C21}"
#define UEDMP_USERMODE_PATH   L"\\\\.\\{7F3A9C82-4E1D-4B65-B824-9F1E3D5A8C21}"

#define IOCTL_UEDMP_READ_MEM      CTL_CODE(FILE_DEVICE_UNKNOWN, 0xC17, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
#define IOCTL_UEDMP_MODULE_INFO   CTL_CODE(FILE_DEVICE_UNKNOWN, 0xC2A, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)

// Cap per-read to 256 KB
#define UEDMP_MAX_READ  (256 * 1024)

typedef struct {
    unsigned long      pid;
    unsigned long long address;
    unsigned long      size;
    unsigned long      bytesRead;
} IO_READ_REQ;

typedef struct {
    unsigned long      pid;
    unsigned long      moduleIndex;
    unsigned long long moduleBase;
    unsigned long      moduleSize;
    wchar_t            moduleName[260];
} IO_MODULE_REQ;
