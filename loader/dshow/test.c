#include <stdio.h>
#include <stdlib.h>

#include "formats.h"
#include "com.h"

#include "DS_VideoDec.h"

int main(int argc,char* argv[]){
    FILE *f;
    BITMAPINFOHEADER bih;
    int len;
    char *src;
    char *dst=0;
    GUID CLSID_DivxDecompressorCF={0x82CCd3E0, 0xF71A, 0x11D0,
    { 0x9f, 0xe5, 0x00, 0x60, 0x97, 0x78, 0xaa, 0xaa}};

    f=fopen("test.divx","rb");
    
    fread(&bih,sizeof(BITMAPINFOHEADER),1,f);
    printf("frame dim: %d x %d \n",(int)bih.biWidth,(int)bih.biHeight);

    src=malloc(512000);
    len=fread(src,1,512000,f);
    printf("frame len = %d\n",len);

    DS_VideoDecoder_Open("divx_c32.ax", &CLSID_DivxDecompressorCF, &bih, 0, &dst);

//    DS_VideoDecoder_SetDestFmt(16,fccYUY2);
    DS_VideoDecoder_SetDestFmt(24,0);

    printf("DivX setting result = %d\n", DS_SetAttr_DivX("Quality",4) );

    DS_VideoDecoder_Start();

    printf("DivX setting result = %d\n", DS_SetValue_DivX("Brightness",60) );

    DS_VideoDecoder_DecodeFrame(src, len, 1, 1);

#if 0
    f2=fopen("test.yuy2","wb");
    fwrite(dst,bih.biWidth*bih.biHeight*2,1,f2);
    fclose(f2);
#endif    

      { unsigned char raw_head[32];
        FILE *f=fopen("test.raw","wb");

        strcpy((char*)raw_head,"mhwanh");
        raw_head[7]=4;
        raw_head[8]=bih.biWidth>>8;
        raw_head[9]=bih.biWidth&0xFF;
        raw_head[10]=bih.biHeight>>8;
        raw_head[11]=bih.biHeight&0xFF;
        raw_head[12]=raw_head[13]=0; // 24bit
        raw_head[14]=1;raw_head[15]=0x2C;
        raw_head[16]=1;raw_head[17]=0x2C;
        memset(raw_head+18,0,32-18);
        fwrite(raw_head,32,1,f);
        
        fwrite(dst,bih.biWidth*bih.biHeight*3,1,f);
        fclose(f);
      }


return 0;
}
