
#ifndef NEW_CONFIG
#warning "Including m_struct.h but NEW_CONFIG is disabled"
#else

///////////////////// A struct setter ////////////////////////

struct m_option;

/// Struct definition
typedef struct m_struct_st {
  char* name; // For error msg and debuging
  unsigned int size; // size of the whole struct
  void* defaults; // Pointer to a struct filled with the default settings
  struct m_option* fields; // settable fields
} m_struct_t;

// Note : the p field of the m_option_t struct must contain the offset
// of the member in the struct (use M_ST_OFF macro for this).

// From glib.h (modified ;-)
#define M_ST_OFF(struct_type, member)    \
    ((void*) &((struct_type*) 0)->member)
#define M_ST_MB_P(struct_p, struct_offset)   \
    ((void*) (struct_p) + (unsigned long) (struct_offset))
#define M_ST_MB(member_type, struct_p, struct_offset)   \
    (*(member_type*) M_ST_MB_P ((struct_p), (struct_offset)))



/// Allocate the struct and set it to the defaults
void*
m_struct_alloc(m_struct_t* st);
/// Set a field of the struct
int
m_struct_set(m_struct_t* st, void* obj, char* field, char* param);
/// Reset a field (or all if field == NULL) to defaults
void
m_struct_reset(m_struct_t* st, void* obj, char* field);
/// Create a copy of an existing struct
void*
m_struct_copy(m_struct_t* st, void* obj);
/// Free an allocated struct
void
m_struct_free(m_struct_t* st, void* obj);
/// Get a field description
struct m_option*
m_struct_get_field(m_struct_t* st,char* f);

#endif
