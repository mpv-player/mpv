#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include "fb.h"
#include "matroxfb.h"

static int help(void) {
	fprintf(stderr, "usage: matroxset [-f fbdev] [-o output] [-m] [value]\n"
	                "\n"
	                "where -f fbdev  is fbdev device (default /dev/fb1)\n"
	                "      -o output is output number to investigate (0=primary, 1=secondary=default)\n"
	                "      -m        says that CRTC->output mapping should be changed/retrieved\n"
			"      -p        print information about blanking\n"
	                "      value     if present, value is set, if missing, value is retrieved\n"
			"\n"
			"For output mode, 128 means monitor, 1 = PAL TV, 2 = NTSC TV\n");
	return 98;
}

int main(int argc, char* argv[]) {
	char* fb = "/dev/fb1";
	int fd;
	struct matroxioc_output_mode mom;
	struct fb_vblank vbl;
	int rtn;
	int output = MATROXFB_OUTPUT_SECONDARY;
	int o_present = 0;
	int m_present = 0;
	int p_present = 0;
	int act;
	u_int32_t conns;
	
	while ((rtn = getopt(argc, argv, "o:f:mhp")) != -1) {
		switch (rtn) {
			case 'o':
				output = strtoul(optarg, NULL, 0);
				o_present = 1;
				break;
			case 'm':
				m_present = 1;
				break;
			case 'f':
				fb = optarg;
				break;
			case 'p':
				p_present = 1;
				break;
			case 'h':
				return help();
			default:
				fprintf(stderr, "Bad commandline\n");
				return 99;
		}
	}
	act = 0;
	if (p_present) {
		if (m_present || o_present) {
			fprintf(stderr, "You cannot use -p together with -m or -o\n");
			return 95;
		}
		act = 4;
	} else if (optind >= argc) {
		if (m_present) {
			if (o_present) {
				fprintf(stderr, "You cannot use -m and -o together\n");
				return 96;
			}
			act = 2;
		} else {
			mom.output = output;
			mom.mode = 0;
		}
	} else {
		if (m_present) {
			conns = strtoul(argv[optind], NULL, 0);
			act = 3;
		} else {
			mom.output = output;
			mom.mode = strtoul(argv[optind], NULL, 0);
			act = 1;
		}
	}
	fd = open(fb, O_RDWR);
	if (fd == -1) {
		fprintf(stderr, "Cannot open %s: %s\n", fb, strerror(errno));
		return 122;
	}
	switch (act) {
		case 0:
			rtn = ioctl(fd, MATROXFB_GET_OUTPUT_MODE, &mom);
			if (rtn)
				break;
			printf("Output mode is %u\n", mom.mode);
			break;
		case 1:
			rtn = ioctl(fd, MATROXFB_SET_OUTPUT_MODE, &mom);
			break;
		case 2:
			rtn = ioctl(fd, MATROXFB_GET_OUTPUT_CONNECTION, &conns);
			if (rtn)
				break;
			printf("This framebuffer is connected to outputs %08X\n", conns);
			break;
		case 3:
			rtn = ioctl(fd, MATROXFB_SET_OUTPUT_CONNECTION, &conns);
			break;
		case 4:
#if 0
			{ int i; for (i = 0; i < 1000000; i++) {
			rtn = ioctl(fd, FBIOGET_VBLANK, &vbl);
			if (rtn)
				break;
			}}
#else
			rtn = ioctl(fd, FBIOGET_VBLANK, &vbl);
			if (rtn)
				break;
#endif
			printf("VBlank flags:          %08X\n", vbl.flags);
			printf("  Symbolic: ");
			{
				static const struct { u_int32_t mask; const char* msg; } *ptr, vals[] = {
					{ FB_VBLANK_HAVE_VBLANK, "vblank" },
					{ FB_VBLANK_HAVE_HBLANK, "hblank" },
					{ FB_VBLANK_HAVE_COUNT, "field no." },
					{ FB_VBLANK_HAVE_VCOUNT, "line no." },
					{ FB_VBLANK_HAVE_HCOUNT, "column no." },
					{ FB_VBLANK_VBLANKING, "vblanking" },
					{ FB_VBLANK_HBLANKING, "hblanking" },
					{ 0, NULL }};
				int ap = 0;
				for (ptr = vals; ptr->msg; ptr++) {
					if (vbl.flags & ptr->mask) {
						if (ap) printf(", ");
						printf(ptr->msg);
						ap = 1;
					}
				}
				if (!ap)
					printf("none");
				printf("\n");
			}
			printf("Field count:       %12u\n", vbl.count);
			printf("Vertical line:     %12u\n", vbl.vcount);
			printf("Horizontal column: %12u\n", vbl.hcount);
			break;
		default:
			rtn = -1; errno = EINVAL;
			break;
	}
	if (rtn) {
		fprintf(stderr, "ioctl failed: %s\n", strerror(errno));
	}
	close(fd);
	return 0;
}
	
