/*
    bios2dump.c - Was designed to extract BIOS of your PC and save it to file.
    Usage: as argument requires DOS interrupt number in hexadecimal form.
    as output - will write 64KB file which will named: SSSS_OOOO.intXX
    where: SSSS - segment of BIOS interrupt handler
           OOOO - offset of BIOS interrupt handler
	   XX   - interrupt number which was passed as argument
    Licence: GNU GPL v2
    Copyright: Nick Kurshev <nickols_k@mail.ru>
*/
#include <stdio.h>
#include <stdlib.h>

int main( int argc, char *argv[])
{
  FILE * fd_mem, *fd_out;
  unsigned short int_seg,int_off;
  unsigned long bios_off;
  int int_no;
  size_t i;
  char outname[80];
  unsigned char ch;
  if(argc < 2)
  {
    printf("Usage: %s int_no(in hex)\n",argv[0]);
    return EXIT_FAILURE;
  }
  int_no = strtol(argv[1],NULL,16);
  if(!(fd_mem = fopen("/dev/mem","rb")))
  {
    perror("Can't open file - /dev/mem");
    return EXIT_FAILURE;
  }
  fseek(fd_mem,int_no*4,SEEK_SET);
  fread(&int_off,sizeof(unsigned short),1,fd_mem);
  fread(&int_seg,sizeof(unsigned short),1,fd_mem);
  sprintf(outname,"%04X_%04X.int%02X",int_seg,int_off,int_no);
  if(!(fd_out = fopen(outname,"wb")))
  {
    perror("Can't open output file");
    fclose(fd_mem);
    return EXIT_FAILURE;
  }
  bios_off = (int_seg << 4) + int_off;
  bios_off &= 0xf0000;
  fseek(fd_mem,bios_off,SEEK_SET);
  for(i=0;i<0x10000;i++)
  {
    fread(&ch,1,1,fd_mem);
    fwrite(&ch,1,1,fd_out);
  }
  fclose(fd_out);
  fclose(fd_mem);
  return EXIT_SUCCESS;
}