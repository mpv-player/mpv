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
/// \ingroup PlaytreeParser

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <limits.h>
#include "asxparser.h"
#include "m_config.h"
#include "playtree.h"
#include "playtreeparser.h"
#include "stream/stream.h"
#include "libmpdemux/demuxer.h"
#include "mp_msg.h"


#define BUF_STEP 1024

#define WHITES " \n\r\t"

static void
strstrip(char* str) {
  char* i;

  if (str==NULL)
    return;
  for(i = str ; i[0] != '\0' && strchr(WHITES,i[0]) != NULL; i++)
    /* NOTHING */;
  if(i[0] != '\0') {
    memmove(str,i,strlen(i) + 1);
    for(i = str + strlen(str) - 1 ; strchr(WHITES,i[0]) != NULL; i--)
      /* NOTHING */;
    i[1] = '\0';
  } else
    str[0] = '\0';
}

static char*
play_tree_parser_get_line(play_tree_parser_t* p) {
  char *end,*line_end;
  int r,resize = 0;

  if(p->buffer == NULL) {
    p->buffer = malloc(BUF_STEP);
    p->buffer_size = BUF_STEP;
    p->buffer[0] = 0;
    p->iter = p->buffer;
  }

  if(p->stream->eof && (p->buffer_end == 0 || p->iter[0] == '\0'))
    return NULL;

  assert(p->buffer_end < p->buffer_size);
  assert(!p->buffer[p->buffer_end]);
  while(1) {

    if(resize) {
      char *tmp;
      r = p->iter - p->buffer;
      end = p->buffer + p->buffer_end;
      if (p->buffer_size > INT_MAX - BUF_STEP)
        break;
      tmp = realloc(p->buffer, p->buffer_size + BUF_STEP);
      if (!tmp)
        break;
      p->buffer = tmp;
      p->iter = p->buffer + r;
      p->buffer_size += BUF_STEP;
      resize = 0;
    }

    if(p->buffer_size - p->buffer_end > 1 && ! p->stream->eof) {
      r = stream_read(p->stream,p->buffer + p->buffer_end,p->buffer_size - p->buffer_end - 1);
      if(r > 0) {
	p->buffer_end += r;
	assert(p->buffer_end < p->buffer_size);
	p->buffer[p->buffer_end] = '\0';
	while(strlen(p->buffer + p->buffer_end - r) != r)
	  p->buffer[p->buffer_end - r + strlen(p->buffer + p->buffer_end - r)] = '\n';
      }
      assert(!p->buffer[p->buffer_end]);
    }

    end = strchr(p->iter,'\n');
    if(!end) {
      if(p->stream->eof) {
	end = p->buffer + p->buffer_end;
	break;
      }
      resize = 1;
      continue;
    }
    break;
  }

  line_end = (end > p->iter && *(end-1) == '\r') ? end-1 : end;
  if(line_end - p->iter >= 0)
    p->line = realloc(p->line, line_end - p->iter + 1);
  else
    return NULL;
  if(line_end - p->iter > 0)
    strncpy(p->line,p->iter,line_end - p->iter);
  p->line[line_end - p->iter] = '\0';
  if(end[0] != '\0')
    end++;

  if(!p->keep) {
    if(end[0] != '\0') {
      p->buffer_end -= end-p->iter;
      memmove(p->buffer,end,p->buffer_end);
    } else
      p->buffer_end = 0;
    p->buffer[p->buffer_end] = '\0';
    p->iter = p->buffer;
  } else
    p->iter = end;

  return p->line;
}

static void
play_tree_parser_reset(play_tree_parser_t* p) {
  p->iter = p->buffer;
}

static void
play_tree_parser_stop_keeping(play_tree_parser_t* p) {
  p->keep = 0;
  if(p->iter && p->iter != p->buffer) {
    p->buffer_end -= p->iter -p->buffer;
    if(p->buffer_end)
      memmove(p->buffer,p->iter,p->buffer_end);
    p->buffer[p->buffer_end] = 0;
    p->iter = p->buffer;
  }
}


static play_tree_t*
parse_asx(play_tree_parser_t* p) {
  int comments = 0,get_line = 1;
  char* line = NULL;

  mp_msg(MSGT_PLAYTREE,MSGL_V,"Trying asx...\n");

  while(1) {
    if(get_line) {
      line = play_tree_parser_get_line(p);
      if(!line)
	return NULL;
      strstrip(line);
      if(line[0] == '\0')
	continue;
    }
    if(!comments) {
      if(line[0] != '<') {
	mp_msg(MSGT_PLAYTREE,MSGL_DBG2,"First char isn't '<' but '%c'\n",line[0]);
	mp_msg(MSGT_PLAYTREE,MSGL_DBG3,"Buffer = [%s]\n",p->buffer);
	return NULL;
      } else if(strncmp(line,"<!--",4) == 0) { // Comments
	comments = 1;
	line += 4;
	if(line[0] != '\0' && strlen(line) > 0)
	  get_line = 0;
      } else if(strncasecmp(line,"<ASX",4) == 0) // We got an asx element
	break;
      else // We don't get an asx
	return NULL;
    } else { // Comments
      char* c;
      c = strchr(line,'-');
      if(c) {
	if (strncmp(c,"--!>",4) == 0) { // End of comments
	  comments = 0;
	  line = c+4;
	  if(line[0] != '\0') // There is some more data on this line : keep it
	    get_line = 0;

	} else {
	  line = c+1; // Jump the -
	  if(line[0] != '\0') // Some more data
	    get_line = 0;
	  else  // End of line
	    get_line = 1;
	}
      } else // No - on this line (or rest of line) : get next one
	get_line = 1;
    }
  }

  mp_msg(MSGT_PLAYTREE,MSGL_V,"Detected asx format\n");

  // We have an asx : load it in memory and parse

  while((line = play_tree_parser_get_line(p)) != NULL)
    /* NOTHING */;

 mp_msg(MSGT_PLAYTREE,MSGL_DBG3,"Parsing asx file: [%s]\n",p->buffer);
 return asx_parser_build_tree(p->buffer,p->deep);
}

static char*
pls_entry_get_value(char* line) {
  char* i;

  i = strchr(line,'=');
  if(!i || i[1] == '\0')
    return NULL;
  else
    return i+1;
}

typedef struct pls_entry {
  char* file;
  char* title;
  char* length;
} pls_entry_t;

static int
pls_read_entry(char* line,pls_entry_t** _e,int* _max_entry,char** val) {
  int num,max_entry = (*_max_entry);
  pls_entry_t* e = (*_e);
  int limit = INT_MAX / sizeof(*e);
  char* v;

  v = pls_entry_get_value(line);
  if(!v) {
    mp_msg(MSGT_PLAYTREE,MSGL_ERR,"No value in entry %s\n",line);
    return -1;
  }

  num = atoi(line);
  if(num <= 0 || num > limit) {
    if (max_entry >= limit) {
        mp_msg(MSGT_PLAYTREE, MSGL_WARN, "Too many index entries\n");
        return -1;
    }
    num = max_entry+1;
    mp_msg(MSGT_PLAYTREE,MSGL_WARN,"No or invalid entry index in entry %s\nAssuming %d\n",line,num);
  }
  if(num > max_entry) {
    e = realloc(e, num * sizeof(pls_entry_t));
    if (!e)
      return -1;
    memset(&e[max_entry],0,(num-max_entry)*sizeof(pls_entry_t));
    max_entry = num;
  }
  (*_e) = e;
  (*_max_entry) = max_entry;
  (*val) = v;

  return num;
}


static play_tree_t*
parse_pls(play_tree_parser_t* p) {
  char *line,*v;
  pls_entry_t* entries = NULL;
  int n_entries = 0,max_entry=0,num;
  play_tree_t *list = NULL, *entry = NULL, *last_entry = NULL;

  mp_msg(MSGT_PLAYTREE,MSGL_V,"Trying Winamp playlist...\n");
  while((line = play_tree_parser_get_line(p))) {
    strstrip(line);
    if(strlen(line))
      break;
  }
  if (!line)
    return NULL;
  if(strcasecmp(line,"[playlist]"))
    return NULL;
  mp_msg(MSGT_PLAYTREE,MSGL_V,"Detected Winamp playlist format\n");
  play_tree_parser_stop_keeping(p);
  line = play_tree_parser_get_line(p);
  if(!line)
    return NULL;
  strstrip(line);
  if(strncasecmp(line,"NumberOfEntries",15) == 0) {
    v = pls_entry_get_value(line);
    n_entries = atoi(v);
    if(n_entries < 0)
      mp_msg(MSGT_PLAYTREE,MSGL_DBG2,"Invalid number of entries: very funny!!!\n");
    else
      mp_msg(MSGT_PLAYTREE,MSGL_DBG2,"Playlist claims to have %d entries. Let's see.\n",n_entries);
    line = play_tree_parser_get_line(p);
  }

  while(line) {
    strstrip(line);
    if(line[0] == '\0') {
      line = play_tree_parser_get_line(p);
      continue;
    }
    if(strncasecmp(line,"File",4) == 0) {
      num = pls_read_entry(line+4,&entries,&max_entry,&v);
      if(num < 0)
	mp_msg(MSGT_PLAYTREE,MSGL_ERR,"No value in entry %s\n",line);
      else
	entries[num-1].file = strdup(v);
    } else if(strncasecmp(line,"Title",5) == 0) {
      num = pls_read_entry(line+5,&entries,&max_entry,&v);
      if(num < 0)
	mp_msg(MSGT_PLAYTREE,MSGL_ERR,"No value in entry %s\n",line);
      else
	entries[num-1].title = strdup(v);
    } else if(strncasecmp(line,"Length",6) == 0) {
      num = pls_read_entry(line+6,&entries,&max_entry,&v);
      if(num < 0)
	mp_msg(MSGT_PLAYTREE,MSGL_ERR,"No value in entry %s\n",line);
      else
	entries[num-1].length = strdup(v);
    } else
      mp_msg(MSGT_PLAYTREE,MSGL_WARN,"Unknown entry type %s\n",line);
    line = play_tree_parser_get_line(p);
  }

  for(num = 0; num < max_entry ; num++) {
    if(entries[num].file == NULL)
      mp_msg(MSGT_PLAYTREE,MSGL_ERR,"Entry %d don't have a file !!!!\n",num+1);
    else {
      mp_msg(MSGT_PLAYTREE,MSGL_DBG2,"Adding entry %s\n",entries[num].file);
      entry = play_tree_new();
      play_tree_add_file(entry,entries[num].file);
      free(entries[num].file);
      if(list)
	play_tree_append_entry(last_entry,entry);
      else
	list = entry;
      last_entry = entry;
    }
    if(entries[num].title) {
      // When we have info in playtree we add this info
      free(entries[num].title);
    }
    if(entries[num].length) {
      // When we have info in playtree we add this info
      free(entries[num].length);
    }
  }

  free(entries);

  entry = play_tree_new();
  play_tree_set_child(entry,list);
  return entry;
}

/*
 Reference Ini-Format: Each entry is assumed a reference
 */
static play_tree_t*
parse_ref_ini(play_tree_parser_t* p) {
  char *line,*v;
  play_tree_t *list = NULL, *entry = NULL, *last_entry = NULL;

  mp_msg(MSGT_PLAYTREE,MSGL_V,"Trying reference-ini playlist...\n");
  if (!(line = play_tree_parser_get_line(p)))
    return NULL;
  strstrip(line);
  if(strcasecmp(line,"[Reference]"))
    return NULL;
  mp_msg(MSGT_PLAYTREE,MSGL_V,"Detected reference-ini playlist format\n");
  play_tree_parser_stop_keeping(p);
  line = play_tree_parser_get_line(p);
  if(!line)
    return NULL;
  while(line) {
    strstrip(line);
    if(strncasecmp(line,"Ref",3) == 0) {
      v = pls_entry_get_value(line+3);
      if(!v)
	mp_msg(MSGT_PLAYTREE,MSGL_ERR,"No value in entry %s\n",line);
      else
      {
        mp_msg(MSGT_PLAYTREE,MSGL_DBG2,"Adding entry %s\n",v);
        entry = play_tree_new();
        play_tree_add_file(entry,v);
        if(list)
  	  play_tree_append_entry(last_entry,entry);
        else
  	  list = entry;
	last_entry = entry;
      }
    }
    line = play_tree_parser_get_line(p);
  }

  if(!list) return NULL;
  entry = play_tree_new();
  play_tree_set_child(entry,list);
  return entry;
}

static play_tree_t*
parse_m3u(play_tree_parser_t* p) {
  char* line;
  play_tree_t *list = NULL, *entry = NULL, *last_entry = NULL;

  mp_msg(MSGT_PLAYTREE,MSGL_V,"Trying extended m3u playlist...\n");
  if (!(line = play_tree_parser_get_line(p)))
    return NULL;
  strstrip(line);
  if(strcasecmp(line,"#EXTM3U"))
    return NULL;
  mp_msg(MSGT_PLAYTREE,MSGL_V,"Detected extended m3u playlist format\n");
  play_tree_parser_stop_keeping(p);

  while((line = play_tree_parser_get_line(p)) != NULL) {
    strstrip(line);
    if(line[0] == '\0')
      continue;
    /* EXTM3U files contain such lines:
     * #EXTINF:<seconds>, <title>
     * followed by a line with the filename
     * for now we have no place to put that
     * so we just skip that extra-info ::atmos
     */
    if(line[0] == '#') {
#if 0 /* code functional */
      if(strncasecmp(line,"#EXTINF:",8) == 0) {
        mp_msg(MSGT_PLAYTREE,MSGL_INFO,"[M3U] Duration: %dsec  Title: %s\n",
          strtol(line+8,&line,10), line+2);
      }
#endif
      continue;
    }
    entry = play_tree_new();
    play_tree_add_file(entry,line);
    if(!list)
      list = entry;
    else
      play_tree_append_entry(last_entry,entry);
    last_entry = entry;
  }

  if(!list) return NULL;
  entry = play_tree_new();
  play_tree_set_child(entry,list);
  return entry;
}

static play_tree_t*
parse_smil(play_tree_parser_t* p) {
  int entrymode=0;
  char* line,source[512],*pos,*s_start,*s_end,*src_line;
  play_tree_t *list = NULL, *entry = NULL, *last_entry = NULL;
  int is_rmsmil = 0;
  unsigned int npkt, ttlpkt;

  mp_msg(MSGT_PLAYTREE,MSGL_V,"Trying smil playlist...\n");

  // Check if smil
  while((line = play_tree_parser_get_line(p)) != NULL) {
    strstrip(line);
    if(line[0] == '\0') // Ignore empties
      continue;
    if (strncasecmp(line,"<?xml",5)==0) // smil in xml
      continue;
    if (strncasecmp(line,"<!DOCTYPE smil",13)==0) // smil in xml
      continue;
    if (strncasecmp(line,"<smil",5)==0 || strncasecmp(line,"<?wpl",5)==0 ||
      strncasecmp(line,"(smil-document",14)==0)
      break; // smil header found
    else
      return NULL; //line not smil exit
  }

  if (!line) return NULL;
  mp_msg(MSGT_PLAYTREE,MSGL_V,"Detected smil playlist format\n");
  play_tree_parser_stop_keeping(p);

  if (strncasecmp(line,"(smil-document",14)==0) {
    mp_msg(MSGT_PLAYTREE,MSGL_V,"Special smil-over-realrtsp playlist header\n");
    is_rmsmil = 1;
    if (sscanf(line, "(smil-document (ver 1.0)(npkt %u)(ttlpkt %u", &npkt, &ttlpkt) != 2) {
      mp_msg(MSGT_PLAYTREE,MSGL_WARN,"smil-over-realrtsp: header parsing failure, assuming single packet.\n");
      npkt = ttlpkt = 1;
    }
    if (ttlpkt == 0 || npkt > ttlpkt) {
      mp_msg(MSGT_PLAYTREE,MSGL_WARN,"smil-over-realrtsp: bad packet counters (npkk = %u, ttlpkt = %u), assuming single packet.\n",
        npkt, ttlpkt);
      npkt = ttlpkt = 1;
    }
  }

  //Get entries from smil
  src_line = line;
  line = NULL;
  do {
    strstrip(src_line);
    if (line) {
      free(line);
      line = NULL;
    }
    /* If we're parsing smil over realrtsp and this is not the last packet and
     * this is the last line in the packet (terminating with ") ) we must get
     * the next line, strip the header, and concatenate it to the current line.
     */
    if (is_rmsmil && npkt != ttlpkt && strstr(src_line,"\")")) {
      char *payload;

      line = strdup(src_line);
      if(!(src_line = play_tree_parser_get_line(p))) {
        mp_msg(MSGT_PLAYTREE,MSGL_WARN,"smil-over-realrtsp: can't get line from packet %u/%u.\n", npkt, ttlpkt);
        break;
      }
      strstrip(src_line);
      // Skip header, packet starts after "
      if(!(payload = strchr(src_line,'\"'))) {
        mp_msg(MSGT_PLAYTREE,MSGL_WARN,"smil-over-realrtsp: can't find start of packet, using complete line.\n");
        payload = src_line;
      } else
        payload++;
      // Skip ") at the end of the last line from the current packet
      line[strlen(line)-2] = 0;
      line = realloc(line, strlen(line)+strlen(payload)+1);
      strcat (line, payload);
      npkt++;
    } else
      line = strdup(src_line);
    /* Unescape \" to " for smil-over-rtsp */
    if (is_rmsmil && line[0] != '\0') {
      int i, j;

      for (i = 0; i < strlen(line); i++)
        if (line[i] == '\\' && line[i+1] == '"')
          for (j = i; line[j]; j++)
            line[j] = line[j+1];
    }
    pos = line;
   while (pos) {
    if (!entrymode) { // all entries filled so far
     while ((pos=strchr(pos, '<'))) {
      if (strncasecmp(pos,"<video",6)==0  || strncasecmp(pos,"<audio",6)==0 || strncasecmp(pos,"<media",6)==0) {
          entrymode=1;
          break; // Got a valid tag, exit '<' search loop
      }
      pos++;
     }
    }
    if (entrymode) { //Entry found but not yet filled
      pos = strstr(pos,"src=");   // Is source present on this line
      if (pos != NULL) {
        entrymode=0;
        if (pos[4] != '"' && pos[4] != '\'') {
          mp_msg(MSGT_PLAYTREE,MSGL_V,"Unknown delimiter %c in source line %s\n", pos[4], line);
          break;
        }
        s_start=pos+5;
        s_end=strchr(s_start,pos[4]);
        if (s_end == NULL) {
          mp_msg(MSGT_PLAYTREE,MSGL_V,"Error parsing this source line %s\n",line);
          break;
        }
        if (s_end-s_start> 511) {
          mp_msg(MSGT_PLAYTREE,MSGL_V,"Cannot store such a large source %s\n",line);
          break;
        }
        strncpy(source,s_start,s_end-s_start);
        source[(s_end-s_start)]='\0'; // Null terminate
        entry = play_tree_new();
        play_tree_add_file(entry,source);
        if(!list)  //Insert new entry
          list = entry;
        else
          play_tree_append_entry(last_entry,entry);
        last_entry = entry;
        pos = s_end;
      }
    }
   }
  } while((src_line = play_tree_parser_get_line(p)) != NULL);

  if (line)
    free(line);

  if(!list) return NULL; // Nothing found

  entry = play_tree_new();
  play_tree_set_child(entry,list);
  return entry;
}

static play_tree_t*
embedded_playlist_parse(char *line) {
  int f=DEMUXER_TYPE_PLAYLIST;
  stream_t* stream;
  play_tree_parser_t* ptp;
  play_tree_t* entry;

  // Get stream opened to link
  stream=open_stream(line,0,&f);
  if(!stream) {
    mp_msg(MSGT_PLAYTREE,MSGL_WARN,"Can't open playlist %s\n",line);
    return NULL;
  }

  //add new playtree
  mp_msg(MSGT_PLAYTREE,MSGL_V,"Adding playlist %s to element entryref\n",line);

  ptp = play_tree_parser_new(stream,1);
  entry = play_tree_parser_get_play_tree(ptp, 1);
  play_tree_parser_free(ptp);
  free_stream(stream);

  return entry;
}

static play_tree_t*
parse_textplain(play_tree_parser_t* p) {
  char* line;
  char *c;
  int embedded;
  play_tree_t *list = NULL, *entry = NULL, *last_entry = NULL;

  mp_msg(MSGT_PLAYTREE,MSGL_V,"Trying plaintext playlist...\n");
  play_tree_parser_stop_keeping(p);

  while((line = play_tree_parser_get_line(p)) != NULL) {
    strstrip(line);
    if(line[0] == '\0' || line[0] == '#' || (line[0] == '/' && line[1] == '/'))
      continue;

    //Special check for embedded smil or ram reference in file
    embedded = 0;
    if (strlen(line) > 5)
      for(c = line; c[0]; c++ )
        if ( ((c[0] == '.') && //start with . and next have smil with optional ? or &
           (tolower(c[1]) == 's') && (tolower(c[2])== 'm') &&
           (tolower(c[3]) == 'i') && (tolower(c[4]) == 'l') &&
           (!c[5] || c[5] == '?' || c[5] == '&')) || // or
          ((c[0] == '.') && // start with . and next have smi or ram with optional ? or &
          ( ((tolower(c[1]) == 's') && (tolower(c[2])== 'm') && (tolower(c[3]) == 'i')) ||
            ((tolower(c[1]) == 'r') && (tolower(c[2])== 'a') && (tolower(c[3]) == 'm')) )
           && (!c[4] || c[4] == '?' || c[4] == '&')) ){
          entry=embedded_playlist_parse(line);
          embedded = 1;
          break;
        }

    if (!embedded) {      //regular file link
      entry = play_tree_new();
      play_tree_add_file(entry,line);
    }

    if (entry != NULL) {
      if(!list)
        list = entry;
      else
        play_tree_append_entry(last_entry,entry);
      last_entry = entry;
    }
  }

  if(!list) return NULL;
  entry = play_tree_new();
  play_tree_set_child(entry,list);
  return entry;
}

play_tree_t*
parse_playtree(stream_t *stream, int forced) {
  play_tree_parser_t* p;
  play_tree_t* ret;

#ifdef MP_DEBUG
  assert(stream != NULL);
#endif

  p = play_tree_parser_new(stream,0);
  if(!p)
    return NULL;

  ret = play_tree_parser_get_play_tree(p, forced);
  play_tree_parser_free(p);

  return ret;
}

static void
play_tree_add_basepath(play_tree_t* pt, char* bp) {
  int i,bl = strlen(bp),fl;

  if(pt->child) {
    play_tree_t* i;
    for(i = pt->child ; i != NULL ; i = i->next)
      play_tree_add_basepath(i,bp);
    return;
  }

  if(!pt->files)
    return;

  for(i = 0 ; pt->files[i] != NULL ; i++) {
    fl = strlen(pt->files[i]);
    // if we find a full unix path, url:// or X:\ at the beginning,
    // don't mangle it.
    if(fl <= 0 || strstr(pt->files[i],"://") || (strstr(pt->files[i],":\\") == pt->files[i] + 1) || (pt->files[i][0] == '/') )
      continue;
    // if the path begins with \ then prepend drive letter to it.
    if (pt->files[i][0] == '\\') {
      if (pt->files[i][1] == '\\')
        continue;
      pt->files[i] = realloc(pt->files[i], 2 + fl + 1);
      memmove(pt->files[i] + 2,pt->files[i],fl+1);
      memcpy(pt->files[i],bp,2);
      continue;
    }
    pt->files[i] = realloc(pt->files[i], bl + fl + 1);
    memmove(pt->files[i] + bl,pt->files[i],fl+1);
    memcpy(pt->files[i],bp,bl);
  }
}

// Wrapper for play_tree_add_basepath (add base path from file)
void play_tree_add_bpf(play_tree_t* pt, char* filename)
{
  char *ls, *file;

  if (pt && filename)
  {
    file = strdup(filename);
    if (file)
    {
      ls = strrchr(file,'/');
      if(!ls) ls = strrchr(file,'\\');
      if(ls) {
        ls[1] = '\0';
        play_tree_add_basepath(pt,file);
      }
      free(file);
    }
  }
}

play_tree_t*
parse_playlist_file(char* file) {
  stream_t *stream;
  play_tree_t* ret;
  int f=DEMUXER_TYPE_PLAYLIST;

  stream = open_stream(file,0,&f);

  if(!stream) {
    mp_msg(MSGT_PLAYTREE,MSGL_ERR,"Error while opening playlist file %s: %s\n",file,strerror(errno));
    return NULL;
  }

  mp_msg(MSGT_PLAYTREE,MSGL_V,"Parsing playlist file %s...\n",file);

  ret = parse_playtree(stream,1);
  free_stream(stream);

  play_tree_add_bpf(ret, file);

  return ret;

}


play_tree_parser_t*
play_tree_parser_new(stream_t* stream,int deep) {
  play_tree_parser_t* p;

  p = calloc(1,sizeof(play_tree_parser_t));
  if(!p)
    return NULL;
  p->stream = stream;
  p->deep = deep;
  p->keep = 1;

  return p;

}

void
play_tree_parser_free(play_tree_parser_t* p) {

#ifdef MP_DEBUG
  assert(p != NULL);
#endif

  if(p->buffer) free(p->buffer);
  if(p->line) free(p->line);
  free(p);
}

play_tree_t*
play_tree_parser_get_play_tree(play_tree_parser_t* p, int forced) {
  play_tree_t* tree = NULL;

#ifdef MP_DEBUG
  assert(p != NULL);
#endif


  while(play_tree_parser_get_line(p) != NULL) {
    play_tree_parser_reset(p);

    tree = parse_asx(p);
    if(tree) break;
    play_tree_parser_reset(p);

    tree = parse_pls(p);
    if(tree) break;
    play_tree_parser_reset(p);

    tree = parse_m3u(p);
    if(tree) break;
    play_tree_parser_reset(p);

    tree = parse_ref_ini(p);
    if(tree) break;
    play_tree_parser_reset(p);

    tree = parse_smil(p);
    if(tree) break;
    play_tree_parser_reset(p);

    // Here come the others formats ( textplain must stay the last one )
    if (forced)
    {
      tree = parse_textplain(p);
      if(tree) break;
    }
    break;
  }

  if(tree)
    mp_msg(MSGT_PLAYTREE,MSGL_V,"Playlist successfully parsed\n");
  else
    mp_msg(MSGT_PLAYTREE,((forced==1)?MSGL_ERR:MSGL_V),"Error while parsing playlist\n");

  if(tree)
    tree = play_tree_cleanup(tree);

  if(!tree) mp_msg(MSGT_PLAYTREE,((forced==1)?MSGL_WARN:MSGL_V),"Warning: empty playlist\n");

  return tree;
}
