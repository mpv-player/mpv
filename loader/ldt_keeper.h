#ifndef MPLAYER_LDT_KEEPER_H
#define MPLAYER_LDT_KEEPER_H

typedef struct {
  void* fs_seg;
  char* prev_struct;
} ldt_fs_t;

void Setup_FS_Segment(void);
ldt_fs_t* Setup_LDT_Keeper(void);
void Restore_LDT_Keeper(ldt_fs_t* ldt_fs);

#endif /* MPLAYER_LDT_KEEPER_H */
