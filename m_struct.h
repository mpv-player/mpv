#ifndef M_STRUCT_H
#define M_STRUCT_H

/// \defgroup OptionsStruct Options struct
/// \ingroup Options
/// An API to manipulate structs using m_option.
///@{

/// \file m_struct.h

struct m_option;

/// Struct definition
typedef struct m_struct_st {
  /// For error messages and debugging
  const char* name;
  /// size of the whole struct
  unsigned int size;
  /// Pointer to a struct filled with the default settings
  const void* defaults;
  /// Field list.
  /** The p field of the \ref m_option struct must contain the offset
   *  of the member in the struct (use M_ST_OFF macro for this).
   */
  const struct m_option* fields;
} m_struct_t;


// From glib.h (modified ;-)

/// Get the offset of a struct field.
/** \param struct_type Struct type.
 *  \param member Name of the field.
 *  \return The offset of the field in bytes.
 */
#define M_ST_OFF(struct_type, member)    \
    ((void*) &((struct_type*) 0)->member)

/// Get a pointer to a struct field.
/** \param struct_p Pointer to the struct.
 *  \param struct_offset Offset of the field in the struct.
 *  \return Pointer to the struct field.
 */
#define M_ST_MB_P(struct_p, struct_offset)   \
    ((void *)((char *)(struct_p) + (unsigned long)(struct_offset)))

/// Access a struct field at a given offset.
/** \param member_type Type of the field.
 *  \param struct_p Pointer to the struct.
 *  \param struct_offset Offset of the field in the struct.
 *  \return The struct field at the given offset.
 */
#define M_ST_MB(member_type, struct_p, struct_offset)   \
    (*(member_type*) M_ST_MB_P ((struct_p), (struct_offset)))



/// Allocate the struct and set it to the defaults.
/** \param st Struct definition.
 *  \return The newly allocated object set to default.
 */
void*
m_struct_alloc(const m_struct_t* st);

/// Set a field of the struct.
/** \param st Struct definition.
 *  \param obj Pointer to the struct to set.
 *  \param field Name of the field to set.
 *  \param param New value of the field.
 *  \return 0 on error, 1 on success.
 */
int
m_struct_set(const m_struct_t* st, void* obj, char* field, char* param);

/// Reset a field (or all if field == NULL) to defaults.
/** \param st Struct definition.
 *  \param obj Pointer to the struct to set.
 *  \param field Name of the field to reset, if NULL all fields are reseted.
 */
void
m_struct_reset(const m_struct_t* st, void* obj, const char* field);

/// Create a copy of an existing struct.
/** \param st Struct definition.
 *  \param obj Pointer to the struct to copy.
 *  \return Newly allocated copy of obj.
 */
void*
m_struct_copy(const m_struct_t* st, void* obj);

/// Free an allocated struct.
/** \param st Struct definition.
 *  \param obj Pointer to the struct to copy.
 */
void
m_struct_free(const m_struct_t* st, void* obj);

/// Get a field description.
/** \param st Struct definition.
 *  \param f Name of the field.
 *  \return The \ref m_option struct describing the field or NULL if not found.
 */
const struct m_option*
m_struct_get_field(const m_struct_t* st,const char* f);

///@}

#endif /* M_STRUCT_H */
