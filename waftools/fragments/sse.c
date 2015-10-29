#pragma GCC push_options
#pragma GCC target("sse4.1")
#include <smmintrin.h>

void *a_ptr;

int main(void)
{
    __m128i xmm0;
    __m128i* p = (__m128i*)a_ptr;

    _mm_sfence();

    xmm0  = _mm_stream_load_si128(p + 1);
    _mm_store_si128(p + 2, xmm0);

    return 0;
}
