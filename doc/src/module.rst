.. module:: cx_Logging

.. _module:

****************
Module Interface
****************

----------------
Starting Logging
----------------

.. note::

   See the :ref:`overview` for more information on the parameters specified
   here.


.. function:: StartLogging(fileName, level, maxFiles = 1, maxFileSize = 1048576, prefix = "%t", reuse = True, rotate = True)

   Start logging to the specified file at the specified level.


.. function:: StartLoggingForThread(fileName, level, maxFiles = 1, maxFileSize = 1048576, prefix = "%t", reuse = True, rotate = True)

   Start logging to the specified file at the specified level, but only for the
   current Python thread.


.. function:: StartLoggingStderr(level, prefix = "%t")

   Start logging to stderr at the specified level.


.. function:: StartLoggingStdout(level, prefix = "%t")

   Start logging to stdout at the specified level.



----------------
Stopping Logging
----------------

.. function:: StopLogging()

   Stop logging.


.. function:: StopLoggingForThread()

   Stop logging for the given Python thread.


----------------
Logging Messages
----------------

.. function:: Critical(format, \*args)

   Log a message at the CRITICAL level. The format and arguments are the
   standard Python format.


.. function:: Debug(format, \*args)

   Log a message at the DEBUG level. The format and arguments are the
   standard Python format.


.. function:: Error(format, \*args)

   Log a message at the ERROR level. The format and arguments are the
   standard Python format.


.. function:: Info(format, \*args)

   Log a message at the INFO level. The format and arguments are the
   standard Python format.


.. function:: Log(level, format, \*args)

   Log a message at the specified level. The format and arguments are the
   standard Python format.


.. function:: Trace(format, \*args)

   Log a message regardless of the current level. The format and arguments are
   the standard Python format.


.. function:: Warning(level, format, \*args)

   Log a message at the WARNING level. The format and arguments are the
   standard Python format.


------------------
Logging Exceptions
------------------

.. function:: LogException([value, configuredExcBaseClass])

   Log the exception just raised. The value is either a string (in which case
   the exception will be retrieved from the thread state) or the value is a
   configured exception (in which case it will be used directly). Not
   specifying either will use the information specified in SetExceptionInfo()
   instead and if that is not specified default values will be used.


.. function:: SetExceptionInfo(baseClass, [builder, message])

   Define the behavior that specifies how exceptions are logged. The first
   parameter specifies the base class of exceptions that are logged in a
   special way and once specified does not need to be specified in the
   method LogException(). The second parameter specifies a method to call to
   build an instance of the base class if the exception is not already an
   instance of that class. The last parameter specifies the message that
   is logged immediately prior to logging the exception.


-------------
Logging State
-------------

.. function:: GetEncoding()

   Return the encoding currently in place for logging Unicode objects.


.. function:: GetLoggingFile()

   Return the file object to which logging is currently taking place.


.. function:: GetLoggingFileName()

   Return the name of the file to which logging is currently taking place.


.. function:: GetLoggingLevel()

   Return the current logging level.


.. function:: GetLoggingState()

   Return the current logging state.


.. function:: SetEncoding(encoding)

   Set the encoding to use for logging Unicode objects.


.. function:: SetLoggingLevel(level)

   Set the current logging level.


.. function:: SetLoggingState(state)

   Set the current logging state.


---------
Constants
---------

.. data:: buildtime

   The date and time when the module was built.


.. data:: CRITICAL

   The level at which critical errors are logged.


.. data:: DEBUG

   The level at which debugging messages are logged.


.. data:: ENV_NAME_FILE_NAME

   The environment variable name used for defining the file to which messages
   are to be logged.


.. data:: ENV_NAME_LEVEL

   The environment variable name used for defining the level at which messages
   are to be logged.


.. data:: ENV_NAME_MAX_FILES

   The environment variable name used for defining the maximum number of files
   to use in the rotating logging scheme.


.. data:: ENV_NAME_MAX_FILE_SIZE

   The environment variable name used for defining the maximum size of files
   before the files are rotated.


.. data:: ENV_NAME_PREFIX

   The environment variable name used for defining the prefix to use for all
   messages.


.. data:: ERROR

   The level at which errors are logged.


.. data:: INFO

   The level at which information messages are logged.


.. data:: NONE

   The level at which no messages are logged.


.. data:: version

   The version of the module.


.. data:: WARNING

   The level at which warning messages are logged.
