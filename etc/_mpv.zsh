#compdef mpv

# ZSH completion for mpv
#
# For customization, see:
#  https://github.com/mpv-player/mpv/wiki/Zsh-completion-customization

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

local curcontext="$curcontext" state state_descr line
typeset -A opt_args

local -a match mbegin mend
local MATCH MBEGIN MEND

# By default, don't complete URLs unless no files match
local -a tag_order
zstyle -a ":completion:*:*:$service:*" tag-order tag_order ||
  zstyle  ":completion:*:*:$service:*" tag-order '!urls'

typeset -ga _mpv_completion_arguments _mpv_completion_protocols

function _mpv_generate_arguments {

  _mpv_completion_arguments=()

  local -a option_aliases=()

  local list_options_line
  for list_options_line in "${(@f)$($~words[1] --list-options)}"; do

    [[ $list_options_line =~ $'^[ \t]+--([^ \t]+)[ \t]*(.*)' ]] || continue

    local name=$match[1] desc=$match[2]

    if [[ $desc == Flag* ]]; then

      _mpv_completion_arguments+="$name"
      if [[ $name != (\{|\}|v|list-options) ]]; then
        # Negated version
        _mpv_completion_arguments+="no-$name"
      fi

    elif [[ -z $desc ]]; then

      # Sub-option for list option

      if [[ $name == *-(clr|help) ]]; then
        # Like a flag
        _mpv_completion_arguments+="$name"
      else
        # Find the parent option and use that with this option's name
        _mpv_completion_arguments+="${_mpv_completion_arguments[(R)${name%-*}=*]/*=/$name=}"
      fi

    elif [[ $desc == Print* ]]; then

      _mpv_completion_arguments+="$name"

    elif [[ $desc =~ $'^alias for (--)?([^ \t]+)' ]]; then

      # Save this for later; we might not have parsed the target option yet
      option_aliases+="$name $match[2]"

    else

      # Option takes argument

      local entry="$name=-:${desc//:/\\:}:"

      if [[ $desc =~ '^Choices: ([^(]*)' ]]; then

        local -a choices=(${(s: :)match[1]})
        entry+="($choices)"
        # If "no" is one of the choices, it can also be negated like a flag
        # (--no-whatever is equivalent to --whatever=no).
        if (( ${+choices[(r)no]} )); then
          _mpv_completion_arguments+="no-$name"
        fi

      elif [[ $desc == *'[file]'* ]]; then

        entry+='->files'

      elif [[ $name == (ao|vo|af|vf|profile|audio-device|vulkan-device) ]]; then

        entry+="->parse-help-$name"

      elif [[ $name == show-profile ]]; then

        entry+="->parse-help-profile"

      fi

      _mpv_completion_arguments+="$entry"

    fi

  done

  # Process aliases
  local to_from real_name arg_spec
  for to_from in $option_aliases; do
    # to_from='alias-name real-name'
    real_name=${to_from##* }
    for arg_spec in "$real_name" "$real_name=*" "no-$real_name"; do
      arg_spec=${_mpv_completion_arguments[(r)$arg_spec]}
      [[ -n $arg_spec ]] &&
        _mpv_completion_arguments+="${arg_spec/$real_name/${to_from%% *}}"
    done
  done

  # Older versions of zsh have a bug where they won't complete an option listed
  # after one that's a prefix of it. To work around this, we can sort the
  # options by length, longest first, so that any prefix of an option will be
  # listed after it. On newer versions of zsh where the bug is fixed, we skip
  # this to avoid slowing down the first tab press any more than we have to.
  autoload -Uz is-at-least
  if ! is-at-least 5.2; then
    # If this were a real language, we wouldn't have to sort by prepending the
    # length, sorting the whole thing numerically, and then removing it again.
    local -a sort_tmp=()
    for arg_spec in $_mpv_completion_arguments; do
      sort_tmp+=${#arg_spec%%=*}_$arg_spec
    done
    _mpv_completion_arguments=(${${(On)sort_tmp}/#*_})
  fi

}

function _mpv_generate_protocols {
  _mpv_completion_protocols=()
  local list_protos_line
  for list_protos_line in "${(@f)$($~words[1] --list-protocols)}"; do
    if [[ $list_protos_line =~ $'^[ \t]+(.*)' ]]; then
      _mpv_completion_protocols+="$match[1]"
    fi
  done
}

function _mpv_generate_if_changed {
  # Called with $1 = 'arguments' or 'protocols'. Generates the respective list
  # on the first run and re-generates it if the executable being completed for
  # is different than the one we used to generate the cached list.
  typeset -gA _mpv_completion_binary
  local current_binary=${~words[1]:c}
  zmodload -F zsh/stat b:zstat
  current_binary+=T$(zstat +mtime $current_binary)
  if [[ $_mpv_completion_binary[$1] != $current_binary ]]; then
    # Use PCRE for regular expression matching if possible. This approximately
    # halves the execution time of generate_arguments compared to the default
    # POSIX regex, which translates to a more responsive first tab press.
    # However, we can't rely on PCRE being available, so we keep all our
    # patterns POSIX-compatible.
    zmodload -s -F zsh/pcre C:pcre-match && setopt re_match_pcre
    _mpv_generate_$1
    _mpv_completion_binary[$1]=$current_binary
  fi
}

# Only consider generating arguments if the argument being completed looks like
# an option. This way, the user should never see a delay when just completing a
# filename.
if [[ $words[$CURRENT] == -* ]]; then
  _mpv_generate_if_changed arguments
fi

local rc=1

_arguments -C -S \*--$_mpv_completion_arguments '*:files:->mfiles' && rc=0

case $state in

  parse-help-*)
    local option_name=${state#parse-help-}
    # Can't do non-capturing groups without pcre, so we index the ones we want
    local pattern name_group=1 desc_group=2
    case $option_name in
      audio-device|vulkan-device)
        pattern=$'^[ \t]+'\''([^'\'']*)'\'$'[ \t]+''\((.*)\)'
      ;;
      profile)
        # The generic pattern would actually work in most cases for --profile,
        # but would break if a profile name contained spaces. This stricter one
        # only breaks if a profile name contains tabs.
        pattern=$'^\t([^\t]*)\t(.*)'
      ;;
      *)
        pattern=$'^[ \t]+(--'${option_name}$'=)?([^ \t]+)[ \t]*[-:]?[ \t]*(.*)'
        name_group=2 desc_group=3
      ;;
    esac
    local -a values
    local current
    for current in "${(@f)$($~words[1] --${option_name}=help)}"; do
      [[ $current =~ $pattern ]] || continue;
      local name=${match[name_group]//:/\\:} desc=${match[desc_group]}
      if [[ -n $desc ]]; then
        values+="${name}:${desc}"
      else
        values+="${name}"
      fi
    done
    (( $#values )) && {
      compset -P '*,'
      compset -S ',*'
      _describe "$state_descr" values -r ',=: \t\n\-' && rc=0
    }
  ;;

  files)
    compset -P '*,'
    compset -S ',*'
    _files -r ',/ \t\n\-' && rc=0
  ;;

  mfiles)
    local expl
    _tags files urls
    while _tags; do
      _requested files expl 'media file' _files && rc=0
      if _requested urls; then
        while _next_label urls expl URL; do
          _urls "$expl[@]" && rc=0
          _mpv_generate_if_changed protocols
          compadd -S '' "$expl[@]" $_mpv_completion_protocols && rc=0
        done
      fi
      (( rc )) || return 0
    done
  ;;

esac

return rc
