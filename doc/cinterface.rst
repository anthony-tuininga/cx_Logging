.. _cinterface:

***********
C Interface
***********

.. note::

   All methods return -1 for an error or 0 for success unless otherwise stated.

----------------
Starting Logging
----------------

.. cfunction:: int StartLogging(const char* fileName, unsigned long level, unsigned long maxfiles, unsigned long maxfilesize, const char* prefix)

   Start logging to the specified file at the specified level.


.. cfunction:: int StartLoggingEx(const char* fileName, unsigned long level, unsigned long maxfiles, unsigned long maxfilesize, const char* prefix, int reuseExistingFiles, int rotateFiles, ExceptionInfo* exceptionInfo)

   Start logging to the specified file at the specified level.


.. cfunction:: int StartLoggingFromEnvironment()

   Start logging by reading the environment variables "CX_LOGGING_FILE_NAME",
   "CX_LOGGING_LEVEL", "CX_LOGGING_MAX_FILES", "CX_LOGGING_MAX_FILE_SIZE",
   "CX_LOGGING_PREFIX" to determine the parameters to the StartLogging method.


.. cfunction:: int StartLoggingForPythonThread(const char* filename, unsigned long level, unsigned long maxfiles, unsigned long maxfilesize, const char* prefix)

   Start logging to the specified file at the specified level but only for the
   currently active Python thread.


.. cfunction:: int StartLoggingForPythonThreadEx(const char* filename, unsigned long level, unsigned long maxfiles, unsigned long maxfilesize, const char* prefix, int reuseExistingFiles, int rotateFiles)

   Start logging to the specified file at the specified level but only for the
   currently active Python thread.


.. cfunction:: int StartLoggingStderr(unsigned long level, const char* prefix)

   Start logging to stderr at the specified level.


.. cfunction:: int StartLoggingStderrEx(unsigned long level, const char* prefix, ExceptionInfo* exceptionInfo)

   Start logging to stderr at the specified level.


.. cfunction:: int StartLoggingStdout(unsigned long level, const char* prefix)

   Start logging to stdout at the given level.


.. cfunction:: int StartLoggingStdoutEx(unsigned long level, const char* prefix, ExcepitonInfo* exceptionInfo)

   Start logging to stdout at the given level.


----------------
Stopping Logging
----------------

.. cfunction:: int StopLogging()

   Stop logging.


.. cfunction:: int StopLoggingForPythonThread()

   Stop logging in the current Python thread.


----------------
Logging Messages
----------------

.. cfunction:: int LogCritical(const char* message)

   Log message at CRITICAL level.


.. cfunction:: int LogDebug(const char* message)

   Log message at DEBUG level.


.. cfunction:: int LogError(const char* message)

   Log message at ERROR level.


.. cfunction:: int LogInfo(const char* message)

   Log message at INFO level.


.. cfunction:: int LogMessage(unsigned long level, const char* message)

   Log a message at the specified level.


.. cfunction:: int LogMessageV(unsigned long level, const char* format, ...)

   Log a message at the specified level using the standard C printf format with
   arguments.


.. cfunction:: int LogMessageVaList(unsigned long level, const char* format, va_list args)

   Log a message at the specified level using the standard C printf format with
   arguments already encoded in a va_list.


.. cfunction:: int LogMessageForPythonV(unsigned long level, const char* format, ...)

   Log a message at the specified level in the logging file defined for the
   current Python thread using the standard C printf format with arguments.


.. cfunction:: int LogMessageForPythonVaList(unsigned long level, const char* format, va_list args)

   Log a message at the specified level in the logging file defined for the
   current Python thread using the standard C printf format with arguments
   already encoded in a va_list.


.. cfunction:: int LogTrace(const char* message)

   Log message regardless of what level is currently being logged. This is
   primarily of use for logging tracing messages.


.. cfunction:: int LogWarning(const char* message)

   Log message at WARNING level.


------------------
Logging Exceptions
------------------

.. cfunction:: int LogConfiguredException(PyObject* errorobj)

   Log the contents of the error object. This method expects attributes named
   "message", "templateId", "arguments", "traaceback", "details" and
   "logLevel".  If the "logLevel" attribute is missing logging is done at the
   ERROR level.  If any of the other attributes are missing or of the wrong
   type that fact is logged and processing continues. This function returns -1
   at all times as a convenience to the caller.


.. cfunction:: int LogPythonException(const char* message)

   Log the current Python exception with the given message as the first message
   that is written to the log. The exception is logged with traceback if the
   traceback module is available. This function returns -1 at all times as a
   convenience to the caller.


.. cfunction:: int LogPythonExceptionWithTraceback(const char* message, PyObject* type, PyObject* value, PyObject* traceback)

   Log the specified Python exception with the given message as the first
   message that is written to the log. The exception is logged with traceback
   if the traceback module is available. This function returns -1 at all times
   as a convenience to the caller.


-------------
Logging State
-------------

.. cfunction:: unsigned long GetLoggingLevel()

   Return the current logging level.


.. cfunction:: udt_LoggingState* GetLoggingState()

   Return the logging state for the current Python thread.


.. cfunction:: int IsLoggingStarted()

   Return 1 if logging has been started and 0 otherwise.


.. cfunction:: int SetLoggingLevel(unsigned long newlevel)

   Set the current logging level.


.. cfunction:: int SetLoggingState(udt_LoggingState* state)

   Set the logging state for the current Python thread.

