// Original by Strepto/Astral
// ported to gcc & bugfixed : A'rpi

#include <inttypes.h>
//#include "attributes.h"
#include "mmx.h"

void rgb15to16_mmx(char* s0,char* d0,int count){
  static uint64_t mask_b  = 0x001F001F001F001FLL; // 00000000 00011111  xxB
  static uint64_t mask_rg = 0x7FE07FE07FE07FE0LL; // 01111111 11100000  RGx
  register char* s=s0+count;
  register char* d=d0+count;
  register int offs=-count;
  movq_m2r (mask_b,  mm4);
  movq_m2r (mask_rg, mm5);
  while(offs<0){
    movq_m2r (*(s+offs), mm0);
    movq_r2r (mm0, mm1);

    movq_m2r (*(s+8+offs), mm2);
    movq_r2r (mm2, mm3);
    
    pand_r2r (mm4, mm0);
    pand_r2r (mm5, mm1);
    
    psllq_i2r(1,mm1);
    pand_r2r (mm4, mm2);

    pand_r2r (mm5, mm3);
    por_r2r  (mm1, mm0);

    psllq_i2r(1,mm3);
    movq_r2m (mm0,*(d+offs));

    por_r2r  (mm3,mm2);
    movq_r2m (mm2,*(d+8+offs));

    offs+=16;
  }
  emms();
}

