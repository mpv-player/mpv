/*
   This file contains implementation of VESA library which is based on
   LRMI (Linux real-mode interface).
   So it's not an emulator - it calls real int 10h handler under Linux.
   Note: VESA is available only on x86 systems.
   You can redistribute this file under terms and conditions
   GNU General Public licence v2.
   Written by Nick Kurshev <nickols_k@mail.ru>
*/
#include "vbelib.h"
#include "lrmi.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/io.h>

static struct VesaProtModeInterface vbe_pm_info;

int vbeInit( void )
{
   if(!LRMI_init()) return VBE_VM86_FAIL;
   /*
    Allow read/write to ALL io ports
   */
   ioperm(0, 1024, 1);
   iopl(3);
   memset(&vbe_pm_info,0,sizeof(struct VesaProtModeInterface));
   vbeGetProtModeInfo(&vbe_pm_info);
   return VBE_OK;
}

int vbeDestroy( void ) { return VBE_OK; }

int vbeGetControllerInfo(struct VbeInfoBlock *data)
{
  struct LRMI_regs r;
  void *rm_space;
  int retval;
  if(!(rm_space = LRMI_alloc_real(sizeof(struct VbeInfoBlock)))) return VBE_OUT_OF_DOS_MEM;
  memcpy(rm_space,data,sizeof(struct VbeInfoBlock));
  memset(&r,0,sizeof(struct LRMI_regs));
  r.eax = 0x4f00;
  r.es  = VirtToPhysSeg(rm_space);
  r.edi = VirtToPhysOff(rm_space);
  if(!LRMI_int(0x10,&r))
  {
     LRMI_free_real(rm_space);
     return VBE_VM86_FAIL;
  }
  retval = r.eax & 0xffff;
  if(retval == 0x4f)
  {
    FarPtr fpdata;
    retval = VBE_OK;
    memcpy(data,rm_space,sizeof(struct VbeInfoBlock));
    fpdata.seg = (unsigned long)(data->OemStringPtr) >> 16;
    fpdata.off = (unsigned long)(data->OemStringPtr) & 0xffff;
    data->OemStringPtr = PhysToVirt(fpdata);
    fpdata.seg = (unsigned long)(data->VideoModePtr) >> 16;
    fpdata.off = (unsigned long)(data->VideoModePtr) & 0xffff;
    data->VideoModePtr = PhysToVirt(fpdata);
    fpdata.seg = (unsigned long)(data->OemVendorNamePtr) >> 16;
    fpdata.off = (unsigned long)(data->OemVendorNamePtr) & 0xffff;
    data->OemVendorNamePtr = PhysToVirt(fpdata);
    fpdata.seg = (unsigned long)(data->OemProductNamePtr) >> 16;
    fpdata.off = (unsigned long)(data->OemProductNamePtr) & 0xffff;
    data->OemProductNamePtr = PhysToVirt(fpdata);
    fpdata.seg = (unsigned long)(data->OemProductRevPtr) >> 16;
    fpdata.off = (unsigned long)(data->OemProductRevPtr) & 0xffff;
    data->OemProductRevPtr = PhysToVirt(fpdata);
  }
  return retval;
}

int vbeGetModeInfo(unsigned mode,struct VesaModeInfoBlock *data)
{
  struct LRMI_regs r;
  void *rm_space;
  int retval;
  if(!(rm_space = LRMI_alloc_real(sizeof(struct VesaModeInfoBlock)))) return VBE_OUT_OF_DOS_MEM;
  memset(&r,0,sizeof(struct LRMI_regs));
  r.eax = 0x4f01;
  r.ecx = mode;
  r.es  = VirtToPhysSeg(rm_space);
  r.edi = VirtToPhysOff(rm_space);
  if(!LRMI_int(0x10,&r))
  {
     LRMI_free_real(rm_space);
     return VBE_VM86_FAIL;
  }
  retval = r.eax & 0xffff;
  if(retval == 0x4f)
  {
    retval = VBE_OK;
    memcpy(data,rm_space,sizeof(struct VesaModeInfoBlock));
  }
  return retval;
}

int vbeSetMode(unsigned mode,struct VesaCRTCInfoBlock *data)
{
  struct LRMI_regs r;
  void *rm_space = NULL;
  int retval;
  memset(&r,0,sizeof(struct LRMI_regs));
  if(data)
  {
    if(!(rm_space = LRMI_alloc_real(sizeof(struct VesaCRTCInfoBlock)))) return VBE_OUT_OF_DOS_MEM;
    r.es  = VirtToPhysSeg(rm_space);
    r.edi = VirtToPhysOff(rm_space);
    memcpy(rm_space,data,sizeof(struct VesaCRTCInfoBlock));
  }
  r.eax = 0x4f02;
  r.ebx = mode;
  retval = LRMI_int(0x10,&r);
  if(rm_space) LRMI_free_real(rm_space);
  if(!retval) return VBE_VM86_FAIL;
  retval = r.eax & 0xffff;
  if(retval == 0x4f) retval = VBE_OK;
  return retval;
}

int vbeGetMode(unsigned *mode)
{
  struct LRMI_regs r;
  int retval;
  memset(&r,0,sizeof(struct LRMI_regs));
  r.eax = 0x4f03;
  if(!LRMI_int(0x10,&r)) return VBE_VM86_FAIL;
  retval = r.eax & 0xffff;
  if(retval == 0x4f)
  {
    *mode = r.ebx;
    retval = VBE_OK;
  }
  return retval;
}

int vbeSaveState(void **data)
{
  struct LRMI_regs r;
  int retval;
  void *rm_space;
  memset(&r,0,sizeof(struct LRMI_regs));
  r.eax = 0x4f04;
  r.edx = 0x00;
  r.ecx = 0x0f;
  if(!LRMI_int(0x10,&r)) return VBE_VM86_FAIL;
  retval = r.eax & 0xffff;
  if(retval != 0x4f) return retval;
  if(!(rm_space = LRMI_alloc_real((r.ebx & 0xffff)*64))) return VBE_OUT_OF_DOS_MEM;
  r.eax = 0x4f04;
  r.edx = 0x01;
  r.ecx = 0x0f;
  r.es  = VirtToPhysSeg(rm_space);
  r.edi = VirtToPhysOff(rm_space);
  if(!LRMI_int(0x10,&r))
  {
    LRMI_free_real(rm_space);
    return VBE_VM86_FAIL;
  }
  retval = r.eax & 0xffff;
  if(retval != 0x4f)
  {
    LRMI_free_real(rm_space);
    return retval;
  }
  *data = rm_space;
  return VBE_OK;
}

int vbeRestoreState(void *data)
{
  struct LRMI_regs r;
  int retval;
  void *rm_space;
  memset(&r,0,sizeof(struct LRMI_regs));
  r.eax = 0x4f04;
  r.edx = 0x02;
  r.ecx = 0x0f;
  r.es  = VirtToPhysSeg(data);
  r.edi = VirtToPhysOff(data);
  retval = LRMI_int(0x10,&r);
  LRMI_free_real(data);
  if(!retval) return VBE_VM86_FAIL;
  retval = r.eax & 0xffff;
  if(retval == 0x4f) retval = VBE_OK;
  return retval;
}

int vbeGetWindow(unsigned *win_num)
{
  struct LRMI_regs r;
  int retval;
  memset(&r,0,sizeof(struct LRMI_regs));
  r.eax = 0x4f05;
  r.ebx = (*win_num & 0x0f) | 0x0100;
  if(!LRMI_int(0x10,&r)) return VBE_VM86_FAIL;
  retval = r.eax & 0xffff;
  if(retval == 0x4f)
  {
    *win_num = r.edx & 0xffff;
    retval = VBE_OK;
  }
  return retval;
}

int vbeSetWindow(unsigned win_num,unsigned win_gran)
{
  int retval;
#if 0
  if(vbe_pm_info.SetWindowCall)
  {
     /* 32-bit function call is much better of int 10h */
     __asm __volatile(
	"pushl	%%ebx\n"
	"movl	%1, %%ebx\n"
	::"a"(0x4f05),"S"(win_num & 0x0f),"d"(win_gran):"memory");
    (*vbe_pm_info.SetWindowCall)();
    __asm __volatile("popl	%%ebx":"=a"(retval)::"memory");
  }
  else
#endif
  {
    struct LRMI_regs r;
    memset(&r,0,sizeof(struct LRMI_regs));
    r.eax = 0x4f05;
    r.ebx = win_num & 0x0f;
    r.edx = win_gran;
    if(!LRMI_int(0x10,&r)) return VBE_VM86_FAIL;
    retval = r.eax & 0xffff;
  }
  if(retval == 0x4f) retval = VBE_OK;
  return retval;
}

struct realVesaProtModeInterface
{
  unsigned short SetWindowCall;
  unsigned short SetDisplayStart;
  unsigned short SetPaletteData;
  unsigned short iopl_ports;
}__attribute__((packed));

int vbeGetProtModeInfo(struct VesaProtModeInterface *pm_info)
{
  struct LRMI_regs r;
  int retval;
  struct realVesaProtModeInterface *rm_info;
  memset(&r,0,sizeof(struct LRMI_regs));
  r.eax = 0x4f0a;
  r.ebx = 0;
  if(!LRMI_int(0x10,&r)) return VBE_VM86_FAIL;
  retval = r.eax & 0xffff;
  if(retval == 0x4f)
  {
    rm_info = PhysToVirtSO(r.es,r.edi&0xffff);
    pm_info->SetWindowCall   = PhysToVirtSO(r.es,rm_info->SetWindowCall);
    pm_info->SetDisplayStart = PhysToVirtSO(r.es,rm_info->SetDisplayStart);
    pm_info->SetPaletteData  = PhysToVirtSO(r.es,rm_info->SetPaletteData);
    pm_info->iopl_ports      = PhysToVirtSO(r.es,rm_info->iopl_ports);
    retval = VBE_OK;
  }
  return retval;
}
