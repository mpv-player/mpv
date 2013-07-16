#include <libavfilter/avfilter.h>
void vf_next_query_format() {}
int main(void) {
    avfilter_register_all();
    vf_next_query_format();
    return 0;
}
