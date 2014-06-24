Release Policy
==============

Every few months, a new release is cut off of the master branch and is assigned
a 0.X.0 version number.

As part of the maintenance process, minor releases are made, which are assigned
0.X.Y version numbers. Minor releases contain bug fixes only. They never merge
the master branch, and no features are added to it. Only the latest release is
maintained.

The goal of releases is to provide stability and an unchanged base for the sake
of Linux distributions. If you want the newest features, just use the master
branch, which is stable most of the time, except sometimes, when it's not.

Releases other than the latest release are unsupported and unmaintained.

Release procedure
-----------------

- Create branch release/0.X or cherry-pick commits to the relevant branch.

- Create and/or update the ``RELEASE_NOTES`` file.

- Create and/or update the ``VERSION`` file.

- Create tag v0.X.Y.

- Push branch and tag to GitHub.

- Create a new GitHub release using the content of ``RELEASE_NOTES``.

Release notes template
----------------------

Here is a template that can be used for writing the ``RELEASE_NOTES`` file.

```markdown
Changes
-------

- List of changes.

Bug fixes
---------

- List of bug fixes.

New features
------------

- List of new features.

This listing is not complete. There are many more bug fixes and changes. The
complete change log can be viewed by running ``git log <start>..<end>`` in
the git repository.
```
