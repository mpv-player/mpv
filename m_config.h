#ifndef _M_CONFIG_H
#define _M_CONFIG_H

typedef struct m_config_option m_config_option_t;
typedef struct m_config_save_slot m_config_save_slot_t;
typedef struct m_profile m_profile_t;
struct m_option;
struct m_option_type;

struct m_config_save_slot {
  m_config_save_slot_t* prev;
  int lvl;
  // we have to store other datatypes in this as well,
  // so make sure we get properly aligned addresses
  unsigned char data[0] __attribute__ ((aligned (8)));
};

struct m_config_option {
  m_config_option_t* next;
  char* name; // Full name (ie option:subopt)
  struct m_option* opt;
  m_config_save_slot_t* slots;
  unsigned int flags; // currently it only tell if the option was set
};

struct m_profile {
  m_profile_t* next;
  char* name;
  char* desc;
  int num_opts;
  char** opts;
};

typedef struct m_config {
  m_config_option_t* opts;
  int lvl; // Current stack level
  int mode;
  m_profile_t* profiles;
  int profile_depth;
  struct m_option* self_opts;
} m_config_t;

#define M_CFG_OPT_SET    (1<<0)
#define M_CFG_OPT_ALIAS  (1<<1)


//////////////////////////// Functions ///////////////////////////////////

m_config_t*
m_config_new(void);

void
m_config_free(m_config_t* config);

void
m_config_push(m_config_t* config);

void
m_config_pop(m_config_t* config);

int
m_config_register_options(m_config_t *config, struct m_option *args);

int
m_config_set_option(m_config_t *config, char* arg, char* param);

int
m_config_check_option(m_config_t *config, char* arg, char* param);

struct m_option*
m_config_get_option(m_config_t *config, char* arg);

void
m_config_print_option_list(m_config_t *config);

m_profile_t*
m_config_get_profile(m_config_t* config, char* name);

m_profile_t*
m_config_add_profile(m_config_t* config, char* name);

void
m_profile_set_desc(m_profile_t* p, char* desc);

int
m_config_set_profile_option(m_config_t* config, m_profile_t* p,
			    char* name, char* val);

#endif /* _M_CONFIG_H */
