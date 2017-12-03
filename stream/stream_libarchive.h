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

    // Current entry, as set by mp_archive_next_entry().
    struct archive_entry *entry;
    char *entry_filename;
    int entry_num;
};

void mp_archive_free(struct mp_archive *mpa);

#define MP_ARCHIVE_FLAG_UNSAFE 1
struct mp_archive *mp_archive_new(struct mp_log *log, struct stream *src,
                                  int flags);

bool mp_archive_next_entry(struct mp_archive *mpa);
