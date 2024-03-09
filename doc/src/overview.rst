.. _overview:

********
Overview
********

cx_Logging writes log entries to files using both a Python and C interface
which makes it particularly convenient for those Python modules written in C as well as for situations where Python is embedded but logging may be desirable
before Python is initialized and loaded. A number of concepts and parameters to
methods are used in several places and are described here for convenience.

When starting logging, if the maximum number of files is greater than 1 then
the possible file names are searched in sequential order and if a file is
missing that file name is used. If all of the file names have already been used
then the file name immediately following the most recently updated file is
chosen to begin. After that, the files are used in sequential order, wrapping
around to the beginning when necessary. Note that if the reuse flag is False,
an exception will be raised if a file exists.


-------------
Maximum Files
-------------

This parameter specifies the maximum number of files to create before rotating
back to the initial log file. If this value is greater than 1, then the log
file name is modified to include the sequence number, zero filled to ensure
that the length of the file name does not change. The default value of this
parameter is 1.


-----------------
Maximum File Size
-----------------

This parameter specifies the maximum size of log files created before switching
to a new log file but it has no effect if the value of the `Rotate`_ parameter
is False or the `Maximum Files`_ parameter is 1. The default value of this
parameter is 1 MB.


------
Prefix
------

This parameter specifies the text that is to be placed in front of every
message that is logged. Several format specifiers are available:

    - %i - write the current thread identifier

    - %l - write the level used for logging the message

    - %d - write the date in the C format %.4d/%.2d/%.2d (year/month/day)

    - %t - write the time in the C format %.2d:%.2d:%.2d.%.3d
           (hour/minute/second/fractional second)

    - %% - write a percent character

The default value of this parameter is "%t".


-----
Reuse
-----

This parameter specifies whether or not files should be reused if they exist.
The default value of this parameter is True.


------
Rotate
------

This parameter specifies whether a new log file should be started when a log
file reaches the `Maximum File Size`_. The default value of this parameter is
True but it has no effect unless `Maximum Files`_ is greater than 1.
