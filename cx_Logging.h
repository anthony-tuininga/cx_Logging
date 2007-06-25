//-----------------------------------------------------------------------------
// cx_Logging.h
//   Include file for managing logging.
//-----------------------------------------------------------------------------

#include <Python.h>

// define platform specific variables
#ifdef WIN32
    #include <windows.h>
    #define LOCK_TYPE CRITICAL_SECTION
    #ifdef __WIN32__
        #ifdef CX_LOGGING_CORE
            #define CX_LOGGING_API __declspec(dllexport) __stdcall
        #else
            #define CX_LOGGING_API __declspec(dllimport) __stdcall
        #endif
    #else
        #ifdef CX_LOGGING_CORE
            #define CX_LOGGING_API __declspec(dllexport)
        #else
            #define CX_LOGGING_API __declspec(dllimport)
        #endif
    #endif
#else
    #include <pthread.h>
    #include <semaphore.h>
    #define LOCK_TYPE sem_t
    #define CX_LOGGING_API
#endif

// define structure for managing logging state
typedef struct {
    FILE *fp;
    char *fileName;
    char *fileMask;
    char *fileNameTemp_1;
    char *fileNameTemp_2;
    char *prefix;
    unsigned long level;
    int fileOwned;
    unsigned long maxFiles;
    unsigned long maxFileSize;
} LoggingState;


// define structure for managing logging state for Python
typedef struct {
    PyObject_HEAD
    LoggingState *state;
    LOCK_TYPE lock;
} udt_LoggingState;


// define logging levels
#define LOG_LEVEL_DEBUG			10
#define LOG_LEVEL_INFO			20
#define LOG_LEVEL_WARNING		30
#define LOG_LEVEL_ERROR			40
#define LOG_LEVEL_CRITICAL		50
#define LOG_LEVEL_NONE			100


// define defaults
#define DEFAULT_MAX_FILE_SIZE		1024 * 1024
#define DEFAULT_PREFIX			"%t"


// declarations of methods exported
CX_LOGGING_API int StartLogging(const char*, unsigned long, unsigned long,
		unsigned long, const char *);
CX_LOGGING_API int StartLoggingForPythonThread(const char*, unsigned long,
		unsigned long, unsigned long, const char *);
CX_LOGGING_API int StartLoggingStderr(unsigned long, const char *);
CX_LOGGING_API int StartLoggingStdout(unsigned long, const char *);
CX_LOGGING_API int StartLoggingFromEnvironment(void);
CX_LOGGING_API void StopLogging(void);
CX_LOGGING_API void StopLoggingForPythonThread(void);
CX_LOGGING_API int LogMessage(unsigned long, const char*);
CX_LOGGING_API int LogMessageV(unsigned long, const char*, ...);
CX_LOGGING_API int LogMessageVaList(unsigned long, const char*, va_list);
CX_LOGGING_API int LogMessageForPythonV(unsigned long, const char*, ...);
CX_LOGGING_API int LogMessageForPythonVaList(unsigned long, const char*,
        va_list);
CX_LOGGING_API int LogDebug(const char*);
CX_LOGGING_API int LogInfo(const char*);
CX_LOGGING_API int LogWarning(const char*);
CX_LOGGING_API int LogError(const char*);
CX_LOGGING_API int LogCritical(const char*);
CX_LOGGING_API int LogTrace(const char*);
CX_LOGGING_API unsigned long GetLoggingLevel(void);
CX_LOGGING_API int SetLoggingLevel(unsigned long);
CX_LOGGING_API int LogPythonObject(unsigned long, const char*, const char*,
        PyObject*);
CX_LOGGING_API int LogPythonException(const char*);
CX_LOGGING_API int LogPythonExceptionWithTraceback(const char*, PyObject*,
        PyObject*, PyObject*);
CX_LOGGING_API int LogConfiguredException(PyObject*);
CX_LOGGING_API udt_LoggingState* GetLoggingState(void);
CX_LOGGING_API int SetLoggingState(udt_LoggingState*);
CX_LOGGING_API int IsLoggingStarted(void);

#if defined WIN32 && !defined UNDER_CE
CX_LOGGING_API int LogWin32Error(DWORD, const char*);
CX_LOGGING_API int LogGUID(unsigned long, const char*, const IID*);
#endif

#ifdef WIN32
CX_LOGGING_API int StartLoggingW(const OLECHAR*, unsigned long, unsigned long,
		unsigned long, const OLECHAR*);
CX_LOGGING_API int LogMessageW(unsigned long, const OLECHAR*);
CX_LOGGING_API int LogDebugW(const OLECHAR*);
CX_LOGGING_API int LogInfoW(const OLECHAR*);
CX_LOGGING_API int LogWarningW(const OLECHAR*);
CX_LOGGING_API int LogErrorW(const OLECHAR*);
CX_LOGGING_API int LogCriticalW(const OLECHAR*);
CX_LOGGING_API int LogTraceW(const OLECHAR*);
#endif
