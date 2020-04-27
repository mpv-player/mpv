#include <locale.h>
#include "osdep/io.h"

#ifdef __APPLE__
# include <string.h>
# include <xlocale.h>
#endif

struct mp_log;

struct mp_archive {
    locale_t locale;
    struct mp_log *log;
    struct archive *arch;
    struct stream *primary_src;
    char buffer[4096];
    int flags;
    int num_volumes; // INT_MAX if unknown (initial state)

    // Current entry, as set by mp_archive_next_entry().
    struct archive_entry *entry;
    char *entry_filename;
    int entry_num;
};

void mp_archive_free(struct mp_archive *mpa);

#define MP_ARCHIVE_FLAG_UNSAFE          (1 << 0)
#define MP_ARCHIVE_FLAG_NO_VOLUMES      (1 << 1)
#define MP_ARCHIVE_FLAG_PRIV            (1 << 2)

struct mp_archive *mp_archive_new(struct mp_log *log, struct stream *src,
                                  int flags, int max_volumes);

bool mp_archive_next_entry(struct mp_archive *mpa);
