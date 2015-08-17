struct mp_log;

struct mp_archive {
    struct archive *arch;
    struct stream *src;
    char buffer[4096];
};

void mp_archive_free(struct mp_archive *mpa);

#define MP_ARCHIVE_FLAG_UNSAFE 1
struct mp_archive *mp_archive_new(struct mp_log *log, struct stream *src,
                                  int flags);
