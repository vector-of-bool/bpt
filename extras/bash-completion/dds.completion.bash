_dds_complete_flag_arg()
{
  if [[ $CWORDI -ge $COMP_CWORD ]]; then
    case $ARG in
    "directory")
      COMPREPLY=( $(compgen -d -- $CWORD) )
      ;;
    "file")
      COMPREPLY=( $(compgen -f -- $CWORD) )
      ;;
    "file-or-list: "*)
      COMPREPLY=( $(compgen -f -- $CWORD) )
      COMPREPLY+=( $(compgen -W "${ARG//file-or-list: /}" -- $CWORD) )
      ;;
    *)
      COMPREPLY=( $(compgen -W "$ARG" -- $CWORD) )
      ;;
    esac

    return
  fi

  ((CWORDI=CWORDI+1))
  CWORD="${COMP_WORDS[$CWORDI]}"
} &&

_dds_complete_flags()
{
  local ARG
  ARG="${FLAGS[$CWORD]}"
  if [[ -n "$ARG" ]]; then
    ((CWORDI=CWORDI+1))
    CWORD="${COMP_WORDS[$CWORDI]}"
    _dds_complete_flag_arg
  fi
} &&

_dds_complete_build_builtin_toolchains()
{
  local gccs clangs cxxstds base ccached debugged cxx98 cxx03 cxx11 cxx14 cxx17 cxx20

  gccs=$(find $(echo $PATH | sed 's/:/ /g') -regextype posix-extended -regex '^.*/gcc(-[0-9]+)?$' -print | sed -E 's/^.*\/(gcc(-[0-9]+)?)/:\1/g')
  clangs=$(find $(echo $PATH | sed 's/:/ /g') -regextype posix-extended -regex '^.*/clang(-[0-9]+)?$' -print | sed -E 's/^.*\/(clang(-[0-9]+)?)/:\1/g')

  base="$gccs $clangs"

  base=($base)
  cxx98=(${base[@]/#/:c++98})
  cxx03=(${base[@]/#/:c++03})
  cxx11=(${base[@]/#/:c++11})
  cxx14=(${base[@]/#/:c++14})
  cxx17=(${base[@]/#/:c++17})
  cxx20=(${base[@]/#/:c++20})
  base=($cxx98 $cxx03 $cxx11 $cxx14 $cxx17 $cxx20)

  ccached=${base[@]/#/:ccache}

  BUILTIN_TOOLCHAINS="${base[@]} $ccached"
  BUILTIN_TOOLCHAINS=($BUILTIN_TOOLCHAINS)
  debugged=${BUILTIN_TOOLCHAINS[@]/#/:debug}
  BUILTIN_TOOLCHAINS="${BUILTIN_TOOLCHAINS[@]} $debugged"
} &&

_dds_complete_build()
{
  local SUBCOMMANDS FLAGS BUILTIN_TOOLCHAINS RESULT_WORDS
  declare -A FLAGS
  declare -A SUBCOMMANDS

  _dds_complete_build_builtin_toolchains

  SUBCOMMANDS=()
  FLAGS=(
    [--toolchain]="file-or-list: $BUILTIN_TOOLCHAINS"
    [--project]='directory'
    [--no-tests]=''
    [--no-apps]=''
    [--no-warnings]=''
    [--output]='directory'
    [--add-repo]=' '
    [--update-repos]=''
    [--libman-index]='file'
    [--jobs]=' '
    [--tweaks-dir]='directory'
  )

  ((CWORDI=CWORDI+1))
  CWORD="${COMP_WORDS[$CWORDI]}"

  if [[ $CWORDI -ge $COMP_CWORD ]]; then
    RESULT_WORDS="${!FLAGS[@]}"
    COMPREPLY=( $(compgen -W "$RESULT_WORDS" -- $CWORD) )
    return
  fi

  _dds_complete_flags
} &&

_dds_complete_compile_file()
{
  local SUBCOMMANDS FLAGS BUILTIN_TOOLCHAINS RESULT_WORDS
  declare -A FLAGS
  declare -A SUBCOMMANDS

  _dds_complete_build_builtin_toolchains

  SUBCOMMANDS=()
  FLAGS=(
    [--toolchain]="file-or-list: $BUILTIN_TOOLCHAINS"
    [--project]='directory'
    [--no-warnings]=''
    [--output]='file'
    [--libman-index]='file'
    [--jobs]=' '
    [--tweaks-dir]='directory'
  )

  ((CWORDI=CWORDI+1))
  CWORD="${COMP_WORDS[$CWORDI]}"

  if [[ $CWORDI -ge $COMP_CWORD ]]; then
    RESULT_WORDS="${!FLAGS[@]}"
    COMPREPLY=( $(compgen -W "$RESULT_WORDS" -- $CWORD) )
    return
  fi

  _dds_complete_flags

  while [[ $CWORDI -lt $COMP_CWORD ]]; do
    ((CWORDI=CWORDI+1))
    CWORD="${COMP_WORDS[$CWORDI]}"
  done
  COMPREPLY+=( $(compgen -f -- $CWORD) )
} &&

_dds_complete_build_deps()
{
  local SUBCOMMANDS FLAGS BUILTIN_TOOLCHAINS RESULT_WORDS
  declare -A FLAGS
  declare -A SUBCOMMANDS

  _dds_complete_build_builtin_toolchains

  SUBCOMMANDS=()
  FLAGS=(
    [--toolchain]="file-or-list: $BUILTIN_TOOLCHAINS"
    [--jobs]=' '
    [--output]='file'
    [--libman-index]=' '
    [--deps-file]='file'
    [--cmake]=' '
    [--tweaks-dir]='directory'
  )

  ((CWORDI=CWORDI+1))
  CWORD="${COMP_WORDS[$CWORDI]}"

  if [[ $CWORDI -ge $COMP_CWORD ]]; then
    RESULT_WORDS="${!FLAGS[@]}"
    COMPREPLY=( $(compgen -W "$RESULT_WORDS" -- $CWORD) )
    return
  fi

  _dds_complete_flags

  # Do nothing for the dependency statement strings
} &&

_dds_complete_pkg_create()
{
  local SUBCOMMANDS FLAGS RESULT_WORDS
  declare -A FLAGS
  declare -A SUBCOMMANDS

  SUBCOMMANDS=()
  FLAGS=(
    [--project]='directory'
    [--out]=' '
    [--if-exists]='replace skip fail'
  )

  ((CWORDI=CWORDI+1))
  CWORD="${COMP_WORDS[$CWORDI]}"

  if [[ $CWORDI -ge $COMP_CWORD ]]; then
    RESULT_WORDS="${!FLAGS[@]}"
    COMPREPLY=( $(compgen -W "$RESULT_WORDS" -- $CWORD) )
    return
  fi

  _dds_complete_flags
} &&

_dds_complete_pkg_get()
{
  local SUBCOMMANDS FLAGS RESULT_WORDS
  declare -A FLAGS
  declare -A SUBCOMMANDS

  SUBCOMMANDS=()
  FLAGS=(
    [--output]='directory'
  )

  ((CWORDI=CWORDI+1))
  CWORD="${COMP_WORDS[$CWORDI]}"

  if [[ $CWORDI -ge $COMP_CWORD ]]; then
    RESULT_WORDS="${!FLAGS[@]}"
    COMPREPLY=( $(compgen -W "$RESULT_WORDS" -- $CWORD) )
    return
  fi

  _dds_complete_flags

  # Ignore positional arguments (pkg-ids)
} &&

_dds_complete_pkg_import()
{
  local SUBCOMMANDS FLAGS RESULT_WORDS
  declare -A FLAGS
  declare -A SUBCOMMANDS

  SUBCOMMANDS=()
  FLAGS=(
    [--stdin]=''
    [--if-exists]='replace skip fail'
  )

  ((CWORDI=CWORDI+1))
  CWORD="${COMP_WORDS[$CWORDI]}"

  if [[ $CWORDI -ge $COMP_CWORD ]]; then
    RESULT_WORDS="${!FLAGS[@]}"
    COMPREPLY=( $(compgen -W "$RESULT_WORDS" -- $CWORD) )
    return
  fi

  _dds_complete_flags

  # <path-or-url> ...
  while [[ $CWORDI -lt $COMP_CWORD ]]; do
    ((CWORDI=CWORDI+1))
    CWORD="${COMP_WORDS[$CWORDI]}"
  done
  COMPREPLY+=( $(compgen -f -- $CWORD) )
} &&

_dds_complete_pkg_repo_add()
{
  local SUBCOMMANDS FLAGS RESULT_WORDS
  declare -A FLAGS
  declare -A SUBCOMMANDS

  SUBCOMMANDS=()
  FLAGS=(
    [--no-update]=''
  )

  ((CWORDI=CWORDI+1))
  CWORD="${COMP_WORDS[$CWORDI]}"

  if [[ $CWORDI -ge $COMP_CWORD ]]; then
    RESULT_WORDS="${!FLAGS[@]}"
    COMPREPLY=( $(compgen -W "$RESULT_WORDS" -- $CWORD) )
    return
  fi

  _dds_complete_flags

  # Do nothing for url
} &&

_dds_complete_pkg_repo_remove()
{
  local SUBCOMMANDS FLAGS RESULT_WORDS
  declare -A FLAGS
  declare -A SUBCOMMANDS

  SUBCOMMANDS=()
  FLAGS=(
    [--if-missing]='fail ignore'
  )

  ((CWORDI=CWORDI+1))
  CWORD="${COMP_WORDS[$CWORDI]}"

  if [[ $CWORDI -ge $COMP_CWORD ]]; then
    RESULT_WORDS="${!FLAGS[@]}"
    COMPREPLY=( $(compgen -W "$RESULT_WORDS" -- $CWORD) )
    return
  fi

  _dds_complete_flags

  # Do nothing for <repo-name>s
} &&

_dds_complete_pkg_repo()
{
  local SUBCOMMANDS FLAGS CWORD RESULT_WORDS
  declare -A FLAGS
  declare -A SUBCOMMANDS
  SUBCOMMANDS=(
    [add]=1
    [remove]=1
    [update]=1
    [ls]=1
  )
  FLAGS=()

  CWORD="${COMP_WORDS[$CWORDI]}"

  if [[ $CWORDI -ge $COMP_CWORD ]]; then
    RESULT_WORDS="${!SUBCOMMANDS[@]}"

    COMPREPLY=( $(compgen -W "$RESULT_WORDS" -- $CWORD) )
    return
  fi

  case $CWORD in
    "ls"|"update")
      # No arguments; completion is done
      ;;
    "add")
      _dds_complete_pkg_repo_add
      ;;
    "remove")
      _dds_complete_pkg_repo_remove
      ;;
    *)
      ;;
  esac
} &&

_dds_complete_pkg()
{
  local SUBCOMMANDS FLAGS CWORD RESULT_WORDS
  declare -A FLAGS
  declare -A SUBCOMMANDS
  SUBCOMMANDS=(
    [init-db]=1
    [ls]=1
    [create]=1
    [get]=1
    [import]=1
    [repo]=1
    [search]=1
  )
  FLAGS=()

  CWORD="${COMP_WORDS[$CWORDI]}"

  if [[ $CWORDI -ge $COMP_CWORD ]]; then
    RESULT_WORDS="${!SUBCOMMANDS[@]}"

    COMPREPLY=( $(compgen -W "$RESULT_WORDS" -- $CWORD) )
    return
  fi

  case $CWORD in
    "init-db"|"ls"|"search")
      # No arguments or can't complete; completion is done
      ;;
    "create")
      _dds_complete_pkg_create
      ;;
    "get")
      _dds_complete_pkg_get
      ;;
    "import")
      _dds_complete_pkg_import
      ;;
    "repo")
      _dds_complete_pkg_repo
      ;;
    *)
      ;;
  esac
} &&

_dds_complete_repoman_init()
{
  local SUBCOMMANDS FLAGS RESULT_WORDS
  declare -A FLAGS
  declare -A SUBCOMMANDS

  SUBCOMMANDS=()
  FLAGS=(
    [--if-exists]='replace skip fail'
    [--name]=' '
  )

  ((CWORDI=CWORDI+1))
  CWORD="${COMP_WORDS[$CWORDI]}"

  if [[ $CWORDI -ge $COMP_CWORD ]]; then
    RESULT_WORDS="${!FLAGS[@]}"
    COMPREPLY=( $(compgen -W "$RESULT_WORDS" -- $CWORD) )
    return
  fi

  _dds_complete_flags

  # repo-dir
  if [[ $CWORDI -lt $COMP_CWORD ]]; then
    ((CWORDI=CWORDI+1))
    CWORD="${COMP_WORDS[$CWORDI]}"

    if [[ $CWORDI -ge $COMP_CWORD ]]; then
      COMPREPLY+=( $(compgen -d -- $CWORD) )
    fi
  fi
} &&

_dds_complete_repoman_ls()
{
  ((CWORDI=CWORDI+1))
  CWORD="${COMP_WORDS[$CWORDI]}"

  if [[ $CWORDI -ge $COMP_CWORD ]]; then
    COMPREPLY=( $(compgen -d -- $CWORD) )
    return
  fi
} &&

_dds_complete_repoman_add()
{
  local SUBCOMMANDS FLAGS RESULT_WORDS
  declare -A FLAGS
  declare -A SUBCOMMANDS

  SUBCOMMANDS=()
  FLAGS=(
    [--description]=' '
  )

  ((CWORDI=CWORDI+1))
  CWORD="${COMP_WORDS[$CWORDI]}"

  if [[ $CWORDI -ge $COMP_CWORD ]]; then
    RESULT_WORDS="${!FLAGS[@]}"
    COMPREPLY=( $(compgen -W "$RESULT_WORDS" -- $CWORD) )
    return
  fi

  _dds_complete_flags

  # repo-dir
  if [[ $CWORDI -lt $COMP_CWORD ]]; then
    ((CWORDI=CWORDI+1))
    CWORD="${COMP_WORDS[$CWORDI]}"

    if [[ $CWORDI -ge $COMP_CWORD ]]; then
      COMPREPLY+=( $(compgen -d -- $CWORD) )
    fi
  fi
} &&

_dds_complete_repoman_import()
{
  ((CWORDI=CWORDI+1))
  CWORD="${COMP_WORDS[$CWORDI]}"

  # <repo-dir>
  if [[ $CWORDI -ge $COMP_CWORD ]]; then
    COMPREPLY=( $(compgen -d -- $CWORD) )
    return
  fi

  ((CWORDI=CWORDI+1))
  CWORD="${COMP_WORDS[$CWORDI]}"

  while [[ $CWORDI -lt $COMP_CWORD ]]; do
    ((CWORDI=CWORDI+1))
    CWORD="${COMP_WORDS[$CWORDI]}"
  done

  COMPREPLY+=( $(compgen -f -- $CWORD) )
} &&

_dds_complete_repoman_remove()
{
  ((CWORDI=CWORDI+1))
  CWORD="${COMP_WORDS[$CWORDI]}"

  # <repo-dir>
  if [[ $CWORDI -ge $COMP_CWORD ]]; then
    COMPREPLY=( $(compgen -d -- $CWORD) )
    return
  fi

  # No completion for <pkg-id>s
} &&

_dds_complete_repoman()
{
  local SUBCOMMANDS FLAGS CWORD RESULT_WORDS
  declare -A FLAGS
  declare -A SUBCOMMANDS
  SUBCOMMANDS=(
    [init]=1
    [ls]=1
    [add]=1
    [import]=1
    [remove]=1
  )
  FLAGS=()

  CWORD="${COMP_WORDS[$CWORDI]}"

  if [[ $CWORDI -ge $COMP_CWORD ]]; then
    RESULT_WORDS="${!SUBCOMMANDS[@]}"

    COMPREPLY=( $(compgen -W "$RESULT_WORDS" -- $CWORD) )
    return
  fi

  case $CWORD in
    "init")
      _dds_complete_repoman_init
      ;;
    "ls")
      _dds_complete_repoman_ls
      ;;
    "add")
      _dds_complete_repoman_add
      ;;
    "import")
      _dds_complete_repoman_import
      ;;
    "remove")
      _dds_complete_repoman_remove
      ;;
    *)
      ;;
  esac
} &&

_dds_complete_install_yourself()
{
  local FLAGS RESULT_WORDS
  declare -A FLAGS

  FLAGS=(
    [--where]='user system'
    [--dry-run]=''
    [--no-modify-path]=''
    [--symlink]=''
  )

  ((CWORDI=CWORDI+1))
  CWORD="${COMP_WORDS[$CWORDI]}"

  if [[ $CWORDI -ge $COMP_CWORD ]]; then
    RESULT_WORDS="${!FLAGS[@]}"
    COMPREPLY=( $(compgen -W "$RESULT_WORDS" -- $CWORD) )
    return
  fi

  _dds_complete_flags
} &&

_dds_complete_impl()
{
  local SUBCOMMANDS FLAGS CWORD RESULT_WORDS
  declare -A FLAGS
  declare -A SUBCOMMANDS
  SUBCOMMANDS=(
    [build]=1
    [compile-file]=1
    [build-deps]=1
    [pkg]=1
    [repoman]=1
    [install-yourself]=1
  )
  FLAGS=(
    [--log-level]='trace debug info warn error critical silent'
    [--data-dir]='directory'
    [--pkg-cache-dir]='directory'
    [--pkg-db-path]='file'
  )

  CWORD="${COMP_WORDS[$CWORDI]}"

  if [[ $CWORDI -ge $COMP_CWORD ]]; then
    RESULT_WORDS="${!SUBCOMMANDS[@]}"

    if [[ $CWORD == "-"* ]]; then
      RESULT_WORDS+=" ${!FLAGS[@]}"
    fi

    COMPREPLY=( $(compgen -W "$RESULT_WORDS" -- $CWORD) )
    return
  fi

  case $CWORD in
    "build")
      _dds_complete_build
      ;;
    "compile-file")
      _dds_complete_compile_file
      ;;
    "build-deps")
      _dds_complete_build_deps
      ;;
    "pkg")
      _dds_complete_pkg
      ;;
    "repoman")
      _dds_complete_repoman
      ;;
    "install-yourself")
      _dds_complete_install_yourself
      ;;
    "-"*)
      _dds_complete_flags
      _dds_complete_impl
      ;;
    *)
      ;;
  esac
} &&

_dds_complete()
{
  local CWORDI
  CWORDI=1
  _dds_complete_impl
  return 0
} &&
complete -F _dds_complete dds
