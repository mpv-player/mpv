
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "playtree.h"
#include "libmpdemux/stream.h"
#include "mp_msg.h"

extern play_tree_t*
asx_parser_build_tree(char* buffer);


static char* buffer = NULL;
static int buffer_size = 0, buffer_end = 0;



play_tree_t*
parse_asx(stream_t* stream) {
  int r;
  int comments = 0,read = 1,eof = 0;

  mp_msg(MSGT_PLAYTREE,MSGL_V,"Trying asx...\n");
  
  while(1) {
    if(read && eof) // Eof reached before anything useful
      return NULL;
    if(read) {
      if(buffer_size - buffer_end < 50) buffer_size += 255;
      buffer = (char*)realloc(buffer,buffer_size*sizeof(char));
      if(buffer == NULL) {
	mp_msg(MSGT_PLAYTREE,MSGL_ERR,"Can't allocate %d bytes of memory\n",buffer_size*sizeof(char));
	buffer_size = buffer_end = 0;
	return NULL;
      }
  
      r = stream_read(stream,buffer+buffer_end,buffer_size-buffer_end-1);
      if(r < 0) {
	mp_msg(MSGT_PLAYTREE,MSGL_ERR,"Can't read from stream r=%d\n",r);
	return NULL;
      } else if(r == 0)
	eof = 1;
      buffer_end += r;
      buffer[buffer_end] = '\0';
    }

    if(comments)  { // Jump comments
      int e;
      char* end = strstr(buffer,"-->");
      if(!end) {
	if(buffer[buffer_end-1] != '-')
	  buffer_end = 0; // Drop buffer content if last char isn't '-'
	continue;
      }
      comments = 0;
      e = end - buffer + 3;
      if(e >= buffer_end) { // > seems impossible
	buffer_end = 0; // Drop buffer content
	read = 1;
	continue;
      }
      buffer_end -= e;
      memmove(buffer,end+3,buffer_end); // Drop comments
      continue;
    } 
    
    for(r= 0 ; r < buffer_end ; r++) {
      if(strchr(" \n\r\t",buffer[r]) != NULL) // Jump space
	continue;
      if(buffer[r] != '<') {
	mp_msg(MSGT_PLAYTREE,MSGL_DBG2,"First char isn't '<' but '%c'\n",buffer[r]);
	mp_msg(MSGT_PLAYTREE,MSGL_DBG3,"Buffer = [%s]\n",buffer);
	return NULL;
      }
      break; // Stop on first '<'
    }
    if(r  > buffer_end-4) { // We need more
      if(r > 0) { // Drop unuseful beggining
	buffer_end -= r;
	memmove(buffer,&buffer[r],buffer_end);
      }
      read = 1;
      continue;
    }

    if(strncmp(&buffer[r],"<!--",4) == 0) { // Comments
      read = 0;
      comments = 1;
      continue;
    }

    if(strncasecmp(&buffer[r],"<ASX",4) != 0) // First element is not a comment nor an asx : end
      return NULL;
	
	
    mp_msg(MSGT_PLAYTREE,MSGL_V,"Detected asx format\n");
    break;
  }
  // We have an asx : load it in memory and parse

  while(!eof) {
    if(buffer_size - buffer_end < 50) buffer_size += 255;
    buffer = (char*)realloc(buffer,buffer_size*sizeof(char));
    if(buffer == NULL) {
	mp_msg(MSGT_PLAYTREE,MSGL_ERR,"Can't allocate %d bytes of memory\n",buffer_size*sizeof(char));
	buffer_size = buffer_end = 0;
	return NULL;
      }
    r = stream_read(stream,buffer+buffer_end,buffer_size-buffer_end-1);
    if(r > 0)
      buffer_end += r;
    if(r <= 0)
      break;
    buffer[buffer_end] = '\0';
  }

 mp_msg(MSGT_PLAYTREE,MSGL_DBG3,"Parsing asx file : [%s]\n",buffer);
  return asx_parser_build_tree(buffer);
  

}

play_tree_t*
parse_textplain(stream_t *stream) {
  char* end;
  char* file;
  int eof = 0,r,p_end=-1,resize = 0;
  play_tree_t *list = NULL, *entry = NULL;

  mp_msg(MSGT_PLAYTREE,MSGL_V,"Trying plaintext...\n");

  if(buffer_size < 255 && ! stream->eof) {
    buffer_size = 255;
    buffer = (char*)realloc(buffer,buffer_size*sizeof(char));
    if(buffer == NULL) {
      mp_msg(MSGT_PLAYTREE,MSGL_ERR,"Can't allocate %d bytes of memory\n",buffer_size*sizeof(char));
      buffer_size = buffer_end = 0;
      return NULL;
      }
  }


  while(!eof) {
    if(resize) {
      buffer_size += 255;
      buffer = (char*)realloc(buffer,buffer_size*sizeof(char));
      resize = 0;
      if(buffer == NULL) {
	mp_msg(MSGT_PLAYTREE,MSGL_ERR,"Can't allocate %d bytes of memory\n",buffer_size*sizeof(char));
	buffer_size = buffer_end = 0;
	if(list) play_tree_free_list(list,1);
	return NULL;
      }
    }
    if(!stream->eof) {
      r = stream_read(stream,buffer+buffer_end,buffer_size-buffer_end-1);
      if(r < 0) {
	mp_msg(MSGT_PLAYTREE,MSGL_ERR,"Can't read from stream r=%d\n",r);
	return NULL;
      } else if(r == 0)
	eof = 1;
    buffer_end += r;
    buffer[buffer_end] = '\0';
    } else eof = 1;
    r = 0;
    while(1) {
      p_end = r;
      for( ; buffer[r] != '\0' ; r++) {
	if(strchr(" \n\r\t",buffer[r]) != NULL)
	  continue;
	break;
      }
      if(buffer[r] == '\0') {
	p_end = r;
	if(!eof)
	  resize = 1;
	break;
      }
      end = strchr(&buffer[r],'\n');      
      if(!end) {
	if(!eof) {
	  p_end = r;
	  if(p_end == 0) {
	    resize = 1;
	  }
	  break;
	}
	entry = play_tree_new();
	play_tree_add_file(entry,&buffer[r]);
	r = buffer_end;
	
      }
      else {
	if(r > 0 && buffer[r-1] == '\r') r--;
	file = (char*)malloc((end-(&buffer[r])+1)*sizeof(char));
	if(file == NULL) {
	  mp_msg(MSGT_PLAYTREE,MSGL_ERR,"Can't allocate %d bytes of memory\n",buffer_size*sizeof(char));
	  buffer_size = buffer_end = 0;
	  if(list) play_tree_free_list(list,1);
	  return NULL;
	}
	// TODO : Check if the given file exist and is readable (or it'a an url)	  
	strncpy(file,&buffer[r],end-(&buffer[r]));
	file[end-(&buffer[r])] = '\0';
	entry = play_tree_new();
	play_tree_add_file(entry,file);	
	free(file);
	r += end-(&buffer[r]);
	p_end = r;
      }
      if(entry) {
	mp_msg(MSGT_PLAYTREE,MSGL_DBG2,"Adding file %s to playlist\n",entry->files[0]);
	if(list) play_tree_append_entry(list,entry);
	else list = entry;
	entry = NULL;
      }
    }
    if(!eof && p_end > 0 && p_end < buffer_end) {
      memmove(buffer,&buffer[p_end],buffer_end-p_end);
      buffer_end -= p_end;
    } else if(!eof && !resize && p_end == buffer_end) {
      buffer_end = 0;
      buffer[0] = '\0';
    }
  }
   
  if(!list) return NULL;
  entry = play_tree_new();
  play_tree_set_child(entry,list);
  return entry;    
}

play_tree_t*
parse_playtree(stream_t *stream) {
  play_tree_t* tree = NULL;
  
#ifdef DEBUG
  assert(stream != NULL);
  assert(stream->type == STREAMTYPE_PLAYLIST);
#endif

  while(1) {
    tree = parse_asx(stream);
    if(tree) break;
    // Here come the others formats ( textplain must stay the last one )
    tree = parse_textplain(stream);
    if(tree) break;
    break;
  }

  if(tree)
    mp_msg(MSGT_PLAYTREE,MSGL_V,"Playlist succefully parsed\n");
  else mp_msg(MSGT_PLAYTREE,MSGL_ERR,"Error while parsing playlist\n");

  if(tree)
    tree = play_tree_cleanup(tree);

  if(!tree) mp_msg(MSGT_PLAYTREE,MSGL_WARN,"Warning empty playlist\n");

  if(buffer) free(buffer);
  buffer = NULL;
  buffer_end = buffer_size = 0;
  return tree;
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
