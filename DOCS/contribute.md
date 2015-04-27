How to contribute
=================

General
-------

The main contact for mpv development is IRC, specifically #mpv
and #mpv-devel on Freenode.

Sending patches
---------------

- Make a github pull request, or send a link to a plaintext patch created with
  ``git format-patch``. diffs posted as pastebins (especially if the http link
  returns HTML) just cause extra work for everyone, because they lack commit
  message and authorship information.
- When creating pull requests, be sure to test your changes. If you didn't, please
  say so in the pull request message. 

- Write informative commit messages. Use present tense to describe the
  situation with the patch applied, and past tense for the situation before
  the change.
- The subject line (the first line in a commit message) should contain a
  prefix identifying the sub system, followed by a short description what
  impact this commit has. This subject line shouldn't be longer than 72
  characters, because it messes up the output of many git tools otherwise.

  For example, you fixed a crash in af_volume.c:

  Bad: ``fixed the bug (wtf?)``
  Good: ``af_volume: fix crash due to null pointer access``

  Having a prefix gives context, and is especially useful when trying to find
  a specific change by looking at the history, or when running ``git blame``.
- The body of the commit message (everything else after the subject line) should
  be as informative as possible and contain everything that isn't obvious. Don't
  hesitate to dump as much information as you can - it doesn't cost you
  anything. Put some effort into it. If someone finds a bug months or years
  later, and finds that it's caused by your commit (even though your commit was
  supposed to fix another bug), it would be bad if there wasn't enough
  information to test the original bug. The old bug might be reintroduced while
  fixing the new bug.

  The commit message should be wrapped on 72 characters per line, because git
  tools usually do not break text automatically. On the other hand, you do not
  need to break text that would be unnatural to break (like data for test cases,
  or long URLs).

  Important: put an empty line between the subject line and the commit message.
  If this is missing, it will break display in common git tools.
- Try to separate cosmetic and functional changes. It's ok to make a few
  additional cosmetic changes in the same file you're working on. But don't do
  something like reformatting a whole file, and hiding an actual functional
  change in the same commit.
- If you add a new command line option, document it in options.rst. If you
  add a new input property, document it in input.rst.

Code formatting
---------------

mpv uses C99 with K&R formatting, with some exceptions.

- Use the K&R indent style.
- Use 4 spaces of indentation, never use tabs (except in Makefiles).
- Add a single space between keywords and binary operators. There are some other
  cases where spaces should be added. Example:

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
- If the body of an if statement uses braces, the else branch should also
  use braces (and reverse).

  Example:

    ```C
    if (a) {
        // do something
        something();
        something_else();
    } else {
        one_line();
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
- Remove any trailing whitespace.
- If the file you're editing uses formatting different from from what is
  described here, it's probably an old file from times when nobody followed a
  consistent style. You're free to use the existing style, or the new style, or
  to send a patch to reformat the file to the new style before making functional
  changes.

General coding
--------------

- Use C99. Also freely make use of C99 features if it's appropriate, such as
  stdbool.h.
- Don't use GNU-only features. In some cases they may be warranted, if they
  are optional (such as attributes enabling printf-like format string checks).
  But in general, standard C99 should be used.
- The same applies to libc functions. We have to be Windows-compatible too. Use
  functions guaranteed by C99 or POSIX only, unless your use is guarded by a
  configure check.
- Prefer fusing declaration and initialization, rather than putting declarations
  on the top of a block. Obvious data flow is more important than avoiding
  mixing declarations and statements, which is just a C90 artifact.
- tech-overview.txt might help to get an overview how mpv is structured.
- If you add features that require intrusive changes, discuss them on the dev
  channel first. There might be a better way to add a feature and it can avoid
  wasted work.
