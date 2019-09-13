Release Policy
==============

Note: this is a proposal and pending for implementation.

Occasionally, a new release is made from the master branch and is assigned a
1.X.Y version number. Normally, Y is incremented if the release contains bug
fixes or minor changes only. If X is incremented, Y is reset to 0.

Releases are tagged on the master branch and will not be maintained separately.
Releases other than the latest release are unsupported and unmaintained.

Releases are made automatically. Commit messages contain control tags, that
affect the release note contents, the release date, and whether a minor or
major release will be made. The timing of releases completely depends on the
actual development pace.

Minor releases
--------------

Minor releases contain the following types of changes:

- Critical or minor bug fixes to a previous release (minor or major)

- Small maintenance changes of no larger consequences

- Minor features

Major releases
--------------

Major releases contain changes that affect the user or features that are
considered interesting. Breaking changes also must happen on major releases.

Breaking changes
----------------

By definition, a breaking change is a change that is incompatible to the
previous release. Normally, a breaking change must be announced 6 months
in advance using a deprecation (listed in the changelog, and optionally logged
by mpv at runtime). The actual change must happen only on a major release.

It is possible that a deadlock with breaking changes and fixes happens. On one
hand, a breaking change is supposed to be releases only after the 6 month
deprecation, on the other hand, unrelated bug fixes should be released
immediately. This is solved using two mechanisms: requiring developers to honor
the deprecation period manually, and making releases that break the policy.

Tags for controlling releases
-----------------------------

Most commits will require control tags for releases. They are part of the
commit message, separated by new lines. List of tags:

``Type: <feature|bug fix|removal|deprecation|change>``

    The type of the commit, if release relevant.

    If the "Severity" or "Fixes" tags are used, the Type must be "bug fix", or
    if the tag is not present, is implied to be this value.

``Fixes: #<bug-ID>``

    Indicate that a bug was fixed, using the issue number in the issue tracker
    associated with the hoster of the git repository's main mirror.

    The issue link may be included in the generated release notes.

``Fixes: <commit-ID>``

    This commit fixes a previous commit. The previous commit was buggy and
    didn't work properly. The releaser must take this into account.

    There can be multiple lines like this, each giving a new commit ID.

``Depends: <commit-ID>``

    This commit requires a previous commit. This may be used in case the release
    workflow includes cherry-picking, but will be pointless otherwise.

    "removal" commits may reference a commit that deprecated the removed thing.

``Severity: <minor|major|critical|security>``

    This commit is a bug fix, and the tag controls the severity of the fix.
    "minor" is the implicit default. "major" will trigger a release within a
    week. "critical" will trigger a release within 2 days. "security" will in
    addition mark the release as security fix.

    If a bug fix is critical, but the impact is minor (e.g. rarely used feature,
    which was completely broken), the Severity may be adjusted down accordingly.

``Release-note: <text>``

    Add a changelog entry with the given text.

``Interface-change: <option/property/command-name>``

    This commit changes the option, property, or command with the given name.
    Example: ``Interface-change: --fullscreen option``.

    Typically, this commit will also modify interface-changes.rst.

``Release-control: major``

    The next release should be a major release, even if the release logic
    indicates a minor release according to the commit history.

Release script
--------------

The release script checks the commits since the last commits on every git push.
It will analyze the tags, and determine when the next release should happen,
and whether it's a minor or major release. The tags will also be used to
generate the release notes.

(A human can be used until such a script is written.)

The script will modify perform the following steps:

- Update `DOCS/client-api-changes.rst` and `DOCS/interface-changes.rst`
  (in particular, update the last version numbers if necessary)

- Commit changes.

- Create tag v1.X.Y.

- Create `RELEASE_NOTES` and `VERSION` files, fill them with generated contents.
  Do not commit these changes.

- Create a tar.gz of the checkout.

- Create a new GitHub release using the tar.gz, and the use `RELEASE_NOTES`
  file as release text.

- Push the changes to github master, discard the uncomitted changes.

It will adhere to the following timings:

- If there are new commits in github master, which are not tagged as release
  worthy, but which have non-empty metadata (basically, which lead to non-empty
  release notes), make a minor release after 3 weeks of inactivity.

- If there is a "minor" severity change, or a new feature, or a deprecation,
  make a minor release after 1 week of no other such changes being added to
  git master.

- If there is a "major" severity change, make a major release after 2 weeks of
  no other activity, or after 4 weeks at the latest.

- If there is a "security" severity change, make a new release after 6 hours.
  The release is major or minor depending on previous unreleased commits.

All this assumes a branch-less release model, that does not involve cherry-
picking individual fixes into release branches.
