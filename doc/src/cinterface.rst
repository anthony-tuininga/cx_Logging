.. _cinterface:

***********
C Interface
***********

.. note::

   All methods return -1 for an error or 0 for success unless otherwise stated.

----------------
Starting Logging
----------------

.. note::

   See the :ref:`overview` for more information on the parameters specified
   here.


.. c:function:: int StartLogging(const char* fileName, unsigned long level, unsigned long maxfiles, unsigned long maxfilesize, const char* prefix)

   Start logging to the specified file at the specified level.


.. c:function:: int StartLoggingEx(const char* fileName, unsigned long level, unsigned long maxfiles, unsigned long maxfilesize, const char* prefix, int reuseExistingFiles, int rotateFiles, ExceptionInfo* exceptionInfo)

   Start logging to the specified file at the specified level.


.. c:function:: int StartLoggingFromEnvironment()

   Start logging by reading the environment variables "CX_LOGGING_FILE_NAME",
   "CX_LOGGING_LEVEL", "CX_LOGGING_MAX_FILES", "CX_LOGGING_MAX_FILE_SIZE",
   "CX_LOGGING_PREFIX" to determine the parameters to the StartLogging method.


.. c:function:: int StartLoggingForPythonThread(const char* filename, unsigned long level, unsigned long maxfiles, unsigned long maxfilesize, const char* prefix)

   Start logging to the specified file at the specified level but only for the
   currently active Python thread.


.. c:function:: int StartLoggingForPythonThreadEx(const char* filename, unsigned long level, unsigned long maxfiles, unsigned long maxfilesize, const char* prefix, int reuseExistingFiles, int rotateFiles)

   Start logging to the specified file at the specified level but only for the
   currently active Python thread.


.. c:function:: int StartLoggingStderr(unsigned long level, const char* prefix)

   Start logging to stderr at the specified level.


.. c:function:: int StartLoggingStderrEx(unsigned long level, const char* prefix, ExceptionInfo* exceptionInfo)

   Start logging to stderr at the specified level.


.. c:function:: int StartLoggingStdout(unsigned long level, const char* prefix)

   Start logging to stdout at the given level.


.. c:function:: int StartLoggingStdoutEx(unsigned long level, const char* prefix, ExcepitonInfo* exceptionInfo)

   Start logging to stdout at the given level.


----------------
Stopping Logging
----------------

.. c:function:: int StopLogging()

   Stop logging.


.. c:function:: int StopLoggingForPythonThread()

   Stop logging in the current Python thread.


----------------
Logging Messages
----------------

.. c:function:: int LogCritical(const char* message)

   Log message at CRITICAL level.


.. c:function:: int LogDebug(const char* message)

   Log message at DEBUG level.


.. c:function:: int LogError(const char* message)

   Log message at ERROR level.


.. c:function:: int LogInfo(const char* message)

   Log message at INFO level.


.. c:function:: int LogMessage(unsigned long level, const char* message)

   Log a message at the specified level.


.. c:function:: int LogMessageV(unsigned long level, const char* format, ...)

   Log a message at the specified level using the standard C printf format with
   arguments.


.. c:function:: int LogMessageVaList(unsigned long level, const char* format, va_list args)

   Log a message at the specified level using the standard C printf format with
   arguments already encoded in a va_list.


.. c:function:: int LogMessageForPythonV(unsigned long level, const char* format, ...)

   Log a message at the specified level in the logging file defined for the
   current Python thread using the standard C printf format with arguments.


.. c:function:: int LogMessageForPythonVaList(unsigned long level, const char* format, va_list args)

   Log a message at the specified level in the logging file defined for the
   current Python thread using the standard C printf format with arguments
   already encoded in a va_list.


.. c:function:: int LogTrace(const char* message)

   Log message regardless of what level is currently being logged. This is
   primarily of use for logging tracing messages.


.. c:function:: int LogWarning(const char* message)

   Log message at WARNING level.


------------------
Logging Exceptions
------------------

.. c:function:: int LogConfiguredException(PyObject* errorobj)

   Log the contents of the error object. This method expects attributes named
   "message", "templateId", "arguments", "traaceback", "details" and
   "logLevel".  If the "logLevel" attribute is missing logging is done at the
   ERROR level.  If any of the other attributes are missing or of the wrong
   type that fact is logged and processing continues. This function returns -1
   at all times as a convenience to the caller.


.. c:function:: int LogPythonException(const char* message)

   Log the current Python exception with the given message as the first message
   that is written to the log. The exception is logged with traceback if the
   traceback module is available. This function returns -1 at all times as a
   convenience to the caller.


.. c:function:: int LogPythonExceptionWithTraceback(const char* message, PyObject* type, PyObject* value, PyObject* traceback)

   Log the specified Python exception with the given message as the first
   message that is written to the log. The exception is logged with traceback
   if the traceback module is available. This function returns -1 at all times
   as a convenience to the caller.


-------------
Logging State
-------------

.. c:function:: unsigned long GetLoggingLevel()

   Return the current logging level.


.. c:function:: udt_LoggingState* GetLoggingState()

   Return the logging state for the current Python thread.


.. c:function:: int IsLoggingAtLevelForPython(unsigned long level)

   Return 1 if the current logging state is such that a message at the
   specified level should be logged. This is only used for cases where the
   amount of time required to calculate the message to log is sufficient that
   checking first would noticeably improve performance; otherwise, the overhead
   of acquiring and releasing the lock twice would be detrimental to
   performance.


.. c:function:: int IsLoggingStarted()

   Return 1 if logging has been started and 0 otherwise.


.. c:function:: int SetLoggingLevel(unsigned long newlevel)

   Set the current logging level.


.. c:function:: int SetLoggingState(udt_LoggingState* state)

   Set the logging state for the current Python thread.
