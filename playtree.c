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
/// \ingroup Playtree

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#ifdef MP_DEBUG
#include <assert.h>
#endif
#include "m_config.h"
#include "playtree.h"
#include "mp_msg.h"

static int
play_tree_is_valid(play_tree_t* pt);

play_tree_t*
play_tree_new(void) {
  play_tree_t* r = calloc(1,sizeof(play_tree_t));
  if(r == NULL) {
    mp_msg(MSGT_PLAYTREE,MSGL_ERR,"Can't allocate %d bytes of memory\n",(int)sizeof(play_tree_t));
    return NULL;
  }
  r->entry_type = PLAY_TREE_ENTRY_NODE;
  return r;
}

void
play_tree_free(play_tree_t* pt, int children) {
  play_tree_t* iter;

#ifdef MP_DEBUG
  assert(pt != NULL);
#endif

  if(children) {
    for(iter = pt->child; iter != NULL; ) {
      play_tree_t* nxt=iter->next;
      play_tree_free(iter,1);
      iter = nxt;
    }
    pt->child = NULL;
  }

  play_tree_remove(pt,0,0);

  for(iter = pt->child ; iter != NULL ; iter = iter->next)
    iter->parent = NULL;

  //if(pt->params) free(pt->params);
  if(pt->files) {
    int i;
    for(i = 0 ; pt->files[i] != NULL ; i++)
      free(pt->files[i]);
    free(pt->files);
  }

  free(pt);
}

void
play_tree_free_list(play_tree_t* pt, int children) {
  play_tree_t* iter;

#ifdef MP_DEBUG
  assert(pt != NULL);
#endif

  for(iter = pt ; iter->prev != NULL ; iter = iter->prev)
    /* NOTHING */;

  while(iter) {
    play_tree_t* nxt = iter->next;
    play_tree_free(iter, children);
    iter = nxt;
  }


}

void
play_tree_append_entry(play_tree_t* pt, play_tree_t* entry) {
  play_tree_t* iter;

#ifdef MP_DEBUG
  assert(pt != NULL);
  assert(entry != NULL);
#endif

  if(pt == entry)
    return;

  for(iter = pt ; iter->next != NULL ; iter = iter->next)
    /* NOTHING */;

  entry->parent = iter->parent;
  entry->prev = iter;
  entry->next = NULL;
  iter->next = entry;
}

void
play_tree_prepend_entry(play_tree_t* pt, play_tree_t* entry) {
  play_tree_t* iter;

#ifdef MP_DEBUG
  assert(pt != NULL);
  assert(entry != NULL);
#endif

  for(iter = pt ; iter->prev != NULL; iter = iter->prev)
    /* NOTHING */;

  entry->prev = NULL;
  entry->next = iter;
  entry->parent = iter->parent;

  iter->prev = entry;
  if(entry->parent) {
#ifdef MP_DEBUG
    assert(entry->parent->child == iter);
#endif
    entry->parent->child = entry;
  }
}

void
play_tree_insert_entry(play_tree_t* pt, play_tree_t* entry) {

#ifdef MP_DEBUG
  assert(pt != NULL);
  assert(entry != NULL);
#endif

  entry->parent = pt->parent;
  entry->prev = pt;
  if(pt->next) {
#ifdef MP_DEBUG
    assert(pt->next->prev == pt);
#endif
    entry->next = pt->next;
    entry->next->prev = entry;
  } else
    entry->next = NULL;
  pt->next = entry;

}

void
play_tree_remove(play_tree_t* pt, int free_it, int with_children) {

#ifdef MP_DEBUG
  assert(pt != NULL);
#endif

  // Middle of list
  if(pt->prev && pt->next) {
#ifdef MP_DEBUG
    assert(pt->prev->next == pt);
    assert(pt->next->prev == pt);
#endif
    pt->prev->next = pt->next;
    pt->next->prev = pt->prev;
  } // End of list
  else if(pt->prev) {
#ifdef MP_DEBUG
    assert(pt->prev->next == pt);
#endif
    pt->prev->next = NULL;
  } // Beginning of list
  else if(pt->next) {
#ifdef MP_DEBUG
    assert(pt->next->prev == pt);
#endif
    pt->next->prev = NULL;
    if(pt->parent) {
#ifdef MP_DEBUG
      assert(pt->parent->child == pt);
#endif
      pt->parent->child = pt->next;
    }
  } // The only one
  else if(pt->parent) {
#ifdef MP_DEBUG
    assert(pt->parent->child == pt);
#endif
    pt->parent->child = NULL;
  }

  pt->prev = pt->next = pt->parent = NULL;
  if(free_it)
    play_tree_free(pt,with_children);

}

void
play_tree_set_child(play_tree_t* pt, play_tree_t* child) {
  play_tree_t* iter;

#ifdef MP_DEBUG
  assert(pt != NULL);
  assert(pt->entry_type == PLAY_TREE_ENTRY_NODE);
#endif

  //DEBUG_FF: Where are the children freed?
  // Attention in using this function!
  for(iter = pt->child ; iter != NULL ; iter = iter->next)
    iter->parent = NULL;

  // Go back to first one
  for(iter = child ; iter->prev != NULL ; iter = iter->prev)
    /* NOTHING */;

  pt->child = iter;

  for( ; iter != NULL ; iter= iter->next)
    iter->parent = pt;

}

void
play_tree_set_parent(play_tree_t* pt, play_tree_t* parent) {
  play_tree_t* iter;

#ifdef MP_DEBUG
  assert(pt != NULL);
#endif

  if(pt->parent)
    pt->parent->child = NULL;

  for(iter = pt ; iter != NULL ; iter = iter->next)
    iter->parent = parent;

  if(pt->prev) {
    for(iter = pt->prev ; iter->prev != NULL ; iter = iter->prev)
      iter->parent = parent;
    iter->parent = parent;
    parent->child = iter;
  } else
    parent->child = pt;

}


void
play_tree_add_file(play_tree_t* pt,char* file) {
  int n = 0;
  char* e;

#ifdef MP_DEBUG
  assert(pt != NULL);
  assert(pt->child == NULL);
  assert(file != NULL);
#endif

  if(pt->entry_type != PLAY_TREE_ENTRY_NODE &&
     pt->entry_type != PLAY_TREE_ENTRY_FILE)
    return;

  if(pt->files) {
    for(n = 0 ; pt->files[n] != NULL ; n++)
      /* NOTHING */;
  }
  pt->files = realloc(pt->files, (n + 2) * sizeof(char*));
  if(pt->files ==NULL) {
    mp_msg(MSGT_PLAYTREE,MSGL_ERR,"Can't allocate %d bytes of memory\n",(n+2)*(int)sizeof(char*));
    return;
  }

  e = pt->files[n] = strdup(file);
  pt->files[n+1] = NULL;

  pt->entry_type = PLAY_TREE_ENTRY_FILE;

}

int
play_tree_remove_file(play_tree_t* pt,char* file) {
  int n,f = -1;

#ifdef MP_DEBUG
  assert(pt != NULL);
  assert(file != NULL);
  assert(pt->entry_type != PLAY_TREE_ENTRY_NODE);
#endif

  for(n=0 ; pt->files[n] != NULL ; n++) {
    if(strcmp(file,pt->files[n]) == 0)
      f = n;
  }

  if(f < 0) // Not found
    return 0;

#ifdef MP_DEBUG
  assert(n > f);
#endif

  free(pt->files[f]);

  if(n > 1) {
    memmove(&pt->files[f],&pt->files[f+1],(n-f)*sizeof(char*));
    pt->files = realloc(pt->files, n * sizeof(char*));
    if(pt->files == NULL) {
      mp_msg(MSGT_PLAYTREE,MSGL_ERR,"Can't allocate %d bytes of memory\n",(n+2)*(int)sizeof(char*));
      return -1;
    }
  } else {
    free(pt->files);
    pt->files = NULL;
  }

  return 1;
}

void
play_tree_set_param(play_tree_t* pt, char* name, char* val) {
  int n = 0;

#ifdef MP_DEBUG
  assert(pt != NULL);
  assert(name != NULL);
#endif

  if(pt->params)
    for ( ; pt->params[n].name != NULL ; n++ ) { }

  pt->params = realloc(pt->params, (n + 2) * sizeof(play_tree_param_t));
  if(pt->params == NULL) {
      mp_msg(MSGT_PLAYTREE,MSGL_ERR,"Can't realloc params (%d bytes of memory)\n",(n+2)*(int)sizeof(play_tree_param_t));
      return;
  }
  pt->params[n].name = strdup(name);
  pt->params[n].value = val != NULL ? strdup(val) : NULL;
  memset(&pt->params[n+1],0,sizeof(play_tree_param_t));

  return;
}

int
play_tree_unset_param(play_tree_t* pt, char* name) {
  int n,ni = -1;

#ifdef MP_DEBUG
  assert(pt != NULL);
  assert(name != NULL);
  assert(pt->params != NULL);
#endif

  for(n = 0 ; pt->params[n].name != NULL ; n++) {
    if(strcasecmp(pt->params[n].name,name) == 0)
      ni = n;
  }

  if(ni < 0)
    return 0;

  if(pt->params[ni].name) free(pt->params[ni].name);
  if(pt->params[ni].value) free(pt->params[ni].value);

  if(n > 1) {
    memmove(&pt->params[ni],&pt->params[ni+1],(n-ni)*sizeof(play_tree_param_t));
    pt->params = realloc(pt->params, n * sizeof(play_tree_param_t));
    if(pt->params == NULL) {
      mp_msg(MSGT_PLAYTREE,MSGL_ERR,"Can't allocate %d bytes of memory\n",n*(int)sizeof(play_tree_param_t));
      return -1;
    }
  } else {
    free(pt->params);
    pt->params = NULL;
  }

  return 1;
}

void
play_tree_set_params_from(play_tree_t* dest,play_tree_t* src) {
  int i;

#ifdef MP_DEBUG
  assert(dest != NULL);
  assert(src != NULL);
#endif

  if(!src->params)
    return;

  for(i = 0; src->params[i].name != NULL ; i++)
    play_tree_set_param(dest,src->params[i].name,src->params[i].value);
  if(src->flags & PLAY_TREE_RND) // pass the random flag too
    dest->flags |= PLAY_TREE_RND;

}

static void
play_tree_unset_flag(play_tree_t* pt, int flags , int deep) {
  play_tree_t*  i;

  pt->flags &= ~flags;

  if(deep && pt->child) {
    if(deep > 0) deep--;
    for(i = pt->child ; i ; i = i->next)
      play_tree_unset_flag(i,flags,deep);
  }
}


//////////////////////////////////// ITERATOR //////////////////////////////////////

static void
play_tree_iter_push_params(play_tree_iter_t* iter) {
  int n;
  play_tree_t* pt;
#ifdef MP_DEBUG
  assert(iter != NULL);
  assert(iter->config != NULL);
  assert(iter->tree != NULL);
#endif

  pt = iter->tree;

  // We always push a config because we can set some option
  // while playing
  m_config_push(iter->config);

  if(pt->params == NULL)
    return;


  for(n = 0; pt->params[n].name != NULL ; n++) {
    int e;
    if((e = m_config_set_option(iter->config,pt->params[n].name,pt->params[n].value)) < 0) {
      mp_msg(MSGT_PLAYTREE,MSGL_ERR,"Error %d while setting option '%s' with value '%s'\n",e,
	     pt->params[n].name,pt->params[n].value);
    }
  }

  if(!pt->child)
    iter->entry_pushed = 1;
  return;
}

play_tree_iter_t*
play_tree_iter_new(play_tree_t* pt,m_config_t* config) {
  play_tree_iter_t* iter;

#ifdef MP_DEBUG
  assert(pt != NULL);
  assert(config != NULL);
#endif

  if( ! play_tree_is_valid(pt))
    return NULL;

  iter = calloc(1,sizeof(play_tree_iter_t));
  if(! iter) {
      mp_msg(MSGT_PLAYTREE,MSGL_ERR,"Can't allocate new iterator (%d bytes of memory)\n",(int)sizeof(play_tree_iter_t));
      return NULL;
  }
  iter->root = pt;
  iter->tree = NULL;
  iter->config = config;

  if(pt->parent)
    iter->loop = pt->parent->loop;

  return iter;
}

void
play_tree_iter_free(play_tree_iter_t* iter) {

#ifdef MP_DEBUG
  assert(iter != NULL);
#endif

  if(iter->status_stack) {
#ifdef MP_DEBUG
    assert(iter->stack_size > 0);
#endif
    free(iter->status_stack);
  }

  free(iter);
}

static play_tree_t*
play_tree_rnd_step(play_tree_t* pt) {
  int count = 0;
  int r;
  play_tree_t *i,*head;

  // Count how many free choice we have
  for(i = pt ; i->prev ; i = i->prev)
    if(!(i->flags & PLAY_TREE_RND_PLAYED)) count++;
  head = i;
  if(!(i->flags & PLAY_TREE_RND_PLAYED)) count++;
  for(i = pt->next ; i ; i = i->next)
    if(!(i->flags & PLAY_TREE_RND_PLAYED)) count++;

  if(!count) return NULL;

  r = (int)((float)(count) * rand() / (RAND_MAX + 1.0));

  for(i = head ; i  ; i=i->next) {
    if(!(i->flags & PLAY_TREE_RND_PLAYED)) r--;
    if(r < 0) return i;
  }

  mp_msg(MSGT_PLAYTREE,MSGL_ERR,"Random stepping error\n");
  return NULL;
}


int
play_tree_iter_step(play_tree_iter_t* iter, int d,int with_nodes) {
  play_tree_t* pt;

  if ( !iter ) return PLAY_TREE_ITER_ENTRY;
  if ( !iter->root ) return PLAY_TREE_ITER_ENTRY;

#ifdef MP_DEBUG
  assert(iter != NULL);
  assert(iter->root != NULL);
  //printf("PT : Stepping = %d\n",d);
#endif

  if(iter->tree == NULL) {
    iter->tree = iter->root;
    return play_tree_iter_step(iter,0,with_nodes);
  }

  if(iter->config && iter->entry_pushed > 0) {
    iter->entry_pushed = 0;
    m_config_pop(iter->config);
  }

  if(iter->tree->parent && (iter->tree->parent->flags & PLAY_TREE_RND))
    iter->mode = PLAY_TREE_ITER_RND;
  else
    iter->mode = PLAY_TREE_ITER_NORMAL;

  iter->file = -1;
  if(iter->mode == PLAY_TREE_ITER_RND)
    pt = play_tree_rnd_step(iter->tree);
  else if( d > 0 ) {
    int i;
    pt = iter->tree;
    for(i = d ; i > 0 && pt ; i--)
      pt = pt->next;
    d = i ? i : 1;
  } else if(d < 0) {
    int i;
    pt = iter->tree;
    for(i = d ; i < 0 && pt ; i++)
      pt = pt->prev;
    d = i ? i : -1;
  } else
    pt = iter->tree;

  if(pt == NULL) { // No next
    // Must we loop?
    if (iter->mode == PLAY_TREE_ITER_RND) {
      if (iter->root->loop == 0)
        return PLAY_TREE_ITER_END;
      play_tree_unset_flag(iter->root, PLAY_TREE_RND_PLAYED, -1);
      if (iter->root->loop > 0) iter->root->loop--;
      // try again
      return play_tree_iter_step(iter, 0, with_nodes);
    } else
    if(iter->tree->parent && iter->tree->parent->loop != 0 && ((d > 0 && iter->loop != 0) || ( d < 0 && (iter->loop < 0 || iter->loop < iter->tree->parent->loop) ) ) ) {
      if(d > 0) { // Go back to the first one
	for(pt = iter->tree ; pt->prev != NULL; pt = pt->prev)
	  /* NOTHNG */;
	if(iter->loop > 0) iter->loop--;
      } else if( d < 0 ) { // Or the last one
	for(pt = iter->tree ; pt->next != NULL; pt = pt->next)
	  /* NOTHNG */;
	if(iter->loop >= 0 && iter->loop < iter->tree->parent->loop) iter->loop++;
      }
      iter->tree = pt;
      return play_tree_iter_step(iter,0,with_nodes);
    }
    // Go up one level
    return play_tree_iter_up_step(iter,d,with_nodes);

  }

  // Is there any valid child?
  if(pt->child && play_tree_is_valid(pt->child)) {
    iter->tree = pt;
    if(with_nodes) { // Stop on the node
      return PLAY_TREE_ITER_NODE;
    } else      // Or follow it
      return play_tree_iter_down_step(iter,d,with_nodes);
  }

  // Is it a valid entry?
  if(! play_tree_is_valid(pt)) {
    if(d == 0) { // Can this happen ? FF: Yes!
      mp_msg(MSGT_PLAYTREE,MSGL_ERR,"What to do now ???? Infinite loop if we continue\n");
      return PLAY_TREE_ITER_ERROR;
    } // Not a valid entry : go to next one
    return play_tree_iter_step(iter,d,with_nodes);
  }

#ifdef MP_DEBUG
  assert(pt->files != NULL);
#endif

  iter->tree = pt;

  for(d = 0 ; iter->tree->files[d] != NULL ; d++)
    /* NOTHING */;
  iter->num_files = d;

  if(iter->config) {
    play_tree_iter_push_params(iter);
    iter->entry_pushed = 1;
    if(iter->mode == PLAY_TREE_ITER_RND)
      pt->flags |= PLAY_TREE_RND_PLAYED;
  }

  return PLAY_TREE_ITER_ENTRY;

}

static int
play_tree_is_valid(play_tree_t* pt) {
  play_tree_t* iter;

#ifdef MP_DEBUG
  assert(pt != NULL);
#endif

  if(pt->entry_type != PLAY_TREE_ENTRY_NODE) {
#ifdef MP_DEBUG
    assert(pt->child == NULL);
#endif
    return 1;
  }
  else if (pt->child != NULL) {
    for(iter = pt->child ; iter != NULL ; iter = iter->next) {
      if(play_tree_is_valid(iter))
	return 1;
    }
  }
  return 0;
}

int
play_tree_iter_up_step(play_tree_iter_t* iter, int d,int with_nodes) {

#ifdef MP_DEBUG
  assert(iter != NULL);
  assert(iter->tree != NULL);
  //printf("PT : Go UP\n");
#endif

  iter->file = -1;
  if(iter->tree->parent == iter->root->parent)
    return PLAY_TREE_ITER_END;

#ifdef MP_DEBUG
  assert(iter->tree->parent != NULL);
  assert(iter->stack_size > 0);
  assert(iter->status_stack != NULL);
#endif

  iter->stack_size--;
  iter->loop = iter->status_stack[iter->stack_size];
  if(iter->stack_size > 0)
    iter->status_stack = realloc(iter->status_stack, iter->stack_size * sizeof(int));
  else {
    free(iter->status_stack);
    iter->status_stack = NULL;
  }
  if(iter->stack_size > 0 && iter->status_stack == NULL) {
    mp_msg(MSGT_PLAYTREE,MSGL_ERR,"Can't allocate %d bytes of memory\n",iter->stack_size*(int)sizeof(char*));
    return PLAY_TREE_ITER_ERROR;
  }
  iter->tree = iter->tree->parent;

  // Pop subtree params
  if(iter->config) {
    m_config_pop(iter->config);
    if(iter->mode == PLAY_TREE_ITER_RND)
      iter->tree->flags |= PLAY_TREE_RND_PLAYED;
  }

  return play_tree_iter_step(iter,d,with_nodes);
}

int
play_tree_iter_down_step(play_tree_iter_t* iter, int d,int with_nodes) {

#ifdef MP_DEBUG
  assert(iter->tree->files == NULL);
  assert(iter->tree->child != NULL);
  assert(iter->tree->child->parent == iter->tree);
  //printf("PT : Go DOWN\n");
#endif

  iter->file = -1;

  //  Push subtree params
  if(iter->config)
    play_tree_iter_push_params(iter);

  iter->stack_size++;
  iter->status_stack = realloc(iter->status_stack, iter->stack_size * sizeof(int));
  if(iter->status_stack == NULL) {
    mp_msg(MSGT_PLAYTREE,MSGL_ERR,"Can't allocate %d bytes of memory\n",iter->stack_size*(int)sizeof(int));
    return PLAY_TREE_ITER_ERROR;
  }
  iter->status_stack[iter->stack_size-1] = iter->loop;
  // Set new status
  iter->loop = iter->tree->loop-1;
  if(d >= 0)
    iter->tree = iter->tree->child;
  else {
    play_tree_t* pt;
    for(pt = iter->tree->child ; pt->next != NULL ; pt = pt->next)
      /*NOTING*/;
    iter->tree = pt;
  }

  return play_tree_iter_step(iter,0,with_nodes);
}

char*
play_tree_iter_get_file(play_tree_iter_t* iter, int d) {
#ifdef MP_DEBUG
  assert(iter != NULL);
  assert(iter->tree->child == NULL);
#endif

  if(iter->tree->files == NULL)
    return NULL;

#ifdef MP_DEBUG
  assert(iter->num_files > 0);
#endif

  if(iter->file >= iter->num_files-1 || iter->file < -1)
    return NULL;

  if(d > 0) {
    if(iter->file >= iter->num_files - 1)
      iter->file = 0;
    else
      iter->file++;
  } else if(d < 0) {
    if(iter->file <= 0)
      iter->file = iter->num_files - 1;
    else
      iter->file--;
  }
  return iter->tree->files[iter->file];
}

play_tree_t*
play_tree_cleanup(play_tree_t* pt) {
  play_tree_t* iter, *tmp, *first;

#ifdef MP_DEBUG
  assert(pt != NULL);
#endif

  if( ! play_tree_is_valid(pt)) {
    play_tree_remove(pt,1,1);
    return NULL;
  }

  first = pt->child;

  for(iter = pt->child ; iter != NULL ; ) {
    tmp = iter;
    iter = iter->next;
    if(! play_tree_is_valid(tmp)) {
      play_tree_remove(tmp,1,1);
      if(tmp == first) first = iter;
    }
  }

  for(iter = first ; iter != NULL ; ) {
    tmp = iter;
    iter = iter->next;
    play_tree_cleanup(tmp);
  }

  return pt;

}

play_tree_iter_t*
play_tree_iter_new_copy(play_tree_iter_t* old) {
  play_tree_iter_t* iter;

#ifdef MP_DEBUG
  assert(old != NULL);
#endif

  iter = malloc(sizeof(play_tree_iter_t));
  if(iter == NULL) {
    mp_msg(MSGT_PLAYTREE,MSGL_ERR,"Can't allocate %d bytes of memory\n",(int)sizeof(play_tree_iter_t));
    return NULL;
  }
;
  memcpy(iter,old,sizeof(play_tree_iter_t));
  if(old->status_stack) {
    iter->status_stack = malloc(old->stack_size * sizeof(int));
    if(iter->status_stack == NULL) {
      mp_msg(MSGT_PLAYTREE,MSGL_ERR,"Can't allocate %d bytes of memory\n",old->stack_size * (int)sizeof(int));
      free(iter);
      return NULL;
    }
    memcpy(iter->status_stack,old->status_stack,iter->stack_size*sizeof(int));
  }
  iter->config = NULL;

  return iter;
}

// HIGH Level API, by Fabian Franz (mplayer@fabian-franz.de)
//
play_tree_iter_t* pt_iter_create(play_tree_t** ppt, m_config_t* config)
{
  play_tree_iter_t* r=NULL;
#ifdef MP_DEBUG
  assert(*ppt!=NULL);
#endif

  *ppt=play_tree_cleanup(*ppt);

  if(*ppt) {
    r = play_tree_iter_new(*ppt,config);
    if (r && play_tree_iter_step(r,0,0) != PLAY_TREE_ITER_ENTRY)
    {
      play_tree_iter_free(r);
      r = NULL;
    }
  }

  return r;
}

void pt_iter_destroy(play_tree_iter_t** iter)
{
  if (iter && *iter)
  {
    free(*iter);
    iter=NULL;
  }
}

char* pt_iter_get_file(play_tree_iter_t* iter, int d)
{
  int i=0;
  char* r;

  if (iter==NULL)
    return NULL;

  r = play_tree_iter_get_file(iter,d);

  while (!r && d!=0)
  {
    if (play_tree_iter_step(iter,d,0) != PLAY_TREE_ITER_ENTRY)
        break;
    r=play_tree_iter_get_file(iter,d);
    i++;
  }

  return r;
}

void pt_iter_insert_entry(play_tree_iter_t* iter, play_tree_t* entry)
{
  play_tree_t *pt = iter->tree;
#ifdef MP_DEBUG
  assert(pt!=NULL);
  assert(entry!=NULL);
  assert(entry!=pt);
#endif

  play_tree_insert_entry(pt, entry);
  play_tree_set_params_from(entry,pt);
}

void pt_iter_replace_entry(play_tree_iter_t* iter, play_tree_t* entry)
{
  play_tree_t *pt = iter->tree;

  pt_iter_insert_entry(iter, entry);
  play_tree_remove(pt, 1, 1);
  iter->tree=entry;
}

//Add a new file as a new entry
void pt_add_file(play_tree_t** ppt, char* filename)
{
  play_tree_t *pt = *ppt, *entry = play_tree_new();
#ifdef MP_DEBUG
  assert(entry!=NULL);
#endif

  play_tree_add_file(entry, filename);
  if (pt)
    play_tree_append_entry(pt, entry);
  else
  {
    pt=entry;
    *ppt=pt;
  }
  play_tree_set_params_from(entry,pt);
}

void pt_iter_goto_head(play_tree_iter_t* iter)
{
  iter->tree=iter->root;
  play_tree_iter_step(iter, 0, 0);
}
