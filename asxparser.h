

typedef struct _ASX_Parser_t ASX_Parser_t;

typedef struct {
  char* buffer;
  int line;
} ASX_LineSave_t;

struct _ASX_Parser_t {
  int line; // Curent line
  ASX_LineSave_t *ret_stack;
  int ret_stack_size;
  char* last_body;
  int deep;
};
  
