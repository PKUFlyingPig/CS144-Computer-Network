#!/bin/bash

show_usage() {
    echo "Usage: $0 <-i|-u> <-c|-s> <-R|-S|-D> [-n|-o]"
    echo "       [-t <rtto>] [-d <size>] [-w <size>] [-l <rate>] [-L <rate>]"
    echo
    echo "  Option                                                      Default"
    echo "  --                                                          --"
    echo "  -i or -u    Select IP or UDP mode                           (must specify)"
    echo "  -c or -s    Select client or server mode                    (must specify)"
    echo "  -R, -S, -D  Receive test, Send test, or Duplex test         (must specify)"
    echo
    echo "  -t <rtto>   Set rtto to <rtto> ms                           12"
    echo "  -d <size>   Set total transfer size to <size>               32"
    echo "  -w <size>   Set window size to <size>                       1452"
    echo
    echo "  -l <rate>   Set downlink loss to <rate> (float in 0..1)     0"
    echo "  -L <rate>   Set uplink loss to <rate> (float in 0..1)       0"
    echo
    echo "  -n          In IP mode, use tcp_native rather tcp_ipv4_ref  False"
    echo "  -o          In IP mode, use socat rather than tcp_ipv4_ref  False"
    [ ! -z "$1" ] && { echo; echo ERROR: "$1"; }
    exit 1
}

get_cmdline_options () {
    # prepare to use getopts
    local OPT= OPTIND=1 OPTARG=
    CSMODE= RSDMODE= DATASIZE=32 WINSIZE= IUMODE= USE_IPV4= RTTO="-t 12" LOSS_UP= LOSS_DN=
    while getopts "t:oniucsRSDd:w:p:l:L:" OPT; do
        case "$OPT" in
            i|u)
                [ ! -z "$IUMODE" ] && show_usage "Only one of -i and -u is allowed."
                IUMODE=$OPT
                ;;
            c|s)
                [ ! -z "$CSMODE" ] && show_usage "Only one of -c and -s is allowed."
                CSMODE=$OPT
                ;;
            R|S|D)
                [ ! -z "$RSDMODE" ] && show_usage "Only one of -R, -S, and -D is allowed."
                RSDMODE=$OPT
                ;;
            d)
                DATASIZE="$OPTARG"
                ;;
            w)
                expand_num "$OPTARG" || show_usage "Bad numeric arg \"$OPTARG\" to -w."
                WINSIZE="-w ${NUM_EXPANDED}"
                ;;
            l)
                LOSS_DN="$OPTARG"
                ;;
            L)
                LOSS_UP="$OPTARG"
                ;;
            n|o)
                [ ! -z "$USE_IPV4" ] && show_usage "Only one of -n and -o is allowed."
                USE_IPV4=$OPT
                ;;
            t)
                expand_num "$OPTARG" || show_usage "Bad numeric arg \"$OPTARG\" to -t."
                RTTO="-t ${NUM_EXPANDED}"
                ;;
            *)
                show_usage "Unknown option $OPT"
                ;;
        esac
    done
    if [ "$OPTIND" != $(($# + 1)) ]; then
        show_usage "Extraneous arguments detected."
    fi
    if [ -z "$CSMODE" ] || [ -z "$RSDMODE" ] || [ -z "$IUMODE" ]; then
        show_usage "You must specify either -i or -u, either -c or -s, and one of -R, -S, and -D."
    fi
    if [ ! -z "$USE_IPV4" ] && [ "$IUMODE" != "i" ]; then
        show_usage "-n and -o may only be specified in IP mode (-i)."
    fi
    # loss param args depend on whether we're applying to ref or test program (test uplink == loss downlink)
    { [ ! -z "$USE_IPV4" ] && local LD_SWITCH="-Ld" LU_SWITCH="-Lu"; } || local LD_SWITCH="-Lu" LU_SWITCH="-Ld"
    [ ! -z "$LOSS_DN" ] && LOSS_DN="${LD_SWITCH} ${LOSS_DN}"
    [ ! -z "$LOSS_UP" ] && LOSS_UP="${LU_SWITCH} ${LOSS_UP}"
}

expand_num () {
    [ "$#" != "1" ] && { echo "bad args"; exit 1; }
    NUM_EXPANDED=$(numfmt --from=iec "$1" 2>/dev/null)
    return $?
}

_socat_listen () {
    coproc socat tcp4-listen:${SERVER_PORT},reuseaddr,reuseport,linger=2 stdio >"$1" <"$2" && sleep 0.1
    set +u
    [ -z "$COPROC_PID" ] && { echo "Error in _socat_listen"; exit 1; }
    set -u
}

_socat_connect () {
    socat tcp4-connect:${TEST_HOST}:${SERVER_PORT},reuseaddr,reuseport,linger=2 stdio >"$1" <"$2" ||
        { echo "Error in _socat_connect"; exit 1; }
}

_rt_listen () {
    coproc $3 -l $4 ${SERVER_PORT} >"$1" <"$2" && sleep 0.1
    set +u
    [ -z "$COPROC_PID" ] && { echo "Error in _rt_listen"; exit 1; }
    set -u
}

_rt_connect () {
    $3 $4 ${SERVER_PORT} >"$1" <"$2" || { echo "Error in _rt_connect"; exit 1; }
}

test_listen () {
    [ "$#" != 2 ] && { echo "bad args"; exit 1; }
    _rt_listen "$1" "$2" "${TEST_PROG}" "${TEST_HOST}"
}

test_connect () {
    [ "$#" != 2 ] && { echo "bad args"; exit 1; }
    _rt_connect "$1" "$2" "${TEST_PROG}" "${REF_HOST}"
}

ref_listen () {
    [ "$#" != 2 ] && { echo "bad args"; exit 1; }
    if [ "$IUMODE" = "u" ] || [ -z "$USE_IPV4" ] || [ "$USE_IPV4" = "n" ]; then
        _rt_listen "$1" "$2" "${REF_PROG}" "${REF_HOST}"
    else
        _socat_listen "$1" "$2"
    fi
}

ref_connect () {
    [ "$#" != 2 ] && { echo "bad args"; exit 1; }
    if [ "$IUMODE" = "u" ] || [ -z "$USE_IPV4" ] || [ "$USE_IPV4" = "n" ]; then
        _rt_connect "$1" "$2" "${REF_PROG}" "${TEST_HOST}"
    else
        _socat_connect "$1" "$2"
    fi
}

hash_file () {
    [ "$#" != "1" ] && { echo "bad args"; exit 1; }
    sha256sum "$1" | cut -d \  -f 1
}

make_test_file () {
    if expand_num "$2"; then
        dd status=none if=/dev/urandom of="$1" bs="${NUM_EXPANDED}" count=1 || { echo "Failed to make test file."; exit 1; }
    else
        # can't interpret as a number, so interpret as literal data to send
        echo -en "$2" >"$1"
    fi
}

exit_cleanup () {
    set +u
    rm -f "${TEST_IN_FILE}" "${TEST_OUT_FILE}" "${TEST_OUT2_FILE}"
    [ ! -z "$COPROC_PID" ] && kill ${COPROC_PID}
}

# make sure tun device is running
ip link show tun144 &>/dev/null || { echo "please enable tun144 and re-run"; exit 1; }
ip link show tun145 &>/dev/null || { echo "please enable tun145 and re-run"; exit 1; }

set -u
trap exit_cleanup EXIT

get_cmdline_options "$@"

. "$(dirname "$0")"/etc/tunconfig
REF_HOST=${TUN_IP_PREFIX}.144.1
TEST_HOST=${TUN_IP_PREFIX}.144.1
SERVER_PORT=$(($((RANDOM % 50000)) + 1025))
if [ "$IUMODE" = "i" ]; then
    # IPv4 mode
    TEST_HOST=${TUN_IP_PREFIX}.144.9
    if [ -z "$USE_IPV4" ]; then
        REF_HOST=${TUN_IP_PREFIX}.145.9
        REF_PROG="./apps/tcp_ipv4 ${RTTO} ${WINSIZE} ${LOSS_UP} ${LOSS_DN} -d tun145 -a ${REF_HOST}"
        TEST_PROG="./apps/tcp_ipv4 ${RTTO} ${WINSIZE} -d tun144 -a ${TEST_HOST}"
    else
        REF_PROG="./apps/tcp_native"
        TEST_PROG="./apps/tcp_ipv4 ${RTTO} ${WINSIZE} ${LOSS_UP} ${LOSS_DN} -d tun144 -a ${TEST_HOST}"
    fi
else
    # UDP mode
    REF_PROG="./apps/tcp_udp ${RTTO} ${WINSIZE} ${LOSS_UP} ${LOSS_DN}"
    TEST_PROG="./apps/tcp_udp ${RTTO} ${WINSIZE}"
fi

TEST_OUT_FILE=$(mktemp)
TEST_IN_FILE=$(mktemp)
make_test_file "${TEST_IN_FILE}" "${DATASIZE}"
HASH_IN=$(sha256sum ${TEST_IN_FILE} | cut -d \  -f 1)
HASH_OUT2=
case "$RSDMODE" in
    S)  # test sending
        if [ "$CSMODE" = "c" ]; then
            ref_listen "${TEST_OUT_FILE}" /dev/null
            test_connect /dev/null "${TEST_IN_FILE}"
        else
            test_listen /dev/null "${TEST_IN_FILE}"
            ref_connect "${TEST_OUT_FILE}" /dev/null
        fi
        ;;
    R)  # test receiving
        if [ "$CSMODE" = "c" ]; then
            ref_listen /dev/null "${TEST_IN_FILE}"
            test_connect "${TEST_OUT_FILE}" /dev/null
        else
            test_listen "${TEST_OUT_FILE}" /dev/null
            ref_connect /dev/null "${TEST_IN_FILE}"
        fi
        ;;
    D)  # test full-duplex
        TEST_OUT2_FILE=$(mktemp)
        if [ "$CSMODE" = "c" ]; then
            ref_listen "${TEST_OUT_FILE}" "${TEST_IN_FILE}"
            test_connect "${TEST_OUT2_FILE}" "${TEST_IN_FILE}"
        else
            test_listen "${TEST_OUT_FILE}" "${TEST_IN_FILE}"
            ref_connect "${TEST_OUT2_FILE}" "${TEST_IN_FILE}"
        fi
        HASH_OUT2=$(hash_file "${TEST_OUT2_FILE}")
        ;;
esac
if ! wait; then
    echo ERROR: subprocess failed
    exit 1
fi

HASH_OUT=$(hash_file ${TEST_OUT_FILE})
if [ ! -z "${HASH_OUT2}" ] && [ "${HASH_OUT}" != "${HASH_OUT2}" ] || [ "${HASH_IN}" != "${HASH_OUT}" ]; then
    echo ERROR: "$HASH_IN" neq "$HASH_OUT" or "$HASH_OUT2"
    exit 1
fi
exit 0
