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

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "playtree.h"
#include "playtreeparser.h"
#include "stream/stream.h"
#include "libmpdemux/demuxer.h"
#include "asxparser.h"
#include "mp_msg.h"
#include "m_config.h"

extern m_config_t* mconfig;

////// List utils

void
asx_list_free(void* list_ptr,ASX_FreeFunc free_func) {
  void** ptr = *(void***)list_ptr;
  if(ptr == NULL) return;
  if(free_func != NULL) {
    for( ; *ptr != NULL ; ptr++)
      free_func(*ptr);
  }
  free(*(void**)list_ptr);
  *(void**)list_ptr = NULL;
}

/////// Attribs utils

char*
asx_get_attrib(const char* attrib,char** attribs) {
  char** ptr;

  if(attrib == NULL || attribs == NULL) return NULL;
  for(ptr = attribs; ptr[0] != NULL; ptr += 2){
    if(strcasecmp(ptr[0],attrib) == 0)
      return strdup(ptr[1]);
  }
  return NULL;
}

int
asx_attrib_to_enum(const char* val,char** valid_vals) {
  char** ptr;
  int r = 0;

  if(valid_vals == NULL || val == NULL) return -2;
  for(ptr = valid_vals ; ptr[0] != NULL ; ptr++) {
    if(strcasecmp(val,ptr[0]) == 0) return r;
    r++;
  }

  return -1;
}

#define asx_warning_attrib_required(p,e,a) mp_msg(MSGT_PLAYTREE,MSGL_WARN,"At line %d : element %s don't have the required attribute %s",p->line,e,a)
#define asx_warning_body_parse_error(p,e) mp_msg(MSGT_PLAYTREE,MSGL_WARN,"At line %d : error while parsing %s body",p->line,e)

ASX_Parser_t*
asx_parser_new(void) {
  ASX_Parser_t* parser = calloc(1,sizeof(ASX_Parser_t));
  return parser;
}

void
asx_parser_free(ASX_Parser_t* parser) {
  if(!parser) return;
  if(parser->ret_stack) free(parser->ret_stack);
  free(parser);

}

#define LETTER "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define SPACE " \n\t\r"

int
asx_parse_attribs(ASX_Parser_t* parser,char* buffer,char*** _attribs) {
  char *ptr1, *ptr2, *ptr3;
  int n_attrib = 0;
  char **attribs = NULL;
  char *attrib, *val;

  ptr1 = buffer;
  while(1) {
    for( ; strchr(SPACE,*ptr1) != NULL; ptr1++) { // Skip space
      if(*ptr1 == '\0') break;
    }
    ptr3 = strchr(ptr1,'=');
    if(ptr3 == NULL) break;
    for(ptr2 = ptr3-1; strchr(SPACE,*ptr2) != NULL; ptr2--) {
      if (ptr2 == ptr1) {
        mp_msg(MSGT_PLAYTREE,MSGL_ERR,"At line %d : this should never append, back to attribute begin while skipping end space",parser->line);
        break;
      }
    }
    attrib = malloc(ptr2-ptr1+2);
    strncpy(attrib,ptr1,ptr2-ptr1+1);
    attrib[ptr2-ptr1+1] = '\0';

    ptr1 = strchr(ptr3,'"');
    if(ptr1 == NULL || ptr1[1] == '\0') ptr1 = strchr(ptr3,'\'');
    if(ptr1 == NULL || ptr1[1] == '\0') {
      mp_msg(MSGT_PLAYTREE,MSGL_WARN,"At line %d : can't find attribute %s value",parser->line,attrib);
      free(attrib);
      break;
    }
    ptr2 = strchr(ptr1+1,ptr1[0]);
    if (ptr2 == NULL) {
      mp_msg(MSGT_PLAYTREE,MSGL_WARN,"At line %d : value of attribute %s isn't finished",parser->line,attrib);
      free(attrib);
      break;
    }
    ptr1++;
    val = malloc(ptr2-ptr1+1);
    strncpy(val,ptr1,ptr2-ptr1);
    val[ptr2-ptr1] = '\0';
    n_attrib++;

    attribs = realloc(attribs, (2 * n_attrib + 1) * sizeof(char*));
    attribs[n_attrib*2-2] = attrib;
    attribs[n_attrib*2-1] = val;

    ptr1 = ptr2+1;
  }

  if(n_attrib > 0)
    attribs[n_attrib*2] = NULL;

  *_attribs = attribs;

  return n_attrib;
}

/*
 * Return -1 on error, 0 when nothing is found, 1 on sucess
 */
int
asx_get_element(ASX_Parser_t* parser,char** _buffer,
                char** _element,char** _body,char*** _attribs) {
  char *ptr1,*ptr2, *ptr3, *ptr4;
  char *attribs = NULL;
  char *element = NULL, *body = NULL, *ret = NULL, *buffer;
  int n_attrib = 0;
  int body_line = 0,attrib_line,ret_line,in = 0;
  int quotes = 0;

  if(_buffer == NULL || _element == NULL || _body == NULL || _attribs == NULL) {
    mp_msg(MSGT_PLAYTREE,MSGL_ERR,"At line %d : asx_get_element called with invalid value",parser->line);
    return -1;
  }

  *_body = *_element = NULL;
  *_attribs =  NULL;
  buffer = *_buffer;

  if(buffer == NULL) return 0;

  if(parser->ret_stack && /*parser->last_body && */buffer != parser->last_body) {
    ASX_LineSave_t* ls = parser->ret_stack;
    int i;
    for(i = 0 ; i < parser->ret_stack_size ; i++) {
      if(buffer == ls[i].buffer) {
        parser->line = ls[i].line;
        break;
      }

    }
    if( i < parser->ret_stack_size) {
      i++;
      if( i < parser->ret_stack_size)
        memmove(parser->ret_stack,parser->ret_stack+i, (parser->ret_stack_size - i)*sizeof(ASX_LineSave_t));
      parser->ret_stack_size -= i;
      if(parser->ret_stack_size > 0)
        parser->ret_stack = realloc(parser->ret_stack,parser->ret_stack_size*sizeof(ASX_LineSave_t));
      else {
        free(parser->ret_stack);
        parser->ret_stack = NULL;
      }
    }
  }

  ptr1 = buffer;
  while(1) {
    for( ; ptr1[0] != '<' ; ptr1++) {
      if(ptr1[0] == '\0') {
        ptr1 = NULL;
        break;
      }
      if(ptr1[0] == '\n') parser->line++;
    }
    //ptr1 = strchr(ptr1,'<');
    if(!ptr1 || ptr1[1] == '\0') return 0; // Nothing found

    if(strncmp(ptr1,"<!--",4) == 0) { // Comments
      for( ; strncmp(ptr1,"-->",3) != 0 ; ptr1++) {
        if(ptr1[0] == '\0') {
          ptr1 = NULL;
          break;
        }
        if(ptr1[0] == '\n') parser->line++;
      }
      //ptr1 = strstr(ptr1,"-->");
      if(!ptr1) {
        mp_msg(MSGT_PLAYTREE,MSGL_ERR,"At line %d : unfinished comment",parser->line);
        return -1;
      }
    } else {
      break;
    }
  }

  // Is this space skip very useful ??
  for(ptr1++; strchr(SPACE,ptr1[0]) != NULL; ptr1++) { // Skip space
    if(ptr1[0] == '\0') {
      mp_msg(MSGT_PLAYTREE,MSGL_ERR,"At line %d : EOB reached while parsing element start",parser->line);
      return -1;
    }
    if(ptr1[0] == '\n') parser->line++;
  }

  for(ptr2 = ptr1; strchr(LETTER,*ptr2) != NULL;ptr2++) { // Go to end of name
    if(*ptr2 == '\0'){
      mp_msg(MSGT_PLAYTREE,MSGL_ERR,"At line %d : EOB reached while parsing element start",parser->line);
      return -1;
    }
    if(ptr2[0] == '\n') parser->line++;
  }

  element = malloc(ptr2-ptr1+1);
  strncpy(element,ptr1,ptr2-ptr1);
  element[ptr2-ptr1] = '\0';

  for( ; strchr(SPACE,*ptr2) != NULL; ptr2++) { // Skip space
    if(ptr2[0] == '\0') {
      mp_msg(MSGT_PLAYTREE,MSGL_ERR,"At line %d : EOB reached while parsing element start",parser->line);
      free(element);
      return -1;
    }
    if(ptr2[0] == '\n') parser->line++;
  }
  attrib_line = parser->line;



  for(ptr3 = ptr2; ptr3[0] != '\0'; ptr3++) { // Go to element end
    if(ptr3[0] == '"') quotes ^= 1;
    if(!quotes && (ptr3[0] == '>' || strncmp(ptr3,"/>",2) == 0))
      break;
    if(ptr3[0] == '\n') parser->line++;
  }
  if(ptr3[0] == '\0' || ptr3[1] == '\0') { // End of file
    mp_msg(MSGT_PLAYTREE,MSGL_ERR,"At line %d : EOB reached while parsing element start",parser->line);
    free(element);
    return -1;
  }

  // Save attribs string
  if(ptr3-ptr2 > 0) {
    attribs = malloc(ptr3-ptr2+1);
    strncpy(attribs,ptr2,ptr3-ptr2);
    attribs[ptr3-ptr2] = '\0';
  }
  //bs_line = parser->line;
  if(ptr3[0] != '/') { // Not Self closed element
    ptr3++;
    for( ; strchr(SPACE,*ptr3) != NULL; ptr3++) { // Skip space on body begin
      if(*ptr3 == '\0') {
        mp_msg(MSGT_PLAYTREE,MSGL_ERR,"At line %d : EOB reached while parsing %s element body",parser->line,element);
        free(element);
        if(attribs) free(attribs);
        return -1;
      }
      if(ptr3[0] == '\n') parser->line++;
    }
    ptr4 = ptr3;
    body_line = parser->line;
    while(1) { // Find closing element
      for( ; ptr4[0] != '<' ; ptr4++) {
        if(ptr4[0] == '\0') {
          ptr4 = NULL;
          break;
        }
        if(ptr4[0] == '\n') parser->line++;
      }
      if(ptr4 && strncmp(ptr4,"<!--",4) == 0) { // Comments
        for( ; strncmp(ptr4,"-->",3) != 0 ; ptr4++) {
        if(ptr4[0] == '\0') {
          ptr4 = NULL;
          break;
        }
        if(ptr1[0] == '\n') parser->line++;
        }
        continue;
      }
      if(ptr4 == NULL || ptr4[1] == '\0') {
        mp_msg(MSGT_PLAYTREE,MSGL_ERR,"At line %d : EOB reached while parsing %s element body",parser->line,element);
        free(element);
        if(attribs) free(attribs);
        return -1;
      }
      if(ptr4[1] != '/' && strncasecmp(element,ptr4+1,strlen(element)) == 0) {
        in++;
        ptr4+=2;
        continue;
      } else if(strncasecmp(element,ptr4+2,strlen(element)) == 0) { // Extract body
        if(in > 0) {
          in--;
          ptr4 += 2+strlen(element);
          continue;
        }
        ret = ptr4+strlen(element)+3;
        if(ptr4 != ptr3) {
          ptr4--;
          for( ; ptr4 != ptr3 && strchr(SPACE,*ptr4) != NULL; ptr4--) ;// Skip space on body end
          //        if(ptr4[0] == '\0') parser->line--;
          //}
          ptr4++;
          body = malloc(ptr4-ptr3+1);
          strncpy(body,ptr3,ptr4-ptr3);
          body[ptr4-ptr3] = '\0';
        }
        break;
      } else {
        ptr4 += 2;
      }
    }
  } else {
    ret = ptr3 + 2; // 2 is for />
  }

  for( ; ret[0] != '\0' && strchr(SPACE,ret[0]) != NULL; ret++) { // Skip space
    if(ret[0] == '\n') parser->line++;
  }

  ret_line = parser->line;

  if(attribs) {
    parser->line = attrib_line;
    n_attrib = asx_parse_attribs(parser,attribs,_attribs);
    free(attribs);
    if(n_attrib < 0) {
      mp_msg(MSGT_PLAYTREE,MSGL_WARN,"At line %d : error while parsing element %s attributes",parser->line,element);
      free(element);
      free(body);
      return -1;
    }
  } else
    *_attribs = NULL;

  *_element = element;
  *_body = body;

  parser->last_body = body;
  parser->ret_stack_size++;
  parser->ret_stack = realloc(parser->ret_stack,parser->ret_stack_size*sizeof(ASX_LineSave_t));
  if(parser->ret_stack_size > 1)
    memmove(parser->ret_stack+1,parser->ret_stack,(parser->ret_stack_size-1)*sizeof(ASX_LineSave_t));
  parser->ret_stack[0].buffer = ret;
  parser->ret_stack[0].line = ret_line;
  parser->line = body ? body_line : ret_line;

  *_buffer = ret;
  return 1;

}

static void
asx_parse_param(ASX_Parser_t* parser, char** attribs, play_tree_t* pt) {
  char *name,*val;

  name = asx_get_attrib("NAME",attribs);
  if(!name) {
    asx_warning_attrib_required(parser,"PARAM" ,"NAME" );
    return;
  }
  val = asx_get_attrib("VALUE",attribs);
  if(m_config_get_option(mconfig,name) == NULL) {
    mp_msg(MSGT_PLAYTREE,MSGL_WARN,"Found unknown param in asx: %s",name);
    if(val)
      mp_msg(MSGT_PLAYTREE,MSGL_WARN,"=%s\n",val);
    else
      mp_msg(MSGT_PLAYTREE,MSGL_WARN,"\n");
    return;
  }
  play_tree_set_param(pt,name,val);
  free(name);
  if(val) free(val);
}

static void
asx_parse_ref(ASX_Parser_t* parser, char** attribs, play_tree_t* pt) {
  char *href;

  href = asx_get_attrib("HREF",attribs);
  if(href == NULL) {
    asx_warning_attrib_required(parser,"REF" ,"HREF" );
    return;
  }
#if 0
  // replace http my mmshttp to avoid infinite loops
  // disabled since some playlists for e.g. WinAMP use asx as well
  // "-user-agent NSPlayer/4.1.0.3856" is a possible workaround
  if (strncmp(href, "http://", 7) == 0) {
    char *newref = malloc(3 + strlen(href) + 1);
    strcpy(newref, "mms");
    strcpy(newref + 3, href);
    free(href);
    href = newref;
  }
#endif

  play_tree_add_file(pt,href);

  mp_msg(MSGT_PLAYTREE,MSGL_V,"Adding file %s to element entry\n",href);

  free(href);

}

static play_tree_t*
asx_parse_entryref(ASX_Parser_t* parser,char* buffer,char** _attribs) {
  play_tree_t* pt;
  char *href;
  stream_t* stream;
  play_tree_parser_t* ptp;
  int f=DEMUXER_TYPE_UNKNOWN;

  if(parser->deep > 0)
    return NULL;

  href = asx_get_attrib("HREF",_attribs);
  if(href == NULL) {
    asx_warning_attrib_required(parser,"ENTRYREF" ,"HREF" );
    return NULL;
  }
  stream=open_stream(href,0,&f);
  if(!stream) {
    mp_msg(MSGT_PLAYTREE,MSGL_WARN,"Can't open playlist %s\n",href);
    free(href);
    return NULL;
  }

  mp_msg(MSGT_PLAYTREE,MSGL_V,"Adding playlist %s to element entryref\n",href);

  ptp = play_tree_parser_new(stream,parser->deep+1);

  pt = play_tree_parser_get_play_tree(ptp, 1);

  play_tree_parser_free(ptp);
  free_stream(stream);
  free(href);
  //mp_msg(MSGT_PLAYTREE,MSGL_INFO,"Need to implement entryref\n");

  return pt;
}

static play_tree_t*
asx_parse_entry(ASX_Parser_t* parser,char* buffer,char** _attribs) {
  char *element,*body,**attribs;
  int r,nref=0;
  play_tree_t *ref;

  ref = play_tree_new();

  while(buffer && buffer[0] != '\0') {
    r = asx_get_element(parser,&buffer,&element,&body,&attribs);
    if(r < 0) {
      asx_warning_body_parse_error(parser,"ENTRY");
      return NULL;
    } else if (r == 0) { // No more element
      break;
    }
    if(strcasecmp(element,"REF") == 0) {
      asx_parse_ref(parser,attribs,ref);
      mp_msg(MSGT_PLAYTREE,MSGL_DBG2,"Adding element %s to entry\n",element);
      nref++;
    } else
      mp_msg(MSGT_PLAYTREE,MSGL_DBG2,"Ignoring element %s\n",element);
    if(body) free(body);
    asx_free_attribs(attribs);
  }

  if(nref <= 0) {
    play_tree_free(ref,1);
    return NULL;
  }
  return ref;

}


static play_tree_t*
asx_parse_repeat(ASX_Parser_t* parser,char* buffer,char** _attribs) {
  char *element,*body,**attribs;
  play_tree_t *repeat, *list=NULL, *entry;
  char* count;
  int r;

  repeat = play_tree_new();

  count = asx_get_attrib("COUNT",_attribs);
  if(count == NULL) {
    mp_msg(MSGT_PLAYTREE,MSGL_DBG2,"Setting element repeat loop to infinit\n");
    repeat->loop = -1; // Infinit
  } else {
    repeat->loop = atoi(count);
    free(count);
    if(repeat->loop == 0) repeat->loop = 1;
    mp_msg(MSGT_PLAYTREE,MSGL_DBG2,"Setting element repeat loop to %d\n",repeat->loop);
  }

  while(buffer && buffer[0] != '\0') {
    r = asx_get_element(parser,&buffer,&element,&body,&attribs);
    if(r < 0) {
      asx_warning_body_parse_error(parser,"REPEAT");
      return NULL;
    } else if (r == 0) { // No more element
      break;
    }
    if(strcasecmp(element,"ENTRY") == 0) {
       entry = asx_parse_entry(parser,body,attribs);
       if(entry) {
         if(!list) list =  entry;
         else play_tree_append_entry(list,entry);
         mp_msg(MSGT_PLAYTREE,MSGL_DBG2,"Adding element %s to repeat\n",element);
       }
    } else if(strcasecmp(element,"ENTRYREF") == 0) {
       entry = asx_parse_entryref(parser,body,attribs);
       if(entry) {
         if(!list) list =  entry;
         else play_tree_append_entry(list,entry);
         mp_msg(MSGT_PLAYTREE,MSGL_DBG2,"Adding element %s to repeat\n",element);
       }
     } else if(strcasecmp(element,"REPEAT") == 0) {
       entry = asx_parse_repeat(parser,body,attribs);
       if(entry) {
         if(!list) list =  entry;
         else play_tree_append_entry(list,entry);
         mp_msg(MSGT_PLAYTREE,MSGL_DBG2,"Adding element %s to repeat\n",element);
       }
     } else if(strcasecmp(element,"PARAM") == 0) {
       asx_parse_param(parser,attribs,repeat);
     } else
       mp_msg(MSGT_PLAYTREE,MSGL_DBG2,"Ignoring element %s\n",element);
    if(body) free(body);
     asx_free_attribs(attribs);
  }

  if(!list) {
    play_tree_free(repeat,1);
    return NULL;
  }
  play_tree_set_child(repeat,list);

  return repeat;

}



play_tree_t*
asx_parser_build_tree(char* buffer,int deep) {
  char *element,*asx_body,**asx_attribs,*body = NULL, **attribs;
  int r;
  play_tree_t *asx,*entry,*list = NULL;
  ASX_Parser_t* parser = asx_parser_new();

  parser->line = 1;
  parser->deep = deep;

   r = asx_get_element(parser,&buffer,&element,&asx_body,&asx_attribs);
  if(r < 0) {
    mp_msg(MSGT_PLAYTREE,MSGL_ERR,"At line %d : Syntax error ???",parser->line);
    asx_parser_free(parser);
    return NULL;
  } else if(r == 0) { // No contents
    mp_msg(MSGT_PLAYTREE,MSGL_ERR,"empty asx element");
    asx_parser_free(parser);
    return NULL;
  }

  if(strcasecmp(element,"ASX") != 0) {
    mp_msg(MSGT_PLAYTREE,MSGL_ERR,"first element isn't ASX, it's %s\n",element);
    asx_free_attribs(asx_attribs);
    asx_parser_free(parser);
    return NULL;
  }

  if(!asx_body) {
    mp_msg(MSGT_PLAYTREE,MSGL_ERR,"ASX element is empty");
    asx_free_attribs(asx_attribs);
    asx_parser_free(parser);
    return NULL;
  }

  asx = play_tree_new();
  buffer = asx_body;
  while(buffer && buffer[0] != '\0') {
    r = asx_get_element(parser,&buffer,&element,&body,&attribs);
     if(r < 0) {
       asx_warning_body_parse_error(parser,"ASX");
       asx_parser_free(parser);
       return NULL;
     } else if (r == 0) { // No more element
       break;
     }
     if(strcasecmp(element,"ENTRY") == 0) {
       entry = asx_parse_entry(parser,body,attribs);
       if(entry) {
         if(!list) list =  entry;
         else play_tree_append_entry(list,entry);
         mp_msg(MSGT_PLAYTREE,MSGL_DBG2,"Adding element %s to asx\n",element);
       }
     } else if(strcasecmp(element,"ENTRYREF") == 0) {
       entry = asx_parse_entryref(parser,body,attribs);
       if(entry) {
         if(!list) list =  entry;
         else play_tree_append_entry(list,entry);
         mp_msg(MSGT_PLAYTREE,MSGL_DBG2,"Adding element %s to asx\n",element);
       }
     } else if(strcasecmp(element,"REPEAT") == 0) {
       entry = asx_parse_repeat(parser,body,attribs);
       if(entry) {
         if(!list) list =  entry;
         else play_tree_append_entry(list,entry);
         mp_msg(MSGT_PLAYTREE,MSGL_DBG2,"Adding element %s to asx\n",element);
       }
     } else
       mp_msg(MSGT_PLAYTREE,MSGL_DBG2,"Ignoring element %s\n",element);
     if(body) free(body);
     asx_free_attribs(attribs);
  }

  free(asx_body);
  asx_free_attribs(asx_attribs);
  asx_parser_free(parser);


  if(!list) {
    play_tree_free(asx,1);

    return NULL;
  }

  play_tree_set_child(asx,list);

  return asx;
}
