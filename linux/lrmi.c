/*
Linux Real Mode Interface - A library of DPMI-like functions for Linux.

Copyright (C) 1998 by Josh Vanderhoof

You are free to distribute and modify this file, as long as you
do not remove this copyright notice and clearly label modified
versions as being modified.

This software has NO WARRANTY.  Use it at your own risk.
Original location: http://cvs.debian.org/lrmi/
*/

#include <stdio.h>
#include <string.h>
#include <sys/io.h>
#include <asm/vm86.h>

#ifdef USE_LIBC_VM86
#include <sys/vm86.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

#include "lrmi.h"

#define REAL_MEM_BASE 	((void *)0x10000)
#define REAL_MEM_SIZE 	0x10000
#define REAL_MEM_BLOCKS 	0x100

struct mem_block
	{
	unsigned int size : 20;
	unsigned int free : 1;
	};

static struct
	{
	int ready;
	int count;
	struct mem_block blocks[REAL_MEM_BLOCKS];
	} mem_info = { 0 };

static int
real_mem_init(void)
	{
	void *m;
	int fd_zero;

	if (mem_info.ready)
		return 1;

	fd_zero = open("/dev/zero", O_RDONLY);
	if (fd_zero == -1)
		{
		perror("open /dev/zero");
		return 0;
		}

	m = mmap((void *)REAL_MEM_BASE, REAL_MEM_SIZE,
	 PROT_READ | PROT_WRITE | PROT_EXEC,
	 MAP_FIXED | MAP_PRIVATE, fd_zero, 0);

	if (m == (void *)-1)
		{
		perror("mmap /dev/zero");
		close(fd_zero);
		return 0;
		}

	mem_info.ready = 1;
	mem_info.count = 1;
	mem_info.blocks[0].size = REAL_MEM_SIZE;
	mem_info.blocks[0].free = 1;

	return 1;
	}


static void
insert_block(int i)
	{
	memmove(
	 mem_info.blocks + i + 1,
	 mem_info.blocks + i,
	 (mem_info.count - i) * sizeof(struct mem_block));

	mem_info.count++;
	}

static void
delete_block(int i)
	{
	mem_info.count--;

	memmove(
	 mem_info.blocks + i,
	 mem_info.blocks + i + 1,
	 (mem_info.count - i) * sizeof(struct mem_block));
	}

void *
LRMI_alloc_real(int size)
	{
	int i;
	char *r = (char *)REAL_MEM_BASE;

	if (!mem_info.ready)
		return NULL;

	if (mem_info.count == REAL_MEM_BLOCKS)
		return NULL;

	size = (size + 15) & ~15;

	for (i = 0; i < mem_info.count; i++)
		{
		if (mem_info.blocks[i].free && size < mem_info.blocks[i].size)
			{
			insert_block(i);

			mem_info.blocks[i].size = size;
			mem_info.blocks[i].free = 0;
			mem_info.blocks[i + 1].size -= size;

			return (void *)r;
			}

		r += mem_info.blocks[i].size;
		}

	return NULL;
	}


void
LRMI_free_real(void *m)
	{
	int i;
	char *r = (char *)REAL_MEM_BASE;

	if (!mem_info.ready)
		return;

	i = 0;
	while (m != (void *)r)
		{
		r += mem_info.blocks[i].size;
		i++;
		if (i == mem_info.count)
			return;
		}

	mem_info.blocks[i].free = 1;

	if (i + 1 < mem_info.count && mem_info.blocks[i + 1].free)
		{
		mem_info.blocks[i].size += mem_info.blocks[i + 1].size;
		delete_block(i + 1);
		}

	if (i - 1 >= 0 && mem_info.blocks[i - 1].free)
		{
		mem_info.blocks[i - 1].size += mem_info.blocks[i].size;
		delete_block(i);
		}
	}


#define DEFAULT_VM86_FLAGS 	(IF_MASK | IOPL_MASK)
#define DEFAULT_STACK_SIZE 	0x1000
#define RETURN_TO_32_INT 	255

static struct
	{
	int ready;
	unsigned short ret_seg, ret_off;
	unsigned short stack_seg, stack_off;
	struct vm86_struct vm;
	} context = { 0 };


static inline void
set_bit(unsigned int bit, void *array)
	{
	unsigned char *a = array;

	a[bit / 8] |= (1 << (bit % 8));
	}


static inline unsigned int
get_int_seg(int i)
	{
	return *(unsigned short *)(i * 4 + 2);
	}


static inline unsigned int
get_int_off(int i)
	{
	return *(unsigned short *)(i * 4);
	}


static inline void
pushw(unsigned short i)
	{
	struct vm86_regs *r = &context.vm.regs;
	r->esp -= 2;
	*(unsigned short *)(((unsigned int)r->ss << 4) + r->esp) = i;
	}


int
LRMI_init(void)
	{
	void *m;
	int fd_mem;

	if (context.ready)
		return 1;

	if (!real_mem_init())
		return 0;

	/*
	 Map the Interrupt Vectors (0x0 - 0x400) + BIOS data (0x400 - 0x502)
	 and the ROM (0xa0000 - 0x100000)
	*/
	fd_mem = open("/dev/mem", O_RDWR);

	if (fd_mem == -1)
		{
		perror("open /dev/mem");
		return 0;
		}

	m = mmap((void *)0, 0x502,
	 PROT_READ | PROT_WRITE | PROT_EXEC,
	 MAP_FIXED | MAP_PRIVATE, fd_mem, 0);

	if (m == (void *)-1)
		{
		perror("mmap /dev/mem");
		return 0;
		}

	m = mmap((void *)0xa0000, 0x100000 - 0xa0000,
	 PROT_READ | PROT_WRITE,
	 MAP_FIXED | MAP_SHARED, fd_mem, 0xa0000);

	if (m == (void *)-1)
		{
		perror("mmap /dev/mem");
		return 0;
		}


	/*
	 Allocate a stack
	*/
	m = LRMI_alloc_real(DEFAULT_STACK_SIZE);

	context.stack_seg = (unsigned int)m >> 4;
	context.stack_off = DEFAULT_STACK_SIZE;

	/*
	 Allocate the return to 32 bit routine
	*/
	m = LRMI_alloc_real(2);

	context.ret_seg = (unsigned int)m >> 4;
	context.ret_off = (unsigned int)m & 0xf;

	((unsigned char *)m)[0] = 0xcd; 	/* int opcode */
	((unsigned char *)m)[1] = RETURN_TO_32_INT;

	memset(&context.vm, 0, sizeof(context.vm));

	/*
	 Enable kernel emulation of all ints except RETURN_TO_32_INT
	*/
	memset(&context.vm.int_revectored, 0, sizeof(context.vm.int_revectored));
	set_bit(RETURN_TO_32_INT, &context.vm.int_revectored);

	context.ready = 1;

	return 1;
	}


static void
set_regs(struct LRMI_regs *r)
	{
	context.vm.regs.edi = r->edi;
	context.vm.regs.esi = r->esi;
	context.vm.regs.ebp = r->ebp;
	context.vm.regs.ebx = r->ebx;
	context.vm.regs.edx = r->edx;
	context.vm.regs.ecx = r->ecx;
	context.vm.regs.eax = r->eax;
	context.vm.regs.eflags = DEFAULT_VM86_FLAGS;
	context.vm.regs.es = r->es;
	context.vm.regs.ds = r->ds;
	context.vm.regs.fs = r->fs;
	context.vm.regs.gs = r->gs;
	}


static void
get_regs(struct LRMI_regs *r)
	{
	r->edi = context.vm.regs.edi;
	r->esi = context.vm.regs.esi;
	r->ebp = context.vm.regs.ebp;
	r->ebx = context.vm.regs.ebx;
	r->edx = context.vm.regs.edx;
	r->ecx = context.vm.regs.ecx;
	r->eax = context.vm.regs.eax;
	r->flags = context.vm.regs.eflags;
	r->es = context.vm.regs.es;
	r->ds = context.vm.regs.ds;
	r->fs = context.vm.regs.fs;
	r->gs = context.vm.regs.gs;
	}

#define DIRECTION_FLAG 	(1 << 10)

static void
em_ins(int size)
	{
	unsigned int edx, edi;

	edx = context.vm.regs.edx & 0xffff;
	edi = context.vm.regs.edi & 0xffff;
	edi += (unsigned int)context.vm.regs.ds << 4;

	if (context.vm.regs.eflags & DIRECTION_FLAG)
		{
		if (size == 4)
			asm volatile ("std; insl; cld"
			 : "=D" (edi) : "d" (edx), "0" (edi));
		else if (size == 2)
			asm volatile ("std; insw; cld"
			 : "=D" (edi) : "d" (edx), "0" (edi));
		else
			asm volatile ("std; insb; cld"
			 : "=D" (edi) : "d" (edx), "0" (edi));
		}
	else
		{
		if (size == 4)
			asm volatile ("cld; insl"
			 : "=D" (edi) : "d" (edx), "0" (edi));
		else if (size == 2)
			asm volatile ("cld; insw"
			 : "=D" (edi) : "d" (edx), "0" (edi));
		else
			asm volatile ("cld; insb"
			 : "=D" (edi) : "d" (edx), "0" (edi));
		}

	edi -= (unsigned int)context.vm.regs.ds << 4;

	context.vm.regs.edi &= 0xffff0000;
	context.vm.regs.edi |= edi & 0xffff;
	}

static void
em_rep_ins(int size)
	{
	unsigned int ecx, edx, edi;

	ecx = context.vm.regs.ecx & 0xffff;
	edx = context.vm.regs.edx & 0xffff;
	edi = context.vm.regs.edi & 0xffff;
	edi += (unsigned int)context.vm.regs.ds << 4;

	if (context.vm.regs.eflags & DIRECTION_FLAG)
		{
		if (size == 4)
			asm volatile ("std; rep; insl; cld"
			 : "=D" (edi), "=c" (ecx)
			 : "d" (edx), "0" (edi), "1" (ecx));
		else if (size == 2)
			asm volatile ("std; rep; insw; cld"
			 : "=D" (edi), "=c" (ecx)
			 : "d" (edx), "0" (edi), "1" (ecx));
		else
			asm volatile ("std; rep; insb; cld"
			 : "=D" (edi), "=c" (ecx)
			 : "d" (edx), "0" (edi), "1" (ecx));
		}
	else
		{
		if (size == 4)
			asm volatile ("cld; rep; insl"
			 : "=D" (edi), "=c" (ecx)
			 : "d" (edx), "0" (edi), "1" (ecx));
		else if (size == 2)
			asm volatile ("cld; rep; insw"
			 : "=D" (edi), "=c" (ecx)
			 : "d" (edx), "0" (edi), "1" (ecx));
		else
			asm volatile ("cld; rep; insb"
			 : "=D" (edi), "=c" (ecx)
			 : "d" (edx), "0" (edi), "1" (ecx));
		}

	edi -= (unsigned int)context.vm.regs.ds << 4;

	context.vm.regs.edi &= 0xffff0000;
	context.vm.regs.edi |= edi & 0xffff;

	context.vm.regs.ecx &= 0xffff0000;
	context.vm.regs.ecx |= ecx & 0xffff;
	}

static void
em_outs(int size)
	{
	unsigned int edx, esi;

	edx = context.vm.regs.edx & 0xffff;
	esi = context.vm.regs.esi & 0xffff;
	esi += (unsigned int)context.vm.regs.ds << 4;

	if (context.vm.regs.eflags & DIRECTION_FLAG)
		{
		if (size == 4)
			asm volatile ("std; outsl; cld"
			 : "=S" (esi) : "d" (edx), "0" (esi));
		else if (size == 2)
			asm volatile ("std; outsw; cld"
			 : "=S" (esi) : "d" (edx), "0" (esi));
		else
			asm volatile ("std; outsb; cld"
			 : "=S" (esi) : "d" (edx), "0" (esi));
		}
	else
		{
		if (size == 4)
			asm volatile ("cld; outsl"
			 : "=S" (esi) : "d" (edx), "0" (esi));
		else if (size == 2)
			asm volatile ("cld; outsw"
			 : "=S" (esi) : "d" (edx), "0" (esi));
		else
			asm volatile ("cld; outsb"
			 : "=S" (esi) : "d" (edx), "0" (esi));
		}

	esi -= (unsigned int)context.vm.regs.ds << 4;

	context.vm.regs.esi &= 0xffff0000;
	context.vm.regs.esi |= esi & 0xffff;
	}

static void
em_rep_outs(int size)
	{
	unsigned int ecx, edx, esi;

	ecx = context.vm.regs.ecx & 0xffff;
	edx = context.vm.regs.edx & 0xffff;
	esi = context.vm.regs.esi & 0xffff;
	esi += (unsigned int)context.vm.regs.ds << 4;

	if (context.vm.regs.eflags & DIRECTION_FLAG)
		{
		if (size == 4)
			asm volatile ("std; rep; outsl; cld"
			 : "=S" (esi), "=c" (ecx)
			 : "d" (edx), "0" (esi), "1" (ecx));
		else if (size == 2)
			asm volatile ("std; rep; outsw; cld"
			 : "=S" (esi), "=c" (ecx)
			 : "d" (edx), "0" (esi), "1" (ecx));
		else
			asm volatile ("std; rep; outsb; cld"
			 : "=S" (esi), "=c" (ecx)
			 : "d" (edx), "0" (esi), "1" (ecx));
		}
	else
		{
		if (size == 4)
			asm volatile ("cld; rep; outsl"
			 : "=S" (esi), "=c" (ecx)
			 : "d" (edx), "0" (esi), "1" (ecx));
		else if (size == 2)
			asm volatile ("cld; rep; outsw"
			 : "=S" (esi), "=c" (ecx)
			 : "d" (edx), "0" (esi), "1" (ecx));
		else
			asm volatile ("cld; rep; outsb"
			 : "=S" (esi), "=c" (ecx)
			 : "d" (edx), "0" (esi), "1" (ecx));
		}

	esi -= (unsigned int)context.vm.regs.ds << 4;

	context.vm.regs.esi &= 0xffff0000;
	context.vm.regs.esi |= esi & 0xffff;

	context.vm.regs.ecx &= 0xffff0000;
	context.vm.regs.ecx |= ecx & 0xffff;
	}

static void
em_inbl(unsigned char literal)
	{
	context.vm.regs.eax = inb(literal) & 0xff;
	}

static void
em_inb(void)
	{
	asm volatile ("inb (%w1), %b0"
	 : "=a" (context.vm.regs.eax)
	 : "d" (context.vm.regs.edx), "0" (context.vm.regs.eax));
	}

static void
em_inw(void)
	{
	asm volatile ("inw (%w1), %w0"
	 : "=a" (context.vm.regs.eax)
	 : "d" (context.vm.regs.edx), "0" (context.vm.regs.eax));
	}

static void
em_inl(void)
	{
	asm volatile ("inl (%w1), %0"
	 : "=a" (context.vm.regs.eax)
	 : "d" (context.vm.regs.edx));
	}

static void
em_outbl(unsigned char literal)
	{
	outb(context.vm.regs.eax & 0xff, literal);
	}

static void
em_outb(void)
	{
	asm volatile ("outb %b0, (%w1)"
	 : : "a" (context.vm.regs.eax),
	 "d" (context.vm.regs.edx));
	}

static void
em_outw(void)
	{
	asm volatile ("outw %w0, (%w1)"
	 : : "a" (context.vm.regs.eax),
	 "d" (context.vm.regs.edx));
	}

static void
em_outl(void)
	{
	asm volatile ("outl %0, (%w1)"
	 : : "a" (context.vm.regs.eax),
	 "d" (context.vm.regs.edx));
	}

static int
emulate(void)
	{
	unsigned char *insn;
	struct
		{
		unsigned int size : 1;
		unsigned int rep : 1;
		} prefix = { 0, 0 };
	int i = 0;

	insn = (unsigned char *)((unsigned int)context.vm.regs.cs << 4);
	insn += context.vm.regs.eip;

	while (1)
		{
		if (insn[i] == 0x66)
			{
			prefix.size = 1 - prefix.size;
			i++;
			}
		else if (insn[i] == 0xf3)
			{
			prefix.rep = 1;
			i++;
			}
		else if (insn[i] == 0xf0 || insn[i] == 0xf2
		 || insn[i] == 0x26 || insn[i] == 0x2e
		 || insn[i] == 0x36 || insn[i] == 0x3e
		 || insn[i] == 0x64 || insn[i] == 0x65
		 || insn[i] == 0x67)
			{
			/* these prefixes are just ignored */
			i++;
			}
		else if (insn[i] == 0x6c)
			{
			if (prefix.rep)
				em_rep_ins(1);
			else
				em_ins(1);
			i++;
			break;
			}
		else if (insn[i] == 0x6d)
			{
			if (prefix.rep)
				{
				if (prefix.size)
					em_rep_ins(4);
				else
					em_rep_ins(2);
				}
			else
				{
				if (prefix.size)
					em_ins(4);
				else
					em_ins(2);
				}
			i++;
			break;
			}
		else if (insn[i] == 0x6e)
			{
			if (prefix.rep)
				em_rep_outs(1);
			else
				em_outs(1);
			i++;
			break;
			}
		else if (insn[i] == 0x6f)
			{
			if (prefix.rep)
				{
				if (prefix.size)
					em_rep_outs(4);
				else
					em_rep_outs(2);
				}
			else
				{
				if (prefix.size)
					em_outs(4);
				else
					em_outs(2);
				}
			i++;
			break;
			}
		else if (insn[i] == 0xe4)
			{
			em_inbl(insn[i + 1]);
			i += 2;
			break;
			}
		else if (insn[i] == 0xe6)
			{
			em_outbl(insn[i + 1]);
			i += 2;
			break;
			}
		else if (insn[i] == 0xec)
			{
			em_inb();
			i++;
			break;
			}
		else if (insn[i] == 0xed)
			{
			if (prefix.size)
				em_inl();
			else
				em_inw();
			i++;
			break;
			}
		else if (insn[i] == 0xee)
			{
			em_outb();
			i++;
			break;
			}
		else if (insn[i] == 0xef)
			{
			if (prefix.size)
				em_outl();
			else
				em_outw();

			i++;
			break;
			}
		else
			return 0;
		}

	context.vm.regs.eip += i;
	return 1;
	}


/*
 I don't know how to make sure I get the right vm86() from libc.
 The one I want is syscall # 113 (vm86old() in libc 5, vm86() in glibc)
 which should be declared as "int vm86(struct vm86_struct *);" in
 <sys/vm86.h>.

 This just does syscall 113 with inline asm, which should work
 for both libc's (I hope).
*/
#if !defined(USE_LIBC_VM86)
static int
lrmi_vm86(struct vm86_struct *vm)
	{
	int r;
#ifdef __PIC__
	asm volatile (
	 "pushl %%ebx\n\t"
	 "movl %2, %%ebx\n\t"
	 "int $0x80\n\t"
	 "popl %%ebx"
	 : "=a" (r)
	 : "0" (113), "r" (vm));
#else
	asm volatile (
	 "int $0x80"
	 : "=a" (r)
	 : "0" (113), "b" (vm));
#endif
	return r;
	}
#else
#define lrmi_vm86 vm86
#endif


static void
debug_info(int vret)
	{
	int i;
	unsigned char *p;

	fputs("vm86() failed\n", stderr);
	fprintf(stderr, "return = 0x%x\n", vret);
	fprintf(stderr, "eax = 0x%08lx\n", context.vm.regs.eax);
	fprintf(stderr, "ebx = 0x%08lx\n", context.vm.regs.ebx);
	fprintf(stderr, "ecx = 0x%08lx\n", context.vm.regs.ecx);
	fprintf(stderr, "edx = 0x%08lx\n", context.vm.regs.edx);
	fprintf(stderr, "esi = 0x%08lx\n", context.vm.regs.esi);
	fprintf(stderr, "edi = 0x%08lx\n", context.vm.regs.edi);
	fprintf(stderr, "ebp = 0x%08lx\n", context.vm.regs.ebp);
	fprintf(stderr, "eip = 0x%08lx\n", context.vm.regs.eip);
	fprintf(stderr, "cs  = 0x%04x\n", context.vm.regs.cs);
	fprintf(stderr, "esp = 0x%08lx\n", context.vm.regs.esp);
	fprintf(stderr, "ss  = 0x%04x\n", context.vm.regs.ss);
	fprintf(stderr, "ds  = 0x%04x\n", context.vm.regs.ds);
	fprintf(stderr, "es  = 0x%04x\n", context.vm.regs.es);
	fprintf(stderr, "fs  = 0x%04x\n", context.vm.regs.fs);
	fprintf(stderr, "gs  = 0x%04x\n", context.vm.regs.gs);
	fprintf(stderr, "eflags  = 0x%08lx\n", context.vm.regs.eflags);

	fputs("cs:ip = [ ", stderr);

	p = (unsigned char *)((context.vm.regs.cs << 4) + (context.vm.regs.eip & 0xffff));

	for (i = 0; i < 16; ++i)
		fprintf(stderr, "%02x ", (unsigned int)p[i]);

	fputs("]\n", stderr);
	}


static int
run_vm86(void)
	{
	unsigned int vret;

	while (1)
		{
		vret = lrmi_vm86(&context.vm);

		if (VM86_TYPE(vret) == VM86_INTx)
			{
			unsigned int v = VM86_ARG(vret);

			if (v == RETURN_TO_32_INT)
				return 1;

			pushw(context.vm.regs.eflags);
			pushw(context.vm.regs.cs);
			pushw(context.vm.regs.eip);

			context.vm.regs.cs = get_int_seg(v);
			context.vm.regs.eip = get_int_off(v);
			context.vm.regs.eflags &= ~(VIF_MASK | TF_MASK);

			continue;
			}

		if (VM86_TYPE(vret) != VM86_UNKNOWN)
			break;

		if (!emulate())
			break;
		}

#ifdef ORIGINAL_LRMI_CODE_THAT_GOT_IFDEFED_OUT
	debug_info(vret);
#endif
	return 0;
	}


int
LRMI_call(struct LRMI_regs *r)
	{
	unsigned int vret;

	memset(&context.vm.regs, 0, sizeof(context.vm.regs));

	set_regs(r);

	context.vm.regs.cs = r->cs;
	context.vm.regs.eip = r->ip;

	if (r->ss == 0 && r->sp == 0)
		{
		context.vm.regs.ss = context.stack_seg;
		context.vm.regs.esp = context.stack_off;
		}
	else
		{
		context.vm.regs.ss = r->ss;
		context.vm.regs.esp = r->sp;
		}

	pushw(context.ret_seg);
	pushw(context.ret_off);

	vret = run_vm86();

	get_regs(r);

	return vret;
	}


int
LRMI_int(int i, struct LRMI_regs *r)
	{
	unsigned int vret;
	unsigned int seg, off;

	seg = get_int_seg(i);
	off = get_int_off(i);

	/*
	 If the interrupt is in regular memory, it's probably
	 still pointing at a dos TSR (which is now gone).
	*/
	if (seg < 0xa000 || (seg << 4) + off >= 0x100000)
		{
#ifdef ORIGINAL_LRMI_CODE_THAT_GOT_IFDEFED_OUT
		fprintf(stderr, "Int 0x%x is not in rom (%04x:%04x)\n", i, seg, off);
#endif
		return 0;
		}

	memset(&context.vm.regs, 0, sizeof(context.vm.regs));

	set_regs(r);

	context.vm.regs.cs = seg;
	context.vm.regs.eip = off;

	if (r->ss == 0 && r->sp == 0)
		{
		context.vm.regs.ss = context.stack_seg;
		context.vm.regs.esp = context.stack_off;
		}
	else
		{
		context.vm.regs.ss = r->ss;
		context.vm.regs.esp = r->sp;
		}

	pushw(DEFAULT_VM86_FLAGS);
	pushw(context.ret_seg);
	pushw(context.ret_off);

	vret = run_vm86();

	get_regs(r);

	return vret;
	}

