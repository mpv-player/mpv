# This code was taken from http://www.mpg123.org
# See ChangeLog of mpg123-0.59s-pre.1 for detail
# Applied to mplayer by Nick Kurshev <nickols_k@mail.ru>
.bss
	.align 8
    	.comm	decwin,2176,32
	.align 8
	.comm	decwins,2176,32
.data
	.align 8
intwinbase_MMX:
	.value      0,    -1,    -1,    -1,    -1,    -1,    -1,    -2
	.value     -2,    -2,    -2,    -3,    -3,    -4,    -4,    -5
	.value     -5,    -6,    -7,    -7,    -8,    -9,   -10,   -11
	.value    -13,   -14,   -16,   -17,   -19,   -21,   -24,   -26
	.value    -29,   -31,   -35,   -38,   -41,   -45,   -49,   -53
	.value    -58,   -63,   -68,   -73,   -79,   -85,   -91,   -97
	.value   -104,  -111,  -117,  -125,  -132,  -139,  -147,  -154
	.value   -161,  -169,  -176,  -183,  -190,  -196,  -202,  -208
	.value   -213,  -218,  -222,  -225,  -227,  -228,  -228,  -227
	.value   -224,  -221,  -215,  -208,  -200,  -189,  -177,  -163
	.value   -146,  -127,  -106,   -83,   -57,   -29,     2,    36
	.value     72,   111,   153,   197,   244,   294,   347,   401
	.value    459,   519,   581,   645,   711,   779,   848,   919
	.value    991,  1064,  1137,  1210,  1283,  1356,  1428,  1498
	.value   1567,  1634,  1698,  1759,  1817,  1870,  1919,  1962
	.value   2001,  2032,  2057,  2075,  2085,  2087,  2080,  2063
	.value   2037,  2000,  1952,  1893,  1822,  1739,  1644,  1535
	.value   1414,  1280,  1131,   970,   794,   605,   402,   185
	.value    -45,  -288,  -545,  -814, -1095, -1388, -1692, -2006
	.value  -2330, -2663, -3004, -3351, -3705, -4063, -4425, -4788
	.value  -5153, -5517, -5879, -6237, -6589, -6935, -7271, -7597
	.value  -7910, -8209, -8491, -8755, -8998, -9219, -9416, -9585
	.value  -9727, -9838, -9916, -9959, -9966, -9935, -9863, -9750
	.value  -9592, -9389, -9139, -8840, -8492, -8092, -7640, -7134
	.value  -6574, -5959, -5288, -4561, -3776, -2935, -2037, -1082
	.value    -70,   998,  2122,  3300,  4533,  5818,  7154,  8540
	.value   9975, 11455, 12980, 14548, 16155, 17799, 19478, 21189
	.value  22929, 24694, 26482, 28289, 30112, 31947,-26209,-24360
	.value -22511,-20664,-18824,-16994,-15179,-13383,-11610, -9863
	.value  -8147, -6466, -4822, -3222, -1667,  -162,  1289,  2684
	.value   4019,  5290,  6494,  7629,  8692,  9679, 10590, 11420
	.value  12169, 12835, 13415, 13908, 14313, 14630, 14856, 14992
	.value  15038

intwindiv:
	.long 0x47800000			# 65536.0
.text
	.align 32
.globl make_decode_tables_MMX
make_decode_tables_MMX:
	pushl %edi
	pushl %esi
	pushl %ebx

	xorl %ecx,%ecx
	xorl %ebx,%ebx
	movl $32,%esi
	movl $intwinbase_MMX,%edi
	negl 16(%esp)				# scaleval
	pushl $2				# intwinbase step
.L00:
	cmpl $528,%ecx
	jnc .L02
	movswl (%edi),%eax
	cmpl $intwinbase_MMX+444,%edi
	jc .L01
	addl $60000,%eax
.L01:
	pushl %eax
	fildl (%esp)
	fdivs intwindiv
	fimull 24(%esp)
	popl %eax
	fsts  decwin(,%ecx,4)
	fstps decwin+64(,%ecx,4)
.L02:
	leal -1(%esi),%edx
	and %ebx,%edx
	cmp $31,%edx
	jnz .L03
	addl $-1023,%ecx
	test %esi,%ebx
	jz  .L03
	negl 20(%esp)
.L03:
	addl %esi,%ecx
	addl (%esp),%edi
	incl %ebx
	cmpl $intwinbase_MMX,%edi
	jz .L04
	cmp $256,%ebx
	jnz .L00
	negl (%esp)
	jmp .L00
.L04:
	popl %eax

	xorl %ecx,%ecx
	xorl %ebx,%ebx
	pushl $2
.L05:
	cmpl $528,%ecx
	jnc .L11
	movswl (%edi),%eax
	cmpl $intwinbase_MMX+444,%edi
	jc .L06
	addl $60000,%eax
.L06:
	cltd
	imull 20(%esp)
	shrdl $17,%edx,%eax
	cmpl $32767,%eax
	movl $1055,%edx
	jle .L07
	movl $32767,%eax
	jmp .L08
.L07:
	cmpl $-32767,%eax
	jge .L08
	movl $-32767,%eax
.L08:
	cmpl $512,%ecx
	jnc .L09
	subl %ecx,%edx
	movw %ax,decwins(,%edx,2)
	movw %ax,decwins-32(,%edx,2)
.L09:
	testl $1,%ecx
	jnz .L10
	negl %eax
.L10:
	movw %ax,decwins(,%ecx,2)
	movw %ax,decwins+32(,%ecx,2)
.L11:
	leal -1(%esi),%edx
	and %ebx,%edx
	cmp $31,%edx
	jnz .L12
	addl $-1023,%ecx
	test %esi,%ebx
	jz  .L12
	negl 20(%esp)
.L12:
	addl %esi,%ecx
	addl (%esp),%edi
	incl %ebx
	cmpl $intwinbase_MMX,%edi
	jz .L13
	cmp $256,%ebx
	jnz .L05
	negl (%esp)
	jmp .L05
.L13:
	popl %eax
	
	popl %ebx
	popl %esi
	popl %edi
	ret

