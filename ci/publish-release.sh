#!/usr/bin/env bash
#
# Attaches the artifacts of the current workflow run to a GitHub release.
# A push to master updates the rolling "git-release" pre-release along with
# its notes. A pushed release tag gets the assets attached to its release,
# which is created as a draft when it does not exist yet, so the notes can be
# filled in before publishing.
#
# Needs GH_REPO and GH_TOKEN on top of the GITHUB_* variables of the run.
set -euo pipefail

artifacts=$(gh api "repos/{owner}/{repo}/actions/runs/$GITHUB_RUN_ID/artifacts")

# Only the builds people are meant to download are published, the other
# artifacts exist for CI coverage.
windows='mingw32-full|i686-w64-mingw32|windows-msvc'
published="$windows|macos-|^libmpv-"

if [[ $GITHUB_REF == refs/tags/* ]]; then
    tag=$GITHUB_REF_NAME
    # Release assets carry the version only, the run id is CI bookkeeping.
    strip="-$GITHUB_RUN_ID"
else
    tag=git-release
    strip=
fi

mkdir -p release
jq -r --arg re "$published" '
    .artifacts[] | select(.name | test($re)) | "\(.id)\t\(.name)"
' <<< "$artifacts" |
while IFS=$'\t' read -r id name; do
    gh api "repos/{owner}/{repo}/actions/artifacts/$id/zip" > "release/${name/$strip/}.zip"
done

if [[ $tag != git-release ]]; then
    if gh release view "$tag" >/dev/null 2>&1; then
        gh release upload "$tag" --clobber release/*
    else
        gh release create "$tag" --draft --title "$tag" --notes-file RELEASE_NOTES release/*
    fi
    exit 0
fi

base_url="https://github.com/$GH_REPO/releases/download/$tag"

section() {
    jq -r --arg url "$base_url" --arg re "$1" '
      def pretty:
        (if   test("i686")    then "i686"
         elif test("aarch64") then "arm64"
         elif test("-arm$")   then "arm64"
         else "x86_64" end) as $arch
        | [ (if   test("mingw32-full") then "full"
             elif test("mingw32$")     then "GCC"
             elif test("windows-msvc") then "Clang"
             elif test("-lgpl$")       then "LGPL"
             elif test("-gpl$")        then "GPL"
             else empty end),
            (if test("macos") then "macOS " + capture("macos-(?<v>[0-9]+)").v else empty end),
            (if test("-pdb$") then "debug symbols" else empty end) ] as $q
        | $arch + (if ($q | length) > 0 then " (" + ($q | join(", ")) + ")" else "" end);
      # x86_64, arm64, i686 in that order, full before Clang, GPL before LGPL,
      # debug symbols last
      def rank:
        [ (if test("-pdb$") then 1 else 0 end),
          (if test("x86_64|intel") then 0 elif test("i686") then 2 else 1 end),
          (if test("windows-msvc|-lgpl$") then 1 else 0 end),
          (if test("macos") then (capture("macos-(?<v>[0-9]+)").v | tonumber) else 0 end) ];
      [ .artifacts[] | select(.name | test($re)) ] | sort_by(.name | rank)
      | map("* [\(.name | pretty)](\($url)/\(.name).zip)") | join("\n")
    ' <<< "$artifacts"
}

# Match the version string baked into the build (see common/meson.build).
version=$(git describe --abbrev=9 --tags --dirty --match "v0.*")
notes=$(cat <<NOTES
Automated development build of the latest \`master\` branch.

- Version: \`$version\`
- Commit: $GITHUB_SHA
- Built: $(date -u '+%Y-%m-%d %H:%M:%S UTC')

### Windows

Full builds are static mingw-w64 builds with every optional feature, such as
DVD and Blu-ray playback. Clang builds are smaller made with the Windows SDK
and come with debug symbols, but does not include all optional features.

$(section "$windows")

### libmpv (Windows)

\`libmpv-2.dll\` with import library and headers. GPL builds are GPLv3, mpv
with all its GPL parts and FFmpeg with \`--enable-gpl --enable-version3\`.
LGPL builds are LGPLv3, made with \`-Dgpl=false\` against the LGPL FFmpeg
variant, for software that cannot link GPL.

$(section '^libmpv-')

### macOS

$(section 'macos')

> [!WARNING]
> These are untested development builds of the very latest \`master\`.
> They are **not** official releases. For stable builds see the
> [latest release](https://github.com/$GH_REPO/releases/latest).
NOTES
)

# Create the release once (which also creates the tag), then only edit it in
# place so watchers aren't notified on every master push. Stale assets are
# dropped first since their names embed the run id.
if gh release view "$tag" >/dev/null 2>&1; then
    gh release view "$tag" --json assets --jq '.assets[].name' |
    while read -r asset; do
        gh release delete-asset "$tag" "$asset" --yes
    done
    gh release edit "$tag" \
        --prerelease \
        --latest=false \
        --title "mpv development build" \
        --notes "$notes"
    gh release upload "$tag" release/*
else
    gh release create "$tag" \
        --target "$GITHUB_SHA" \
        --prerelease \
        --latest=false \
        --title "mpv development build" \
        --notes "$notes" \
        release/*
fi
