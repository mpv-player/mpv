/* File generated automatically from mplayer_wine.spec; do not edit! */
/* This file can be copied, modified and distributed without restriction. */

extern char pe_header[];
asm(".section .text\n\t"
    ".align 4096\n"
    "pe_header:\t.fill 4096,1,0\n\t");
static const char dllname[] = "mplayer_wine";

extern int __wine_spec_exports[];

#define __stdcall __attribute__((__stdcall__))


static struct {
  struct {
    void        *OriginalFirstThunk;
    unsigned int TimeDateStamp;
    unsigned int ForwarderChain;
    const char  *Name;
    void        *FirstThunk;
  } imp[3];
  const char *data[8];
} imports = {
  {
    { 0, 0, 0, "kernel32.dll", &imports.data[0] },
    { 0, 0, 0, "ntdll.dll", &imports.data[5] },
    { 0, 0, 0, 0, 0 },
  },
  {
    /* kernel32.dll */
    "\0\0ExitProcess",
    "\0\0FreeLibrary",
    "\0\0GetProcAddress",
    "\0\0LoadLibraryA",
    0,
    /* ntdll.dll */
    "\0\0RtlRaiseException",
    "\0\0__wine_get_main_args",
    0,
  }
};

#ifndef __GNUC__
static void __asm__dummy_import(void) {
#endif

asm(".data\n\t.align 8\n"
    "\t.type ExitProcess,@function\n"
    "\t.globl ExitProcess\n"
    "ExitProcess:\n\tjmp *(imports+60)\n\tmovl %esi,%esi\n"
    "\t.type FreeLibrary,@function\n"
    "\t.globl FreeLibrary\n"
    "FreeLibrary:\n\tjmp *(imports+64)\n\tmovl %esi,%esi\n"
    "\t.type GetProcAddress,@function\n"
    "\t.globl GetProcAddress\n"
    "GetProcAddress:\n\tjmp *(imports+68)\n\tmovl %esi,%esi\n"
    "\t.type LoadLibraryA,@function\n"
    "\t.globl LoadLibraryA\n"
    "LoadLibraryA:\n\tjmp *(imports+72)\n\tmovl %esi,%esi\n"
    "\t.type RtlRaiseException,@function\n"
    "\t.globl RtlRaiseException\n"
    "RtlRaiseException:\n\tjmp *(imports+80)\n\tmovl %esi,%esi\n"
    "\t.type __wine_get_main_args,@function\n"
    "\t.globl __wine_get_main_args\n"
    "__wine_get_main_args:\n\tjmp *(imports+84)\n\tmovl %esi,%esi\n"
".previous");
#ifndef __GNUC__
}
#endif


int _ARGC;
char **_ARGV;
extern void __stdcall ExitProcess(int);
static void __wine_exe_main(void)
{
    extern int main( int argc, char *argv[] );
    extern int __wine_get_main_args( char ***argv );
    _ARGC = __wine_get_main_args( &_ARGV );
    ExitProcess( main( _ARGC, _ARGV ) );
}

static const struct image_nt_headers
{
  int Signature;
  struct file_header {
    short Machine;
    short NumberOfSections;
    int   TimeDateStamp;
    void *PointerToSymbolTable;
    int   NumberOfSymbols;
    short SizeOfOptionalHeader;
    short Characteristics;
  } FileHeader;
  struct opt_header {
    short Magic;
    char  MajorLinkerVersion, MinorLinkerVersion;
    int   SizeOfCode;
    int   SizeOfInitializedData;
    int   SizeOfUninitializedData;
    void *AddressOfEntryPoint;
    void *BaseOfCode;
    void *BaseOfData;
    void *ImageBase;
    int   SectionAlignment;
    int   FileAlignment;
    short MajorOperatingSystemVersion;
    short MinorOperatingSystemVersion;
    short MajorImageVersion;
    short MinorImageVersion;
    short MajorSubsystemVersion;
    short MinorSubsystemVersion;
    int   Win32VersionValue;
    int   SizeOfImage;
    int   SizeOfHeaders;
    int   CheckSum;
    short Subsystem;
    short DllCharacteristics;
    int   SizeOfStackReserve;
    int   SizeOfStackCommit;
    int   SizeOfHeapReserve;
    int   SizeOfHeapCommit;
    int   LoaderFlags;
    int   NumberOfRvaAndSizes;
    struct { const void *VirtualAddress; int Size; } DataDirectory[16];
  } OptionalHeader;
} nt_header = {
  0x4550,
  { 0x014c,
    0, 0, 0, 0,
    sizeof(nt_header.OptionalHeader),
    0x0000 },
  { 0x010b,
    0, 0,
    0, 0, 0,
    __wine_exe_main,
    0, 0,
    pe_header,
    4096,
    4096,
    1, 0,
    0, 0,
    4, 0,
    0,
    4096,
    4096,
    0,
    0x0003,
    0,
    0, 0,
    0, 0,
    0,
    16,
    {
      { 0, 0 },
      { &imports, sizeof(imports) },
      { 0, 0 },
    }
  }
};

#ifndef __GNUC__
static void __asm__dummy_dll_init(void) {
#endif /* defined(__GNUC__) */
asm("\t.section	.init ,\"ax\"\n"
    "\tcall __wine_spec_mplayer_wine_init\n"
    "\t.previous\n");
#ifndef __GNUC__
}
#endif /* defined(__GNUC__) */

void __wine_spec_mplayer_wine_init(void)
{
    extern void __wine_dll_register( const struct image_nt_headers *, const char * );
    extern void *__wine_dbg_register( char * const *, int );
    __wine_dll_register( &nt_header, "mplayer_wine.exe" );
}
