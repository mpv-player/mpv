    quvi_t q;
    if (quvi_init(&q) == QUVI_OK)
        quvi_supported(q, "http://nope");
