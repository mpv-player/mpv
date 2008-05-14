/*
 *  A program to modify a registry file.
 *
 *  Copyright (C) 2007 Alan Nisota
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <unistd.h>
#include <getopt.h>
#include "loader/registry.c"

#include "mp_msg.h"
#ifdef __GNUC__
#define mp_msg(t, l, m, args...) fprintf(stderr, m, ##args)
#else
#define mp_msg(t, l, ...) fprintf(stderr, __VA_ARGS__)
#endif

#include "get_path.c"

static void remove_key(long handle, const char* name) {
    int i, len;
    char *fullname;

    fullname = build_keyname(handle, name);
    len = strlen(fullname);
    for (i=0; i < reg_size;) {
        if (!strncmp(regs[i].name, fullname, len)) {
            free(regs[i].value);
            free(regs[i].name);
            memmove(&regs[i], &regs[i+1], --reg_size*sizeof(struct reg_value));
        } else {
            i++;
        }
    }
    free(fullname);
    save_registry();
}

static void parse_key(char *raw, HKEY *root, char *path, char *key) {
    char *tmpkey, *start;
    tmpkey = strrchr(raw, '\\');
    if (tmpkey == raw || tmpkey == NULL) {
        printf("Couldn't process key \"%s\"\n", raw);
        return;
    }
    start = strchr(raw, '\\');
    if (start == raw || start == NULL) {
        printf("Couldn't process key \"%s\"\n", raw);
        return;
    }
    if (strncmp(raw, "HKEY_CURRENT_USER\\", 18) == 0 ||
            strncmp(raw, "HKCU\\", 5) == 0) {
        *root = HKEY_CURRENT_USER;
    } else if (strncmp(raw, "HKEY_LOCAL_MACHINE\\", 19) == 0 ||
            strncmp(raw, "HKLM\\", 5) == 0) {
        *root = HKEY_LOCAL_MACHINE;
    } else {
        printf("Couldn't process key \"%s\"\n", raw);
        return;
    }
    strncpy(key, tmpkey + 1, strlen(tmpkey)-1);
    key[strlen(tmpkey)-1] = 0;
    while(*start == '\\')
        start++;
    while(*(tmpkey-1) == '\\')
        tmpkey--;
    strncpy(path, start, tmpkey - start);
    path[tmpkey - start] = 0;
}

int main(int argc, char *argv[]) {
    int i;
    long type = REG_SZ;
    char c, path[256], key[256], *value = NULL;
    HKEY root = 0;
    int Option_Index;
    int list = 0, del = 0;
    int newkey, status;
    static struct option Long_Options[] = {
        {"registry", 1, 0, 'r'},
        {"list", 0, 0, 'l'},
        {"key", 1, 0, 'k'},
        {"value", 1, 0, 'v'},
        {"type", 1, 0, 't'},
        {"del", 0, 0, 'd'},
    };
    while(1) {
        c = getopt_long(argc, argv, "r:lk:v:t:id", Long_Options, &Option_Index);
        if (c == EOF)
            break;
        switch(c) {
          case 'r': 
            localregpathname = optarg;
            break;
          case 'l':
            list = 1;
            break;
          case 'k':
            parse_key(optarg, &root, path, key);
            break;
          case 'v':
            value = optarg;
            break;
          case 't':
            if (!strcmp(optarg, "string"))
                type = REG_SZ;
            else if (!strcmp(optarg, "dword"))
                type = REG_DWORD;
            break;
          case 'd':
            del = 1;
            break;
        }
    }
    if (localregpathname == NULL || (! list && ! root)) {
        printf("Must specify '-r' and either '-k' or '-l'\n");
        return 1;
    }
    if (del && (list || value)) {
        printf("Can't specify '-d' along with '-l' or '-v'\n");
        return 1;
    }
    open_registry();
    insert_handle(HKEY_LOCAL_MACHINE, "HKLM");
    insert_handle(HKEY_CURRENT_USER, "HKCU");

    if (del) {
        char tmpname[256];
        sprintf(tmpname, "%s\\%s", path, key);
        remove_key(root, tmpname);
        return 0;
    }

    if (list) {
        for (i=0; i < reg_size; i++) {
            if (regs[i].type == DIR) {
                printf("Directory:  %s\n", regs[i].name);
            } else if (regs[i].type == REG_DWORD) {
                DWORD v = *(DWORD *)regs[i].value;
                printf("%s :: %08x  type: DWORD\n", regs[i].name, v);
            } else if (regs[i].type == REG_SZ) {
                printf("%s :: '%s' len: %d type: String\n",
                        regs[i].name, regs[i].value, regs[i].len);
            } else {
                printf("%s :: '%s' len: %d type: %08x\n",
                        regs[i].name, regs[i].value, regs[i].len, regs[i].type);
            }
        }
    }
    if (root) {
        RegCreateKeyExA(root, path, 0, 0, 0, 0, 0, &newkey, &status);
        if (value != NULL) {
            int len;
            DWORD v;
            if (type == REG_DWORD) {
                len = sizeof(DWORD);
                v = strtoul(value, NULL, 0);
                value = (char *)&v;
            } else
                len = strlen(value)+1;
            printf("%08x -- %d\n", *value, len);
            RegSetValueExA(newkey, key, 0, type, value, len);
        }
    }
    return 0;
}

