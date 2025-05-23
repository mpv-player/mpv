#include "common/common.h"
#include "osdep/timer.h"
#include "test_utils.h"

#include <time.h>
#include <limits.h>

int main(void)
{
    mp_time_init();

    /* timekeeping */
    {
        int64_t now = mp_time_ns();
        assert_true(now >= 0);

        mp_sleep_ns(MP_TIME_MS_TO_NS(10));

        int64_t now2 = mp_time_ns();
        assert_true(now2 > now);

        mp_sleep_ns(MP_TIME_MS_TO_NS(10));

        double now3 = mp_time_sec();
        assert_true(now3 > MP_TIME_NS_TO_S(now2));
    }

    /* arithmetic */
    {
        const int64_t test = 123456;
        assert_int_equal(mp_time_ns_add(test, 1.0), test + MP_TIME_S_TO_NS(1));
        assert_int_equal(mp_time_ns_add(test, DBL_MAX), INT64_MAX);
        assert_int_equal(mp_time_ns_add(test, -1e13), 1);

        const int64_t test2 = INT64_MAX - MP_TIME_S_TO_NS(20);
        assert_int_equal(mp_time_ns_add(test2, 20.44), INT64_MAX);
    }

    return 0;
}
