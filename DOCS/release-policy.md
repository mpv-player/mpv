Release Policy
==============

Once or twice a month, a new release is cut off of the master branch and is
assigned a 0.X.Y version number, where X is incremented each time a release
contains breaking changes, such as changed options or added/removed features,
and Y is incremented if a release contains only bugfixes and other minor
changes.

Releases are tagged on the master branch and will not be maintained separately.

The goal of releases is to provide Linux distributions with something to
package. If you want the newest features, just use the master branch.
We try our best to keep it deployable at all times.

Releases other than the latest release are unsupported and unmaintained.

Release procedure
-----------------

While on master:

- Update the `RELEASE_NOTES` file, replacing the previous release notes.

- Update the `VERSION` file.

- Update `DOCS/client-api-changes.rst` and `DOCS/interface-changes.rst`
  (in particular, update the last version numbers if necessary)

- Create signed commit with changes.

- Create signed tag v0.X.Y.

- Add -UNKNOWN suffix to version in `VERSION` file.

- Commit changes, push release branch (`release/0.X`) and tag to GitHub.

- Create a new GitHub release using the content of `RELEASE_NOTES` related to
  the new version.

If necessary (to e.g. exclude commits already on master), the release can
be done on a branch with different commit history. The release branch **must**
then be merged to master so `git describe` will pick up the tag.

Release notes template
----------------------

Here is a template that can be used for writing the `RELEASE_NOTES` file.

```markdown
Release 0.X.Y
=============

Features
--------

New
~~~

- List of new features

Removed
~~~~~~~

- List of removed features

Deprecated
~~~~~~~~~~

- List of deprecated features

Behavior
--------

- List of user-visible changes in behavior

Options and Commands
--------------------

Added
~~~~~

- List of added options and commands

Changed
~~~~~~~

- List of changed options and commands

Renamed
~~~~~~~

- List of renamed options and commands

Deprecated
~~~~~~~~~~

- List of deprecated options and commands

Removed
~~~~~~~

- List of removed options and commands

Fixes and Minor Enhancements
----------------------------

- List of fixes and minor enhancements

This listing is not complete. There are many more bug fixes and changes. The
complete change log can be viewed by running `git log <start>..<end>` in
the git repository.
```

Note that the "Release 0.X.Y" title should be removed when creating a new GitHub
release.

When creating a new point release its changes should be added on top of the
`RELEASE_NOTES` file (with the appropriate title) so that all the changes in
the current 0.X branch will be included. This way the `RELEASE_NOTES` file
can be used by distributors as changelog for point releases too.
