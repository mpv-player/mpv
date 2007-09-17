include ../../config.mak

all: dhasetup.exe dhahelper.sys

dhasetup.exe: dhasetup.c
	$(CC) -o dhasetup.exe dhasetup.c

dhahelper.o: dhahelper.c dhahelper.h
	$(CC) -Wall -Os -c dhahelper.c -o dhahelper.o

dhahelper-rc.o: dhahelper.rc common.ver ntverp.h
	windres -I. dhahelper.rc $@

base.tmp: dhahelper.o dhahelper-rc.o
	$(CC)   -Wl,--base-file,base.tmp \
		-Wl,--entry,_DriverEntry@8 \
		-nostartfiles -nostdlib \
		-o junk.tmp \
		dhahelper.o dhahelper-rc.o \
		-lntoskrnl
	-rm -f junk.tmp

temp.exp: base.tmp
	dlltool --dllname dhahelper.sys --base-file base.tmp --output-exp temp.exp

dhahelper.sys: dhahelper.o dhahelper-rc.o temp.exp
	$(CC)	-Wl,--subsystem,native \
		-Wl,--image-base,0x10000 \
		-Wl,--file-alignment,0x1000 \
		-Wl,--section-alignment,0x1000 \
		-Wl,--entry,_DriverEntry@8 \
		-Wl,temp.exp \
		-mdll -nostartfiles -nostdlib \
		-o dhahelper.sys \
		dhahelper.o dhahelper-rc.o \
		-lntoskrnl
	strip dhahelper.sys

clean:
	rm -f *.o *~ dhahelper.sys dhasetup.exe base.tmp temp.exp

distclean: clean
