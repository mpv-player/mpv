
#ifndef NEW_CONFIG
#warning "Including m_config.h but NEW_CONFIG is disabled"
#else

typedef struct m_config_option m_config_option_t;
typedef struct m_config_save_slot m_config_save_slot_t;
struct m_option;
struct m_option_type;

struct m_config_save_slot {
  m_config_save_slot_t* prev;
  int lvl;
  unsigned char data[0];
};

struct m_config_option {
  m_config_option_t* next;
  char* name; // Full name (ie option:subopt)
  struct m_option* opt;
  m_config_save_slot_t* slots;
  unsigned int flags; // currently it only tell if the option was set
};

typedef struct m_config {
  m_config_option_t* opts;
  int lvl; // Current stack level
  int mode;
} m_config_t;


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

/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// Backward compat. stuff ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////

typedef struct config config_t;
struct config {
  char *name;
  void *p; 
  struct m_option_type* type;
  unsigned int flags;
  float min,max;
  void* priv;
};


#define CONF_MIN		(1<<0)
#define CONF_MAX		(1<<1)
#define CONF_RANGE	(CONF_MIN|CONF_MAX)
#define CONF_NOCFG	(1<<2)
#define CONF_NOCMD	(1<<3)
#define CONF_GLOBAL	(1<<4)
#define CONF_NOSAVE	(1<<5)
#define CONF_OLD		(1<<6)

#define ERR_NOT_AN_OPTION	 -1
#define ERR_MISSING_PARAM	 -2
#define ERR_OUT_OF_RANGE	 -3
#define ERR_FUNC_ERR	 -4

#endif
