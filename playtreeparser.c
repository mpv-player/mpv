
#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef MP_DEBUG
#include <assert.h>
#endif
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "playtree.h"
#include "playtreeparser.h"
#include "libmpdemux/stream.h"
#include "mp_msg.h"

extern play_tree_t*
asx_parser_build_tree(char* buffer, int ref);

#define BUF_STEP 1024

#define WHITES " \n\r\t"

static void
strstrip(char* str) {
  char* i;

  for(i = str ; i[0] != '\0' && strchr(WHITES,i[0]) != NULL; i++)
    /* NOTHING */;
  if(i[0] != '\0') {
    memmove(str,i,strlen(i));
    for(i = str + strlen(str) ; strchr(WHITES,i[0]) != NULL; i--)
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
    p->buffer = (char*)malloc(BUF_STEP);
    p->buffer_size = BUF_STEP;
    p->iter = p->buffer;
  }

  if(p->stream->eof && (p->buffer_end == 0 || p->iter[0] == '\0'))
    return NULL;
    
  while(1) {

    if(resize) {
      r = p->iter - p->buffer;
      p->buffer = (char*)realloc(p->buffer,p->buffer_size+BUF_STEP);
      p->iter = p->buffer + r;
      p->buffer_size += BUF_STEP;
      resize = 0;
    }
    
    if(p->buffer_size - p->buffer_end > 1 && ! p->stream->eof) {
      r = stream_read(p->stream,p->buffer + p->buffer_end,p->buffer_size - p->buffer_end - 1);
      if(r > 0) {
	p->buffer_end += r;
	p->buffer[p->buffer_end] = '\0';
      }
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

  line_end = ((*(end-1)) == '\r') ? end-1 : end;
  if(line_end - p->iter >= 0)
    p->line = (char*)realloc(p->line,line_end - p->iter+1);
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
    p->iter = p->buffer;
  }
}


play_tree_t*
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

 mp_msg(MSGT_PLAYTREE,MSGL_DBG3,"Parsing asx file : [%s]\n",p->buffer);
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
  char* v;

  v = pls_entry_get_value(line);
  if(!v) {
    mp_msg(MSGT_PLAYTREE,MSGL_ERR,"No value in entry %s\n",line);
    return 0;
  }

  num = atoi(line);
  if(num < 0) {
    num = max_entry+1;
    mp_msg(MSGT_PLAYTREE,MSGL_WARN,"No entry index in entry %s\nAssuming %d\n",line,num);
  }
  if(num > max_entry) {
    e = (pls_entry_t*)realloc(e,num*sizeof(pls_entry_t));
    memset(&e[max_entry],0,(num-max_entry)*sizeof(pls_entry_t));
    max_entry = num;
  }
  (*_e) = e;
  (*_max_entry) = max_entry;
  (*val) = v;

  return num;
}


play_tree_t*
parse_pls(play_tree_parser_t* p) {
  char *line,*v;
  pls_entry_t* entries = NULL;
  int n_entries = 0,max_entry=0,num;
  play_tree_t *list = NULL, *entry = NULL;

  mp_msg(MSGT_PLAYTREE,MSGL_V,"Trying winamp playlist...\n");
  line = play_tree_parser_get_line(p);
  strstrip(line);
  if(strcasecmp(line,"[playlist]"))
    return NULL;
  mp_msg(MSGT_PLAYTREE,MSGL_V,"Detected winamp playlist format\n");
  play_tree_parser_stop_keeping(p);
  line = play_tree_parser_get_line(p);
  if(!line)
    return NULL;
  strstrip(line);
  if(strncasecmp(line,"NumberOfEntries",15) == 0) {
    v = pls_entry_get_value(line);
    n_entries = atoi(v);
    if(n_entries < 0)
      mp_msg(MSGT_PLAYTREE,MSGL_DBG2,"Invalid number of entries : very funny !!!\n");
    else
      mp_msg(MSGT_PLAYTREE,MSGL_DBG2,"Playlist claim to have %d entries. Let's see.\n",n_entries);
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
      mp_msg(MSGT_PLAYTREE,MSGL_WARN,"Unknow entry type %s\n",line);
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
	play_tree_append_entry(list,entry);
      else
	list = entry;
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

play_tree_t*
parse_m3u(play_tree_parser_t* p) {
  char* line;
  play_tree_t *list = NULL, *entry = NULL;

  mp_msg(MSGT_PLAYTREE,MSGL_V,"Trying extended m3u playlist...\n");
  line = play_tree_parser_get_line(p);
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
      play_tree_append_entry(list,entry);
  }
   
  if(!list) return NULL;
  entry = play_tree_new();
  play_tree_set_child(entry,list);
  return entry;    
}

play_tree_t*
parse_textplain(play_tree_parser_t* p) {
  char* line;
  play_tree_t *list = NULL, *entry = NULL;

  mp_msg(MSGT_PLAYTREE,MSGL_V,"Trying plaintext playlist...\n");
  play_tree_parser_stop_keeping(p);

  while((line = play_tree_parser_get_line(p)) != NULL) {
    strstrip(line);
    if(line[0] == '\0')
      continue;
    entry = play_tree_new();
    play_tree_add_file(entry,line);
    if(!list)
      list = entry;
    else
      play_tree_append_entry(list,entry);
  }
   
  if(!list) return NULL;
  entry = play_tree_new();
  play_tree_set_child(entry,list);
  return entry;    
}

play_tree_t*
parse_playtree(stream_t *stream) {
  play_tree_parser_t* p;
  play_tree_t* ret;

#ifdef MP_DEBUG
  assert(stream != NULL);
  assert(stream->type == STREAMTYPE_PLAYLIST);
#endif

  p = play_tree_parser_new(stream,0);
  if(!p)
    return NULL;

  ret = play_tree_parser_get_play_tree(p);
  play_tree_parser_free(p);

  return ret;
}

play_tree_t*
parse_playlist_file(char* file) {
  stream_t *stream;
  play_tree_t* ret;
  int fd;

  if(!strcmp(file,"-"))
    fd = 0;
  else
    fd = open(file,O_RDONLY);

  if(fd < 0) {
    mp_msg(MSGT_PLAYTREE,MSGL_ERR,"Error while opening playlist file %s : %s\n",file,strerror(errno));
    return NULL;
  }

  mp_msg(MSGT_PLAYTREE,MSGL_V,"Parsing playlist file %s...\n",file);

  stream = new_stream(fd,STREAMTYPE_PLAYLIST);
  ret = parse_playtree(stream);
  if(close(fd) < 0)
    mp_msg(MSGT_PLAYTREE,MSGL_ERR,"Warning error while closing playlist file %s : %s\n",file,strerror(errno));
  free_stream(stream);

  return ret;

}


play_tree_parser_t*
play_tree_parser_new(stream_t* stream,int deep) {
  play_tree_parser_t* p;

  p = (play_tree_parser_t*)calloc(1,sizeof(play_tree_parser_t));
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
play_tree_parser_get_play_tree(play_tree_parser_t* p) {
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

    // Here come the others formats ( textplain must stay the last one )
    tree = parse_textplain(p);
    if(tree) break;
    break;
  }

  if(tree)
    mp_msg(MSGT_PLAYTREE,MSGL_V,"Playlist succefully parsed\n");
  else mp_msg(MSGT_PLAYTREE,MSGL_ERR,"Error while parsing playlist\n");

  if(tree)
    tree = play_tree_cleanup(tree);
  
  if(!tree) mp_msg(MSGT_PLAYTREE,MSGL_WARN,"Warning empty playlist\n");

  return tree;
}
