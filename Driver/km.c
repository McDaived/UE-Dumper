
#include <ntifs.h>          
#include <ntimage.h>
#include <ntstrsafe.h>

#ifndef UEDMP_DEVICE_NAME
#define UEDMP_DEVICE_NAME     L"\\Device\\{7F3A9C82-4E1D-4B65-B824-9F1E3D5A8C21}"
#define UEDMP_DOS_NAME        L"\\DosDevices\\{7F3A9C82-4E1D-4B65-B824-9F1E3D5A8C21}"
#define IOCTL_UEDMP_READ_MEM      CTL_CODE(FILE_DEVICE_UNKNOWN, 0xC17, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
#define IOCTL_UEDMP_MODULE_INFO   CTL_CODE(FILE_DEVICE_UNKNOWN, 0xC2A, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
#define UEDMP_MAX_READ  (256 * 1024)

typedef struct {
    ULONG   pid;
    ULONG64 address;
    ULONG   size;
    ULONG   bytesRead;
} IO_READ_REQ;

typedef struct {
    ULONG   pid;
    ULONG   moduleIndex;
    ULONG64 moduleBase;
    ULONG   moduleSize;
    WCHAR   moduleName[260];
} IO_MODULE_REQ;
#endif

NTSTATUS NTAPI MmCopyVirtualMemory(
    PEPROCESS  SourceProcess,
    PVOID      SourceAddress,
    PEPROCESS  TargetProcess,
    PVOID      TargetAddress,
    SIZE_T     BufferSize,
    KPROCESSOR_MODE PreviousMode,
    PSIZE_T    ReturnSize
);

NTSTATUS NTAPI IoCreateDriver(
    PUNICODE_STRING    DriverName,
    PDRIVER_INITIALIZE InitializationFunction
);

PPEB NTAPI PsGetProcessPeb(PEPROCESS Process);
#define PEB_LDR_OFFSET  0x18

// Minimal LDR layout
typedef struct _KM_PEB_LDR_DATA {
    ULONG      Length;
    UCHAR      Initialized;
    PVOID      SsHandle;
    LIST_ENTRY InLoadOrderModuleList;
} KM_PEB_LDR_DATA;

typedef struct _KM_LDR_ENTRY {
    LIST_ENTRY      InLoadOrderLinks;
    LIST_ENTRY      InMemoryOrderLinks;
    LIST_ENTRY      InInitOrderLinks;
    PVOID           DllBase;
    PVOID           EntryPoint;
    ULONG           SizeOfImage;
    UNICODE_STRING  FullDllName;
    UNICODE_STRING  BaseDllName;
} KM_LDR_ENTRY;


static PDRIVER_OBJECT g_drvObj = NULL;
static UNICODE_STRING g_dosName;


static VOID WipePEHeader(PVOID base) {
    if (!base || !MmIsAddressValid(base)) return;
    __try {
        PMDL mdl = IoAllocateMdl(base, 0x400, FALSE, FALSE, NULL);
        if (!mdl) return;
        BOOLEAN locked = FALSE;
        __try {
            __try {
                MmProbeAndLockPages(mdl, KernelMode, IoWriteAccess);
                locked = TRUE;
                PVOID mapped = MmGetSystemAddressForMdlSafe(
                    mdl, NormalPagePriority | MdlMappingNoExecute);
                if (mapped && MmIsAddressValid(mapped)) {
                    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)mapped;
                    LONG lfanew = dos->e_lfanew;
                    dos->e_magic = 0;
                    if (lfanew > 0 && lfanew < 0x400 - (LONG)sizeof(ULONG))
                        *(PULONG)((PUCHAR)mapped + lfanew) = 0;
                }
            } __except(EXCEPTION_EXECUTE_HANDLER) { }
        } __finally {
            if (locked) MmUnlockPages(mdl);
        }
        IoFreeMdl(mdl);
    } __except(EXCEPTION_EXECUTE_HANDLER) { }
}

static NTSTATUS HandleRead(PIRP Irp, PIO_STACK_LOCATION stack) {
    ULONG inLen  = stack->Parameters.DeviceIoControl.InputBufferLength;
    ULONG outLen = stack->Parameters.DeviceIoControl.OutputBufferLength;

    if (inLen < sizeof(IO_READ_REQ))
        return STATUS_INVALID_PARAMETER;

    IO_READ_REQ* req = (IO_READ_REQ*)Irp->AssociatedIrp.SystemBuffer;

    if (req->pid == 0 || req->pid == 4)
        return STATUS_ACCESS_DENIED;
    if (req->size == 0 || req->size > UEDMP_MAX_READ)
        return STATUS_INVALID_PARAMETER;
    if (req->address < 0x10000 || req->address > 0x7FFFFFFFFFFF)
        return STATUS_INVALID_PARAMETER;
    if (outLen < sizeof(IO_READ_REQ) + req->size)
        return STATUS_BUFFER_TOO_SMALL;

    PEPROCESS process = NULL;
    NTSTATUS  status  = PsLookupProcessByProcessId(
        (HANDLE)(ULONG_PTR)req->pid, &process);
    if (!NT_SUCCESS(status))
        return status;

    PVOID  dest   = (PUCHAR)Irp->AssociatedIrp.SystemBuffer + sizeof(IO_READ_REQ);
    SIZE_T copied = 0;

    __try {
        status = MmCopyVirtualMemory(
            process,               (PVOID)(ULONG_PTR)req->address,
            PsGetCurrentProcess(), dest,
            (SIZE_T)req->size,
            KernelMode,
            &copied);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        status = STATUS_INTERNAL_ERROR;
    }

    ObDereferenceObject(process);

    req->bytesRead = (ULONG)copied;
    Irp->IoStatus.Information = sizeof(IO_READ_REQ) + (ULONG)copied;
    return status;
}

static NTSTATUS HandleModuleInfo(PIRP Irp, PIO_STACK_LOCATION stack) {
    ULONG inLen  = stack->Parameters.DeviceIoControl.InputBufferLength;
    ULONG outLen = stack->Parameters.DeviceIoControl.OutputBufferLength;

    if (inLen < sizeof(IO_MODULE_REQ) || outLen < sizeof(IO_MODULE_REQ))
        return STATUS_INVALID_PARAMETER;

    IO_MODULE_REQ* req = (IO_MODULE_REQ*)Irp->AssociatedIrp.SystemBuffer;

    if (req->pid == 0 || req->pid == 4)
        return STATUS_ACCESS_DENIED;
    if (req->moduleIndex > 1024)
        return STATUS_INVALID_PARAMETER;

    PEPROCESS process = NULL;
    NTSTATUS  status  = PsLookupProcessByProcessId(
        (HANDLE)(ULONG_PTR)req->pid, &process);
    if (!NT_SUCCESS(status))
        return status;

    KAPC_STATE apc;
    KeEnterCriticalRegion();
    KeStackAttachProcess(process, &apc);

    status = STATUS_NO_MORE_ENTRIES;

    __try {
        PVOID peb = (PVOID)PsGetProcessPeb(process);
        if (!peb || !MmIsAddressValid(peb))
            __leave;

        KM_PEB_LDR_DATA* ldr =
            *(KM_PEB_LDR_DATA**)((PUCHAR)peb + PEB_LDR_OFFSET);
        if (!ldr || !MmIsAddressValid(ldr))
            __leave;

        PLIST_ENTRY head = &ldr->InLoadOrderModuleList;
        PLIST_ENTRY curr = head->Flink;
        ULONG idx = 0;

        while (curr && curr != head && idx <= 1024) {
            if (!MmIsAddressValid(curr))
                break;

            if (idx == req->moduleIndex) {
                KM_LDR_ENTRY* entry =
                    CONTAINING_RECORD(curr, KM_LDR_ENTRY, InLoadOrderLinks);

                if (!entry->DllBase || !entry->SizeOfImage)
                    break;

                req->moduleBase = (ULONG64)(ULONG_PTR)entry->DllBase;
                req->moduleSize = entry->SizeOfImage;

                RtlZeroMemory(req->moduleName, sizeof(req->moduleName));
                if (entry->BaseDllName.Buffer && entry->BaseDllName.Length > 0) {
                    USHORT copyLen = min(
                        entry->BaseDllName.Length,
                        (USHORT)(sizeof(req->moduleName) - sizeof(WCHAR)));
                    if (MmIsAddressValid(entry->BaseDllName.Buffer))
                        RtlCopyMemory(req->moduleName,
                                      entry->BaseDllName.Buffer, copyLen);
                }

                status = STATUS_SUCCESS;
                Irp->IoStatus.Information = sizeof(IO_MODULE_REQ);
                break;
            }

            ++idx;
            curr = curr->Flink;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        status = STATUS_INTERNAL_ERROR;
    }

    KeUnstackDetachProcess(&apc);
    KeLeaveCriticalRegion();
    ObDereferenceObject(process);

    return status;
}

static NTSTATUS DispatchControl(PDEVICE_OBJECT DevObj, PIRP Irp) {
    UNREFERENCED_PARAMETER(DevObj);

    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;
    Irp->IoStatus.Information = 0;

    if (!Irp->AssociatedIrp.SystemBuffer) {
        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_INVALID_PARAMETER;
    }

    switch (stack->Parameters.DeviceIoControl.IoControlCode) {
    case IOCTL_UEDMP_READ_MEM:     status = HandleRead(Irp, stack);       break;
    case IOCTL_UEDMP_MODULE_INFO:  status = HandleModuleInfo(Irp, stack); break;
    }

    Irp->IoStatus.Status = status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

static NTSTATUS DispatchPassthrough(PDEVICE_OBJECT DevObj, PIRP Irp) {
    UNREFERENCED_PARAMETER(DevObj);
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

static VOID DriverUnload(PDRIVER_OBJECT DriverObject) {
    IoDeleteSymbolicLink(&g_dosName);
    if (DriverObject->DeviceObject)
        IoDeleteDevice(DriverObject->DeviceObject);
    g_drvObj = NULL;
}

static NTSTATUS RealEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegPath) {
    UNREFERENCED_PARAMETER(RegPath);

    g_drvObj = DriverObject;
    DriverObject->DriverUnload = DriverUnload;

    UNICODE_STRING devName;
    RtlInitUnicodeString(&devName, UEDMP_DEVICE_NAME);
    RtlInitUnicodeString(&g_dosName, UEDMP_DOS_NAME);

    PDEVICE_OBJECT devObj = NULL;
    NTSTATUS status = IoCreateDevice(
        DriverObject, 0, &devName,
        FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN,
        FALSE, &devObj);
    if (!NT_SUCCESS(status)) {
        if (status != STATUS_OBJECT_NAME_COLLISION)
            return status;
    }

    IoDeleteSymbolicLink(&g_dosName);

    status = IoCreateSymbolicLink(&g_dosName, &devName);
    if (!NT_SUCCESS(status)) {
        if (devObj) IoDeleteDevice(devObj);
        return status;
    }

    if (devObj) {
        devObj->Flags |= DO_BUFFERED_IO;
        devObj->Flags &= ~DO_DEVICE_INITIALIZING;
    }

    DriverObject->MajorFunction[IRP_MJ_CREATE]         = DispatchPassthrough;
    DriverObject->MajorFunction[IRP_MJ_CLOSE]          = DispatchPassthrough;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchControl;

    if (DriverObject->DriverStart)
        WipePEHeader(DriverObject->DriverStart);

    return STATUS_SUCCESS;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    if (DriverObject)
        return RealEntry(DriverObject, RegistryPath);

    LARGE_INTEGER ts;
    KeQuerySystemTime(&ts);
    WCHAR nameBuf[128];
    RtlStringCbPrintfW(nameBuf, sizeof(nameBuf),
                       L"\\Driver\\UeDmpBus_%llx", ts.QuadPart);
    UNICODE_STRING name;
    RtlInitUnicodeString(&name, nameBuf);
    return IoCreateDriver(&name, &RealEntry);
}
