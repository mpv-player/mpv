from waflib.Options import OptionsContext
import optparse

class Feature(object):
    def __init__(self, group, feature):
        self.group = group
        self.identifier, self.attributes = feature['name'], feature

    def add_options(self):
        [self.add_option(option_rule) for option_rule in self.option_rules()]

    def add_option(self, rule):
        self.group.add_option(self.option(rule['state']),
                              action=rule['action'],
                              default=rule['default'],
                              dest=self.storage(),
                              help=self.help(rule['state']))

    # private
    def option_rules(self):
        return {
            'autodetect': [
                {'state': 'disable', 'action': 'store_false', 'default': None},
                {'state': 'enable',  'action': 'store_true',  'default': None},
            ],
            'disable': [
                {'state': 'disable', 'action': 'store_false', 'default': False},
                {'state': 'enable',  'action': 'store_true',  'default': False},
            ],
            'enable': [
                {'state': 'disable', 'action': 'store_false', 'default': True},
                {'state': 'enable',  'action': 'store_true',  'default': True},
            ],
        }[self.behaviour()]


    def behaviour(self):
        if 'default' in self.attributes:
            return self.attributes['default']
        else:
            return 'autodetect'


    def option(self, state):
        return "--{0}-{1}".format(state, self.identifier)

    def help(self, state):
        default = self.behaviour()
        if (default, state) == ("autodetect", "enable") or default == state:
            return optparse.SUPPRESS_HELP
        return "{0} {1} [{2}]" \
            .format(state, self.attributes['desc'], default)

    def storage(self):
        return "enable_{0}".format(self.identifier)

def add_feature(group, feature):
    Feature(group, feature).add_options()

def parse_features(opt, group_name, features):
    def is_feature(dep):
        return dep['name'].find('--') >= 0

    def strip_feature(dep):
        dep['name'] = dep['name'].lstrip('-')
        return dep

    features = [strip_feature(dep) for dep in features if is_feature(dep)]
    group = opt.get_option_group(group_name)
    if not group:
        group = opt.add_option_group(group_name)
    [add_feature(group, feature) for feature in features]

OptionsContext.parse_features = parse_features
