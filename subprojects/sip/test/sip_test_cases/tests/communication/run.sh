#!/bin/sh
set -e

sip=$1
tmp_dir=$(mktemp -d)
trap 'rm -r "$tmp_dir"' EXIT
pkg="$tmp_dir/package"
"$sip" communication init "$pkg" example
test ! -f "$pkg/Simfile.communication"
test -f "$pkg/manager/manager.cc"
grep '^communication: true$' "$pkg/Simfile"
if grep '^protocol:' "$pkg/Simfile"; then
    exit 1
fi
cp "$pkg/Simfile" "$tmp_dir/initial-Simfile"
if "$sip" communication init "$pkg" example; then
    exit 1
fi
cmp "$tmp_dir/initial-Simfile" "$pkg/Simfile"
cat > "$pkg/prog/model.cc" <<'CPP'
#include <iostream>
#include <string>
int main() {
    std::string role, message;
    std::getline(std::cin, role);
    std::getline(std::cin, message);
    std::cout << message << '\n';
}
CPP
cat > "$pkg/in/ex1.in" <<'INPUT'
hello
INPUT
cat > "$pkg/Sipfile" <<'SIP'
default_time_limit: 2
static: [ex1]
SIP
"$sip" -C "$pkg" gen
test ! -f "$pkg/out/ex1.out"
"$sip" -C "$pkg" test > "$tmp_dir/report"
grep 'Total score:.*100 / 100' "$tmp_dir/report"
grep 'instances: 2' "$tmp_dir/report"
cp "$pkg/manager/manager.cc" "$tmp_dir/manager.cc"
sed 's/decoded == message + "\\n", 100/decoded == message + "\\n", 50/' \
    "$tmp_dir/manager.cc" > "$pkg/manager/manager.cc"
cp "$pkg/Simfile" "$tmp_dir/Simfile"
"$sip" -C "$pkg" test --keep-limits > "$tmp_dir/partial-report"
grep 'Total score:.*50 / 100' "$tmp_dir/partial-report"
cmp "$tmp_dir/Simfile" "$pkg/Simfile"
"$sip" -C "$pkg" test model --keep-limits > "$tmp_dir/partial-pattern-report"
grep 'Total score:.*50 / 100' "$tmp_dir/partial-pattern-report"
cmp "$tmp_dir/Simfile" "$pkg/Simfile"
"$sip" -C "$pkg" communication test model --keep-limits > "$tmp_dir/partial-alias-report"
grep 'Total score:.*50 / 100' "$tmp_dir/partial-alias-report"
cmp "$tmp_dir/Simfile" "$pkg/Simfile"
"$sip" -C "$pkg" test > "$tmp_dir/calibration-report"
grep 'Total score:.*50 / 100' "$tmp_dir/calibration-report"
cp "$tmp_dir/manager.cc" "$pkg/manager/manager.cc"
"$sip" -C "$pkg" regen
if "$sip" -C "$pkg" genout; then
    exit 1
fi
if "$sip" -C "$pkg" communication genout; then
    exit 1
fi
sed 's/instances: 2/instances: 32/' "$pkg/Simfile" > "$tmp_dir/variable-Simfile"
printf 'min_instances: 2\n' >> "$tmp_dir/variable-Simfile"
cp "$tmp_dir/variable-Simfile" "$pkg/Simfile"
"$sip" -C "$pkg" test --keep-limits > "$tmp_dir/variable-report"
grep 'Total score:.*100 / 100' "$tmp_dir/variable-report"
grep 'instances: 2' "$tmp_dir/variable-report"
cmp "$tmp_dir/variable-Simfile" "$pkg/Simfile"
"$sip" -C "$pkg" communication test --keep-limits > "$tmp_dir/alias-report"
grep 'Total score:.*100 / 100' "$tmp_dir/alias-report"
"$sip" -C "$pkg" save scoring
for setting in 'communication: true' 'instances: 32' 'min_instances: 2' 'max_concurrent: 1'; do
    grep "^$setting$" "$pkg/Simfile"
done
"$sip" -C "$pkg" pack
test -f "$tmp_dir/package.zip"
python3 - "$tmp_dir/package.zip" <<'PY'
import sys
import zipfile
with zipfile.ZipFile(sys.argv[1]) as archive:
    names = archive.namelist()
    assert not any(name.endswith('/Simfile.communication') for name in names)
    simfile = archive.read('package/Simfile').decode()
    assert 'protocol:' not in simfile
    for setting in ['communication: true', 'manager:', 'instances: 32', 'min_instances: 2', 'max_concurrent: 1']:
        assert setting in simfile, simfile
PY
cp "$pkg/Simfile" "$tmp_dir/valid-Simfile"
sed '/^instances:/d; /^min_instances:/d; /^max_concurrent:/d' \
    "$tmp_dir/valid-Simfile" > "$pkg/Simfile"
"$sip" -C "$pkg" test --keep-limits > "$tmp_dir/defaults-report"
grep 'Total score:.*100 / 100' "$tmp_dir/defaults-report"
grep 'instances: 2' "$tmp_dir/defaults-report"
for setting in 'instances: 0' 'min_instances: 33' 'max_concurrent: 9' 'manager: ../outside.cc' 'communication: [true]'; do
    cp "$tmp_dir/valid-Simfile" "$pkg/Simfile"
    printf '%s\n' "$setting" >> "$pkg/Simfile"
    if "$sip" -C "$pkg" test --keep-limits; then
        exit 1
    fi
done
cp "$tmp_dir/valid-Simfile" "$pkg/Simfile"
if "$sip" -C "$pkg" checker manager/manager.cc; then
    exit 1
fi
cmp "$tmp_dir/valid-Simfile" "$pkg/Simfile"
if "$sip" -C "$pkg" template checker; then
    exit 1
fi
test ! -f "$pkg/check/checker.cc"
if "$sip" -C "$pkg" interactive false; then
    exit 1
fi
cmp "$tmp_dir/valid-Simfile" "$pkg/Simfile"
"$sip" -C "$pkg" communication gen
"$sip" -C "$pkg" communication regen
"$sip" -C "$pkg" communication pack
legacy="$tmp_dir/legacy"
"$sip" init "$legacy" legacy
cat > "$legacy/Simfile.communication" <<'LEGACY'
This unrelated sidecar must not select the communication judge.
LEGACY
cat > "$legacy/prog/model.cc" <<'CPP'
#include <iostream>
int main() { int n; std::cin >> n; std::cout << n << '\n'; }
CPP
printf 'communication: false\nmanager: ../ignored.cc\ninstances: 0\n' >> "$legacy/Simfile"
printf '42\n' > "$legacy/in/legacy1.in"
printf 'default_time_limit: 2\nstatic: [legacy1]\n' > "$legacy/Sipfile"
"$sip" -C "$legacy" gen
test -f "$legacy/out/legacy1.out"
"$sip" -C "$legacy" test > "$tmp_dir/legacy-report"
grep 'Total score:.*100 / 100' "$tmp_dir/legacy-report"
if "$sip" -C "$legacy" communication test; then
    exit 1
fi
cp "$legacy/Simfile" "$tmp_dir/legacy-Simfile"
if "$sip" communication init "$legacy" replacement; then
    exit 1
fi
cmp "$tmp_dir/legacy-Simfile" "$legacy/Simfile"
test ! -f "$legacy/manager/manager.cc"
if "$sip" -C "$legacy" test --keep-limits; then
    exit 1
fi
cmp "$tmp_dir/legacy-Simfile" "$legacy/Simfile"
