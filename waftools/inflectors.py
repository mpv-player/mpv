import re

class DependencyInflector(object):
    def __init__(self, dependency):
        self.dep = dependency

    def storage_key(self):
        return self.__underscore__(self.dep)

    def define_key(self):
        return ("have_" + self.storage_key()).upper()

    def define_dict(self):
        return {'define_name': self.define_key()}

    def storage_dict(self):
        return {'uselib_store': self.storage_key()}

    def __underscore__(self, word):
        """ Converts a word "into_it_s_underscored_version"
        Convert any "CamelCased" or "ordinary Word" into an
        "underscored_word"."""

        return  re.sub('[^A-Z^a-z^0-9]+', '_', \
                re.sub('([a-z\d])([A-Z])', '\\1_\\2', \
                re.sub('([A-Z]+)([A-Z][a-z])', '\\1_\\2', re.sub('::', '/', word)))).lower()
