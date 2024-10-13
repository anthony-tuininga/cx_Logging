.. _releasenotes:

cx_Logging Release Notes
========================

Version 3.2.1 (October 2024)
----------------------------

#)  Added support for Python 3.13. Support is now for Python 3.9 and higher.


Version 3.2 (March 2024)
------------------------

#)  Support is now for Python 3.8 and higher.
#)  Added ``cx_Logging.__version__`` for compatibility with other Python
    modules that assume this attribute is present.
#)  Eliminated use of deprecated file system encoding global.
#)  Black and ruff are now used to format and lint Python code.


Version 3.1 (November 2022)
---------------------------

#)  Support is now for Python 3.7 and higher.
#)  Fix file layout for mingw
    (`PR 11 <https://github.com/anthony-tuininga/cx_Logging/pull/11>`__).
#)  Use WCHAR instead of OLECHAR on Windows
    (`PR 10 <https://github.com/anthony-tuininga/cx_Logging/pull/10>`__).
#)  Moved metadata to pyproject.toml
    (`PR 9 <https://github.com/anthony-tuininga/cx_Logging/pull/9>`__).
#)  Moved setup.py to use setuptools instead of distutils
    (`PR 8 <https://github.com/anthony-tuininga/cx_Logging/pull/8>`__).
#)  Corrected build for Python 3.11
    (`PR 7 <https://github.com/anthony-tuininga/cx_Logging/pull/7>`__).
#)  Use "deny write" (`SH_DENYWR`) when opening the log file in Windows in
    order to prevent multiple threads from opening the same file when
    `reuse=False`
    (`PR 6 <https://github.com/anthony-tuininga/cx_Logging/pull/6>`__).
#)  On Windows, include cx_Logging.h in the wheel as well
    (`PR 4 <https://github.com/anthony-tuininga/cx_Logging/pull/4>`__).
#)  Use text files as metadata for wheels
    (`PR 5 <https://github.com/anthony-tuininga/cx_Logging/pull/5>`__).
#)  Convert Python code to be PEP 8 compliant.


Version 3.0 (January 2021)
--------------------------

#)  Support is now for Python 3.6 and higher.
#)  Fix source package to include header files.


Version 2.2 (July 2018)
-----------------------

#)  Support is now for Python 2.7, 3.5, 3.6 and 3.7.
#)  If the log file is not open, don't attempt to write to it!
#)  Normalize the exception which is needed for Python 3.x.
#)  Reorganized documentation and added release notes to documentation.
#)  Switched development from Mercurial to Git.


Version 2.1 (December 2010)
---------------------------

#)  Added support for Python 2.7 and Python 3.1.
#)  Only perform the repr() calculation if the logging level is at the level
    that a log message needs to be written in order to improve performance.
#)  Ensure that stdcall calling convention is used on Windows across the board
    so that other applications which assume stdcall work as expected without
    extra work.
#)  Include export symbols to make the Microsoft compiler on Windows export
    symbols; otherwise, it ignores the request when using the stcall
    convention.
#)  Fix determination of import library for the Microsoft compiler.
#)  Expose WriteMessageForPython() and IsLoggingAtLevelForPython() which are
    needed by ceODBC.
#)  Eliminate segmentation fault if unicode string cannot be encoded.
#)  Remove situation where a failure in writing to the log file was masked.
#)  Fix support for AIX as suggested by Tamas Gulacsi.


Version 2.0 (June 2009)
-----------------------

#)  Added support for Python 3.x.
#)  Added support for logging Unicode strings in Python 2.x.
#)  Added support for setting the encoding to use for Unicode strings when
    starting logging and a Python method SetEncoding() for setting it
    afterwards and a Python method GetEncoding() to view the value currently
    being used.
#)  Added C methods StartLoggingEx(), StartLoggingStderrEx(),
    StartLoggingStdoutEx(), StartLoggingExW() and
    StartLoggingForPythonThreadEx() which provide exception information to
    the caller and (if applicable) allow the specification of whether files
    are reused and rotated (see documentation for more information).
#)  Added Python method SetExceptionInfo() which allows specification of the
    base exception class, a method for creating an instance of that class and a
    message that will be displayed prior to the logging of exceptions of that
    class.
#)  The Python method LogException() now returns a configured exception if one
    was built or passed in directly.
#)  Transformed documentation to new style used in Python 2.6 and higher and
    enhanced the contents.
#)  Added support for compiling on 64-bit Windows.


Version 1.4 (October 2007)
--------------------------

#)  On Windows, ensure that the log files are opened in such a way that they
    are not inherited by subprocesses; otherwise, the existence of a
    subprocess prevents log rotation.
#)  Build an import library on Windows and change the shared object name of
    the module so that other projects can use it directly at the C level.
#)  Removed unnecessary dependency on the win32api package on Windows.
#)  Tweaked setup script to build PKG-INFO and MANIFEST using metadata in the
    setup script instead of separate files.


Version 1.3 (March 2007)
------------------------

#)  Fix support for importing in Python 2.5 on Windows. The module now uses the
    .pyd extension instead of the .dll extension since support for importing
    modules with the .dll extension was removed in Python 2.5.
#)  Add support for 64-bit Python installations, particularly in Python 2.5.


Version 1.2 (March 2007)
------------------------

#)  Changed macros to support building with the Microsoft compiler as
    requested by Christopher Mioni.
#)  Added keywords arguments for the StartLogging() method as requested by
    Christopher Mioni.
#)  Use __DATE__ and __TIME__ to determine the date and time of the build
    rather than passing it through directly.
#)  Added support for getting a file object for the file that is currently
    being logged to or None if no logging is taking place.


Version 1.1 (January 2006)
--------------------------

#)  Raise an exception if a write fails during logging.
#)  Add module constants version and buildtime in order to aid in support and
    debugging.
