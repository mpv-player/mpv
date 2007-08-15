/* -*- c-basic-offset: 2; indent-tabs-mode: nil -*- */
#ifndef DVDREAD_INTERNAL_H
#define DVDREAD_INTERNAL_H


#define CHECK_VALUE(arg)


int get_verbose(void);
int dvdread_verbose(dvd_reader_t *dvd);
dvd_reader_t *device_of_file(dvd_file_t *file);

#endif /* DVDREAD_INTERNAL_H */
