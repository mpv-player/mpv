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
#include <ctype.h>

static struct VesaProtModeInterface vbe_pm_info;

static inline int VERR(void *p)
{
  register int retval;
  __asm __volatile(
	"xorl	%0, %0\n\t"
	"verr	%1\n\t"
	"setnz	%b0"
	:"=r"(retval)
	:"m"(*(unsigned char *)p)
	:"memory");
  return retval;
}

#if 0
static inline int VERW(void *p)
{
  register int retval;
  __asm __volatile(
	"xorl	%0, %0\n\t"
	"verw	%1\n\t"
	"setnz	%b0"
	:"=r"(retval)
	:"m"(*(unsigned char *)p)
	:"memory");
  return retval;
}
#endif

#define HAVE_VERBOSE_VAR 1

#ifdef HAVE_VERBOSE_VAR
extern int verbose;

static void __dump_regs(struct LRMI_regs *r)
{
  printf("vbelib:    eax=%08lXh ebx=%08lXh ecx=%08lXh edx=%08lXh\n"
	 "vbelib:    edi=%08lXh esi=%08lXh ebp=%08lXh esp=%08lXh\n"
	 "vbelib:    ds=%04Xh es=%04Xh ss=%04Xh cs:ip=%04X:%04X\n"
	 "vbelib:    fs=%04Xh gs=%04Xh ss:sp=%04X:%04X flags=%04X\n"
	 ,(unsigned long)r->eax,(unsigned long)r->ebx,(unsigned long)r->ecx,(unsigned long)r->edx
	 ,(unsigned long)r->edi,(unsigned long)r->esi,(unsigned long)r->ebp,(unsigned long)r->reserved
	 ,r->ds,r->es,r->ss,r->cs,r->ip
	 ,r->fs,r->gs,r->ss,r->sp,r->flags);
}

static inline int VBE_LRMI_int(int int_no, struct LRMI_regs *r)
{
  int retval;
  if(verbose > 1) 
  {
    printf("vbelib: registers before int %02X\n",int_no);
    __dump_regs(r);
  }    
  retval = LRMI_int(int_no,r);
  if(verbose > 1)
  {
    printf("vbelib: Interrupt handler returns: %X\n",retval);
    printf("vbelib: registers after int %02X\n",int_no);
    __dump_regs(r);
  }    
  return retval;
}
#else
#define VBE_LRMI_int(int_no,regs) (VBE_LRMI_int(int_no,regs))
#endif

static unsigned hh_int_10_seg;
int vbeInit( void )
{
   unsigned short iopl_port;
   size_t i;
   if(!LRMI_init()) return VBE_VM86_FAIL;
   /*
    Allow read/write to ALL io ports
   */
   hh_int_10_seg = *(unsigned short *)PhysToVirtSO(0x0000,0x0042);
   /* Video BIOS should be at C000:0000 and above */
   hh_int_10_seg >>= 12;
   if(hh_int_10_seg < 0xC) return VBE_BROKEN_BIOS;
   ioperm(0, 1024, 1);
   iopl(3);
   memset(&vbe_pm_info,0,sizeof(struct VesaProtModeInterface));
   vbeGetProtModeInfo(&vbe_pm_info);
   i = 0;
   while((iopl_port=vbe_pm_info.iopl_ports[i++]) != 0xFFFF) ioperm(iopl_port,1,1);
   iopl(3);
   return VBE_OK;
}

int vbeDestroy( void ) { return VBE_OK; }

static int check_str(unsigned char *str)
{
  size_t i;
  int null_found = 0;
  for(i = 0;i < 256;i++) 
  {
    if(VERR(&str[i]))
    {
      if(!str[i]) { null_found = 1; break; }
    }
    else break;
  }
  return null_found;
}

static int check_wrd(unsigned short *str)
{
  size_t i;
  int ffff_found = 0;
  for(i = 0;i < 1024;i++) 
  {
    if(VERR(&str[i]))
    {
      if(str[i] == 0xffff) { ffff_found = 1; break; }
    }
    else break;
  }
  return ffff_found;
}

static void print_str(unsigned char *str)
{
  size_t i;
  fflush(stdout);
  printf("vbelib:    ");
  for(i = 0;i < 256;i++) { printf("%02X(%c) ",str[i],isprint(str[i])?str[i]:'.'); if(!str[i]) break; }
  printf("\n");
  fflush(stdout);
}

static void print_wrd(unsigned short *str)
{
  size_t i;
  fflush(stdout);
  printf("vbelib:    ");
  for(i = 0;i < 256;i++) { printf("%04X ",str[i]); if(str[i] == 0xffff) break; }
  printf("\n");
  fflush(stdout);
}

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
  if(!VBE_LRMI_int(0x10,&r))
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
    if(!check_str(data->OemStringPtr)) data->OemStringPtr = NULL;
#ifdef HAVE_VERBOSE_VAR
    if(verbose > 1)
    {
      printf("vbelib:  OemStringPtr=%04X:%04X => %p\n",fpdata.seg,fpdata.off,data->OemStringPtr);
      if(data->OemStringPtr) print_str(data->OemStringPtr);
    }
#endif
    fpdata.seg = (unsigned long)(data->VideoModePtr) >> 16;
    fpdata.off = (unsigned long)(data->VideoModePtr) & 0xffff;
    data->VideoModePtr = PhysToVirt(fpdata);
    if(!check_wrd(data->VideoModePtr))
    {
	data->VideoModePtr = NULL;
	retval = VBE_BROKEN_BIOS;
    }   
#ifdef HAVE_VERBOSE_VAR
    if(verbose > 1)
    {
      printf("vbelib:  VideoModePtr=%04X:%04X => %p\n",fpdata.seg,fpdata.off,data->VideoModePtr);
      if(data->VideoModePtr) print_wrd(data->VideoModePtr);
    }
#endif
    fpdata.seg = (unsigned long)(data->OemVendorNamePtr) >> 16;
    fpdata.off = (unsigned long)(data->OemVendorNamePtr) & 0xffff;
    data->OemVendorNamePtr = PhysToVirt(fpdata);
    if(!check_str(data->OemVendorNamePtr)) data->OemVendorNamePtr = NULL;
#ifdef HAVE_VERBOSE_VAR
    if(verbose > 1)
    {
      printf("vbelib:  OemVendorNamePtr=%04X:%04X => %p\n",fpdata.seg,fpdata.off,data->OemVendorNamePtr);
      if(data->OemVendorNamePtr) print_str(data->OemVendorNamePtr);
    }
#endif
    fpdata.seg = (unsigned long)(data->OemProductNamePtr) >> 16;
    fpdata.off = (unsigned long)(data->OemProductNamePtr) & 0xffff;
    data->OemProductNamePtr = PhysToVirt(fpdata);
    if(!check_str(data->OemProductNamePtr)) data->OemProductNamePtr = NULL;
#ifdef HAVE_VERBOSE_VAR
    if(verbose > 1)
    {
      printf("vbelib:  OemProductNamePtr=%04X:%04X => %p\n",fpdata.seg,fpdata.off,data->OemProductNamePtr);
      if(data->OemVendorNamePtr) print_str(data->OemProductNamePtr);
    }
#endif
    fpdata.seg = (unsigned long)(data->OemProductRevPtr) >> 16;
    fpdata.off = (unsigned long)(data->OemProductRevPtr) & 0xffff;
    data->OemProductRevPtr = PhysToVirt(fpdata);
    if(!check_str(data->OemProductRevPtr)) data->OemProductRevPtr = NULL;
#ifdef HAVE_VERBOSE_VAR
    if(verbose > 1)
    {
      printf("vbelib:  OemProductRevPtr=%04X:%04X => %p\n",fpdata.seg,fpdata.off,data->OemProductRevPtr);
      if(data->OemProductRevPtr) print_str(data->OemProductRevPtr);
    }
#endif
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
  if(!VBE_LRMI_int(0x10,&r))
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
  retval = VBE_LRMI_int(0x10,&r);
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
  if(!VBE_LRMI_int(0x10,&r)) return VBE_VM86_FAIL;
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
  if(!VBE_LRMI_int(0x10,&r)) return VBE_VM86_FAIL;
  retval = r.eax & 0xffff;
  if(retval != 0x4f) return retval;
  if(!(rm_space = LRMI_alloc_real((r.ebx & 0xffff)*64))) return VBE_OUT_OF_DOS_MEM;
  r.eax = 0x4f04;
  r.edx = 0x01;
  r.ecx = 0x0f;
  r.es  = VirtToPhysSeg(rm_space);
  r.ebx = VirtToPhysOff(rm_space);
  if(!VBE_LRMI_int(0x10,&r))
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
  memset(&r,0,sizeof(struct LRMI_regs));
  r.eax = 0x4f04;
  r.edx = 0x02;
  r.ecx = 0x0f;
  r.es  = VirtToPhysSeg(data);
  r.ebx = VirtToPhysOff(data);
  retval = VBE_LRMI_int(0x10,&r);
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
  if(!VBE_LRMI_int(0x10,&r)) return VBE_VM86_FAIL;
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
  if(vbe_pm_info.SetWindowCall)
  {
     /* Don't verbose this stuff from performance reasons */
     /* 32-bit function call is much better of int 10h */
     __asm __volatile(
	"pushl	%%ebx\n"
	"movl	%1, %%ebx\n"
	::"a"(0x4f05),"S"(win_num & 0x0f),"d"(win_gran):"memory");
    (*vbe_pm_info.SetWindowCall)();
    __asm __volatile("popl	%%ebx":::"memory");
    retval = VBE_OK;
  }
  else
  {
    struct LRMI_regs r;
    memset(&r,0,sizeof(struct LRMI_regs));
    r.eax = 0x4f05;
    r.ebx = win_num & 0x0f;
    r.edx = win_gran;
    if(!VBE_LRMI_int(0x10,&r)) return VBE_VM86_FAIL;
    retval = r.eax & 0xffff;
    if(retval == 0x4f) retval = VBE_OK;
  }
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
  unsigned info_offset;
  struct realVesaProtModeInterface *rm_info;
  memset(&r,0,sizeof(struct LRMI_regs));
  r.eax = 0x4f0a;
  r.ebx = 0;
  if(!VBE_LRMI_int(0x10,&r)) return VBE_VM86_FAIL;
  retval = r.eax & 0xffff;
  if(retval == 0x4f)
  {
    retval = VBE_OK;
    info_offset = r.edi&0xffff;
    if((r.es >> 12) != hh_int_10_seg) retval = VBE_BROKEN_BIOS;
    rm_info = PhysToVirtSO(r.es,info_offset);
    pm_info->SetWindowCall   = PhysToVirtSO(r.es,info_offset+rm_info->SetWindowCall);
#ifdef HAVE_VERBOSE_VAR
    if(verbose > 1) printf("vbelib:  SetWindowCall=%04X:%04X => %p\n",r.es,info_offset+rm_info->SetWindowCall,pm_info->SetWindowCall);
#endif
    pm_info->SetDisplayStart = PhysToVirtSO(r.es,info_offset+rm_info->SetDisplayStart);
#ifdef HAVE_VERBOSE_VAR
    if(verbose > 1) printf("vbelib:  SetDisplayStart=%04X:%04X => %p\n",r.es,info_offset+rm_info->SetDisplayStart,pm_info->SetDisplayStart);
#endif
    pm_info->SetPaletteData  = PhysToVirtSO(r.es,info_offset+rm_info->SetPaletteData);
#ifdef HAVE_VERBOSE_VAR
    if(verbose > 1) printf("vbelib:  SetPaletteData=%04X:%04X => %p\n",r.es,info_offset+rm_info->SetPaletteData,pm_info->SetPaletteData);
#endif
    pm_info->iopl_ports      = PhysToVirtSO(r.es,info_offset+rm_info->iopl_ports);
    if(!check_wrd(pm_info->iopl_ports))
    {
	pm_info->iopl_ports = NULL;
	retval = VBE_BROKEN_BIOS;
    }   
#ifdef HAVE_VERBOSE_VAR
    if(verbose > 1)
    {
      printf("vbelib:  iopl_ports=%04X:%04X => %p\n",r.es,info_offset+rm_info->iopl_ports,pm_info->iopl_ports);
      if(pm_info->iopl_ports) print_wrd(pm_info->iopl_ports);
    }
#endif
  }
  return retval;
}
/* --------- Standard VGA stuff -------------- */
int vbeWriteString(int x, int y, int attr, char *str)
{
  struct LRMI_regs r;
  void *rm_space = NULL;
  int retval;
  memset(&r,0,sizeof(struct LRMI_regs));
  r.ecx = strlen(str);
  r.edx = ((y<<8)&0xff00)|(x&0xff);
  r.ebx = attr;
  if(!(rm_space = LRMI_alloc_real(r.ecx))) return VBE_OUT_OF_DOS_MEM;
  r.es  = VirtToPhysSeg(rm_space);
  r.ebp = VirtToPhysOff(rm_space);
  memcpy(rm_space,str,r.ecx);
  r.eax = 0x1300;
  retval = VBE_LRMI_int(0x10,&r);
  if(rm_space) LRMI_free_real(rm_space);
  if(!retval) return VBE_VM86_FAIL;
  retval = r.eax & 0xffff;
  if(retval == 0x4f) retval = VBE_OK;
  return retval;
}
