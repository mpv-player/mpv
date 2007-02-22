
/// \file
/// \ingroup Playtree

#ifndef __PLAYTREE_H
#define __PLAYTREE_H

struct stream_st;
struct m_config;

/// \defgroup PlaytreeIterReturn Playtree iterator return code
/// \ingroup PlaytreeIter
///@{
#define PLAY_TREE_ITER_ERROR 0
#define PLAY_TREE_ITER_ENTRY 1
#define PLAY_TREE_ITER_NODE  2
#define PLAY_TREE_ITER_END 3
///@}

/// \defgroup PlaytreeEntryTypes Playtree entry types
/// \ingroup Playtree
///@{
#define PLAY_TREE_ENTRY_NODE -1
#define PLAY_TREE_ENTRY_DVD 0
#define PLAY_TREE_ENTRY_VCD 1
#define PLAY_TREE_ENTRY_TV    2
#define PLAY_TREE_ENTRY_FILE  3
///@}


/// \defgroup PlaytreeEntryFlags Playtree flags
/// \ingroup Playtree
///@{
/// Play the item childs in random order.
#define PLAY_TREE_RND  (1<<0)
/// Playtree flags used by the iterator to mark items already "randomly" played.
#define PLAY_TREE_RND_PLAYED  (1<<8)
///@}

/// \defgroup PlaytreeIterMode Playtree iterator mode
/// \ingroup PlaytreeIter
///@{
#define PLAY_TREE_ITER_NORMAL 0
#define PLAY_TREE_ITER_RND 1
///@}

/// \defgroup Playtree
///@{

typedef struct play_tree play_tree_t;
/// \ingroup PlaytreeIter
typedef struct play_tree_iter play_tree_iter_t;
typedef struct play_tree_param play_tree_param_t;


#if 0
typedef struct play_tree_info play_tree_info_t;
// TODO : a attrib,val pair system and not something hardcoded
struct play_tree_info {
  char* title;
  char* author;
  char* copyright;
  char* abstract;
  // Some more ??
}
#endif

struct play_tree_param {
  char* name;
  char* value;
};


/// Playtree item
struct play_tree {
  play_tree_t* parent;
  play_tree_t* child;
  play_tree_t* next;
  play_tree_t* prev;

  //play_tree_info_t info;
  play_tree_param_t* params;
  int loop;
  char** files;
  int entry_type;
  int flags;
};


/// \defgroup PlaytreeIter Playtree iterator
/// \ingroup Playtree
///@{

/// Playtree iterator
struct play_tree_iter {
  /// Root of the iterated tree.
  play_tree_t* root;
  /// Current position in the tree.
  play_tree_t* tree;
  /// \ref Config used.
  struct m_config* config;
  /// Looping status
  int loop;
  /// Selected file in the current item.
  int file;
  /// Number of files in the current item.
  int num_files;
  int entry_pushed;
  int mode;
 
  ///  loop/valid stack to save/revert status when we go up/down.
  int* status_stack;
  /// status stack size
  int stack_size;
};
///@}

/// Create a new empty playtree item.
play_tree_t* 
play_tree_new(void);

/// Free a playtree item.
/** \param pt Item to free.
 *  \param childs If non-zero the item's childs are recursively freed.
 */
void
play_tree_free(play_tree_t* pt, int childs);


/// Free an item and its siblings.
/** \param pt Item to free.
 *  \param childs If non-zero the items' childs are recursively freed.
 */
void
play_tree_free_list(play_tree_t* pt, int childs);


/// Set the childs of a playtree item.
void
play_tree_set_child(play_tree_t* pt, play_tree_t* child);

/// Set the parent of a playtree item.
void
play_tree_set_parent(play_tree_t* pt, play_tree_t* parent);


/// Append an item after its siblings.
void
play_tree_append_entry(play_tree_t* pt, play_tree_t* entry);

/// Prepend an item before its siblings.
void
play_tree_prepend_entry(play_tree_t* pt, play_tree_t* entry);

/// Insert an item right after a siblings.
void
play_tree_insert_entry(play_tree_t* pt, play_tree_t* entry);

/// Detach an item from the tree.
void
play_tree_remove(play_tree_t* pt, int free_it,int with_childs);

/// Add a file to an item.
void
play_tree_add_file(play_tree_t* pt,char* file);

/// Remove a file from an item.
int
play_tree_remove_file(play_tree_t* pt,char* file);


/// Add a config paramter to an item.
void
play_tree_set_param(play_tree_t* pt, char* name, char* val);

/// Remove a config parameter from an item.
int
play_tree_unset_param(play_tree_t* pt, char* name);

/// Copy the config parameters from one item to another.
void
play_tree_set_params_from(play_tree_t* dest,play_tree_t* src);

/// \addtogroup PlaytreeIter
///@{

/// Create a new iterator.
play_tree_iter_t*
play_tree_iter_new(play_tree_t* pt, struct m_config* config);

/// Duplicate an iterator.
play_tree_iter_t*
play_tree_iter_new_copy(play_tree_iter_t* old);

/// Free an iterator.
void
play_tree_iter_free(play_tree_iter_t* iter);

/// Step an iterator.
/** \param iter The iterator.
 *  \param d The direction: d > 0 == next , d < 0 == prev
 *  \param with_node TRUE == stop on nodes with childs, FALSE == go directly to the next child
 *  \return See \ref PlaytreeIterReturn.
 */
int 
play_tree_iter_step(play_tree_iter_t* iter, int d,int with_nodes);

/// Step up, useful to break a loop, etc.
/** \param iter The iterator.
 *  \param d The direction: d > 0 == next , d < 0 == prev
 *  \param with_node TRUE == stop on nodes with childs, FALSE == go directly to the next child
 *  \return See \ref PlaytreeIterReturn.
 */
int
play_tree_iter_up_step(play_tree_iter_t* iter, int d,int with_nodes);

/// Enter a node child list, only useful when stopping on nodes.
int
play_tree_iter_down_step(play_tree_iter_t* iter, int d,int with_nodes);

/// Get a file from the current item.
char*
play_tree_iter_get_file(play_tree_iter_t* iter, int d);

///@}
// PlaytreeIter group

/// Create a playtree from a playlist file.
/** \ingroup PlaytreeParser
 */
play_tree_t*
parse_playtree(struct stream_st *stream, int forced);

/// Clean a tree by destroying all empty elements.
play_tree_t*
play_tree_cleanup(play_tree_t* pt);

/// Create a playtree from a playlist file.
/** \ingroup PlaytreeParser
 */
play_tree_t*
parse_playlist_file(char* file);

/// \defgroup PtAPI Playtree highlevel API
/// \ingroup Playtree
/// Highlevel API with pt-suffix to different from low-level API
/// by Fabian Franz (mplayer@fabian-franz.de).
///@{

// Cleans up pt and creates a new iter.
play_tree_iter_t* pt_iter_create(play_tree_t** pt, struct m_config* config);

/// Frees the iter.
void pt_iter_destroy(play_tree_iter_t** iter);

/// Gets the next available file in the direction (d=-1 || d=+1).
char* pt_iter_get_file(play_tree_iter_t* iter, int d);

// Two Macros that implement forward and backward direction.
#define pt_iter_get_next_file(iter) pt_iter_get_file(iter, 1)
#define pt_iter_get_prev_file(iter) pt_iter_get_file(iter, -1)

/// Inserts entry into the playtree.
void pt_iter_insert_entry(play_tree_iter_t* iter, play_tree_t* entry);

/// Replaces current entry in playtree with entry by doing insert and remove.
void pt_iter_replace_entry(play_tree_iter_t* iter, play_tree_t* entry);

/// Adds a new file to the playtree, if it is not valid it is created.
void pt_add_file(play_tree_t** ppt, char* filename);

/// \brief Performs a convert to playtree-syntax, by concat path/file 
/// and performs pt_add_file
void pt_add_gui_file(play_tree_t** ppt, char* path, char* file);

// Two macros to use only the iter and not the other things.
#define pt_iter_add_file(iter, filename) pt_add_file(&iter->tree, filename)
#define pt_iter_add_gui_file(iter, path, name) pt_add_gui_file(&iter->tree, path, name)

/// Resets the iter and goes back to head.
void pt_iter_goto_head(play_tree_iter_t* iter);

///@}

#endif

///@}
