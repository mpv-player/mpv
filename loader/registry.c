/*
 * Modified for use with MPlayer, detailed changelog at
 * http://svn.mplayerhq.hu/mplayer/trunk/
 * $Id$
 */

#include "config.h"
#include "debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>

#include "wine/winbase.h"
#include "wine/winreg.h"
#include "wine/winnt.h"
#include "wine/winerror.h"

#include "ext.h"
#include "registry.h"

//#undef TRACE
//#define TRACE printf

extern char *get_path ( const char * );

// ...can be set before init_registry() call
char* regpathname = NULL;

static char* localregpathname = NULL;

typedef struct reg_handle_s
{
	int handle;
	char* name;
	struct reg_handle_s* next;
	struct reg_handle_s* prev;
} reg_handle_t;

struct reg_value
{
	int type;
	char* name;
	int len;
	char* value;
};

static struct reg_value* regs = NULL;
static int reg_size;
static reg_handle_t* head = NULL;

#define DIR -25

static void create_registry(void);
static void open_registry(void);
static void save_registry(void);
static void init_registry(void);


static void create_registry(void){
    if(regs)
    {
	printf("Logic error: create_registry() called with existing registry\n");
	save_registry();
	return;
    }
    regs=malloc(3*sizeof(struct reg_value));
    regs[0].type=regs[1].type=DIR;
    regs[0].name=malloc(5);
    strcpy(regs[0].name, "HKLM");
    regs[1].name=malloc(5);
    strcpy(regs[1].name, "HKCU");
    regs[0].value=regs[1].value=NULL;
    regs[0].len=regs[1].len=0;
    reg_size=2;
    head = 0;
    save_registry();
}

static void open_registry(void)
{
	int fd;
	int i;
	unsigned int len;
	if(regs)
	{
		printf("Multiple open_registry(>\n");
		return;
	}
	fd = open(localregpathname, O_RDONLY);
	if (fd == -1)
	{
	    printf("Creating new registry\n");
	    create_registry();
	    return;
	}
	read(fd, &reg_size, 4);
	regs=malloc(reg_size*sizeof(struct reg_value));
	head = 0;
	for(i=0; i<reg_size; i++)
	{
		read(fd,&regs[i].type,4);
		read(fd,&len,4);
		regs[i].name=malloc(len+1);
		if(regs[i].name==0)
		{
			reg_size=i+1;
			goto error;
		}
		read(fd, regs[i].name, len);
		regs[i].name[len]=0;
		read(fd,&regs[i].len,4);
		regs[i].value=malloc(regs[i].len+1);
		if(regs[i].value==0)
		{
		        free(regs[i].name);
			reg_size=i+1;
			goto error;
		}
		read(fd, regs[i].value, regs[i].len);
		regs[i].value[regs[i].len]=0;
	}
error:
	close(fd);
	return;
}

static void save_registry(void)
{
	int fd, i;
	if (!regs)
		init_registry();
	fd = open(localregpathname, O_WRONLY | O_CREAT, 00666);
	if (fd == -1)
	{
	    printf("Failed to open registry file '%s' for writing.\n",
		   localregpathname);
	    return;
	}
	write(fd, &reg_size, 4);
	for(i=0; i<reg_size; i++)
	{
	        unsigned len=strlen(regs[i].name);
		write(fd, &regs[i].type, 4);
		write(fd, &len, 4);
		write(fd, regs[i].name, len);
		write(fd, &regs[i].len, 4);
		write(fd, regs[i].value, regs[i].len);
	}
	close(fd);
}

void free_registry(void)
{
    reg_handle_t* t = head;
    while (t)
    {
	reg_handle_t* f = t;
        if (t->name)
	    free(t->name);
	t=t->prev;
        free(f);
    }
    head = 0;
    if (regs)
    {
        int i;
	for(i=0; i<reg_size; i++)
	{
	    free(regs[i].name);
	    free(regs[i].value);
	}
	free(regs);
	regs = 0;
    }

    if (localregpathname && localregpathname != regpathname)
	free(localregpathname);
    localregpathname = 0;
}


static reg_handle_t* find_handle_by_name(const char* name)
{
	reg_handle_t* t;
	for(t=head; t; t=t->prev)
	{
		if(!strcmp(t->name, name))
		{
			return t;
		}
	}
	return 0;
}
static struct reg_value* find_value_by_name(const char* name)
{
	int i;
	for(i=0; i<reg_size; i++)
		if(!strcmp(regs[i].name, name))
			return regs+i;
	return 0;
}
static reg_handle_t* find_handle(int handle)
{
	reg_handle_t* t;
	for(t=head; t; t=t->prev)
	{
		if(t->handle==handle)
		{
			return t;
		}
	}
	return 0;
}
static int generate_handle()
{
	static unsigned int zz=249;
	zz++;
	while((zz==HKEY_LOCAL_MACHINE) || (zz==HKEY_CURRENT_USER))
		zz++;
	return zz;
}

static reg_handle_t* insert_handle(long handle, const char* name)
{
	reg_handle_t* t;
	t=malloc(sizeof(reg_handle_t));
	if(head==0)
	{
		t->prev=0;
	}
	else
	{
		head->next=t;
		t->prev=head;
	}
	t->next=0;
	t->name=malloc(strlen(name)+1);
	strcpy(t->name, name);
	t->handle=handle;
	head=t;
	return t;
}
static char* build_keyname(long key, const char* subkey)
{
	char* full_name;
	reg_handle_t* t;
 	if((t=find_handle(key))==0)
	{
		TRACE("Invalid key\n");
		return NULL;
	}
	if(subkey==NULL)
		subkey="<default>";
	full_name=malloc(strlen(t->name)+strlen(subkey)+10);
	strcpy(full_name, t->name);
	strcat(full_name, "\\");
	strcat(full_name, subkey);
	return full_name;
}
static struct reg_value* insert_reg_value(int handle, const char* name, int type, const void* value, int len)
{
	reg_handle_t* t;
	struct reg_value* v;
	char* fullname;
	if((fullname=build_keyname(handle, name))==NULL)
	{
		TRACE("Invalid handle\n");
		return NULL;
	}

	if((v=find_value_by_name(fullname))==0)
	//creating new value in registry
	{
		if(regs==0)
		    create_registry();
		regs=(struct reg_value*)realloc(regs, sizeof(struct reg_value)*(reg_size+1));
		//regs=(struct reg_value*)my_realloc(regs, sizeof(struct reg_value)*(reg_size+1));
		v=regs+reg_size;
		reg_size++;
	}
	else
	//replacing old one
	{
	    free(v->value);
	    free(v->name);
	}
	TRACE("RegInsert '%s'  %p  v:%d  len:%d\n", name, value, *(int*)value, len);
	v->type=type;
	v->len=len;
	v->value=malloc(len);
	memcpy(v->value, value, len);
	v->name=malloc(strlen(fullname)+1);
	strcpy(v->name, fullname);
        free(fullname);
	save_registry();
	return v;
}

static void init_registry(void)
{
	TRACE("Initializing registry\n");
	// can't be free-ed - it's static and probably thread
	// unsafe structure which is stored in glibc

#ifdef MPLAYER
	regpathname = get_path("registry");
	localregpathname = regpathname;
#else
	// regpathname is an external pointer
        //
	// registry.c is holding its own internal pointer
	// localregpathname  - which is being allocate/deallocated

	if (localregpathname == 0)
	{
            const char* pthn = regpathname;
	    if (!regpathname)
	    {
		// avifile - for now reading data from user's home
		struct passwd* pwent;
		pwent = getpwuid(geteuid());
                pthn = pwent->pw_dir;
	    }

	    localregpathname = malloc(strlen(pthn)+20);
	    strcpy(localregpathname, pthn);
	    strcat(localregpathname, "/.registry");
	}
#endif

	open_registry();
	insert_handle(HKEY_LOCAL_MACHINE, "HKLM");
	insert_handle(HKEY_CURRENT_USER, "HKCU");
}

static reg_handle_t* find_handle_2(long key, const char* subkey)
{
	char* full_name;
	reg_handle_t* t;
 	if((t=find_handle(key))==0)
	{
		TRACE("Invalid key\n");
		return (reg_handle_t*)-1;
	}
	if(subkey==NULL)
		return t;
	full_name=malloc(strlen(t->name)+strlen(subkey)+10);
	strcpy(full_name, t->name);
	strcat(full_name, "\\");
	strcat(full_name, subkey);
	t=find_handle_by_name(full_name);
	free(full_name);
	return t;
}

long __stdcall RegOpenKeyExA(long key, const char* subkey, long reserved, long access, int* newkey)
{
    char* full_name;
    reg_handle_t* t;
    struct reg_value* v;
    TRACE("Opening key %s\n", subkey);

    if(!regs)
        init_registry()
;
/*	t=find_handle_2(key, subkey);

	if(t==0)
		return -1;

	if(t==(reg_handle_t*)-1)
		return -1;
*/
    full_name=build_keyname(key, subkey);
    if(!full_name)
        return -1;
    TRACE("Opening key Fullname %s\n", full_name);
    v=find_value_by_name(full_name);

    t=insert_handle(generate_handle(), full_name);
    *newkey=t->handle;
    free(full_name);

    return 0;
}
long __stdcall RegCloseKey(long key)
{
    reg_handle_t *handle;
    if(key==(long)HKEY_LOCAL_MACHINE)
	return 0;
    if(key==(long)HKEY_CURRENT_USER)
	return 0;
    handle=find_handle(key);
    if(handle==0)
	return 0;
    if(handle->prev)
	handle->prev->next=handle->next;
    if(handle->next)
	handle->next->prev=handle->prev;
    if(handle->name)
	free(handle->name);
    if(handle==head)
	head=head->prev;
    free(handle);
    return 1;
}

long __stdcall RegQueryValueExA(long key, const char* value, int* reserved, int* type, int* data, int* count)
{
    struct reg_value* t;
    char* c;
    TRACE("Querying value %s\n", value);
    if(!regs)
	init_registry();

    c=build_keyname(key, value);
    if (!c)
	return 1;
    t=find_value_by_name(c);
    free(c);
    if (t==0)
	return 2;
    if (type)
	*type=t->type;
    if (data)
    {
	memcpy(data, t->value, (t->len<*count)?t->len:*count);
	TRACE("returning %d bytes: %d\n", t->len, *(int*)data);
    }
    if(*count<t->len)
    {
	*count=t->len;
	return ERROR_MORE_DATA;
    }
    else
    {
	*count=t->len;
    }
    return 0;
}
long __stdcall RegCreateKeyExA(long key, const char* name, long reserved,
		     void* classs, long options, long security,
		     void* sec_attr, int* newkey, int* status)
{
    reg_handle_t* t;
    char* fullname;
    struct reg_value* v;
    //        TRACE("Creating/Opening key %s\n", name);
    if(!regs)
	init_registry();

    fullname=build_keyname(key, name);
    if (!fullname)
	return 1;
    TRACE("Creating/Opening key %s\n", fullname);
    v=find_value_by_name(fullname);
    if(v==0)
    {
	int qw=45708;
	v=insert_reg_value(key, name, DIR, &qw, 4);
	if (status) *status=REG_CREATED_NEW_KEY;
	//		return 0;
    }

    t=insert_handle(generate_handle(), fullname);
    *newkey=t->handle;
    free(fullname);
    return 0;
}

/*
LONG RegEnumValue(
  HKEY hKey,              // handle to key to query
  DWORD dwIndex,          // index of value to query
  LPTSTR lpValueName,     // address of buffer for value string
  LPDWORD lpcbValueName,  // address for size of value buffer
  LPDWORD lpReserved,     // reserved
  LPDWORD lpType,         // address of buffer for type code
  LPBYTE lpData,          // address of buffer for value data
  LPDWORD lpcbData        // address for size of data buffer
);
*/

long __stdcall RegEnumValueA(HKEY hkey, DWORD index, LPSTR value, LPDWORD val_count,
		   LPDWORD reserved, LPDWORD type, LPBYTE data, LPDWORD count)
{
    // currenly just made to support MSZH & ZLIB
    //printf("Reg Enum 0x%x %d  %s %d   data: %p %d  %d >%s<\n", hkey, index,
    //       value, *val_count, data, *count, reg_size, data);
    reg_handle_t* t = find_handle(hkey);
    if (t && index < 10)
    {
	struct reg_value* v=find_value_by_name(t->name);
	if (v)
	{
	    memcpy(data, v->value, (v->len < *count) ? v->len : *count);
	    if(*count < v->len)
		*count = v->len;
	    if (type)
		*type = v->type;
	    //printf("Found handle  %s\n", v->name);
	    return 0;
	}
    }
    return ERROR_NO_MORE_ITEMS;
}

long __stdcall RegSetValueExA(long key, const char* name, long v1, long v2, const void* data, long size)
{
    struct reg_value* t;
    char* c;
    TRACE("Request to set value %s %d\n", name, *(const int*)data);
    if(!regs)
	init_registry();

    c=build_keyname(key, name);
    if(c==NULL)
	return 1;
    insert_reg_value(key, name, v2, data, size);
    free(c);
    return 0;
}

long __stdcall RegEnumKeyExA(HKEY hKey, DWORD dwIndex, LPSTR lpName, LPDWORD lpcbName,
		   LPDWORD lpReserved, LPSTR lpClass, LPDWORD lpcbClass,
		   LPFILETIME lpftLastWriteTime)
{
    return ERROR_NO_MORE_ITEMS;
}
