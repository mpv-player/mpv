/*
    bios2dump.c - Was designed to dump memory block to file.
    Usage: as argument requires absolute address of memory dump and its lenght
    (int hexadecimal form).
    as output - will write file which will named: memADDR_LEN.dump
    where: ADDR - given address of memory
           LEN  - given length of memory
    Licence: GNU GPL v2
    Copyright: Nick Kurshev <nickols_k@mail.ru>
*/
#include <stdio.h>
#include <stdlib.h>

int main( int argc, char *argv[])
{
  FILE * fd_mem, *fd_out;
  unsigned long i,addr,len;
  int int_no;
  char outname[80];
  unsigned char ch;
  if(argc < 3)
  {
    printf("Usage: %s address length (in hex)\n",argv[0]);
    return EXIT_FAILURE;
  }
  addr = strtol(argv[1],NULL,16);
  len  = strtol(argv[2],NULL,16);
  if(!(fd_mem = fopen("/dev/mem","rb")))
  {
    perror("Can't open file - /dev/mem");
    return EXIT_FAILURE;
  }
  sprintf(outname,"mem%08X_%08X.dump",addr,len);
  if(!(fd_out = fopen(outname,"wb")))
  {
    perror("Can't open output file");
    fclose(fd_mem);
    return EXIT_FAILURE;
  }
  fseek(fd_mem,addr,SEEK_SET);
  for(i=0;i<len;i++)
  {
    fread(&ch,1,1,fd_mem);
    fwrite(&ch,1,1,fd_out);
  }
  fclose(fd_out);
  fclose(fd_mem);
  return EXIT_SUCCESS;
}