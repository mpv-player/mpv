#include <stdio.h>

int main(void){
int c;
unsigned int head=-1;
int pos=-3;

while((c=getchar())>=0){
    head<<=8;
    if(head==0x100){
	int startcode=head|c;
        printf("%08X 1%02X ",pos,c);
            if     (startcode<=0x11F) printf("Video Object Start");
            else if(startcode<=0x12F) printf("Video Object Layer Start");
            else if(startcode<=0x13F) printf("Reserved");
            else if(startcode<=0x15F) printf("FGS bp start");
            else if(startcode<=0x1AF) printf("Reserved");
            else if(startcode==0x1B0) printf("Visual Object Seq Start");
            else if(startcode==0x1B1) printf("Visual Object Seq End");
            else if(startcode==0x1B2) printf("User Data");
            else if(startcode==0x1B3) printf("Group of VOP start");
            else if(startcode==0x1B4) printf("Video Session Error");
            else if(startcode==0x1B5) printf("Visual Object Start");
            else if(startcode==0x1B6) printf("Video Object Plane start");
            else if(startcode==0x1B7) printf("slice start");
            else if(startcode==0x1B8) printf("extension start");
            else if(startcode==0x1B9) printf("fgs start");
            else if(startcode==0x1BA) printf("FBA Object start");
            else if(startcode==0x1BB) printf("FBA Object Plane start");
            else if(startcode==0x1BC) printf("Mesh Object start");
            else if(startcode==0x1BD) printf("Mesh Object Plane start");
            else if(startcode==0x1BE) printf("Still Textutre Object start");
            else if(startcode==0x1BF) printf("Textutre Spatial Layer start");
            else if(startcode==0x1C0) printf("Textutre SNR Layer start");
            else if(startcode==0x1C1) printf("Textutre Tile start");
            else if(startcode==0x1C2) printf("Textutre Shape Layer start");
            else if(startcode==0x1C3) printf("stuffing start");
            else if(startcode<=0x1C5) printf("reserved");
            else if(startcode<=0x1FF) printf("System start");
	    printf("\n");
    }
    head|=c;
    ++pos;
}

return 0;
}
