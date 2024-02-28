Release Policy
==============

Once or twice a year, a new release is cut off of the master branch and is
assigned a 0.X.Y version number, where X is incremented each time a release
contains breaking changes, such as changed options or added/removed features,
and Y is incremented if a release contains only bugfixes and other minor
changes.

Releases are tagged on the master branch and will not be maintained separately.
Patch releases may be made if the amount or severity of bugs justify it, or in
the event of security issues.

The goal of releases is to provide Linux distributions with something to
package. If you want the newest features, just use the master branch.
We try our best to keep it deployable at all times.

Releases other than the latest release are unsupported and unmaintained.

Release procedure
-----------------

While on master:

- Update the `RELEASE_NOTES` file, replacing the previous release notes.

- Update the `VERSION` file.

- Update `DOCS/client-api-changes.rst` (in particular, update the last version
  number if necessary)

- Run `TOOLS/gen-interface-changes.py` to refresh `DOCS/interface-changes.rst`,
  edit manually as necessary.

- Delete all `.txt` files in the `DOCS/interface-changes` directory except for `example.txt`.

- Create signed commit with changes.

- Create signed tag v0.X.Y.

- Push release branch (`release/0.X`) and tag to GitHub.

- Create a new GitHub release using the content of `RELEASE_NOTES` related to
  the new version.

- Readd -UNKNOWN suffix to version in `VERSION` file.

If necessary (to e.g. exclude commits already on master), the release can
be done on a branch with different commit history. The release branch **must**
then be merged to master so `git describe` will pick up the tag.

This does not apply to patch releases, which are tagged directly on the
`release/0.X` branch. The master branch always remains at v0.X.0.

Release notes template
----------------------

Here is a template that can be used for writing the `RELEASE_NOTES` file:

```markdown
Release 0.X.Y
=============

This release requires FFmpeg <ver> or newer.

Features
--------

New
~~~

- List of new features


Changed
~~~~~~~

- List of changed features


Removed
~~~~~~~

- List of removed features


Options and Commands
--------------------

Added
~~~~~

- List of added options and commands


Changed
~~~~~~~

- List of changed options and commands


Deprecated
~~~~~~~~~~

- List of deprecated options and commands


Removed
~~~~~~~

- List of removed options and commands


Fixes and Minor Enhancements
----------------------------

- List of fixes and minor enhancements


This listing is not complete. Check DOCS/client-api-changes.rst for a history
of changes to the client API, and DOCS/interface-changes.rst for a history
of changes to other user-visible interfaces.

A complete changelog can be seen by running `git log <start>..<end>`
in the git repository.
```

When creating a new point release its changes should be added on top of the
`RELEASE_NOTES` file (with the appropriate title) so that all the changes in
the current 0.X branch will be included. This way the `RELEASE_NOTES` file
can be used by distributors as changelog for point releases too.

The changelog of lists all changes since the last release, including those
that have been backported to patch releases already.

Some additional advice:
- Especially for features, try to reword the messages so that the user-visible
  change is clear to the reader. But don't simplify too much or be too verbose.
- It often makes sense to merge multiple related changes into one line
- Changes that have been made and reverted within the same release must not
  appear in the changelog
- Limit the "Options and Commands" section to relevant changes
- When filling in the GitHub release, remove the "Release 0.X.Y" heading
