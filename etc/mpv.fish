#
# This file is part of mpv.
#
# mpv is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# mpv is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
#
#

# Add the completion for mpv. The first argument is the option with the dash
# included and the rest of the arguments are directly passed to complete
function _complete_mpv
    set --local opt $argv[1]
    set --local opt_type --short-option
    if string match -q -r '--' -- $opt
        set opt_type --long-option
    end
    complete --command mpv $opt_type (string trim --left --chars '-' -- $opt) $argv[2..]
end

function _complete_mpv_run_help
    mpv $argv \
        # Remove the text before the option list
        | string match --regex --invert '^\w.*:' \
        | string trim
end

# Print the possible values for the option
function _complete_mpv_option -a opt
    # We insert a tab so that it is recognized as the description of the argument
    set --local value_description_rewrite 's/ /\t/'
    switch $opt
        case "--audio-device" "--vulkan-device"
            # The value is enclosed in single quotes and the description in parentheses
            set value_description_rewrite 's/\'([^\']+)\'\s+\((.*)\)$/\1\t\2/'
    end

    _complete_mpv_run_help $opt=help \
        | sed -E $value_description_rewrite
end

# Print the possible values for the image option
function _complete_mpv_image_option -a opt
    mpv $opt=help | sed -e 's/.*: //' -e 's/ /\n/g'
end

# Print the possible values for the profile option
function _complete_mpv_profile_option -a opt
    _complete_mpv_run_help $opt=
end

# Cache all the mpv options
set --local mpv_options (mpv --no-config --list-options | string match --entire --regex '^\s*--' | string trim)

for opt_line in $mpv_options
    set --local opt_line_split (string split --max 1 ' ' -- $opt_line | string trim)
    set --local opt $opt_line_split[1]
    set --local doc $opt_line_split[2]
    set --local type (string split --field 1 ' ' $doc)
    if test $opt = '--show-profile'
        set type 'Profile'
    end
    switch $type
        case String
            if string match -q -r '\[file\]' $doc
                _complete_mpv $opt --force-files
            else
                _complete_mpv $opt --exclusive --arguments "help (_complete_mpv_option $opt)"
            end
        case Flag
            if string match -q -r '\[not in config files\]' $doc
                _complete_mpv $opt
            else
                _complete_mpv $opt --exclusive --arguments 'yes no help'
            end
        case 'Choices:' Object
            _complete_mpv $opt --exclusive --arguments "help (_complete_mpv_option $opt)"
        case Image
            _complete_mpv $opt --exclusive --arguments "help (_complete_mpv_image_option $opt)"
        case Profile
            _complete_mpv $opt --exclusive --arguments "help (_complete_mpv_profile_option $opt)"
        case '*'
            # Unimplemented categories
            _complete_mpv $opt
    end
end
