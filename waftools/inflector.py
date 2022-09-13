import re

def sanitize_id(word):
    """ Converts a word "into_it_s_underscored_version"
    Convert any "CamelCased" or "ordinary Word" into an
    "underscored_word"."""

    return  re.sub('[^A-Z^a-z^0-9]+', '_', \
            re.sub('([a-z\d])([A-Z])', '\\1_\\2', \
            re.sub('([A-Z]+)([A-Z][a-z])', '\\1_\\2', re.sub('::', '/', word)))).lower()

def storage_key(dep):
    return sanitize_id(dep)

def define_key(dep):
    return ("have_" + storage_key(dep)).upper()

def define_dict(dep):
    return {'define_name': define_key(dep)}

def storage_dict(dep):
    return {'uselib_store': storage_key(dep)}
