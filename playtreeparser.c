
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


play_tree_t*
parse_asx(play_tree_parser_t* p) {
  int r;
  int comments = 0,read = 1,eof = 0;

  mp_msg(MSGT_PLAYTREE,MSGL_V,"Trying asx...\n");
  
  while(1) {
    if(read && eof) // Eof reached before anything useful
      return NULL;
    if(read) {
      if(p->buffer_size - p->buffer_end < 50) p->buffer_size += 255;
      p->buffer = (char*)realloc(p->buffer,p->buffer_size*sizeof(char));
      if(p->buffer == NULL) {
	mp_msg(MSGT_PLAYTREE,MSGL_ERR,"Can't allocate %d bytes of memory\n",p->buffer_size*sizeof(char));
	p->buffer_size = p->buffer_end = 0;
	return NULL;
      }
  
      r = stream_read(p->stream,p->buffer+p->buffer_end,p->buffer_size-p->buffer_end-1);
      if(r < 0) {
	mp_msg(MSGT_PLAYTREE,MSGL_ERR,"Can't read from stream r=%d\n",r);
	return NULL;
      } else if(r == 0)
	eof = 1;
      p->buffer_end += r;
      p->buffer[p->buffer_end] = '\0';
    }

    if(comments)  { // Jump comments
      int e;
      char* end = strstr(p->buffer,"-->");
      if(!end) {
	if(p->buffer[p->buffer_end-1] != '-')
	  p->buffer_end = 0; // Drop buffer content if last char isn't '-'
	continue;
      }
      comments = 0;
      e = end - p->buffer + 3;
      if(e >= p->buffer_end) { // > seems impossible
	p->buffer_end = 0; // Drop buffer content
	read = 1;
	continue;
      }
      p->buffer_end -= e;
      memmove(p->buffer,end+3,p->buffer_end); // Drop comments
      continue;
    } 
    
    for(r= 0 ; r < p->buffer_end ; r++) {
      if(strchr(" \n\r\t",p->buffer[r]) != NULL) // Jump space
	continue;
      if(p->buffer[r] != '<') {
	mp_msg(MSGT_PLAYTREE,MSGL_DBG2,"First char isn't '<' but '%c'\n",p->buffer[r]);
	mp_msg(MSGT_PLAYTREE,MSGL_DBG3,"Buffer = [%s]\n",p->buffer);
	return NULL;
      }
      break; // Stop on first '<'
    }
    if(r  > p->buffer_end-4) { // We need more
      if(r > 0) { // Drop unuseful beggining
	p->buffer_end -= r;
	memmove(p->buffer,&p->buffer[r],p->buffer_end);
      }
      read = 1;
      continue;
    }

    if(strncmp(&p->buffer[r],"<!--",4) == 0) { // Comments
      read = 0;
      comments = 1;
      continue;
    }

    if(strncasecmp(&p->buffer[r],"<ASX",4) != 0) // First element is not a comment nor an asx : end
      return NULL;
	
	
    mp_msg(MSGT_PLAYTREE,MSGL_V,"Detected asx format\n");
    break;
  }
  // We have an asx : load it in memory and parse

  while(!eof) {
    if(p->buffer_size - p->buffer_end < 50) p->buffer_size += 255;
    p->buffer = (char*)realloc(p->buffer,p->buffer_size*sizeof(char));
    if(p->buffer == NULL) {
	mp_msg(MSGT_PLAYTREE,MSGL_ERR,"Can't allocate %d bytes of memory\n",p->buffer_size*sizeof(char));
	p->buffer_size = p->buffer_end = 0;
	return NULL;
      }
    r = stream_read(p->stream,p->buffer+p->buffer_end,p->buffer_size-p->buffer_end-1);
    if(r > 0)
      p->buffer_end += r;
    if(r <= 0)
      break;
    p->buffer[p->buffer_end] = '\0';
  }

 mp_msg(MSGT_PLAYTREE,MSGL_DBG3,"Parsing asx file : [%s]\n",p->buffer);
 return asx_parser_build_tree(p->buffer,p->deep);
}

play_tree_t*
parse_textplain(play_tree_parser_t* p) {
  char* end;
  char* file;
  int eof = 0,r,p_end=-1,resize = 0;
  play_tree_t *list = NULL, *entry = NULL;

  mp_msg(MSGT_PLAYTREE,MSGL_V,"Trying plaintext...\n");

  if(p->buffer_size < 255 && ! p->stream->eof) {
    p->buffer_size = 255;
    p->buffer = (char*)realloc(p->buffer,p->buffer_size*sizeof(char));
    if(p->buffer == NULL) {
      mp_msg(MSGT_PLAYTREE,MSGL_ERR,"Can't allocate %d bytes of memory\n",p->buffer_size*sizeof(char));
      p->buffer_size = p->buffer_end = 0;
      return NULL;
      }
  }


  while(!eof) {
    if(resize) {
      p->buffer_size += 255;
      p->buffer = (char*)realloc(p->buffer,p->buffer_size*sizeof(char));
      resize = 0;
      if(p->buffer == NULL) {
	mp_msg(MSGT_PLAYTREE,MSGL_ERR,"Can't allocate %d bytes of memory\n",p->buffer_size*sizeof(char));
	p->buffer_size = p->buffer_end = 0;
	if(list) play_tree_free_list(list,1);
	return NULL;
      }
    }
    if(!p->stream->eof) {
      r = stream_read(p->stream,p->buffer+p->buffer_end,p->buffer_size-p->buffer_end-1);
      if(r < 0) {
	mp_msg(MSGT_PLAYTREE,MSGL_ERR,"Can't read from stream r=%d\n",r);
	return NULL;
      } else if(r == 0)
	eof = 1;
    p->buffer_end += r;
    p->buffer[p->buffer_end] = '\0';
    } else eof = 1;
    r = 0;
    while(1) {
      p_end = r;
      for( ; p->buffer[r] != '\0' ; r++) {
	if(strchr(" \n\r\t",p->buffer[r]) != NULL)
	  continue;
	break;
      }
      if(p->buffer[r] == '\0') {
	p_end = r;
	if(!eof)
	  resize = 1;
	break;
      }
      end = strchr(&p->buffer[r],'\n');      
      if(!end) {
	if(!eof) {
	  p_end = r;
	  if(p_end == 0) {
	    resize = 1;
	  }
	  break;
	}
	entry = play_tree_new();
	play_tree_add_file(entry,&p->buffer[r]);
	r = p->buffer_end;
	
      }
      else {
	if(r > 0 && p->buffer[r-1] == '\r') r--;
	file = (char*)malloc((end-(&p->buffer[r])+1)*sizeof(char));
	if(file == NULL) {
	  mp_msg(MSGT_PLAYTREE,MSGL_ERR,"Can't allocate %d bytes of memory\n",p->buffer_size*sizeof(char));
	  p->buffer_size = p->buffer_end = 0;
	  if(list) play_tree_free_list(list,1);
	  return NULL;
	}
	// TODO : Check if the given file exist and is readable (or it'a an url)	  
	strncpy(file,&p->buffer[r],end-(&p->buffer[r]));
	file[end-(&p->buffer[r])] = '\0';
	entry = play_tree_new();
	play_tree_add_file(entry,file);	
	free(file);
	r += end-(&p->buffer[r]);
	p_end = r;
      }
      if(entry) {
	mp_msg(MSGT_PLAYTREE,MSGL_DBG2,"Adding file %s to playlist\n",entry->files[0]);
	if(list) play_tree_append_entry(list,entry);
	else list = entry;
	entry = NULL;
      }
    }
    if(!eof && p_end > 0 && p_end < p->buffer_end) {
      memmove(p->buffer,&p->buffer[p_end],p->buffer_end-p_end);
      p->buffer_end -= p_end;
    } else if(!eof && !resize && p_end == p->buffer_end) {
      p->buffer_end = 0;
      p->buffer[0] = '\0';
    }
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

  return p;

}

void
play_tree_parser_free(play_tree_parser_t* p) {

#ifdef MP_DEBUG
  assert(p != NULL);
#endif

  if(p->buffer) free(p->buffer);
  free(p->buffer);
}

play_tree_t*
play_tree_parser_get_play_tree(play_tree_parser_t* p) {
  play_tree_t* tree = NULL;

#ifdef MP_DEBUG
  assert(p != NULL);
#endif

  while(1) {
    tree = parse_asx(p);
    if(tree) break;
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

  if(p->buffer) free(p->buffer);
  p->buffer = NULL;
  p->buffer_end = p->buffer_size = 0;

  return tree;
}
