#!/bin/bash
# Packs a release archive from built executables (used by
# .github/workflows/build.yml, also runnable by hand from the repository root).
#
#   tools/ci/package-release.sh amiga <version> <outdir> <exe>...
#       -> <outdir>/ctp2-amiga-<version>.lha and
#          <outdir>/ctp2-amiga-exe-<version>.lha (programs only); needs lha
#          (LHa for UNIX)
#   tools/ci/package-release.sh linux <version> <outdir> <ctp2> <mapgen .so>...
#       -> <outdir>/ctp2-linux-x86_64-<version>.tar.gz
#
# The archive holds the tree the game expects: the program in
# CTP2/ctp2_code/ctp/, the repository's ctp2_data and Scenarios two levels up.
# The original game data (CD or GOG) is not included; README.txt says how to
# merge it.

set -euo pipefail

platform=$1
version=$2
outdir=$(mkdir -p "$3" && cd "$3" && pwd)
shift 3

root=$(pwd)
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT

top=$stage/CTP2
prog=$top/ctp2_code/ctp
mkdir -p "$prog"

# The data and the program dir's runtime files, as committed (no build output,
# no local game data).
git -C "$root" archive --format=tar HEAD ctp2_data Scenarios | tar -x -C "$top"
for f in anet.inf appstr.txt bootserv.txt civpaths.txt nguid.ctp; do
	cp "$root/ctp2_code/ctp/$f" "$prog/"
done
cp "$root/tools/ci/README-release.txt" "$top/README.txt"
cp "$root/LICENSE.md" "$root/EULA - Source Code for CTP2.md" "$top/"
printf '%s\n' "$version" > "$top/VERSION.txt"

case $platform in
amiga)
	for exe in "$@"; do
		cp "$exe" "$prog/"
	done
	cp "$root/ctp2_code/os/amiga/README.md" "$top/README-Amiga.md"
	name=ctp2-amiga-$version
	rm -f "$outdir/$name.lha" "$outdir/ctp2-amiga-exe-$version.lha"
	(cd "$stage" && lha -aq "$outdir/$name.lha" CTP2)
	# The programs alone, to update an installed tree.
	(cd "$stage" && lha -aq "$outdir/ctp2-amiga-exe-$version.lha" \
		CTP2/VERSION.txt $(cd "$stage" && for exe in "$@"; do echo "CTP2/ctp2_code/ctp/$(basename "$exe")"; done))
	;;
linux)
	cp "$1" "$prog/ctp2"
	chmod +x "$prog/ctp2"
	shift
	mkdir -p "$prog/dll/map"
	for so in "$@"; do
		cp "$so" "$prog/dll/map/"
	done
	name=ctp2-linux-x86_64-$version
	tar -C "$stage" -czf "$outdir/$name.tar.gz" CTP2
	;;
*)
	echo "unknown platform $platform" >&2
	exit 1
	;;
esac

ls -la "$outdir"
