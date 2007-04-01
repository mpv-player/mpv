
#ifndef VIDIX_DRIVERS_H
#define VIDIX_DRIVERS_H

void vidix_register_all_drivers (void);

int vidix_find_driver (VDXContext *ctx, const char *name,
                       unsigned int cap, int verbose);

#endif /* VIDIX_DRIVERS_H */
