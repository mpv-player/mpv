/* avi-fix v0.1 (C) A'rpi
 * simple tool to fix chunk sizes in a RIFF AVI file
 * it doesn't check/fix index, use mencoder -forceidx -oac copy -ovc copy to fix index!
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"
#ifdef MP_DEBUG
#define mp_debug(...) printf(__VA_ARGS__)
#else
#define mp_debug(...)
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FCC(a,b,c,d)  (((a)<<24)|((b)<<16)|((c)<<8)|(d))

static inline char xx(unsigned char c){
    if(c>=32 && c<128) return c;
    return '?';
}

static inline unsigned int getid(FILE* f){
    unsigned int id;
    id=fgetc(f);
    id=(id<<8)|fgetc(f);
    id=(id<<8)|fgetc(f);
    id=(id<<8)|fgetc(f);
    return id;
}

int main(int argc,char* argv[]){
//FILE* f=fopen("edgar.avi","rb");  // readonly (report errors)
//FILE* f=fopen("edgar.avi","rb+"); // fix mode (fix chunk sizes)
unsigned int lastgood=0;
unsigned int fixat=0;
unsigned int offset=0;
int fix_flag=0;
FILE* f;

if(argc<=1){
    printf("Usage: %s [-fix] badfile.avi\n",argv[0]);
    exit(1);
}

if(!strcmp(argv[1],"-fix")){
    fix_flag=1;
    f=fopen(argv[argc-1],"rb+");
} else
    f=fopen(argv[argc-1],"rb");

if(!f){
    perror("error");
    printf("couldnt open '%s'\n",argv[argc-1]);
    exit(2);
}

while(1){
    unsigned int id,len;
again:
    id=fgetc(f);
    id=(id<<8)|fgetc(f);
    id=(id<<8)|fgetc(f);
faszom:
    if(feof(f)) break;
//    if(!lastgood && feof(f)) break;
    id=(id<<8)|fgetc(f);
//    lastgood=ftell(f);
    mp_debug("%08X: %c%c%c%c\n",(int)ftell(f)-4,xx(id>>24),xx(id>>16),xx(id>>8),xx(id));
    switch(id){
    case FCC('R','I','F','F'):
	fread(&len,4,1,f); // filesize
	id=getid(f);  // AVI
	mp_debug("RIFF header, filesize=0x%X  format=%c%c%c%c\n",len,xx(id>>24),xx(id>>16),xx(id>>8),xx(id));
	break;
    case FCC('L','I','S','T'):
	fread(&len,4,1,f); // size
	id=getid(f);  // AVI
	mp_debug("LIST size=0x%X  format=%c%c%c%c\n",len,xx(id>>24),xx(id>>16),xx(id>>8),xx(id));
	//case FCC('h','d','r','l'):
	//case FCC('s','t','r','l'):
	//case FCC('o','d','m','l'):
	//case FCC('m','o','v','i'):
	break;
    // legal chunk IDs:
    case FCC('a','v','i','h'): // avi header
    case FCC('s','t','r','h'): // stream header
    case FCC('s','t','r','f'): // stream format
    case FCC('J','U','N','K'): // official shit
    // index:
    case FCC('i','d','x','1'): // main index??
    case FCC('d','m','l','h'): // opendml header
    case FCC('i','n','d','x'): // opendml main index??
    case FCC('i','x','0','0'): // opendml sub index??
    case FCC('i','x','0','1'): // opendml sub index??
    // data:
    case FCC('0','1','w','b'): // audio track #1
    case FCC('0','2','w','b'): // audio track #2
    case FCC('0','3','w','b'): // audio track #3
    case FCC('0','0','d','b'): // uncompressed video
    case FCC('0','0','d','c'): // compressed video
    case FCC('0','0','_','_'): // A-V interleaved (type2 DV file)
    // info:
    case FCC('I','S','F','T'): // INFO: software
    case FCC('I','S','R','C'): // INFO: source
    case FCC('I','N','A','M'): // INFO: name
    case FCC('I','S','B','J'): // INFO: subject
    case FCC('I','A','R','T'): // INFO: artist
    case FCC('I','C','O','P'): // INFO: copyright
    case FCC('I','C','M','T'): // INFO: comment
	lastgood=ftell(f);
	if(fixat && fix_flag){
	    // fix last chunk's size field:
	    fseek(f,fixat,SEEK_SET);
	    len=lastgood-fixat-8;
	    mp_debug("Correct len to 0x%X\n",len);
	    fwrite(&len,4,1,f);
	    fseek(f,lastgood,SEEK_SET);
	    fixat=0;
	}
	fread(&len,4,1,f); // size
	mp_debug("ID ok, chunk len=0x%X\n",len);
	len+=len&1; // align at 2
	fseek(f,len,SEEK_CUR); // skip data
	break;
    default:
	if(!lastgood){
	    ++offset;
	    mp_debug("invalid ID, trying %d byte offset\n",offset);
	    goto faszom; // try again @ next post
	}
	mp_debug("invalid ID, parsing next chunk's data at 0x%X\n",lastgood);
	fseek(f,lastgood,SEEK_SET);
	fixat=lastgood;
	lastgood=0;
	goto again;
    }
    offset=0;
}

return 0;
}
