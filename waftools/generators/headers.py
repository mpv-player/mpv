def __cp_to_variant__(ctx, variant, basename):
    src = ctx.bldnode.search_node(basename).read()
    node = ctx.bldnode.make_node("{0}/{1}".format(variant, basename))
    node.parent.mkdir()
    node.write(src)

def __write_config_h__(ctx):
    ctx.start_msg("Writing configuration header:")
    ctx.write_config_header('config.h')
    __cp_to_variant__(ctx, ctx.options.variant, 'config.h')
    ctx.end_msg("config.h", "PINK")

# Approximately escape the string as C string literal
def __escape_c_string(s):
    return s.replace("\"", "\\\"").replace("\n", "\\n")

def __get_features_string__(ctx):
    import inflector
    stuff = []
    for dependency_identifier in ctx.satisfied_deps:
        defkey = inflector.define_key(dependency_identifier)
        if ctx.is_defined(defkey) and ctx.get_define(defkey) == "1":
            stuff.append(dependency_identifier)
    stuff.sort()
    return " ".join(stuff)

def __add_mpv_defines__(ctx):
    from sys import argv
    ctx.define("CONFIGURATION", " ".join(argv))
    ctx.define("MPV_CONFDIR", ctx.env.CONFLOADDIR)
    ctx.define("FULLCONFIG", __escape_c_string(__get_features_string__(ctx)))

def configure(ctx):
    __add_mpv_defines__(ctx)
    __write_config_h__(ctx)
