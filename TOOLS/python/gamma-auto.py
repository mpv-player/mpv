import math
from mpvclient import mpv


def lux_to_gamma(lmin, lmax, rmin, rmax, lux):
    if lmax <= lmin or lux == 0:
        return 1

    num = (rmax - rmin) * (math.log(lux, 10) - math.log(lmin, 10))
    den = math.log(lmax, 10) - math.log(lmin, 10)
    result = num / den + rmin

    # clamp the result
    _max = max(rmax, rmin)
    _min = min(rmax, rmin)

    return max(min(result, _max), _min)


@mpv.observe_property("ambient-light", mpv.MPV_FORMAT_DOUBLE)
def lux_changed(lux):
    gamma = lux_to_gamma(16.0, 256.0, 1.0, 1.2, lux or 0)
    mpv.set_property_float("gamma-factor", gamma)
