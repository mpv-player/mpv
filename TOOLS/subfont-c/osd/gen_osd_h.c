#include <stdio.h>

int main()
{
    int c;
    int cnt;
    printf("unsigned char *osd_font_pfb = {");
    for (cnt = 0;;cnt++) {
	if (cnt % 16 == 0) printf("\n");
	c = getchar();
	if (c < 0) break;
	printf("0x%02x,", c);
    }
    printf("};\n");
}
