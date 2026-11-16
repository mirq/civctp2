#!/bin/bash
# Packs a release archive from built executables (used by
# .github/workflows/build.yml, also runnable by hand from the repository root).
#
#   tools/ci/package-release.sh amiga <version> <outdir> <exe>...
#       -> <outdir>/CTP2-Amiga-<version>.lha (everything) and
#          <outdir>/CTP2-Amiga-<version>-programs-only.lha (to update an
#          installed copy); needs lha (LHa for UNIX). An <exe> named CTP2 is
#          the launcher: it goes to the top of the drawer, with an icon.
#   tools/ci/package-release.sh linux <version> <outdir> <ctp2> <mapgen .so>...
#       -> <outdir>/CTP2-Linux-x86_64-<version>.tar.gz
#
# <version> without a leading "v" (0.2, not v0.2). The archive holds the tree
# the game expects: the program in CTP2/ctp2_code/ctp/, the repository's
# ctp2_data and Scenarios two levels up.
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
	update=()
	for exe in "$@"; do
		if [ "$(basename "$exe")" = CTP2 ]; then
			cp "$exe" "$top/CTP2"
			python3 "$root/tools/ci/make-icon.py" "$top/CTP2.info"
			update+=(CTP2/CTP2 CTP2/CTP2.info)
		else
			cp "$exe" "$prog/"
			update+=("CTP2/ctp2_code/ctp/$(basename "$exe")")
		fi
	done
	cp "$root/ctp2_code/os/amiga/README.md" "$top/README-Amiga.md"
	name=CTP2-Amiga-$version
	rm -f "$outdir/$name.lha" "$outdir/$name-programs-only.lha"
	python3 "$root/tools/ci/make-icon.py" --drawer "$stage/CTP2.info"
	(cd "$stage" && lha -aq "$outdir/$name.lha" CTP2 CTP2.info)
	# The programs alone, to update an installed tree.
	(cd "$stage" && lha -aq "$outdir/$name-programs-only.lha" CTP2/VERSION.txt "${update[@]}")
	;;
linux)
	cp "$1" "$prog/ctp2"
	chmod +x "$prog/ctp2"
	shift
	mkdir -p "$prog/dll/map"
	for so in "$@"; do
		cp "$so" "$prog/dll/map/"
	done
	name=CTP2-Linux-x86_64-$version
	tar -C "$stage" -czf "$outdir/$name.tar.gz" CTP2
	;;
*)
	echo "unknown platform $platform" >&2
	exit 1
	;;
esac

ls -la "$outdir"
