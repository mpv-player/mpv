#ifndef SUB_CC_H
#define SUB_CC_H

extern int subcc_enabled;

void subcc_init(void);
void subcc_process_data(unsigned char *inputdata,unsigned int len);

#endif /*SUB_CC_H*/

