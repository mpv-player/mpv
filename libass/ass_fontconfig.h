#ifndef __ASS_FONTCONFIG_H__
#define __ASS_FONTCONFIG_H__

typedef struct fc_instance_s fc_instance_t;

fc_instance_t* fontconfig_init(const char* dir, const char* family, const char* path);
char* fontconfig_select(fc_instance_t* priv, const char* family, unsigned bold, unsigned italic, int* index);
void fontconfig_done(fc_instance_t* priv);

#endif

