#!/usr/bin/env bash
#
# Attaches the artifacts of the current workflow run to the rolling
# "git-release" pre-release and rewrites its notes.
#
# Needs GH_REPO and GH_TOKEN on top of the GITHUB_* variables of the run.
set -euo pipefail
tag=git-release
base_url="https://github.com/$GH_REPO/releases/download/$tag"

artifacts=$(gh api "repos/{owner}/{repo}/actions/runs/$GITHUB_RUN_ID/artifacts")

# Attach every build artifact unchanged; the notes link to them with
# labels derived from the verbose names.
mkdir -p release
jq -r '.artifacts[] | "\(.id)\t\(.name)"' <<< "$artifacts" |
while IFS=$'\t' read -r id name; do
  gh api "repos/{owner}/{repo}/actions/artifacts/$id/zip" > "release/$name.zip"
done

section() {
  jq -r --arg url "$base_url" --arg re "$1" '
    def pretty:
      (if   test("i686")    then "i686"
       elif test("aarch64") then "arm64"
       elif test("-arm$")   then "arm64"
       elif test("intel")   then "x86_64"
       else "x86_64" end) as $arch
      | [ (if test("mingw") then "GCC" elif test("msvc") then "Clang" else empty end),
          (if test("macos") then "macOS " + capture("macos-(?<v>[0-9]+)").v else empty end),
          (if test("-pdb") then "debug symbols" else empty end) ] as $q
      | $arch + (if ($q | length) > 0 then " (" + ($q | join(", ")) + ")" else "" end);
    # primary builds first (x86_64, arm64, i686; Clang before GCC), debug symbols last
    def rank:
      [ (if test("-pdb") then 1 else 0 end),
        (if test("x86_64|intel") then 0 elif test("i686") then 2 else 1 end),
        (if test("mingw") then 1 else 0 end),
        (if test("macos") then (capture("macos-(?<v>[0-9]+)").v | tonumber) else 0 end) ];
    [ .artifacts[] | select(.name | test($re)) ] | sort_by(.name | rank)
    | map("* [\(.name | pretty)](\($url)/\(.name).zip)") | join("\n")
  ' <<< "$artifacts"
}

# Match the version string baked into the build (see common/meson.build).
version=$(git describe --abbrev=9 --tags --dirty --match "v0.*")
notes=$(cat <<EOF
Automated development build of the latest \`master\` branch.

- Version: \`$version\`
- Commit: $GITHUB_SHA
- Built: $(date -u '+%Y-%m-%d %H:%M:%S UTC')

### Windows

$(section 'w64|msvc')

### macOS

$(section 'macos')

> [!WARNING]
> These are untested development builds of the very latest \`master\`.
> They are **not** official releases. For stable builds see the
> [latest release](https://github.com/$GH_REPO/releases/latest).
EOF
)

# Create the release once (which also creates the tag), then only edit
# it in place so watchers aren't notified on every master push. Stale
# assets are dropped first since their names embed the run id.
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
