
#include "libmpdemux/stream.h"

#define PLAY_TREE_ITER_ERROR 0
#define PLAY_TREE_ITER_ENTRY 1
#define PLAY_TREE_ITER_NODE  2
#define PLAY_TREE_ITER_END 3

typedef struct play_tree play_tree_t;
typedef struct play_tree_iter play_tree_iter_t;

#if 0
typedef struct play_tree_info play_tree_info_t;
typedef struct play_tree_param play_tree_param_t;

// TODO : a attrib,val pair system and not something hardcoded
struct play_tree_info {
  char* title;
  char* author;
  char* copyright;
  char* abstract;
  // Some more ??
}

struct play_tree_param {
  char* name;
  char* value;
}
#endif

struct play_tree {
  play_tree_t* parent;
  play_tree_t* child;
  play_tree_t* next;
  play_tree_t* prev;

  //play_tree_info_t info;
  //int n_param;
  //play_tree_param_t* params;
  int loop;
  char** files;
};
  
struct play_tree_iter {
  play_tree_t* root; // Iter root tree
  play_tree_t* tree; // Current tree
  // struct m_config* config; 
  int loop;  // Looping status
  int file;
  int num_files;
 
  int* status_stack; //  loop/valid stack to save/revert status when we go up/down
  int stack_size;  // status stack size
};

play_tree_t* 
play_tree_new(void);

// If childs is true free also the childs
void
play_tree_free(play_tree_t* pt, int childs);


void
play_tree_free_list(play_tree_t* pt, int childs);


// Childs
void
play_tree_set_child(play_tree_t* pt, play_tree_t* child);
// Or parent
void
play_tree_set_parent(play_tree_t* pt, play_tree_t* parent);


// Add at end
void
play_tree_append_entry(play_tree_t* pt, play_tree_t* entry);

// And on begining
void
play_tree_prepend_entry(play_tree_t* pt, play_tree_t* entry);

// Insert after
void
play_tree_insert_entry(play_tree_t* pt, play_tree_t* entry);

// Detach from the tree 
void
play_tree_remove(play_tree_t* pt, int free_it,int with_childs);


void
play_tree_add_file(play_tree_t* pt,char* file);

int
play_tree_remove_file(play_tree_t* pt,char* file);


#if 0
// Val can be NULL
void
play_tree_set_param(play_tree_t* pt, char* name, char* val);

int
play_tree_unset_param(play_tree_t* pt, char* name);

#endif


/// Iterator

play_tree_iter_t*
play_tree_iter_new(play_tree_t* pt);

play_tree_iter_t*
play_tree_iter_new_copy(play_tree_iter_t* old);

void
play_tree_iter_free(play_tree_iter_t* iter);

// d is the direction : d > 0 == next , d < 0 == prev
// with_node : TRUE == stop on nodes with childs, FALSE == go directly to the next child

int 
play_tree_iter_step(play_tree_iter_t* iter, int d,int with_nodes);

int // Break a loop, etc
play_tree_iter_up_step(play_tree_iter_t* iter, int d,int with_nodes);

int // Enter a node child list
play_tree_iter_down_step(play_tree_iter_t* iter, int d,int with_nodes);

char*
play_tree_iter_get_file(play_tree_iter_t* iter, int d);

play_tree_t*
parse_playtree(stream_t *stream);

play_tree_t*
play_tree_cleanup(play_tree_t* pt);

play_tree_t*
parse_playlist_file(char* file);
