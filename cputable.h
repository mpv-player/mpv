/* cputable.h - Maps CPUID to real CPU name.
 * Copyleft 2001 by Felix Buenemann <atmosfear at users dot sourceforge dot net>
 * This file comes under the GNU GPL, see www.fsf.org for more info!
 * Family F codenames owe much thanks to Neil Phillips, author of x86test.
 */

#define MAX_VENDORS 8 /* Number of CPU Vendors */

//#define N_UNKNOWN "unknown"
//#define N_UNKNOWNEXT "unknown extended model"
#define N_UNKNOWN "" 
#define N_UNKNOWNEXT ""

#define F_UNKNOWN { \
N_UNKNOWN, \
N_UNKNOWN, \
N_UNKNOWN, \
N_UNKNOWN, \
N_UNKNOWN, \
N_UNKNOWN, \
N_UNKNOWN, \
N_UNKNOWN, \
N_UNKNOWN, \
N_UNKNOWN, \
N_UNKNOWN, \
N_UNKNOWN, \
N_UNKNOWN, \
N_UNKNOWN, \
N_UNKNOWN, \
N_UNKNOWN \
}

static const char *cpuname
		/* Vendor */ [MAX_VENDORS]
			/* Family */ [16]
				/* Model  */ [16]
	={
		/* Intel Corporation, "GenuineIntel" */ {
			/* 0 */ F_UNKNOWN, 
			/* 1 */ F_UNKNOWN, 
			/* 2 */ F_UNKNOWN, 
			/* 3 i386 */ F_UNKNOWN, /* XXX new 386 chips may support CPUID! */
			/* 4 i486 */ {
				/* 0 */ "i486DX-25/33", /*  only few of these */
				/* 1 */ "i486DX-50",    /* support CPUID!     */
				/* 2 */ "i486SX",
				/* 3 */ "i486DX2", /* CPUID only on new chips! */
				/* 4 */ "i486SL",
				/* 5 */ "i486SX2",
				/* 6 */ N_UNKNOWN, 
				/* 7 */ "i486DX2/write-back", /* returns 3 in write-through mode */
				/* 8 */ "i486DX4",
				/* 9 */ "i486DX4/write-back",
				/* A */ N_UNKNOWN, 
				/* B */ N_UNKNOWN, 
				/* C */ N_UNKNOWN, 
				/* D */ N_UNKNOWN, 
				/* E */ N_UNKNOWN, 
				/* F */ N_UNKNOWNEXT 
			},
			/* 5 i586 */ {
				/* 0 */ "Pentium P5 A-step",
				/* 1 */ "Pentium P5",
				/* 2 */ "Pentium P54C",
				/* 3 */ "Pentium OverDrive P24T",
				/* 4 */ "Pentium MMX P55C",
				/* 5 */ N_UNKNOWN, /* XXX DX4 OverDrive? */ 
				/* 6 */ N_UNKNOWN, /* XXX P5 OverDrive? */
				/* 7 */ "Pentium P54C (new)",
				/* 8 */ "Pentium MMX P55C (new)",
				/* 9 */ N_UNKNOWN, 
				/* A */ N_UNKNOWN, 
				/* B */ N_UNKNOWN, 
				/* C */ N_UNKNOWN, 
				/* D */ N_UNKNOWN, 
				/* E */ N_UNKNOWN, 
				/* F */ N_UNKNOWNEXT 
			},
			/* 6 i686 */ {
				/* 0 */ "PentiumPro A-step",
				/* 1 */ "PentiumPro",
				/* 2 */ N_UNKNOWN, 
				/* 3 */ "Pentium II Klamath/Pentium II OverDrive",
				/* 4 */ N_UNKNOWN, /* XXX P55CT - OverDrive for P54? */ 
				/* 5 */ "Celeron Covington/Pentium II Deschutes,Tonga/Pentium II Xeon",
				/* 6 */ "Celeron A Mendocino/Pentium II Dixon",
				/* 7 */ "Pentium III Katmai/Pentium III Xeon Tanner",
				/* 8 */ "Celeron 2/Pentium III Coppermine,Geyserville",
				/* 9 */ "Pentium M Banias", /* XXX check */
				/* A */ "Pentium III Xeon Cascades",
				/* B */ "Celeron 2/Pentium III Tualatin",
				/* C */ N_UNKNOWN, 
				/* D */ "Pentium M Dothan", 
				/* E */ N_UNKNOWN, 
				/* F */ N_UNKNOWNEXT 
			},
			/* 7 Itanium */ { /* XXX check */
				/* 0 */ "Itanium Merced", 
				/* 1 */ N_UNKNOWN, 
				/* 2 */ N_UNKNOWN, 
				/* 3 */ N_UNKNOWN, 
				/* 4 */ N_UNKNOWN, 
				/* 5 */ N_UNKNOWN, 
				/* 6 */ N_UNKNOWN, 
				/* 7 */ N_UNKNOWN, 
				/* 8 */ N_UNKNOWN, 
				/* 9 */ N_UNKNOWN, 
				/* A */ N_UNKNOWN, 
				/* B */ N_UNKNOWN, 
				/* C */ N_UNKNOWN, 
				/* D */ N_UNKNOWN, 
				/* E */ N_UNKNOWN, 
				/* F */ N_UNKNOWNEXT 
			},
			/* 8 */ F_UNKNOWN, 
			/* 9 */ F_UNKNOWN, 
			/* A */ F_UNKNOWN, 
			/* B */ F_UNKNOWN, 
			/* C */ F_UNKNOWN, 
			/* D */ F_UNKNOWN, 
			/* E */ F_UNKNOWN, 
			/* F extended family (P4/new IA-64)*/ {
				/* 0 */ "Pentium 4 Willamette; Xeon Foster",
				/* 1 */ "Pentium 4 Willamette; Xeon Foster",
				/* 2 */ "Pentium 4/Celeron 4 Northwood; Pentium 4 EE/Xeon Prestonia,Gallatin",
				/* 3 */ "Pentium 4/Celeron D Prescott; Xeon Nocona",
				/* 4 */ "Pentium 4/Celeron D Prescott; Pentium D/XE Smithfield; Xeon Nocona,Irwindale",
				/* 5 */ N_UNKNOWN, 
				/* 6 */ N_UNKNOWN, 
				/* 7 */ N_UNKNOWN, 
				/* 8 */ N_UNKNOWN, 
				/* 9 */ N_UNKNOWN, 
				/* A */ N_UNKNOWN, 
				/* B */ N_UNKNOWN, 
				/* C */ N_UNKNOWN, 
				/* D */ N_UNKNOWN, 
				/* E */ N_UNKNOWN, 
				/* F */ N_UNKNOWNEXT 
			}
			#if 0 /* out of table index boundaries */
			/* 1F Itanium 2 */ { /* XXX check */
				/* 0 */ "Itanium 2 McKinley", 
				/* 1 */ "Itanium 2 Madison", /* I coded on that :-) */
				/* 2 */ N_UNKNOWN, 
				/* 3 */ N_UNKNOWN, 
				/* 4 */ N_UNKNOWN, 
				/* 5 */ N_UNKNOWN, 
				/* 6 */ N_UNKNOWN, 
				/* 7 */ N_UNKNOWN, 
				/* 8 */ N_UNKNOWN, 
				/* 9 */ N_UNKNOWN, 
				/* A */ N_UNKNOWN, 
				/* B */ N_UNKNOWN, 
				/* C */ N_UNKNOWN, 
				/* D */ N_UNKNOWN, 
				/* E */ N_UNKNOWN, 
				/* F */ N_UNKNOWNEXT 
			},
			#endif
		},
		/* United Microelectronics Corporation, "UMC UMC UMC " */ {
			/* 0 */ F_UNKNOWN, 
			/* 1 */ F_UNKNOWN, 
			/* 2 */ F_UNKNOWN, 
			/* 3 */ F_UNKNOWN, 
			/* 4 486 (U5) */ {
				/* 0 */ N_UNKNOWN, 
				/* 1 */ "486DX U5D",
				/* 2 */ "486SX U5S",
				/* 3 */ N_UNKNOWN, 
				/* 4 */ N_UNKNOWN, 
				/* 5 */ N_UNKNOWN, 
				/* 6 */ N_UNKNOWN, 
				/* 7 */ N_UNKNOWN, 
				/* 8 */ N_UNKNOWN, 
				/* 9 */ N_UNKNOWN, 
				/* A */ N_UNKNOWN, 
				/* B */ N_UNKNOWN, 
				/* C */ N_UNKNOWN, 
				/* D */ N_UNKNOWN, 
				/* E */ N_UNKNOWN, 
				/* F */ N_UNKNOWN 
			}, 
			/* 5 */ F_UNKNOWN, 
			/* 6 */ F_UNKNOWN, 
			/* 7 */ F_UNKNOWN, 
			/* 8 */ F_UNKNOWN, 
			/* 9 */ F_UNKNOWN, 
			/* A */ F_UNKNOWN, 
			/* B */ F_UNKNOWN, 
			/* C */ F_UNKNOWN, 
			/* D */ F_UNKNOWN, 
			/* E */ F_UNKNOWN, 
			/* F */ F_UNKNOWN 
		},
		/* Advanced Micro Devices, "AuthenticAMD" (very rare: "AMD ISBETTER") */ {
			/* 0 */ F_UNKNOWN, 
			/* 1 */ F_UNKNOWN, 
			/* 2 */ F_UNKNOWN, 
			/* 3 */ F_UNKNOWN, 
			/* 4 486/5x86 */ {
				/* 0 */ N_UNKNOWN, 
				/* 1 */ N_UNKNOWN, 
				/* 2 */ N_UNKNOWN, 
				/* 3 */ "486DX2",
				/* 4 */ N_UNKNOWN, 
				/* 5 */ N_UNKNOWN, 
				/* 6 */ N_UNKNOWN, 
				/* 7 */ "486DX2/write-back",
				/* 8 */ "486DX4/5x86",
				/* 9 */ "486DX4/write-back",
				/* A */ N_UNKNOWN, 
				/* B */ N_UNKNOWN, 
				/* C */ N_UNKNOWN, 
				/* D */ N_UNKNOWN, 
				/* E */ "5x86",
				/* F */ "5x86/write-back"
			}, 
			/* 5 K5/K6 */ {
				/* 0 */ "K5 SSA5 (PR75,PR90,PR100)",
				/* 1 */ "K5 5k86 (PR120,PR133)",
				/* 2 */ "K5 5k86 (PR166)",
				/* 3 */ "K5 5k86 (PR200)",
				/* 4 */ N_UNKNOWN, 
				/* 5 */ N_UNKNOWN, 
				/* 6 */ "K6",
				/* 7 */ "K6 Little Foot",
				/* 8 */ "K6-2",
				/* 9 */ "K6-III Chomper",
				/* A */ N_UNKNOWN, 
				/* B */ N_UNKNOWN, 
				/* C */ N_UNKNOWN, 
				/* D */ "K6-2+/K6-III+ Sharptooth",
				/* E */ N_UNKNOWN, 
				/* F */ N_UNKNOWN 
			}, 
			/* 6 K7 */ {
				/* 0 */ N_UNKNOWN, /* Argon? */
				/* 1 */ "Athlon K7",
				/* 2 */ "Athlon K75 Pluto,Orion",
				/* 3 */ "Duron Spitfire",
				/* 4 */ "Athlon Thunderbird",
				/* 5 */ N_UNKNOWN, 
				/* 6 */ "Duron/Athlon 4/MP/XP Palomino",
				/* 7 */ "Duron Morgan,Camaro",
				/* 8 */ "Sempron/Athlon MP/XP Thoroughbred; Duron Applebred", 
				/* 9 */ N_UNKNOWN,
				/* A */ "Sempron/Athlon MP/XP/XP-M Barton,Thorton", 
				/* B */ N_UNKNOWN, 
				/* E */ N_UNKNOWN, 
				/* C */ N_UNKNOWN, 
				/* D */ N_UNKNOWN, 
				/* F */ N_UNKNOWN 
			}, 
			/* 7 */ F_UNKNOWN, 
			/* 8 */ F_UNKNOWN, 
			/* 9 */ F_UNKNOWN, 
			/* A */ F_UNKNOWN, 
			/* B */ F_UNKNOWN, 
			/* C */ F_UNKNOWN, 
			/* D */ F_UNKNOWN, 
			/* E */ F_UNKNOWN, 
			/* F K8 */ {
				/* 0 */ N_UNKNOWN, 
				/* 1 */ "Opteron Egypt,Italy,Denmark", 
				/* 2 */ N_UNKNOWN, 
				/* 3 */ "Athlon 64 X2 Manchester,Toledo", 
				/* 4 */ "Athlon 64 Clawhammer; Athlon 64 X2 Toledo; Turion Newark,Lancaster", 
				/* 5 */ "Athlon 64 FX/Opteron Sledgehammer,Athens,Troy,Venus", 
				/* 6 */ N_UNKNOWN, 
				/* 7 */ "Athlon 64/FX Sledgehammer,San Diego,Venice", 
				/* 8 */ "Mobile Athlon 64 Newcastle; Mobile Sempron Dublin", 
				/* 9 */ N_UNKNOWN,
				/* A */ N_UNKNOWN, 
				/* B */ "Athlon 64 Newcastle; Athlon 64 X2 Manchester", 
				/* E */ N_UNKNOWN, 
				/* C */ "Athlon 64 Newcastle,Odesssa,Oakville,Venice; Sempron Palermo,Paris,Dublin", 
				/* D */ N_UNKNOWN, 
				/* F */ "Athlon 64 Newcastle,Winchester,San Diego,Venice; Sempron Palermo" 
			}
				
		},
		/* Cyrix Corp./VIA Inc., "CyrixInstead" */ {
			/* 0 */ F_UNKNOWN, 
			/* 1 */ F_UNKNOWN, 
			/* 2 */ F_UNKNOWN, 
			/* 3 */ F_UNKNOWN, 
			/* 4 5x86 */ {
				/* 0 */ N_UNKNOWN, 
				/* 1 */ N_UNKNOWN, 
				/* 2 */ N_UNKNOWN, 
				/* 3 */ N_UNKNOWN, 
				/* 4 */ "MediaGX", 
				/* 5 */ N_UNKNOWN, 
				/* 6 */ N_UNKNOWN, 
				/* 7 */ N_UNKNOWN, 
				/* 8 */ N_UNKNOWN, 
				/* 9 */ "5x86", /* CPUID maybe only on newer chips */
				/* A */ N_UNKNOWN, 
				/* B */ N_UNKNOWN, 
				/* E */ N_UNKNOWN, 
				/* C */ N_UNKNOWN, 
				/* D */ N_UNKNOWN, 
				/* F */ N_UNKNOWN 
			}, 
			/* 5 M1 */ {
				/* 0 */ "M1 test-sample", /*?*/ 
				/* 1 */ N_UNKNOWN, 
				/* 2 */ "6x86 M1", 
				/* 3 */ "6x86L M1", 
				/* 4 */ "GXm", 
				/* 5 */ N_UNKNOWN, 
				/* 6 */ N_UNKNOWN, 
				/* 7 */ N_UNKNOWN, 
				/* 8 */ N_UNKNOWN, 
				/* 9 */ N_UNKNOWN, 
				/* A */ N_UNKNOWN, 
				/* B */ N_UNKNOWN, 
				/* E */ N_UNKNOWN, 
				/* C */ N_UNKNOWN, 
				/* D */ N_UNKNOWN, 
				/* F */ N_UNKNOWN 
			}, 
			/* 6 M2 */ {
				/* 0 */ "6x86MX M2/M-II", 
				/* 1 */ N_UNKNOWN, 
				/* 2 */ N_UNKNOWN, 
				/* 3 */ N_UNKNOWN, 
				/* 4 */ N_UNKNOWN, 
				/* 5 */ "Cyrix III Joshua (M2 core)", 
				/* 6 */ N_UNKNOWN, 
				/* 7 */ N_UNKNOWN, 
				/* 8 */ N_UNKNOWN, 
				/* 9 */ N_UNKNOWN, 
				/* A */ N_UNKNOWN, 
				/* B */ N_UNKNOWN, 
				/* E */ N_UNKNOWN, 
				/* C */ N_UNKNOWN, 
				/* D */ N_UNKNOWN, 
				/* F */ N_UNKNOWN 
			}, 
			/* 7 */ F_UNKNOWN, 
			/* 8 */ F_UNKNOWN, 
			/* 9 */ F_UNKNOWN, 
			/* A */ F_UNKNOWN, 
			/* B */ F_UNKNOWN, 
			/* C */ F_UNKNOWN, 
			/* D */ F_UNKNOWN, 
			/* E */ F_UNKNOWN, 
			/* F */ F_UNKNOWN 
		},
		/* NexGen Inc., "NexGenDriven" */ {
			/* 0 */ F_UNKNOWN, 
			/* 1 */ F_UNKNOWN, 
			/* 2 */ F_UNKNOWN, 
			/* 3 */ F_UNKNOWN, 
			/* 4 */ F_UNKNOWN, 
			/* 5 Nx586 */ {
				/* 0 */ "Nx586/Nx586FPU", /* only newer ones support CPUID! */ 
				/* 1 */ N_UNKNOWN, 
				/* 2 */ N_UNKNOWN, 
				/* 3 */ N_UNKNOWN, 
				/* 4 */ N_UNKNOWN, 
				/* 5 */ N_UNKNOWN, 
				/* 6 */ N_UNKNOWN, 
				/* 7 */ N_UNKNOWN, 
				/* 8 */ N_UNKNOWN, 
				/* 9 */ N_UNKNOWN, 
				/* A */ N_UNKNOWN, 
				/* B */ N_UNKNOWN, 
				/* E */ N_UNKNOWN, 
				/* C */ N_UNKNOWN, 
				/* D */ N_UNKNOWN, 
				/* F */ N_UNKNOWN 
			}, 
			/* 6 */ F_UNKNOWN, 
			/* 7 */ F_UNKNOWN, 
			/* 8 */ F_UNKNOWN, 
			/* 9 */ F_UNKNOWN, 
			/* A */ F_UNKNOWN, 
			/* B */ F_UNKNOWN, 
			/* C */ F_UNKNOWN, 
			/* D */ F_UNKNOWN, 
			/* E */ F_UNKNOWN, 
			/* F */ F_UNKNOWN 
		},
		/* IDT/Centaur/VIA, "CentaurHauls" */ {
			/* 0 */ F_UNKNOWN, 
			/* 1 */ F_UNKNOWN, 
			/* 2 */ F_UNKNOWN, 
			/* 3 */ F_UNKNOWN, 
			/* 4 */ F_UNKNOWN, 
			/* 5 IDT C6 WinChip */ {
				/* 0 */ N_UNKNOWN, 
				/* 1 */ N_UNKNOWN, 
				/* 2 */ N_UNKNOWN, 
				/* 3 */ N_UNKNOWN, 
				/* 4 */ "WinChip C6", 
				/* 5 */ N_UNKNOWN, 
				/* 6 */ "Samuel", 
				/* 7 */ N_UNKNOWN, 
				/* 8 */ "WinChip 2 C6+,W2,W2A,W2B", 
				/* 9 */ "WinChip 3 W3", 
				/* A */ "WinChip 4 W4A",  /* XXX check */
				/* B */ N_UNKNOWN, 
				/* E */ N_UNKNOWN, 
				/* C */ N_UNKNOWN, 
				/* D */ N_UNKNOWN, 
				/* F */ N_UNKNOWN 
			}, 
			/* 6 VIA C3 */ {
				/* 0 */ N_UNKNOWN, 
				/* 1 */ N_UNKNOWN, 
				/* 2 */ N_UNKNOWN, 
				/* 3 */ N_UNKNOWN, 
				/* 4 */ N_UNKNOWN, 
				/* 5 */ N_UNKNOWN, 
				/* 6 */ "Cyrix III Samuel (WinChip C5A core)", 
				/* 7 */ "C3 Samuel 2 (WinChip C5B core)/C3 Ezra", 
				/* 8 */ "C3 Ezra-T",
				/* 9 */ "C3 Nehemiah", 
				/* A */ N_UNKNOWN, 
				/* B */ N_UNKNOWN, 
				/* E */ N_UNKNOWN, 
				/* C */ N_UNKNOWN, 
				/* D */ N_UNKNOWN, 
				/* F */ N_UNKNOWN 
			}, 
			/* 7 */ F_UNKNOWN, 
			/* 8 */ F_UNKNOWN, 
			/* 9 */ F_UNKNOWN, 
			/* A */ F_UNKNOWN, 
			/* B */ F_UNKNOWN, 
			/* C */ F_UNKNOWN, 
			/* D */ F_UNKNOWN, 
			/* E */ F_UNKNOWN, 
			/* F */ F_UNKNOWN 
		},
		/* Rise, "RiseRiseRise" */ {
			/* 0 */ F_UNKNOWN, 
			/* 1 */ F_UNKNOWN, 
			/* 2 */ F_UNKNOWN, 
			/* 3 */ F_UNKNOWN, 
			/* 4 */ F_UNKNOWN, 
			/* 5 mP6 */ {
				/* 0 */ "mP6 iDragon 6401,6441 Kirin", 
				/* 1 */ "mP6 iDragon 6510 Lynx", 
				/* 2 */ N_UNKNOWN, 
				/* 3 */ N_UNKNOWN, 
				/* 4 */ N_UNKNOWN, 
				/* 5 */ N_UNKNOWN, 
				/* 6 */ N_UNKNOWN, 
				/* 7 */ N_UNKNOWN, 
				/* 8 */ "mP6 iDragon II", 
				/* 9 */ "mP6 iDragon II (new)", 
				/* A */ N_UNKNOWN, 
				/* B */ N_UNKNOWN, 
				/* E */ N_UNKNOWN, 
				/* C */ N_UNKNOWN, 
				/* D */ N_UNKNOWN, 
				/* F */ N_UNKNOWN 
			}, 
			/* 6 */ F_UNKNOWN, 
			/* 7 */ F_UNKNOWN, 
			/* 8 */ F_UNKNOWN, 
			/* 9 */ F_UNKNOWN, 
			/* A */ F_UNKNOWN, 
			/* B */ F_UNKNOWN, 
			/* C */ F_UNKNOWN, 
			/* D */ F_UNKNOWN, 
			/* E */ F_UNKNOWN, 
			/* F */ F_UNKNOWN 
		},
		/* Transmeta, "GenuineTMx86" */ {
			/* 0 */ F_UNKNOWN, 
			/* 1 */ F_UNKNOWN, 
			/* 2 */ F_UNKNOWN, 
			/* 3 */ F_UNKNOWN, 
			/* 4 */ F_UNKNOWN, 
			/* 5 Crusoe */ {
				/* 0 */ N_UNKNOWN, 
				/* 1 */ N_UNKNOWN, 
				/* 2 */ N_UNKNOWN, 
				/* 3 */ N_UNKNOWN, 
				/* 4 */ "Crusoe TM3x00,TM5x00", 
				/* 5 */ N_UNKNOWN, 
				/* 6 */ N_UNKNOWN, 
				/* 7 */ N_UNKNOWN, 
				/* 8 */ N_UNKNOWN, 
				/* 9 */ N_UNKNOWN, 
				/* A */ N_UNKNOWN, 
				/* B */ N_UNKNOWN, 
				/* E */ N_UNKNOWN, 
				/* C */ N_UNKNOWN, 
				/* D */ N_UNKNOWN, 
				/* F */ N_UNKNOWN 
			}, 
			/* 6 */ F_UNKNOWN, 
			/* 7 */ F_UNKNOWN, 
			/* 8 */ F_UNKNOWN, 
			/* 9 */ F_UNKNOWN, 
			/* A */ F_UNKNOWN, 
			/* B */ F_UNKNOWN, 
			/* C */ F_UNKNOWN, 
			/* D */ F_UNKNOWN, 
			/* E */ F_UNKNOWN, 
			/* F */ F_UNKNOWN 
		}
};

#undef N_UNKNOWNEXT
#undef N_UNKNOWN
#undef F_UNKNOWN

static const struct {
	char string[13];
	char name[48];
} cpuvendors[MAX_VENDORS] ={
	{"GenuineIntel","Intel"},
	{"UMC UMC UMC ","United Microelectronics Corporation"},
	{"AuthenticAMD","Advanced Micro Devices"},
	{"CyrixInstead","Cyrix/VIA"},
	{"NexGenDriven","NexGen"},
	{"CentaurHauls","IDT/Centaur/VIA"},
	{"RiseRiseRise","Rise"},
	{"GenuineTMx86","Transmeta"}
};	

