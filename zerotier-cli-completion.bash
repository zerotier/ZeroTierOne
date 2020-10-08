#compdef zerotier-cli
#autoload


_get_network_ids ()
{
    if [[ "$OSTYPE" == "darwin"* ]]; then
        COMPREPLY=($(compgen -W "$(ls -1 /Library/Application\ Support/ZeroTier/One/networks.d | cut -c 1-16)" -- ${cur}))
    else
        COMPREPLY=($(compgen -W "$(ls -1 /var/lib/zerotier-one/networks.d | cut -c 1-16)" -- ${cur}))
    fi
}

_get_network_ids_from_history ()
{
    COMPREPLY=($(compgen -W "$(fc -l -1000 -1 | sed -n 's/.*\([[:xdigit:]]\{16\}\).*/\1/p')" -- ${cur}))
}

_zerotier-cli_completions()
{
    local cur prev

    cur=${COMP_WORDS[COMP_CWORD]}
    prev=${COMP_WORDS[COMP_CWORD-1]}

    case ${COMP_CWORD} in
        1)
            COMPREPLY=($(compgen -W "info listpeers peers listnetworks join leave set get listmoons orbit deorbit" -- ${cur}))
            ;;
        2)
            case ${prev} in
                leave)
                    _get_network_ids
                    ;;
                join)
                    _get_network_ids_from_history
                    ;;
                set)
                    _get_network_ids
                    ;;
                get)
                    _get_network_ids
                    ;;
                *)
                    COMPREPLY=()
                    ;;
            esac
            ;;
        *)
            COMPREPLY=()
            ;;
    esac
}

complete -F _zerotier-cli_completions zerotier-cli


