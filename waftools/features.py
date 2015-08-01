from waflib.Options import OptionsContext
import optparse

__OPTION_RULES__ = {
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
    ],
}

def _feature_behaviour(feature):
    return feature.get('default', 'autodetect')

def _feature_storage(feature):
    return "enable_{0}".format(feature['name'])

def _feature_option(feature, rule):
    return "--{0}-{1}".format(rule['state'], feature['name'])

def _feature_help(feature, rule):
    default = _feature_behaviour(feature)
    state   = rule['state']
    if (default, state) == ("autodetect", "enable") or default == state:
        return optparse.SUPPRESS_HELP
    return "{0} {1} [{2}]".format(state, feature['desc'], default)

def _add_option(group, feature, rule):
    group.add_option(_feature_option(feature, rule),
                     action=rule['action'],
                     default=rule['default'],
                     dest=_feature_storage(feature),
                     help=_feature_help(feature, rule))

def _add_options(group, feature):
    rules = __OPTION_RULES__[_feature_behaviour(feature)]
    [_add_option(group, feature, rule) for rule in rules]

def parse_features(opt, group_name, features):
    def _is_feature(dep):
        return dep['name'].find('--') >= 0

    def _strip(dep):
        dep['name'] = dep['name'].lstrip('-')
        return dep

    features = [_strip(feature) for feature in features if _is_feature(feature)]
    group    = opt.get_option_group(group_name)
    if not group:
        group = opt.add_option_group(group_name)
    [_add_options(group, feature) for feature in features]

OptionsContext.parse_features = parse_features
