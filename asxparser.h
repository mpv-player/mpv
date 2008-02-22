#ifndef MPLAYER_ASXPARSER_H
#define MPLAYER_ASXPARSER_H

typedef struct ASX_Parser_t ASX_Parser_t;

typedef struct {
  char* buffer;
  int line;
} ASX_LineSave_t;

struct ASX_Parser_t {
  int line; // Curent line
  ASX_LineSave_t *ret_stack;
  int ret_stack_size;
  char* last_body;
  int deep;
};
  
ASX_Parser_t*
asx_parser_new(void);

void
asx_parser_free(ASX_Parser_t* parser);

/*
 * Return -1 on error, 0 when nothing is found, 1 on sucess
 */
int
asx_get_element(ASX_Parser_t* parser,char** _buffer,
		char** _element,char** _body,char*** _attribs);

int
asx_parse_attribs(ASX_Parser_t* parser,char* buffer,char*** _attribs);

/////// Attribs utils

char*
asx_get_attrib(const char* attrib,char** attribs);

int
asx_attrib_to_enum(const char* val,char** valid_vals);

#define asx_free_attribs(a) asx_list_free(&a,free)

////// List utils

typedef void (*ASX_FreeFunc)(void* arg);

void
asx_list_free(void* list_ptr,ASX_FreeFunc free_func);

#endif /* MPLAYER_ASXPARSER_H */
