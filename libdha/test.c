#include "libdha.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main( void )
{
  pciinfo_t lst[MAX_PCI_DEVICES];
  unsigned i,num_pci;
  int err;
  err = pci_scan(lst,&num_pci);
  if(err)
  {
    printf("Error occurred during pci scan: %s\n",strerror(err));
    return EXIT_FAILURE;
  }
  else
  {
    printf(" Bus:card:func vend:dev  command base0   :base1   :base2   :baserom\n");
    for(i=0;i<num_pci;i++)
      printf("%04X:%04X:%04X %04X:%04X %04X    %08X:%08X:%08X:%08X\n"
    	    ,lst[i].bus,lst[i].card,lst[i].func
	    ,lst[i].vendor,lst[i].device,lst[i].command
	    ,lst[i].base0,lst[i].base1,lst[i].base2,lst[i].baserom);
  }
  return EXIT_SUCCESS;
}
