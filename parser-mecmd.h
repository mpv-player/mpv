
/// A simple parser with per-entry settings.

typedef struct m_entry_st {
  char* name; // Filename, url or whatever
  char** opts; // NULL terminated list of name,val pairs 
} m_entry_t;

// Free a list returned by m_config_parse_command_line
void
m_entry_list_free(m_entry_t* lst);
// Use this when you switch to another entry
int
m_entry_set_options(m_config_t *config, m_entry_t* entry);

m_entry_t*
m_config_parse_me_command_line(m_config_t *config, int argc, char **argv);

