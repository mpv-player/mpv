
#ifndef __CFG_H
#define __CFG_H

extern int gtkEnableAudioEqualizer;

extern int    gtkVopPP;
extern int    gtkVopLAVC;
extern int    gtkVopFAME;

extern int    gtkAONoSound;
extern float  gtkAODelay;
extern int    gtkAONorm;
extern int    gtkAOFakeSurround;
extern int    gtkAOExtraStereo;
extern float  gtkAOExtraStereoMul;
extern char * gtkAOOSSMixer;
extern char * gtkAOOSSDevice;

extern int    gtkSubDumpMPSub;
extern int    gtkSubDumpSrt;

extern char * gtkEquChannel1;
extern char * gtkEquChannel2;
extern char * gtkEquChannel3;
extern char * gtkEquChannel4;
extern char * gtkEquChannel5;
extern char * gtkEquChannel6;

extern int cfg_read( void );
extern int cfg_write( void );

#endif
