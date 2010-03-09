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
/// \ingroup OptionsStruct

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "m_option.h"
#include "m_struct.h"
#include "mp_msg.h"

const m_option_t*
m_struct_get_field(const m_struct_t* st,const char* f) {
  int i;

  for(i = 0 ; st->fields[i].name ; i++) {
    if(strcasecmp(st->fields[i].name,f) == 0)
      return &st->fields[i];
  }
  return NULL;
}

void*
m_struct_alloc(const m_struct_t* st) {
  int i;
  void* r;

  if(!st->defaults) {
    mp_msg(MSGT_CFGPARSER, MSGL_ERR,"Struct %s needs defaults\n",st->name);
    return NULL;
  }
  // Check the struct fields
  for(i = 0 ; st->fields[i].name ; i++) {
    if(st->fields[i].type->flags & M_OPT_TYPE_INDIRECT) {
      mp_msg(MSGT_CFGPARSER, MSGL_ERR,"Struct %s->%s: Option types with the indirect flag are forbidden.\n",st->name,st->fields[i].name);
      return NULL;
    }
  }

  r = calloc(1,st->size);
  memcpy(r,st->defaults,st->size);

  for(i = 0 ; st->fields[i].name ; i++) {
    if(st->fields[i].type->flags & M_OPT_TYPE_DYNAMIC)
      memset(M_ST_MB_P(r,st->fields[i].p),0,st->fields[i].type->size);
    m_option_copy(&st->fields[i],M_ST_MB_P(r,st->fields[i].p),M_ST_MB_P(st->defaults,st->fields[i].p));
  }
  return r;
}

int
m_struct_set(const m_struct_t* st, void* obj, const char* field, const char* param) {
  const m_option_t* f = m_struct_get_field(st,field);

  if(!f) {
    mp_msg(MSGT_CFGPARSER, MSGL_ERR,"Struct %s doesn't have any %s field\n",
	   st->name,field);
    return 0;
  }

  if(f->type->parse(f,field,param,M_ST_MB_P(obj,f->p),M_CONFIG_FILE) < 0) {
    mp_msg(MSGT_CFGPARSER, MSGL_ERR,"Struct %s, field %s parsing error: %s\n",
	   st->name,field,param);
    return 0;
  }

  return 1;
}

void
m_struct_reset(const m_struct_t* st, void* obj, const char* field) {
  const m_option_t* f;

  if(!field) { // Reset all options
    int i;
    for(i = 0 ; st->fields[i].name ; i++)
      m_option_copy(&st->fields[i],M_ST_MB_P(obj,st->fields[i].p),M_ST_MB_P(st->defaults,st->fields[i].p));
    return;
  }

  // Only one
  f = m_struct_get_field(st,field);
  if(!f) {
    mp_msg(MSGT_CFGPARSER, MSGL_ERR,"Struct %s doesn't have any %s field\n",
	   st->name,field);
    return;
  }
  m_option_copy(f,M_ST_MB_P(obj,f->p),M_ST_MB_P(st->defaults,f->p));
}

/// Free an allocated struct
void
m_struct_free(const m_struct_t* st, void* obj) {
  int i;

  for(i = 0 ; st->fields[i].name ; i++)
    m_option_free(&st->fields[i],M_ST_MB_P(obj,st->fields[i].p));
  free(obj);
}

void*
m_struct_copy(const m_struct_t* st, void* obj) {
  void* r = malloc(st->size);
  int i;

  memcpy(r,obj,st->size);
  for(i = 0 ; st->fields[i].name ; i++) {
    if(st->fields[i].type->flags & M_OPT_TYPE_DYNAMIC)
      memset(M_ST_MB_P(r,st->fields[i].p),0,st->fields[i].type->size);
    m_option_copy(&st->fields[i],M_ST_MB_P(r,st->fields[i].p),M_ST_MB_P(obj,st->fields[i].p));
  }

  return r;
}
