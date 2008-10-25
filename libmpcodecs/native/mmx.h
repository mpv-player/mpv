#ifndef MPLAYER_MMX_H
#define MPLAYER_MMX_H

typedef union {
    long long               q;      /* Quadword (64-bit) value */
    unsigned long long      uq;     /* Unsigned Quadword */
    int                     d[2];   /* 2 Doubleword (32-bit) values */
    unsigned int            ud[2];  /* 2 Unsigned Doubleword */
    short                   w[4];   /* 4 Word (16-bit) values */
    unsigned short          uw[4];  /* 4 Unsigned Word */
    char                    b[8];   /* 8 Byte (8-bit) values */
    unsigned char           ub[8];  /* 8 Unsigned Byte */
    float                   s[2];   /* Single-precision (32-bit) value */
} mmx_t;        /* On an 8-byte (64-bit) boundary */


#define movq_m2r(var, reg)        mmx_m2r(movq, var, reg)
#define movq_r2m(reg, var)        mmx_r2m(movq, reg, var)
#define movq_r2r(regs, regd)      mmx_r2r(movq, regs, regd)

#define punpcklwd_m2r(var, reg)   mmx_m2r(punpcklwd, var, reg)
#define punpcklwd_r2r(regs, regd) mmx_r2r(punpcklwd, regs, regd)

#define punpckhwd_m2r(var, reg)   mmx_m2r(punpckhwd, var, reg)
#define punpckhwd_r2r(regs, regd) mmx_r2r(punpckhwd, regs, regd)

#define punpcklbw_r2r(regs, regd) mmx_r2r(punpcklbw, regs, regd)
#define punpckhbw_r2r(regs, regd) mmx_r2r(punpckhbw, regs, regd)
#define punpckhdq_r2r(regs, regd) mmx_r2r(punpckhdq, regs, regd)
#define punpckldq_r2r(regs, regd) mmx_r2r(punpckldq, regs, regd)

#define psubw_m2r(var, reg)       mmx_m2r(psubw, var, reg)
#define psubw_r2r(regs, regd)     mmx_r2r(psubw, regs, regd)
#define psubsw_r2r(regs, regd)    mmx_r2r(psubsw, regs, regd)

#define pmaddwd_r2r(regs, regd)   mmx_r2r(pmaddwd, regs, regd)
#define paddw_m2r(var, reg)       mmx_m2r(paddw, var, reg)
#define paddw_r2r(regs, regd)     mmx_r2r(paddw, regs, regd)

#define psrad_i2r(imm, reg)       mmx_i2r(psrad, imm, reg)

#define psllw_i2r(imm, reg)       mmx_i2r(psllw, imm, reg)

#define pmulhw_r2r(regs, regd)    mmx_r2r(pmulhw, regs, regd)
#define pmulhw_m2r(var, reg)      mmx_m2r(pmulhw, var, reg)

#define psraw_i2r(imm, reg)       mmx_i2r(psraw, imm, reg)

#define packssdw_r2r(regs, regd)  mmx_r2r(packssdw, regs, regd)
#define packuswb_r2r(regs, regd)  mmx_r2r(packuswb, regs, regd)

#define pxor_r2r(regs, regd)      mmx_r2r(pxor, regs, regd)

#define pcmpgtw_r2r(regs, regd)   mmx_r2r(pcmpgtw, regs, regd)

#define por_r2r(regs, regd)       mmx_r2r(por, regs, regd)


#define mmx_i2r(op,imm,reg) \
        __asm__ volatile (#op " %0, %%" #reg \
                              : /* nothing */ \
                              : "i" (imm) )

#define mmx_m2r(op, mem, reg) \
        __asm__ volatile (#op " %0, %%" #reg \
                              : /* nothing */ \
                              : "m" (mem))

#define mmx_r2m(op, reg, mem) \
        __asm__ volatile (#op " %%" #reg ", %0" \
                              : "=m" (mem) \
                              : /* nothing */ )

#define mmx_r2r(op, regs, regd) \
        __asm__ volatile (#op " %" #regs ", %" #regd)


#define emms() __asm__ volatile ("emms")

#endif /* MPLAYER_MMX_H */
