
#ifndef __CFG_H
#define __CFG_H

extern int gtkEnableAudioEqualizer;
extern int gtkEnableVideoEqualizer;

extern char * gtkVODriver;
extern int    gtkVODoubleBuffer;
extern int    gtkVODirectRendering;

extern int    gtkVFrameDrop;
extern int    gtkVHardFrameDrop;
extern int    gtkVNIAVI;
extern int    gtkVFlip;
extern int    gtkVIndex;
extern int    gtkVVFM;
extern int    gtkVPP;
extern int    gtkVAutoq;

extern char * gtkAODriver;
extern int    gtkAONoSound;
extern float  gtkAODelay;
extern int    gtkAONorm;
extern int    gtkAOFakeSurround;
extern int    gtkAOExtraStereo;
extern float  gtkAOExtraStereoMul;
extern char * gtkAOOSSMixer;
extern char * gtkAOOSSDevice;

extern int    gtkSubAuto; 
extern int    gtkSubUnicode; 
extern int    gtkSubDumpMPSub;
extern int    gtkSubDumpSrt;
extern float  gtkSubDelay;
extern float  gtkSubFPS;
extern int    gtkSubPos;
extern float  gtkSubFFactor;

extern char * gtkEquChannel1;
extern char * gtkEquChannel2;
extern char * gtkEquChannel3;
extern char * gtkEquChannel4;
extern char * gtkEquChannel5;
extern char * gtkEquChannel6;

extern int cfg_read( void );
extern int cfg_write( void );

#endif
