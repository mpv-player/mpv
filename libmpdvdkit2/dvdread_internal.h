#ifndef DVDREAD_INTERNAL_H
#define DVDREAD_INTERNAL_H


#define CHECK_VALUE(arg) \
 if(!(arg)) { \
   fprintf(stderr, "\n*** libdvdread: CHECK_VALUE failed in %s:%i ***" \
                   "\n*** for %s ***\n\n", \
                   __FILE__, __LINE__, # arg ); \
 }

#endif /* DVDREAD_INTERNAL_H */
