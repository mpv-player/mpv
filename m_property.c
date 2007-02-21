
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

int m_property_do(m_option_t* prop, int action, void* arg, void *ctx) {
    if(!prop) return M_PROPERTY_UNKNOWN;
    return ((m_property_ctrl_f)prop->p)(prop,action,arg,ctx);
}


char* m_property_print(m_option_t* prop, void *ctx) {
    m_property_ctrl_f ctrl;
    void* val;
    char* ret;
    
    if(!prop) return NULL;

    ctrl = prop->p;
    // look if the property have it's own print func
    if(ctrl(prop,M_PROPERTY_PRINT,&ret, ctx) >= 0)
        return ret;
    // fallback on the default print for this type
    val = calloc(1,prop->type->size);    
    if(ctrl(prop,M_PROPERTY_GET,val,ctx) <= 0) {
        free(val);
        return NULL;
    }
    ret = m_option_print(prop,val);
    free(val);
    return ret == (char*)-1 ? NULL : ret;
}

int m_property_parse(m_option_t* prop, char* txt, void *ctx) {
    m_property_ctrl_f ctrl;
    void* val;
    int r;
    
    if(!prop) return M_PROPERTY_UNKNOWN;

    ctrl = prop->p;
    // try the property own parsing func
    if((r = ctrl(prop,M_PROPERTY_PARSE,txt,ctx)) !=  M_PROPERTY_NOT_IMPLEMENTED)
        return r;
    // fallback on the default
    val = calloc(1,prop->type->size);
    if((r = m_option_parse(prop,prop->name,txt,val,M_CONFIG_FILE)) <= 0) {
        free(val);
        return r;
    }
    r = ctrl(prop,M_PROPERTY_SET,val,ctx);
    m_option_free(prop,val);
    free(val);
    return r;
}

char* m_properties_expand_string(m_option_t* prop_list,char* str, void *ctx) {
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
            m_option_t* prop;
            memcpy(pname,str+2,pl);
            pname[pl] = 0;
            if((prop = m_option_list_find(prop_list,pname)) &&
               (p = m_property_print(prop, ctx)))
                l = strlen(p), fr = 1;
            else
                l = 0;
            str = e+1;
        } else if(str[0] == '?' && str[1] == '(' && (e = strchr(str+2,':'))) {
            int pl = e-str-2;
            char pname[pl+1];
            m_option_t* prop;
            lvl++;
            if(!skip) {            
                memcpy(pname,str+2,pl);
                pname[pl] = 0;
                if(!(prop = m_option_list_find(prop_list,pname)) ||
                   m_property_do(prop,M_PROPERTY_GET,NULL, ctx) < 0)
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

void m_properties_print_help_list(m_option_t* list) {
    char min[50],max[50];
    int i,count = 0;
    
    mp_msg(MSGT_CFGPARSER, MSGL_INFO, MSGTR_PropertyListHeader);
    for(i = 0 ; list[i].name ; i++) {
        m_option_t* opt = &list[i];
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

int m_property_int_ro(m_option_t* prop,int action,
                      void* arg,int var) {
    switch(action) {
    case M_PROPERTY_GET:
        if(!arg) return 0;
        *(int*)arg = var;
        return 1;
    }
    return M_PROPERTY_NOT_IMPLEMENTED;
}

int m_property_int_range(m_option_t* prop,int action,
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

int m_property_choice(m_option_t* prop,int action,
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

int m_property_flag(m_option_t* prop,int action,
                    void* arg,int* var) {
    switch(action) {
    case M_PROPERTY_STEP_UP:
    case M_PROPERTY_STEP_DOWN:
        *var = *var == prop->min ? prop->max : prop->min;
        return 1;
    case M_PROPERTY_PRINT:
        if(!arg) return 0;
        *(char**)arg = strdup((*var > prop->min) ? MSGTR_Enabled : MSGTR_Disabled);
        return 1;
    }
    return m_property_int_range(prop,action,arg,var);
}

int m_property_float_ro(m_option_t* prop,int action,
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

int m_property_float_range(m_option_t* prop,int action,
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

int m_property_delay(m_option_t* prop,int action,
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

int m_property_double_ro(m_option_t* prop,int action,
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

int m_property_string_ro(m_option_t* prop,int action,void* arg,char* str) {
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
