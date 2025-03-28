# -*- mode: sh -*-

# Make sure all commands print out messages in English
export LC_ALL=C

canonical_name() {
  case $1 in
  i?86) echo i386 ;;
  arm*) echo arm ;;
  powerpc) echo ppc ;;
  powerpc64) echo ppc64 ;;
  powerpc64le) echo ppc64le ;;
  *) echo $1
  esac
}

HOST=$(canonical_name $(uname -m))

# Set tool names
if [ "$TRIPLE" = "" ]; then
  MACHINE=$HOST
  TESTDIR=out/test/elf/$HOST
  CC="${TEST_CC:-cc}"
  CXX="${TEST_CXX:-c++}"
  GCC="${TEST_GCC:-gcc}"
  GXX="${TEST_GXX:-g++}"
  OBJDUMP=objdump
  OBJCOPY=objcopy
  STRIP=strip
  QEMU=
elif [ "$TRIPLE" = powerpc64le_power10-linux-gnu ]; then
  TRIPLE=powerpc64le-linux-gnu
  MACHINE=ppc64le
  TESTDIR=out/test/elf/ppc64le-power10
  CC="${TEST_CC:-$TRIPLE-gcc} -mcpu=power10"
  CXX="${TEST_CXX:-$TRIPLE-g++} -mcpu=power10"
  GCC="${TEST_GCC:-$TRIPLE-gcc} -mcpu=power10"
  GXX="${TEST_GXX:-$TRIPLE-g++} -mcpu=power10"
  OBJDUMP="$TRIPLE-objdump"
  OBJCOPY="$TRIPLE-objcopy"
  STRIP="$TRIPLE-strip"
  QEMU="qemu-ppc64le -L /usr/$TRIPLE -cpu power10"
else
  MACHINE=$(canonical_name $(echo $TRIPLE | sed 's/-.*//'))
  TESTDIR=out/test/elf/$MACHINE
  CC="${TEST_CC:-$TRIPLE-gcc}"
  CXX="${TEST_CXX:-$TRIPLE-g++}"
  GCC="${TEST_GCC:-$TRIPLE-gcc}"
  GXX="${TEST_GXX:-$TRIPLE-g++}"
  OBJDUMP="$TRIPLE-objdump"
  OBJCOPY="$TRIPLE-objcopy"
  STRIP="$TRIPLE-strip"
  QEMU="qemu-$MACHINE -L /usr/$TRIPLE"
fi

if [ $MACHINE = x86_64 -o $MACHINE = i386 -o $MACHINE = arm ]; then
  tlsdesc_opt=-mtls-dialect=gnu2
elif [ $MACHINE = aarch64 ]; then
  tlsdesc_opt=-mtls-dialect=desc
fi

if [ $MACHINE = ppc64le -a -e /proc/cpuinfo ] && grep -q POWER10 /proc/cpuinfo; then
  CC="$CC -mcpu=power10"
  CXX="$CXX -mcpu=power10"
  GCC="$GCC -mcpu=power10"
  GXX="$GXX -mcpu=power10"
fi

# Common functions
test_cflags() {
  echo 'int main() {}' | $CC "$@" -o /dev/null -xc - >& /dev/null
}

supports_ifunc() {
  echo 'void x() __attribute__((ifunc("y"))); void *y() { return 0; }' | \
    $CC -c -o /dev/null -xc - >& /dev/null
}

is_musl() {
  ldd --help 2>&1 | grep -q musl
}

supports_tlsdesc() {
  # musl's tlsdesc on arm32 seems to be broken
  [ $MACHINE = arm ] && is_musl && return 1

  [ -n "$tlsdesc_opt" ]
}

skip() {
  echo skipped
  trap - EXIT
  exit 0
}

on_error() {
  code=$?
  echo "command failed: $1: $BASH_COMMAND"
  trap - EXIT
  exit $code
}

on_exit() {
  echo OK
  exit 0
}

trap 'on_error $LINENO' ERR
trap on_exit EXIT

# Print out the startup message
testname=$(basename "$0" .sh)
echo -n "Testing $testname ... "
t=$TESTDIR/$testname
mkdir -p $t
