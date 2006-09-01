/******************************************************************************
 * dhahelper.c: direct hardware access under Windows NT/2000/XP
 * Copyright (c) 2004 Sascha Sommer <saschasommer@freenet.de>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 *****************************************************************************/


#include <ntddk.h>
#include "dhahelper.h"

#define OutputDebugString DbgPrint

#define IOPM_SIZE 0x2000
typedef char IOPM[IOPM_SIZE];
static IOPM *pIOPM = NULL;



typedef struct {
  PMDL Mdl;
  PVOID SystemVirtualAddress;
  PVOID UserVirtualAddress;
  ULONG PhysMemSizeInBytes;
}alloc_priv;
static alloc_priv* alloclist;
static unsigned int alloccount=0;







static NTSTATUS dhahelperdispatch(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
static void dhahelperunload(IN PDRIVER_OBJECT DriverObject);
static NTSTATUS UnmapPhysicalMemory(PVOID UserVirtualAddress);
static NTSTATUS MapPhysicalMemoryToLinearSpace(PVOID pPhysAddress,ULONG PhysMemSizeInBytes,PVOID *PhysMemLin);

void Ke386SetIoAccessMap(int, IOPM *);
void Ke386QueryIoAccessMap(int, IOPM *);
void Ke386IoSetAccessProcess(PEPROCESS, int);




//entry point
NTSTATUS DriverEntry (IN PDRIVER_OBJECT DriverObject,IN PUNICODE_STRING RegistryPath){
  UNICODE_STRING  DeviceNameUnicodeString;
  UNICODE_STRING  DeviceLinkUnicodeString;
  NTSTATUS        ntStatus;
  PDEVICE_OBJECT  DeviceObject = NULL;

  OutputDebugString ("dhahelper: entering DriverEntry");

  RtlInitUnicodeString (&DeviceNameUnicodeString, L"\\Device\\DHAHELPER");

  // Create an EXCLUSIVE device object (only 1 thread at a time
  // can make requests to this device).

  ntStatus = IoCreateDevice(DriverObject,0,&DeviceNameUnicodeString,FILE_DEVICE_DHAHELPER,0,TRUE,&DeviceObject);
	
  if (NT_SUCCESS(ntStatus)){
    // Create dispatch points for device control, create, close.
    DriverObject->MajorFunction[IRP_MJ_CREATE] =
    DriverObject->MajorFunction[IRP_MJ_CLOSE] =
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = dhahelperdispatch;
    DriverObject->DriverUnload = dhahelperunload;

    // Create a symbolic link, e.g. a name that a Win32 app can specify
    // to open the device.

    RtlInitUnicodeString (&DeviceLinkUnicodeString, L"\\DosDevices\\DHAHELPER");

    ntStatus = IoCreateSymbolicLink(&DeviceLinkUnicodeString,&DeviceNameUnicodeString);

    if (!NT_SUCCESS(ntStatus)){
      // Symbolic link creation failed- note this & then delete the
      // device object (it's useless if a Win32 app can't get at it).
      OutputDebugString ("dhahelper: IoCreateSymbolicLink failed");
      IoDeleteDevice (DeviceObject);
    }
  }
  else{
    OutputDebugString ("dhahelper: IoCreateDevice failed");
  }
  OutputDebugString ("dhahelper: leaving DriverEntry");
  return ntStatus;
}


// Process the IRPs sent to this device

static NTSTATUS dhahelperdispatch(IN PDEVICE_OBJECT DeviceObject,IN PIRP Irp){
  PIO_STACK_LOCATION IrpStack;
  ULONG              dwInputBufferLength;
  ULONG              dwOutputBufferLength;
  ULONG              dwIoControlCode;
  PVOID              pvIOBuffer;
  NTSTATUS           ntStatus;
  dhahelper_t      dhahelper_priv;

  OutputDebugString ("dhahelper: entering dhahelperdispatch");

  // Init to default settings

  Irp->IoStatus.Status      = STATUS_SUCCESS;
  Irp->IoStatus.Information = 0;
  
  IrpStack = IoGetCurrentIrpStackLocation(Irp);

  // Get the pointer to the input/output buffer and it's length

  pvIOBuffer           = Irp->AssociatedIrp.SystemBuffer;
  dwInputBufferLength  = IrpStack->Parameters.DeviceIoControl.InputBufferLength;
  dwOutputBufferLength = IrpStack->Parameters.DeviceIoControl.OutputBufferLength;

  switch (IrpStack->MajorFunction){
    case IRP_MJ_CREATE:
      OutputDebugString("dhahelper: IRP_MJ_CREATE");
      break;
    case IRP_MJ_CLOSE:
      OutputDebugString("dhahelper: IRP_MJ_CLOSE");
      break;
    case IRP_MJ_DEVICE_CONTROL:
      OutputDebugString("dhahelper: IRP_MJ_DEVICE_CONTROL");
      dwIoControlCode = IrpStack->Parameters.DeviceIoControl.IoControlCode;
      switch (dwIoControlCode){
        case IOCTL_DHAHELPER_ENABLEDIRECTIO:
          OutputDebugString("dhahelper: IOCTL_DHAHELPER_ENABLEDIRECTIO");
          pIOPM = MmAllocateNonCachedMemory(sizeof(IOPM));
          if (pIOPM){
            RtlZeroMemory(pIOPM, sizeof(IOPM));
            Ke386IoSetAccessProcess(PsGetCurrentProcess(), 1);
            Ke386SetIoAccessMap(1, pIOPM);
          }
          else Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
          break;
        case IOCTL_DHAHELPER_DISABLEDIRECTIO:
          OutputDebugString("dhahelper: IOCTL_DHAHELPER_DISABLEDIRECTIO");
          if (pIOPM){
            Ke386IoSetAccessProcess(PsGetCurrentProcess(), 0);
            Ke386SetIoAccessMap(1, pIOPM);
            MmFreeNonCachedMemory(pIOPM, sizeof(IOPM));
            pIOPM = NULL;
          }
          break;
        case IOCTL_DHAHELPER_MAPPHYSTOLIN:
          OutputDebugString("dhahelper: IOCTL_DHAHELPER_MAPPHYSTOLIN");
          if (dwInputBufferLength){
            memcpy (&dhahelper_priv, pvIOBuffer, dwInputBufferLength);
            ntStatus = MapPhysicalMemoryToLinearSpace(dhahelper_priv.base,dhahelper_priv.size,&dhahelper_priv.ptr);
            if (NT_SUCCESS(ntStatus)){
              memcpy (pvIOBuffer, &dhahelper_priv, dwInputBufferLength);
              Irp->IoStatus.Information = dwInputBufferLength;
            }
            Irp->IoStatus.Status = ntStatus;
          }
          else Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        break;
        case IOCTL_DHAHELPER_UNMAPPHYSADDR:
          OutputDebugString("dhahelper: IOCTL_DHAHELPER_UNMAPPHYSADDR");
          if (dwInputBufferLength){
            memcpy (&dhahelper_priv, pvIOBuffer, dwInputBufferLength);
            ntStatus = UnmapPhysicalMemory(dhahelper_priv.ptr);
            Irp->IoStatus.Status = ntStatus;
          }
          else
            Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
            break;
         default:
          OutputDebugString("dhahelper: unknown IRP_MJ_DEVICE_CONTROL");
          Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
          break;
      }
      break;
  }

  // DON'T get cute and try to use the status field of the irp in the
  // return status.  That IRP IS GONE as soon as you call IoCompleteRequest.

  ntStatus = Irp->IoStatus.Status;

  IoCompleteRequest (Irp, IO_NO_INCREMENT);

  // We never have pending operation so always return the status code.

  OutputDebugString("dhahelper: leaving dhahelperdispatch");

  return ntStatus;
}

// Delete the associated device and return

static void dhahelperunload(IN PDRIVER_OBJECT DriverObject){
  UNICODE_STRING DeviceLinkUnicodeString;
  NTSTATUS ntStatus=STATUS_SUCCESS;
  OutputDebugString ("dhahelper: entering dhahelperunload");
  OutputDebugString ("dhahelper: unmapping remaining memory");
  
  while(alloccount && (ntStatus==STATUS_SUCCESS))ntStatus = UnmapPhysicalMemory(alloclist[alloccount-1].UserVirtualAddress);
  RtlInitUnicodeString (&DeviceLinkUnicodeString, L"\\DosDevices\\DHAHELPER");
  ntStatus = IoDeleteSymbolicLink (&DeviceLinkUnicodeString);

  if (NT_SUCCESS(ntStatus)){
    IoDeleteDevice (DriverObject->DeviceObject);
  }
  else {
    OutputDebugString ("dhahelper: IoDeleteSymbolicLink failed");
  }
  OutputDebugString ("dhahelper: leaving dhahelperunload");
}






/************************* memory mapping functions ******************************/
//unlike the functions of other io helpers these functions allow to map adapter memory on windows xp
//even if it has alread been mapped by the original driver
//the technique used is described in
//http://support.microsoft.com/default.aspx?scid=kb;en-us;q189327
//furthermore it keeps a list of mapped areas to free them when the driver gets unloaded
//I'm not sure what the limitations of ZwMapViewOfSection are but mapping 128MB videoram (that is probably already mapped by the gfxcard driver)
//won't work so it is generally a good idea to map only the memory you really need

static NTSTATUS MapPhysicalMemoryToLinearSpace(PVOID pPhysAddress,ULONG PhysMemSizeInBytes,PVOID *PhysMemLin){
  alloc_priv* alloclisttmp;
  PMDL Mdl=NULL;
  PVOID SystemVirtualAddress=NULL;
  PVOID UserVirtualAddress=NULL;
  PHYSICAL_ADDRESS   pStartPhysAddress;
  OutputDebugString ("dhahelper: entering MapPhysicalMemoryToLinearSpace");
    
  pStartPhysAddress.QuadPart = (ULONGLONG)pPhysAddress;
  __try {
    SystemVirtualAddress=MmMapIoSpace(pStartPhysAddress,PhysMemSizeInBytes, /*MmWriteCombined*/MmNonCached);
    if(!SystemVirtualAddress){
      OutputDebugString("dhahelper: MmMapIoSpace failed");
      return STATUS_INVALID_PARAMETER;
    } 
    OutputDebugString("dhahelper: SystemVirtualAddress 0x%x",SystemVirtualAddress);
    Mdl=IoAllocateMdl(SystemVirtualAddress, PhysMemSizeInBytes, FALSE, FALSE,NULL);
    if(!Mdl){
      OutputDebugString("dhahelper: IoAllocateMdl failed");
      return STATUS_INSUFFICIENT_RESOURCES;
    }
    OutputDebugString("dhahelper: Mdl 0x%x",Mdl);
    MmBuildMdlForNonPagedPool(Mdl);
    UserVirtualAddress = (PVOID)(((ULONG)PAGE_ALIGN(MmMapLockedPages(Mdl,UserMode))) + MmGetMdlByteOffset(Mdl));
    if(!UserVirtualAddress){
      OutputDebugString("dhahelper: MmMapLockedPages failed");
      return STATUS_INSUFFICIENT_RESOURCES;
    }
    OutputDebugString("dhahelper: UserVirtualAddress 0x%x",UserVirtualAddress);
  }__except(EXCEPTION_EXECUTE_HANDLER){  
       NTSTATUS           ntStatus; 
       ntStatus = GetExceptionCode(); 
       OutputDebugString("dhahelper: MapPhysicalMemoryToLinearSpace failed due to exception 0x%0x\n", ntStatus);
       return ntStatus;       
  }

  
  OutputDebugString("dhahelper: adding data to internal allocation list");
  alloclisttmp=MmAllocateNonCachedMemory((alloccount+1)*sizeof(alloc_priv));
  

  if(!alloclisttmp){
    OutputDebugString("dhahelper: not enough memory to create temporary allocation list");
    MmUnmapLockedPages(UserVirtualAddress, Mdl); 
    IoFreeMdl(Mdl);
    return STATUS_INSUFFICIENT_RESOURCES;
  }
  if(alloccount){
    memcpy(alloclisttmp,alloclist,alloccount * sizeof(alloc_priv));
    MmFreeNonCachedMemory(alloclist,alloccount*sizeof(alloc_priv));
  }
  alloclist=alloclisttmp;
  alloclist[alloccount].Mdl=Mdl;
  alloclist[alloccount].SystemVirtualAddress=SystemVirtualAddress;
  alloclist[alloccount].UserVirtualAddress=UserVirtualAddress;
  alloclist[alloccount].PhysMemSizeInBytes=PhysMemSizeInBytes;
  ++alloccount;
  
   *PhysMemLin=UserVirtualAddress;	
  
  OutputDebugString("dhahelper: leaving MapPhysicalMemoryToLinearSpace");
  return STATUS_SUCCESS;
}

static NTSTATUS UnmapPhysicalMemory(PVOID UserVirtualAddress){
  unsigned int i;
  unsigned int x=0;
  unsigned int alloccounttmp=alloccount;
  OutputDebugString("dhahelper: entering UnmapPhysicalMemory to unmapp 0x%x",UserVirtualAddress);
  if(!alloccount){
    OutputDebugString("dhahelper: UnmapPhysicalMemory: nothing todo -> leaving...");
    return STATUS_SUCCESS;
  }
 
  for(i=0;i<alloccount;i++){
    if(alloclist[i].UserVirtualAddress!=UserVirtualAddress){
      if(x!=i){
       alloclist[x].Mdl=alloclist[i].Mdl;
       alloclist[x].SystemVirtualAddress=alloclist[i].SystemVirtualAddress;
       alloclist[x].UserVirtualAddress=alloclist[i].UserVirtualAddress;
       alloclist[x].PhysMemSizeInBytes=alloclist[i].PhysMemSizeInBytes;
 
      }
      x++;
    }
    else if(alloclist[i].UserVirtualAddress==UserVirtualAddress){
      if(x==i){
        __try {
          MmUnmapLockedPages(alloclist[x].UserVirtualAddress, alloclist[x].Mdl); 
          IoFreeMdl(alloclist[x].Mdl);
          MmUnmapIoSpace(alloclist[x].SystemVirtualAddress,alloclist[x].PhysMemSizeInBytes);       
        }__except(EXCEPTION_EXECUTE_HANDLER){  
          NTSTATUS           ntStatus; 
          ntStatus = GetExceptionCode(); 
          OutputDebugString("dhahelper: UnmapPhysicalMemory failed due to exception 0x%0x (Mdl 0x%x)\n", ntStatus,alloclist[x].Mdl);
          return ntStatus;       
        }
      }
      alloccounttmp--;
    }
    
  }
  
  if(alloccounttmp){
      alloc_priv* alloclisttmp;
      alloclisttmp=MmAllocateNonCachedMemory(alloccounttmp*sizeof(alloc_priv));
      if(!alloclisttmp){
        OutputDebugString("dhahelper: not enough memory to create temporary allocation list");
        return STATUS_INSUFFICIENT_RESOURCES;
      }
      memcpy(alloclisttmp,alloclist,alloccounttmp * sizeof(alloc_priv));
      MmFreeNonCachedMemory(alloclist,alloccount*sizeof(alloc_priv));
      alloclist=alloclisttmp;
  }
  alloccount=alloccounttmp;
  
  OutputDebugString("dhahelper: leaving UnmapPhysicalMemory");
  return STATUS_SUCCESS;
}
