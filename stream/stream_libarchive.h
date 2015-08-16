struct mp_log;

struct mp_archive {
    struct archive *arch;
    struct stream *src;
    char buffer[4096];
};

void mp_archive_free(struct mp_archive *mpa);
struct mp_archive *mp_archive_new(struct mp_log *log, struct stream *src);
