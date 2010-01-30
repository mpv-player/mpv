/*
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

/// \file
/// \ingroup Properties

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>

#include "m_option.h"
#include "m_property.h"
#include "mp_msg.h"
#include "help_mp.h"

#define ROUND(x) ((int)((x)<0 ? (x)-0.5 : (x)+0.5))

static int do_action(const m_option_t* prop_list, const char* name,
                     int action, void* arg, void *ctx) {
    const char* sep;
    const m_option_t* prop;
    m_property_action_t ka;
    int r;
    if((sep = strchr(name,'/')) && sep[1]) {
        int len = sep-name;
        char base[len+1];
        memcpy(base,name,len);
        base[len] = 0;
        prop = m_option_list_find(prop_list, base);
        ka.key = sep+1;
        ka.action = action;
        ka.arg = arg;
        action = M_PROPERTY_KEY_ACTION;
        arg = &ka;
    } else
        prop = m_option_list_find(prop_list, name);
    if(!prop) return M_PROPERTY_UNKNOWN;
    r = ((m_property_ctrl_f)prop->p)(prop,action,arg,ctx);
    if(action == M_PROPERTY_GET_TYPE && r < 0) {
        if(!arg) return M_PROPERTY_ERROR;
        *(const m_option_t**)arg = prop;
        return M_PROPERTY_OK;
    }
    return r;
}

int m_property_do(const m_option_t* prop_list, const char* name,
                  int action, void* arg, void *ctx) {
    const m_option_t* opt;
    void* val;
    char* str;
    int r;

    switch(action) {
    case M_PROPERTY_PRINT:
        if((r = do_action(prop_list,name,M_PROPERTY_PRINT,arg,ctx)) >= 0)
            return r;
        // fallback on the default print for this type
    case M_PROPERTY_TO_STRING:
        if((r = do_action(prop_list,name,M_PROPERTY_TO_STRING,arg,ctx)) !=
           M_PROPERTY_NOT_IMPLEMENTED)
            return r;
        // fallback on the options API. Get the type, value and print.
        if((r = do_action(prop_list,name,M_PROPERTY_GET_TYPE,&opt,ctx)) <= 0)
            return r;
        val = calloc(1,opt->type->size);
        if((r = do_action(prop_list,name,M_PROPERTY_GET,val,ctx)) <= 0) {
            free(val);
            return r;
        }
        if(!arg) return M_PROPERTY_ERROR;
        str = m_option_print(opt,val);
        free(val);
        *(char**)arg = str == (char*)-1 ? NULL : str;
        return str != (char*)-1;
    case M_PROPERTY_PARSE:
        // try the property own parsing func
        if((r = do_action(prop_list,name,M_PROPERTY_PARSE,arg,ctx)) !=
           M_PROPERTY_NOT_IMPLEMENTED)
            return r;
        // fallback on the options API, get the type and parse.
        if((r = do_action(prop_list,name,M_PROPERTY_GET_TYPE,&opt,ctx)) <= 0)
            return r;
        if(!arg) return M_PROPERTY_ERROR;
        val = calloc(1,opt->type->size);
        if((r = m_option_parse(opt,opt->name,arg,val,M_CONFIG_FILE)) <= 0) {
            free(val);
            return r;
        }
        r = do_action(prop_list,name,M_PROPERTY_SET,val,ctx);
        m_option_free(opt,val);
        free(val);
        return r;
    }
    return do_action(prop_list,name,action,arg,ctx);
}

char* m_properties_expand_string(const m_option_t* prop_list,char* str, void *ctx) {
    int l,fr=0,pos=0,size=strlen(str)+512;
    char *p = NULL,*e,*ret = malloc(size), num_val;
    int skip = 0, lvl = 0, skip_lvl = 0;

    while(str[0]) {
        if(str[0] == '\\') {
            int sl = 1;
            switch(str[1]) {
            case 'e':
                p = "\x1b", l = 1; break;
            case 'n':
                p = "\n", l = 1; break;
            case 'r':
                p = "\r", l = 1; break;
            case 't':
                p = "\t", l = 1; break;
            case 'x':
                if(str[2]) {
                    char num[3] = { str[2], str[3], 0 };
                    char* end = num;
                    num_val = strtol(num,&end,16);
                    sl = end-num;
                    l = 1;
                    p = &num_val;
                } else
                    l = 0;
                break;
            default:
                p = str+1, l = 1;
            }
            str+=1+sl;
        } else if(lvl > 0 && str[0] == ')') {
            if(skip && lvl <= skip_lvl) skip = 0;
            lvl--, str++, l = 0;
        } else if(str[0] == '$' && str[1] == '{' && (e = strchr(str+2,'}'))) {
            int pl = e-str-2;
            char pname[pl+1];
            memcpy(pname,str+2,pl);
            pname[pl] = 0;
            if(m_property_do(prop_list, pname,
                             M_PROPERTY_PRINT, &p, ctx) >= 0 && p)
                l = strlen(p), fr = 1;
            else
                l = 0;
            str = e+1;
        } else if(str[0] == '?' && str[1] == '(' && (e = strchr(str+2,':'))) {
            lvl++;
            if(!skip) {
                int is_not = str[2] == '!';
                int pl = e - str - (is_not ? 3 : 2);
                char pname[pl+1];
                memcpy(pname, str + (is_not ? 3 : 2), pl);
                pname[pl] = 0;
                if(m_property_do(prop_list,pname,M_PROPERTY_GET,NULL,ctx) < 0) {
                    if (!is_not)
                        skip = 1, skip_lvl = lvl;
                }
                else if (is_not)
                    skip = 1, skip_lvl = lvl;
            }
            str = e+1, l = 0;
        } else
            p = str, l = 1, str++;

        if(skip || l <= 0) continue;

        if(pos+l+1 > size) {
            size = pos+l+512;
            ret = realloc(ret,size);
        }
        memcpy(ret+pos,p,l);
        pos += l;
        if(fr) free(p), fr = 0;
    }

    ret[pos] = 0;
    return ret;
}

void m_properties_print_help_list(const m_option_t* list) {
    char min[50],max[50];
    int i,count = 0;

    mp_msg(MSGT_CFGPARSER, MSGL_INFO, MSGTR_PropertyListHeader);
    for(i = 0 ; list[i].name ; i++) {
        const m_option_t* opt = &list[i];
        if(opt->flags & M_OPT_MIN)
            sprintf(min,"%-8.0f",opt->min);
        else
            strcpy(min,"No");
        if(opt->flags & M_OPT_MAX)
            sprintf(max,"%-8.0f",opt->max);
        else
            strcpy(max,"No");
        mp_msg(MSGT_CFGPARSER, MSGL_INFO, " %-20.20s %-15.15s %-10.10s %-10.10s\n",
               opt->name,
               opt->type->name,
               min,
               max);
        count++;
    }
    mp_msg(MSGT_CFGPARSER, MSGL_INFO, MSGTR_TotalProperties, count);
}

// Some generic property implementations

int m_property_int_ro(const m_option_t* prop,int action,
                      void* arg,int var) {
    switch(action) {
    case M_PROPERTY_GET:
        if(!arg) return 0;
        *(int*)arg = var;
        return 1;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

int m_property_int_range(const m_option_t* prop,int action,
                         void* arg,int* var) {
    switch(action) {
    case M_PROPERTY_SET:
        if(!arg) return 0;
        M_PROPERTY_CLAMP(prop,*(int*)arg);
        *var = *(int*)arg;
        return 1;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        *var += (arg ? *(int*)arg : 1) *
            (action == M_PROPERTY_STEP_DOWN ? -1 : 1);
        M_PROPERTY_CLAMP(prop,*var);
        return 1;
    }
    return m_property_int_ro(prop,action,arg,*var);
}

int m_property_choice(const m_option_t* prop,int action,
                      void* arg,int* var) {
    switch(action) {
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        *var += action == M_PROPERTY_STEP_UP ? 1 : prop->max;
        *var %= (int)prop->max+1;
        return 1;
    }
    return m_property_int_range(prop,action,arg,var);
}

int m_property_flag_ro(const m_option_t* prop,int action,
                       void* arg,int var) {
    switch(action) {
    case M_PROPERTY_PRINT:
        if(!arg) return 0;
        *(char**)arg = strdup((var > prop->min) ? MSGTR_Enabled : MSGTR_Disabled);
        return 1;
    }
    return m_property_int_ro(prop,action,arg,var);
}

int m_property_flag(const m_option_t* prop,int action,
                    void* arg,int* var) {
    switch(action) {
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        *var = *var == prop->min ? prop->max : prop->min;
        return 1;
    case M_PROPERTY_PRINT:
        return m_property_flag_ro(prop, action, arg, *var);
    }
    return m_property_int_range(prop,action,arg,var);
}

int m_property_float_ro(const m_option_t* prop,int action,
                        void* arg,float var) {
    switch(action) {
    case M_PROPERTY_GET:
        if(!arg) return 0;
        *(float*)arg = var;
        return 1;
    case M_PROPERTY_PRINT:
        if(!arg) return 0;
        *(char**)arg = malloc(20);
        sprintf(*(char**)arg,"%.2f",var);
        return 1;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

int m_property_float_range(const m_option_t* prop,int action,
                           void* arg,float* var) {
    switch(action) {
    case M_PROPERTY_SET:
        if(!arg) return 0;
        M_PROPERTY_CLAMP(prop,*(float*)arg);
        *var = *(float*)arg;
        return 1;
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        *var += (arg ? *(float*)arg : 0.1) *
            (action == M_PROPERTY_STEP_DOWN ? -1 : 1);
        M_PROPERTY_CLAMP(prop,*var);
        return 1;
    }
    return m_property_float_ro(prop,action,arg,*var);
}

int m_property_delay(const m_option_t* prop,int action,
                     void* arg,float* var) {
    switch(action) {
    case M_PROPERTY_PRINT:
        if(!arg) return 0;
        *(char**)arg = malloc(20);
        sprintf(*(char**)arg,"%d ms",ROUND((*var)*1000));
        return 1;
    default:
        return m_property_float_range(prop,action,arg,var);
    }
}

int m_property_double_ro(const m_option_t* prop,int action,
                         void* arg,double var) {
    switch(action) {
    case M_PROPERTY_GET:
        if(!arg) return 0;
        *(double*)arg = var;
        return 1;
    case M_PROPERTY_PRINT:
        if(!arg) return 0;
        *(char**)arg = malloc(20);
        sprintf(*(char**)arg,"%.2f",var);
        return 1;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

int m_property_time_ro(const m_option_t* prop,int action,
                       void* arg,double var) {
    switch(action) {
    case M_PROPERTY_PRINT:
	if (!arg)
	    return M_PROPERTY_ERROR;
	else {
	    int h, m, s = var;
	    h = s / 3600;
	    s -= h * 3600;
	    m = s / 60;
	    s -= m * 60;
	    *(char **) arg = malloc(20);
	    if (h > 0)
		sprintf(*(char **) arg, "%d:%02d:%02d", h, m, s);
	    else if (m > 0)
		sprintf(*(char **) arg, "%d:%02d", m, s);
	    else
		sprintf(*(char **) arg, "%d", s);
	    return M_PROPERTY_OK;
        }
    }
    return m_property_double_ro(prop,action,arg,var);
}

int m_property_string_ro(const m_option_t* prop,int action,void* arg,char* str) {
    switch(action) {
    case M_PROPERTY_GET:
        if(!arg) return 0;
        *(char**)arg = str;
        return 1;
    case M_PROPERTY_PRINT:
        if(!arg) return 0;
        *(char**)arg = str ? strdup(str) : NULL;
        return 1;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

int m_property_bitrate(const m_option_t* prop,int action,void* arg,int rate) {
    switch(action) {
    case M_PROPERTY_PRINT:
        if (!arg)
	    return M_PROPERTY_ERROR;
        *(char**)arg = malloc (16);
        sprintf(*(char**)arg, "%d kbps", rate*8/1000);
        return M_PROPERTY_OK;
    }
    return m_property_int_ro(prop, action, arg, rate);
}
