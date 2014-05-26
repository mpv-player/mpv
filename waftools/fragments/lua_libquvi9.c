    quvi_t q = quvi_new();
    if (quvi_ok(q))
        quvi_supports(q, "http://nope", QUVI_SUPPORTS_MODE_OFFLINE, QUVI_SUPPORTS_TYPE_MEDIA);
