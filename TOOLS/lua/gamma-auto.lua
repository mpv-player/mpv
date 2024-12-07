local function lux_to_gamma(lmin, lmax, rmin, rmax, lux)
    if lmax <= lmin or lux == 0 then
        return 1
    end

    local num = (rmax - rmin) * (math.log(lux, 10) - math.log(lmin, 10))
    local den = math.log(lmax, 10) - math.log(lmin, 10)
    local result = num / den + rmin

    -- clamp the result
    local max = math.max(rmax, rmin)
    local min = math.min(rmax, rmin)

    return math.max(math.min(result, max), min)
end

local function lux_changed(_, lux)
    local gamma = lux_to_gamma(16.0, 256.0, 1.0, 1.2, lux or 0)
    mp.set_property_number("gamma-factor", gamma)
end

mp.observe_property("ambient-light", "number", lux_changed)
