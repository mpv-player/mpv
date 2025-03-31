PYTHON SCRIPTING
================

mpv can load Python scripts. (See `Script location`_.)


Python specific options
-----------------------

- enable-python

    Config option (/ runtime option) `enable-python` has the default value
    `false` (/ `no`) and hence by default Python doesn't initialize on runs of
    mpv, unable to load any python script. This option has been set to `false`
    because if there's no script need to run then having python on the heap is a
    waste of resource. To be able to run python scripts, set `enable-pyhton` to
    `yes` on mpv.conf file.
