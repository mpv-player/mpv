
#ifndef NEW_CONFIG
#warning "Including m_option.h but NEW_CONFIG is disabled"
#else

typedef struct m_option_type m_option_type_t;
typedef struct m_option m_option_t;
struct m_struct;

///////////////////////////// Options types declarations ////////////////////////////

// Simple types
extern m_option_type_t m_option_type_flag;
extern m_option_type_t m_option_type_int;
extern m_option_type_t m_option_type_float;
extern m_option_type_t m_option_type_string;
extern m_option_type_t m_option_type_string_list;
extern m_option_type_t m_option_type_position;

extern m_option_type_t m_option_type_print;
extern m_option_type_t m_option_type_print_indirect;
extern m_option_type_t m_option_type_subconfig;
extern m_option_type_t m_option_type_imgfmt;

// Func based types 
extern m_option_type_t m_option_type_func_full;
extern m_option_type_t m_option_type_func_param;
extern m_option_type_t m_option_type_func;

typedef void (*m_opt_default_func_t)(m_option_t *, char*);
typedef int (*m_opt_func_full_t)(m_option_t *, char *, char *);
typedef int (*m_opt_func_param_t)(m_option_t *, char *);
typedef int (*m_opt_func_t)(m_option_t *);
///////////// Backward compat
typedef m_opt_default_func_t cfg_default_func_t;
typedef m_opt_func_full_t cfg_func_arg_param_t;
typedef m_opt_func_param_t cfg_func_param_t;
typedef m_opt_func_t cfg_func_t;

// Track/Chapter range
// accept range in the form 1[hh:mm:ss.zz]-5[hh:mm:ss.zz]
// ommited fields are assumed to be 0
// Not finished !!!!
typedef struct {
  int idx; // in the e.g 1 or 5
  unsigned int seconds; // hh:mm:ss converted in seconds
  unsigned int sectors; // zz
} m_play_pos_t;

typedef struct {
  m_play_pos_t start;
  m_play_pos_t end;
} m_span_t;
extern m_option_type_t m_option_type_span;

typedef struct {
  void** list;
  void* name_off;
  void* info_off;
  void* desc_off;
} m_obj_list_t;

typedef struct {
  char* name;
  char** attribs;
} m_obj_settings_t;
extern m_option_type_t m_option_type_obj_settings_list;

// Presets are mean to be used with options struct


typedef struct {
  struct m_struct* in_desc;
  struct m_struct* out_desc;
  void* presets; // Pointer to an arry of struct defined by in_desc
  void* name_off; // Offset of the preset name inside the in_struct
} m_obj_presets_t;
extern m_option_type_t m_option_type_obj_presets;

// Don't be stupid keep tho old names ;-)
#define CONF_TYPE_FLAG		(&m_option_type_flag)
#define CONF_TYPE_INT		(&m_option_type_int)
#define CONF_TYPE_FLOAT		(&m_option_type_float)
#define CONF_TYPE_STRING		(&m_option_type_string)
#define CONF_TYPE_FUNC		(&m_option_type_func)
#define CONF_TYPE_FUNC_PARAM	(&m_option_type_func_param)
#define CONF_TYPE_PRINT		(&m_option_type_print)
#define CONF_TYPE_PRINT_INDIRECT (&m_option_type_print_indirect)
#define CONF_TYPE_FUNC_FULL	(&m_option_type_func_full)
#define CONF_TYPE_SUBCONFIG	(&m_option_type_subconfig)
#define CONF_TYPE_STRING_LIST           (&m_option_type_string_list)
#define CONF_TYPE_POSITION	(&m_option_type_position)
#define CONF_TYPE_IMGFMT		(&m_option_type_imgfmt)
#define CONF_TYPE_SPAN		(&m_option_type_span)
#define CONF_TYPE_OBJ_SETTINGS_LIST (&m_option_type_obj_settings_list)
#define CONF_TYPE_OBJ_PRESETS (&m_option_type_obj_presets)

/////////////////////////////////////////////////////////////////////////////////////////////

struct m_option_type {
  char* name;
  char* comments; // syntax desc, etc
  unsigned int size; // size needed for a save slot
  unsigned int flags;

  // parse is the only requiered function all others can be NULL
  // If dst if non-NULL it should create/update the save slot
  // If dst is NULL it should just test the validity of the arg if possible
  // Src tell from where come this setting (ie cfg file, command line, playlist, ....
  // It should return 1 if param was used, 0 if not.
  // On error it must return 1 of the error code below
  int (*parse)(m_option_t* opt,char *name, char *param, void* dst, int src);
  // Print back a value in human form
  char* (*print)(m_option_t* opt,  void* val);

  // These 3 will be a memcpy in 50% of the case, it's called to save/restore the status of
  // the var it's there for complex type like CONF_TYPE_FUNC*
  // update a save slot (dst) from the current value in the prog (src)
  void (*save)(m_option_t* opt,void* dst, void* src);
  // set the current value (dst) from a save slot
  void (*set)(m_option_t* opt,void* dst, void* src);
  // Copy betewen 2 slot (if NULL and size > 0 a memcpy will be used
  void (*copy)(m_option_t* opt,void* dst, void* src);
  // Free the data allocated for a save slot if needed
  void (*free)(void* dst);
};

/// This is the same thing as a struct config it have been renamed
/// to remove this config_t, m_config_t mess. Sorry about that,
/// config_t is still provided for backward compat.
struct m_option {
  char *name;
  void *p; 
  m_option_type_t* type;
  unsigned int flags;
  float min,max;
  // This used to be function pointer to hold a 'reverse to defaults' func.
  // Nom it can be used to pass any type of extra args.
  // Passing a 'default func' is still valid for all func based option types
  void* priv; // Type dependent data (for all kind of extended setting)
};


//////////////////////////////// Option flags /////////////////////////////////

// Option flags
#define M_OPT_MIN		(1<<0)
#define M_OPT_MAX		(1<<1)
#define M_OPT_RANGE		(M_OPT_MIN|M_OPT_MAX)
#define M_OPT_NOCFG		(1<<2)
#define M_OPT_NOCMD		(1<<3)
// This option is global : it won't be saved on push and the command
// line parser will set it when it's parsed (ie. it won't be set later)
// e.g options : -v, -quiet
#define M_OPT_GLOBAL		(1<<4)
// Do not save this option : it won't be saved on push but the command
// line parser will put it with it's entry (ie : it may be set later)
// e.g options : -include
#define M_OPT_NOSAVE		(1<<5)
// Emulate old behaviour by pushing the option only if it was set by the user
#define M_OPT_OLD		(1<<6)


///////////////////////////// Option type flags ///////////////////////////////////

// These flags are for the parsers. The description here apply to the m_config_t
// based parsers (ie. cmd line and config file parsers)
// Some parser will refuse option types that have some of these flags

// This flag is used for the subconfig
// When this flag is set, opt->p should point to another m_option_t array
// Only the parse function will be called. If dst is set, it should create/update
// an array of char* containg opt/val pairs.
// Then the options in the child array will then be set automaticly.
// You can only affect the way suboption are parsed.
// Also note that suboptions may be directly accessed by using -option:subopt blah :-)
#define M_OPT_TYPE_HAS_CHILD		(1<<0)
// If this flag is set the option type support option name with * at the end (used for -aa*)
// This only affect the option name matching, the option type have to implement
// the needed stuff.
#define M_OPT_TYPE_ALLOW_WILDCARD	(1<<1)
// This flag indicate that the data is dynamicly allocated (opt->p point to a pointer)
// It enable a little hack wich replace the initial value by a dynamic copy
// in case the initial value is staticly allocated (pretty common with strings)
#define M_OPT_TYPE_DYNAMIC		(1<<2)
/// If this is set the parse function doesn't directly return
// the wanted thing. Options use this if for some reasons they have to wait
// until the set call to be able to correctly set the target var.
// So for those types you have to first parse and then set the target var
// If this flag isn't set you can parse directly to the target var
// It's used for the callback based option as the callback call may append
// later on.
#define M_OPT_TYPE_INDIRECT		(1<<3)


///////////////////////////// Parser flags ////////////////////////////////////////

// Config mode : some parser type behave differently depending
// on config->mode value wich is passed in the src param of parse()
#define M_CONFIG_FILE 0
#define M_COMMAND_LINE 1

// Option parser error code
#define M_OPT_UNKNOW		-1
#define M_OPT_MISSING_PARAM	-2
#define M_OPT_INVALID		-3
#define M_OPT_OUT_OF_RANGE	-4
#define M_OPT_PARSER_ERR		-5

m_option_t* m_option_list_find(m_option_t* list,char* name);

inline static int
m_option_parse(m_option_t* opt,char *name, char *param, void* dst, int src) {
  return opt->type->parse(opt,name,param,dst,src);
}

inline static  char*
m_option_print(m_option_t* opt,  void* val_ptr) {
  if(opt->type->print)
    return opt->type->print(opt,val_ptr);
  else
    return (char*)-1;
}

inline static  void
m_option_save(m_option_t* opt,void* dst, void* src) {
  if(opt->type->save)
    opt->type->save(opt,dst,src);
}

inline static  void
m_option_set(m_option_t* opt,void* dst, void* src) {
  if(opt->type->set)
    opt->type->set(opt,dst,src);
}

inline  static void
m_option_copy(m_option_t* opt,void* dst, void* src) {
  if(opt->type->copy)
    opt->type->set(opt,dst,src);
  else if(opt->type->size > 0)
    memcpy(dst,src,opt->type->size);
}

inline static void
m_option_free(m_option_t* opt,void* dst) {
  if(opt->type->free)
    opt->type->free(dst);
}

#endif
