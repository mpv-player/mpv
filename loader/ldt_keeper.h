#ifndef LDT_KEEPER_H
#define LDT_KEEPER_H

#ifdef __cplusplus
extern "C"
{
#endif
void Setup_FS_Segment(void);
void Setup_LDT_Keeper(void);
void Restore_LDT_Keeper(void);
#ifdef __cplusplus
}
#endif

#endif /* LDT_KEEPER_H */
