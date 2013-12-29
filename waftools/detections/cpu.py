def x86(ctx):
    ctx.define('ARCH_X86', 1)
    ctx.define('ARCH_X86_32', 1)

def x86_64(ctx):
    ctx.define('ARCH_X86', 1)
    ctx.define('ARCH_X86_64', 1)
    ctx.define('HAVE_FAST_64BIT', 1)

def ia64(ctx):
    ctx.define('HAVE_FAST_64BIT', 1)

def default(ctx):
    pass

def configure(ctx):
    ctx.define('ARCH_X86', 0)
    ctx.define('ARCH_X86_32', 0)
    ctx.define('ARCH_X86_64', 0)
    ctx.define('HAVE_FAST_64BIT', 0)

    ctx.define('HAVE_MMX',   'HAVE_ASM && ARCH_X86', quote=False)
    ctx.define('HAVE_MMX2',  'HAVE_ASM && ARCH_X86', quote=False)
    ctx.define('HAVE_SSE',   'HAVE_ASM && ARCH_X86', quote=False)
    ctx.define('HAVE_SSE2',  'HAVE_ASM && ARCH_X86', quote=False)
    ctx.define('HAVE_SSSE3', 'HAVE_ASM && ARCH_X86', quote=False)

    globals().get(ctx.env.DEST_CPU, default)(ctx)
