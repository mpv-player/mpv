How to contribute
=================

General
-------

The main contact for mpv development is IRC, specifically #mpv
and #mpv-devel on Libera.chat. Github is used for code review and
long term discussions.

Sending patches
---------------

- Make a github pull request, or send a link to a plaintext patch created with
  ``git format-patch``.
- Plain diffs posted as pastebins are not acceptable! (Especially if the http
  link returns HTML.) They only cause extra work for everyone, because they lack
  commit message and authorship information.
- Never send patches to any of the developers email addresses.
- If your changes are not supposed to be merged immediately, mark them as
  "[RFC]" in the commit message or the pull request title.
- Be sure to test your changes. If you didn't, please say so in the commit
  message and the pull request text.

Copyright of contributions
--------------------------

- The copyright belongs to contributors. The project is a collaborative work. By
  sending your changes, you agree to license your contributions according to the
  requirements of this project.
- All new code must be LGPLv2.1+ licensed, or come with the implicit agreement
  that it will be relicensed to LGPLv2.1+ later (see ``Copyright`` in the
  repository root directory).
- 100% compatible licenses are allowed too.
- Changes in files with more liberal licenses (such as BSD, MIT, or ISC) are
  assumed to be dual-licensed under LGPLv2.1+ and the license indicated in the
  file header.
- You must be either the exclusive author of the patch, or acknowledge all
  authors involved in the commit message. If you take 3rd party code, authorship
  and copyright must be properly acknowledged. If you're making changes on
  behalf of your employer, and the employer owns the copyright, you must mention
  this. If the license of the code is not LGPLv2.1+, you must mention this.
- These license statements are legally binding.
- Don't use fake names (something that looks like an actual name, and may be
  someone else's name, but is not your legal name). Using a pseudonyms is
  allowed if it can be used to identify or contact you, even if whatever
  account you used to submit the patch dies.
- Do not add your name to the license header. This convention is not used by
  this project, and neither copyright law nor any of the used licenses require
  it.

Write good commit messages
--------------------------

- Write informative commit messages. Use present tense to describe the
  situation with the patch applied, and past tense for the situation before
  the change.
- The subject line (the first line in a commit message) must contain a
  prefix identifying the sub system, followed by a short description what
  impact this commit has. This subject line and the commit message body
  must not be longer than 72 characters per line, because it messes up the
  output of many git tools otherwise.

  For example, you fixed a crash in af_volume.c:

  - Bad: ``fixed the bug (wtf?)``
  - Good: ``af_volume: fix crash due to null pointer access``

  Having a prefix gives context, and is especially useful when trying to find
  a specific change by looking at the history, or when running ``git blame``.

  Sample prefixes: ``vo_gpu: ...``, ``command: ...``, ``DOCS/input: ...``,
  ``TOOLS/osxbundle: ...``, ``osc.lua: ...``, etc. You can always check the git
  log for commits which modify specific files to see which prefixes are used.

- The first word after the ``:`` is lower case.
- Don't end the subject line with a ``.``.
- Put an empty line between the subject line and the commit message.
  If this is missing, it will break display in common git tools.
- The body of the commit message (everything else after the subject line) must
  be as informative as possible and contain everything that isn't obvious. Don't
  hesitate to dump as much information as you can - it doesn't cost you
  anything. Put some effort into it. If someone finds a bug months or years
  later, and finds that it's caused by your commit (even though your commit was
  supposed to fix another bug), it would be bad if there wasn't enough
  information to test the original bug. The old bug might be reintroduced while
  fixing the new bug.

  The commit message must be wrapped on 72 characters per line, because git
  tools usually do not break text automatically. On the other hand, you do not
  need to break text that would be unnatural to break (like data for test cases,
  or long URLs).
- Another summary of good conventions: https://chris.beams.io/posts/git-commit/

Split changes into multiple commits
-----------------------------------

- Follow git good practices, and split independent changes into several commits.
  It's usually OK to put them into a single pull request.
- Try to separate cosmetic and functional changes. It's ok to make a few
  additional cosmetic changes in the same file you're working on. But don't do
  something like reformatting a whole file, and hiding an actual functional
  change in the same commit.
- Splitting changes does _not_ mean that you should make them as fine-grained
  as possible. Commits should form logical steps in development. The way you
  split changes is important for code review and analyzing bugs.

Always squash fixup commits when making changes to pull requests
----------------------------------------------------------------

- If you make fixup commits to your pull request, you should generally squash
  them with "git rebase -i". We prefer to have pull requests in a merge
  ready state.
- We don't squash-merge (nor do we use github's feature that does this) because
  pull requests with multiple commits are perfectly legitimate, and the only
  thing that makes sense in non-trivial cases.
- With complex pull requests, it *may* make sense to keep them separate, but
  they should be clearly marked as such. Reviewing commits is generally easier
  with fixups squashed.
- Reviewers are encouraged to look at individual commits instead of github's
  "changes from all commits" view (which just encourages bad git and review
  practices).

Touching user-visible parts may require updating the mpv docs
-------------------------------------------------------------

- Most user-visible things are normally documented in DOCS/man/. If your commit
  touches documented behavior, list of sub-options, etc., you need to adjust the
  documentation.
- These changes usually go into the same commit that changes the code.
- Changes to command line options (addition/modification/removal) must be
  documented in options.rst.
- Changes to input properties or input commands must be documented in input.rst.
- All incompatible changes to the user interface (options, properties, commands)
  must be documented with a small note in interface-changes.rst. (Additions may
  be documented there as well, but this isn't required.)
- Changes to the libmpv API must be reflected in the libmpv's headers doxygen,
  and in client-api-changes.rst.

Code formatting
---------------

mpv uses C99 with K&R formatting, with some exceptions.

- Use the K&R indent style.
- Use 4 spaces of indentation, never use tabs (except in Makefiles).
- Add a single space between keywords and binary operators. There are some other
  cases where spaces must be added. Example:

    ```C
    if ((a * b) > c) {
        // code
        some_function(a, b, c);
    }
    ```
- Break lines on 80 columns. There is a hard limit of 85 columns. You may ignore
  this limit if there's a strong case that not breaking the line will increase
  readability. Going over 85 columns might provoke endless discussions about
  whether such a limit is needed or not, so avoid it.
- If the body of an if/for/while statement has more than 1 physical lines, then
  always add braces, even if they're technically redundant.

  Bad:

    ```C
    if (a)
        // do something if b
        if (b)
            do_something();
    ```

  Good:

    ```C
    if (a) {
        // do something if b
        if (b)
            do_something();
    }
    ```
- If the if has an else branch, both branches must use braces, even if they're
  technically redundant.

  Example:

    ```C
    if (a) {
        one_line();
    } else {
        one_other_line();
    }
    ```
- If an if condition spans multiple physical lines, then put the opening brace
  for the if body on the next physical line. (Also, preferably always add a
  brace, even if technically none is needed.)

  Example:

    ```C
    if (very_long_condition_a &&
        very_long_condition_b)
    {
        code();
    } else {
        ...
    }
    ```

  (If the if body is simple enough, this rule can be skipped.)
- Remove any trailing whitespace.
- Do not make stray whitespaces changes.

Header #include statement order
-------------------------------

The order of ``#include`` statements in the source code is not very consistent.
New code must follow the following conventions:

- Put standard includes (``#include <stdlib.h>`` etc.) on the top,
- then after a blank line, add library includes (``#include <zlib.h>`` etc.)
- then after a blank line, add internal includes (``#include "player/core.h"``)
- sort them alphabetically within these sections

General coding
--------------

- Use C99. Also freely make use of C99 features if it's appropriate, such as
  stdbool.h. (Except VLA and complex number types.)
- Don't use non-standard language (such as GNU C-only features). In some cases
  they may be warranted, if they are optional (such as attributes enabling
  printf-like format string checks). "#pragma once" is allowed as an exception.
  But in general, standard C99 must be used.
- The same applies to libc functions. We have to be Windows-compatible too. Use
  functions guaranteed by C99 or POSIX only, unless your use is guarded by a
  configure check. There is some restricted use of C11 (ask on IRC for details).
- Prefer fusing declaration and initialization, rather than putting declarations
  on the top of a block. Obvious data flow is more important than avoiding
  mixing declarations and statements, which is just a C90 artifact.
- If you add features that require intrusive changes, discuss them on the dev
  channel first. There might be a better way to add a feature and it can avoid
  wasted work.

Code of Conduct
---------------

Please note that this project is released with a Contributor Code of Conduct.
By participating in this project you agree to abide by its terms.
The Contributor Code of Conduct can be found here:
https://www.contributor-covenant.org/version/2/0/code_of_conduct/

Rules for git push access
-------------------------

Push access to the main git repository is handed out on an arbitrary basis. If
you got access, the following rules must be followed:

- You are expected to follow the general development rules as outlined in this
  whole document.
- You must be present on the IRC dev channel when you push something.
- Anyone can push small fixes: typo corrections, small/obvious/uncontroversial
  bug fixes, edits to the user documentation or code comments, and so on.
- You can freely make changes to parts of the code which you maintain. For
  larger changes, it's recommended to let others review the changes first.
- You automatically maintain code if you wrote or modified most of it before
  (e.g. you made larger changes to it before, did partial or full rewrites, did
  major bug fixes, or you're the original author of the code). If there is more
  than one maintainer, you may need to come to an agreement with the others how
  to handle this to avoid conflict.
- If you make a pull requests (especially if it's to code you maintain), and you
  want reviews, explicitly ping the people from which you expect reviews.
- As a maintainer, you can approve pull requests by others to "your" code.
- If you approve or merge 3rd party changes, make sure they follow the general
  development rules.
- Changes to user interface and public API must always be approved by the
  project leader.
- Seasoned project members are allowed to revert commits that broke the build,
  or broke basic functionality in a catastrophic way, and the developer who
  broke it is unavailable. (Depending on severity.)
- Adhere to the CoC.
- The project leader is not bound by these rules.
