
struct menu_priv_s;
typedef struct  menu_s menu_t;

struct  menu_s {
  struct MPContext *ctx;
  void (*draw)(menu_t* menu,mp_image_t* mpi);
  void (*read_cmd)(menu_t* menu,int cmd);
  void (*read_key)(menu_t* menu,int cmd);
  void (*close)(menu_t* menu);
  m_struct_t* priv_st;
  struct menu_priv_s* priv;
  int show; // Draw it ?
  int cl; // Close request (user sent a close cmd or
  menu_t* parent;
};

typedef struct menu_info_s {
  const char *info;
  const char *name;
  const char *author;
  const char *comment;
  m_struct_t priv_st; // Config struct definition
  // cfg is a config struct as defined in cfg_st, it may be used as a priv struct
  // cfg is filled from the attributs found in the cfg file
  // the args param hold the content of the balise in the cfg file (if any)
  int (*open)(menu_t* menu, char* args);
} menu_info_t;


#define MENU_CMD_UP 0
#define MENU_CMD_DOWN 1
#define MENU_CMD_OK 2
#define MENU_CMD_CANCEL 3
#define MENU_CMD_LEFT 4
#define MENU_CMD_RIGHT 5 
#define MENU_CMD_ACTION 6

/// Global init/uninit
int menu_init(struct MPContext *mpctx, char* cfg_file);
void menu_unint(void);

/// Open a menu defined in the config file
menu_t* menu_open(char *name);

void menu_draw(menu_t* menu,mp_image_t* mpi);
void menu_read_cmd(menu_t* menu,int cmd);
void menu_close(menu_t* menu);
void menu_read_key(menu_t* menu,int cmd);

//// Default implementation
void menu_dflt_read_key(menu_t* menu,int cmd);

/////////// Helpers

#define MENU_TEXT_TOP	(1<<0)
#define MENU_TEXT_VCENTER	(1<<1)
#define MENU_TEXT_BOT	(1<<2)
#define MENU_TEXT_VMASK	(MENU_TEXT_TOP|MENU_TEXT_VCENTER|MENU_TEXT_BOT)
#define MENU_TEXT_LEFT	(1<<3)
#define MENU_TEXT_HCENTER	(1<<4)
#define MENU_TEXT_RIGHT	(1<<5)
#define MENU_TEXT_HMASK	(MENU_TEXT_LEFT|MENU_TEXT_HCENTER|MENU_TEXT_RIGHT)
#define MENU_TEXT_CENTER	(MENU_TEXT_VCENTER|MENU_TEXT_HCENTER)

void menu_draw_text(mp_image_t* mpi, char* txt, int x, int y);
int menu_text_length(char* txt);
int menu_text_num_lines(char* txt, int max_width);

void menu_text_size(char* txt,int max_width, 
		    int vspace, int warp,
		    int* _w, int* _h);

void menu_draw_text_full(mp_image_t* mpi,char* txt,
			 int x, int y,int w, int h,
			 int vspace, int warp, int align, int anchor);

void menu_draw_box(mp_image_t* mpi, unsigned char grey, unsigned char alpha, int x, int y, int w, int h);
