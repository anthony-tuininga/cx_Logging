//-----------------------------------------------------------------------------
// cx_Logging.c
//   Shared library for logging used by Python and C code.
//-----------------------------------------------------------------------------

#include "cx_Logging.h"

#ifndef UNDER_CE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

#define KEY_LOGGING_STATE       "cx_Logging_LoggingState"
#define KEY_EXC_BASE_CLASS      "cx_Logging_ExcBaseClass"
#define KEY_EXC_MESSAGE         "cx_Logging_ExcMessage"
#define KEY_EXC_BUILDER         "cx_Logging_ExcBuilder"
#define ENV_NAME_FILE_NAME      "CX_LOGGING_FILE_NAME"
#define ENV_NAME_LEVEL          "CX_LOGGING_LEVEL"
#define ENV_NAME_MAX_FILES      "CX_LOGGING_MAX_FILES"
#define ENV_NAME_MAX_FILE_SIZE  "CX_LOGGING_MAX_FILE_SIZE"
#define ENV_NAME_PREFIX         "CX_LOGGING_PREFIX"
#define THREAD_FORMAT           "%.5ld"
#define DATE_FORMAT             "%.4d/%.2d/%.2d"
#define TIME_FORMAT             "%.2d:%.2d:%.2d.%.3d"
#define TICKS_FORMAT            "%.10d"


// define platform specific methods for manipulating locks
#ifdef WIN32
#include <malloc.h>
#define INITIALIZE_LOCK(lock)   InitializeCriticalSection(&lock)
#define ACQUIRE_LOCK(lock)      EnterCriticalSection(&lock)
#define RELEASE_LOCK(lock)      LeaveCriticalSection(&lock)
#else
#define INITIALIZE_LOCK(lock)   sem_init(&lock, 0, 1)
#define ACQUIRE_LOCK(lock)      sem_wait(&lock)
#define RELEASE_LOCK(lock)      sem_post(&lock)
#endif

// define macro to get the build version as a string
#define xstr(s)                 str(s)
#define str(s)                  #s
#define BUILD_VERSION_STRING    xstr(BUILD_VERSION)


// define Py_ssize_t for versions before Python 2.5
#if PY_VERSION_HEX < 0x02050000
typedef int Py_ssize_t;
#define PY_SSIZE_T_MAX INT_MAX
#define PY_SSIZE_T_MIN INT_MIN
#endif


// use the bytes methods in cx_Logging and define them as the equivalent string
// type methods as is done in Python 2.6
#ifndef PyBytes_Check
    #define PyBytes_AS_STRING           PyString_AS_STRING
    #define PyBytes_AsString            PyString_AsString
    #define PyBytes_Check               PyString_Check
    #define PyBytes_FromString          PyString_FromString
#endif


// define PyInt_* macros for Python 3.x
#ifndef PyInt_Check
    #define PyInt_AsLong                PyLong_AsLong
    #define PyInt_FromLong              PyLong_FromLong
#endif


// define global logging state
static LoggingState *gLoggingState;
static LOCK_TYPE gLoggingStateLock;


//-----------------------------------------------------------------------------
// OpenFileForWriting()
//   Open the file for writing, if possible. In order to workaround the nasty
// fact that on Windows file handles are inherited by child processes by
// default and open files cannot be renamed, the handle opened by fopen() is
// first cloned to a non-inheritable file handle and the original closed. In
// this manner, any subprocess created does not prevent log file rotation from
// occurring.
//-----------------------------------------------------------------------------
static FILE* OpenFileForWriting(const char* path)
{
#ifdef WIN32
    #ifndef UNDER_CE
    HANDLE sourceHandle, targetHandle;
    int fd, dupfd;
    #endif
#endif
    FILE *fp;

    fp = fopen(path, "w");
#ifdef WIN32
    #ifndef UNDER_CE
    if (!fp)
        return NULL;
    fd = fileno(fp);
    sourceHandle = (HANDLE) _get_osfhandle(fd);
    if (!DuplicateHandle(GetCurrentProcess(), sourceHandle,
            GetCurrentProcess(), &targetHandle, 0, FALSE,
            DUPLICATE_SAME_ACCESS)) {
        fclose(fp);
        return NULL;
    }
    fclose(fp);
    dupfd = _open_osfhandle((intptr_t) targetHandle, O_TEXT);
    fp = _fdopen(dupfd, "w");
    #endif
#endif
    return fp;
}


//-----------------------------------------------------------------------------
// WriteLevel()
//   Write the level to the file.
//-----------------------------------------------------------------------------
static int WriteLevel(
    LoggingState *state,                // state to use for writing
    unsigned long level)                // level to write to the file
{
    char temp[20];
    int result;

    switch(level) {
        case LOG_LEVEL_DEBUG:
            result = fputs("DEBUG", state->fp);
            break;
        case LOG_LEVEL_INFO:
            result = fputs("INFO", state->fp);
            break;
        case LOG_LEVEL_WARNING:
            result = fputs("WARN", state->fp);
            break;
        case LOG_LEVEL_ERROR:
            result = fputs("ERROR", state->fp);
            break;
        case LOG_LEVEL_CRITICAL:
            result = fputs("CRIT", state->fp);
            break;
        case LOG_LEVEL_NONE:
            result = fputs("TRACE", state->fp);
            break;
        default:
            sprintf(temp, "%ld", level);
            result = fputs(temp, state->fp);
    }

    if (result == EOF)
        return -1;
    return 0;
}


//-----------------------------------------------------------------------------
// WritePrefix()
//   Write the prefix to the file.
//-----------------------------------------------------------------------------
static int WritePrefix(
    LoggingState *state,                // state to use for writing
    unsigned long level)                // level at which to write
{
#ifdef WIN32
    SYSTEMTIME time;
    #ifdef UNDER_CE
    DWORD ticks;
    #endif
#else
    struct timeval timeOfDay;
    struct tm time;
#endif
    char temp[40], *ptr;
    int gotTime;

    gotTime = 0;
    ptr = state->prefix;
    while (*ptr) {
        if (*ptr != '%') {
            if (fputc(*ptr++, state->fp) == EOF)
                return -1;
            continue;
        }
        ptr++;
        switch(*ptr) {
            case 'i':
#ifdef WIN32
                sprintf(temp, THREAD_FORMAT, (long) GetCurrentThreadId());
#else
                sprintf(temp, THREAD_FORMAT, (long) pthread_self());
#endif
                if (fputs(temp, state->fp) == EOF)
                    return -1;
                break;
            case 'd':
            case 't':
                if (!gotTime) {
                    gotTime = 1;
#if defined WIN32
                    GetLocalTime(&time);
    #if defined UNDER_CE
                    ticks = GetTickCount();
    #endif
#else
                    gettimeofday(&timeOfDay, NULL);
                    localtime_r(&timeOfDay.tv_sec, &time);
#endif
                }
#ifdef WIN32
                if (*ptr == 'd')
                    sprintf(temp, DATE_FORMAT, time.wYear, time.wMonth,
                            time.wDay);
                else
    #if defined UNDER_CE
                    sprintf(temp, TICKS_FORMAT, ticks);
    #else
                    sprintf(temp, TIME_FORMAT, time.wHour, time.wMinute,
                            time.wSecond, time.wMilliseconds);
    #endif
#else
                if (*ptr == 'd')
                    sprintf(temp, DATE_FORMAT, time.tm_year + 1900,
                            time.tm_mon + 1, time.tm_mday);
                else
                    sprintf(temp, TIME_FORMAT, time.tm_hour, time.tm_min,
                            time.tm_sec, (int) (timeOfDay.tv_usec / 1000));
#endif
                if (fputs(temp, state->fp) == EOF)
                    return -1;
                break;
            case 'l':
                if (WriteLevel(state, level) < 0)
                    return -1;
                break;
            case '\0':
                break;
            default:
                if (fputc('%', state->fp) == EOF)
                    return -1;
                if (fputc(*ptr, state->fp) == EOF)
                    return -1;
        }
        if (*ptr)
            ptr++;
    }
    if (*state->prefix) {
        if (fputc(' ', state->fp) == EOF)
            return -1;
    }

    return 0;
}


#ifndef UNDER_CE
//-----------------------------------------------------------------------------
// SwitchLogFiles()
//   Switch log files by renaming the current log file and any other log files
// that would be in the way. The last log file that would be renamed is deleted
// if keeping it would exceed the maximum number of files to keep.
//-----------------------------------------------------------------------------
static int SwitchLogFiles(
    LoggingState *state)                // state to use
{
    unsigned long lastFileFound, i;
    struct stat statBuffer;

    // attempt to find a gap
    lastFileFound = 0;
    for (i = 1; i < state->maxFiles; i++) {
        sprintf(state->fileNameTemp_1, state->fileMask, i);
        if (stat(state->fileNameTemp_1, &statBuffer) < 0)
            break;
        lastFileFound = i;
    }

    // if no gap, delete the last log file
    if (lastFileFound == state->maxFiles - 1) {
        if (unlink(state->fileNameTemp_1) < 0)
            return -1;
        lastFileFound--;
    }

    // rename each of the other files in sequence
    for (i = lastFileFound; i > 0; i--) {
        sprintf(state->fileNameTemp_1, state->fileMask, i);
        sprintf(state->fileNameTemp_2, state->fileMask, i + 1);
        if (rename(state->fileNameTemp_1, state->fileNameTemp_2) < 0)
            return -1;
    }

    // finally, rename the original log file
    sprintf(state->fileNameTemp_1, state->fileMask, 1);
    if (rename(state->fileName, state->fileNameTemp_1) < 0)
        return -1;

    return 0;
}


//-----------------------------------------------------------------------------
// CheckForLogFileFull()
//   Checks to determine if the current file has reached its maximum size and
// if so, starts a new one.
//-----------------------------------------------------------------------------
static int CheckForLogFileFull(
    LoggingState *state)                // state to use for writing
{
    long position;

    if (state->fileOwned && state->fp && state->maxFiles > 1) {
        position = ftell(state->fp);
        if (position < 0)
            return -1;
        if (position >= state->maxFileSize) {
            if (WritePrefix(state, LOG_LEVEL_NONE) < 0)
                return -1;
            if (fputs("switching to a new log file\n", state->fp) == EOF)
                return -1;
            fclose(state->fp);
            state->fp = NULL;
            if (SwitchLogFiles(state) < 0)
                return -1;
            state->fp = OpenFileForWriting(state->fileName);
            if (!state->fp)
                return -1;
            if (WritePrefix(state, LOG_LEVEL_NONE) < 0)
                return -1;
            if (fputs("starting logging (after switch) at level ",
                    state->fp) == EOF)
                return -1;
            if (WriteLevel(state, state->level) < 0)
                return -1;
            if (fputs("\n", state->fp) == EOF)
                return -1;
            if (fflush(state->fp) == EOF)
                return -1;
        }
    }
    return 0;
}
#endif


//-----------------------------------------------------------------------------
// WriteMessage()
//   Write the message to the file.
//-----------------------------------------------------------------------------
static int WriteMessage(
    LoggingState *state,                // state to use for writing
    unsigned long level,                // level at which to write
    const char *message)                // message to write
{
#ifndef UNDER_CE
    if (CheckForLogFileFull(state) < 0)
        return -1;
#endif
    if (state->fp) {
        if (WritePrefix(state, level) < 0)
            return -1;
        if (!message)
            message = "(null)";
        if (fputs(message, state->fp) == EOF)
            return -1;
        if (fputs("\n", state->fp) == EOF)
            return -1;
        if (fflush(state->fp) == EOF)
            return -1;
    }
    return 0;
}


//-----------------------------------------------------------------------------
// WriteMessageWithFormat()
//   Write the formatted message to the file given a variable number of
// arguments.
//-----------------------------------------------------------------------------
static int WriteMessageWithFormat(
    LoggingState *state,                // state to stop logging for
    unsigned long level,                // level at which message is written
    const char *format,                 // format of message to log
    va_list arguments)                  // argument list
{
#ifndef UNDER_CE
    if (CheckForLogFileFull(state) < 0)
        return -1;
#endif
    if (state->fp) {
        if (WritePrefix(state, level) < 0)
            return -1;
        if (vfprintf(state->fp, format, arguments) < 0)
            return -1;
        if (fputs("\n", state->fp) == EOF)
            return -1;
        if (fflush(state->fp) == EOF)
            return -1;
    }
    return 0;
}


//-----------------------------------------------------------------------------
// IsLoggingAtLevelForPython()
//   Return a boolean indicating if the current logging state is such that a
// message at the given level should be logged. This is only used for cases
// where the amount of time required to calculate the message to log is
// sufficient that checking first would be helpful; otherwise, the overhead of
// acquiring and releasing the lock twice would be detrimental to performance.
//-----------------------------------------------------------------------------
static int IsLoggingAtLevelForPython(
    unsigned long level)                // desired level
{
    udt_LoggingState *loggingState;
    int result;

    loggingState = GetLoggingState();
    if (loggingState)
        result = (level >= loggingState->state->level);
    else {
        ACQUIRE_LOCK(gLoggingStateLock);
        result = (gLoggingState && level >= gLoggingState->level);
        RELEASE_LOCK(gLoggingStateLock);
    }
    return result;
}


//-----------------------------------------------------------------------------
// WriteMessageForPython()
//   Write a message for Python given the known logging state.
//-----------------------------------------------------------------------------
static int WriteMessageForPython(
    unsigned long level,                // level at which message is written
    const char *message)                // message to write
{
    udt_LoggingState *loggingState;
    int result = 0;

    loggingState = GetLoggingState();
    Py_BEGIN_ALLOW_THREADS
    if (loggingState) {
        ACQUIRE_LOCK(loggingState->lock);
        result = WriteMessage(loggingState->state, level, message);
        RELEASE_LOCK(loggingState->lock);
    } else {
        ACQUIRE_LOCK(gLoggingStateLock);
        if (gLoggingState)
            result = WriteMessage(gLoggingState, level, message);
        RELEASE_LOCK(gLoggingStateLock);
    }
    Py_END_ALLOW_THREADS
    return result;
}


//-----------------------------------------------------------------------------
// LoggingState_Free()
//   Free the logging state.
//-----------------------------------------------------------------------------
static void LoggingState_Free(
    LoggingState *state)                // state to stop logging for
{
    if (state->fp) {
        if (state->fileOwned) {
            WriteMessage(state, LOG_LEVEL_NONE, "ending logging");
            fclose(state->fp);
        }
    }
    if (state->fileName)
        free(state->fileName);
    if (state->fileMask)
        free(state->fileMask);
    if (state->fileNameTemp_1)
        free(state->fileNameTemp_1);
    if (state->fileNameTemp_2)
        free(state->fileNameTemp_2);
    if (state->prefix)
        free(state->prefix);
    free(state);
}


//-----------------------------------------------------------------------------
// LoggingState_OnCreate()
//   Called when the logging state is created. It opens the file and writes the
// initial log messages to it.
//-----------------------------------------------------------------------------
static int LoggingState_OnCreate(
    LoggingState *state)                // logging state just created
{
    struct stat statBuffer;

    // open the file, if necessary
    if (!state->fp) {
#ifndef UNDER_CE
        if (state->maxFiles > 1 && stat(state->fileName, &statBuffer) == 0) {
            if (SwitchLogFiles(state) < 0)
                return -1;
        }
#endif
        state->fileOwned = 1;
        state->fp = OpenFileForWriting(state->fileName);
        if (!state->fp)
            return -1;
    }

    // put out an initial message regardless of level
    if (state->fileOwned) {
        if (WritePrefix(state, LOG_LEVEL_NONE) < 0)
            return -1;
        if (fputs("starting logging at level ", state->fp) == EOF)
            return -1;
        if (WriteLevel(state, state->level) < 0)
            return -1;
        if (fputs("\n", state->fp) == EOF)
            return -1;
        if (fflush(state->fp) == EOF)
            return -1;
    }

    return 0;
}


//-----------------------------------------------------------------------------
// LoggingState_New()
//   Create a new logging state.
//-----------------------------------------------------------------------------
static LoggingState* LoggingState_New(
    FILE *fp,                           // file to associate
    const char *fileName,               // name of file to open
    unsigned long level,                // level to use
    unsigned long maxFiles,             // maximum number of files
    unsigned long maxFileSize,          // maximum size of each file
    const char *prefix)                 // prefix to use
{
    LoggingState *state;
    char *tmp;

    // initialize the logging state
    state = (LoggingState*) malloc(sizeof(LoggingState));
    if (!state)
        return NULL;
    state->fp = fp;
    state->fileOwned = 0;
    state->level = level;
    state->fileName = NULL;
    state->fileMask = NULL;
    state->prefix = NULL;
    if (maxFiles == 0)
        state->maxFiles = 1;
    else state->maxFiles = maxFiles;
#ifdef UNDER_CE
    if (state->maxFiles != 1) {
        LogMessage(LOG_LEVEL_WARNING,
                "rotating log files not supported on Windows CE");
        state->maxFiles = 1;
    }
#endif
    if (maxFileSize == 0)
        state->maxFileSize = DEFAULT_MAX_FILE_SIZE;
    else state->maxFileSize = maxFileSize;

    // copy the file name
    state->fileName = malloc(strlen(fileName) + 1);
    if (!state->fileName) {
        LoggingState_Free(state);
        return NULL;
    }
    strcpy(state->fileName, fileName);

    // copy the prefix
    state->prefix = malloc(strlen(prefix) + 1);
    if (!state->prefix) {
        LoggingState_Free(state);
        return NULL;
    }
    strcpy(state->prefix, prefix);

    // create a space for generated file names
    state->fileNameTemp_1 = malloc(strlen(fileName) + 11);
    state->fileNameTemp_2 = malloc(strlen(fileName) + 11);
    if (!state->fileNameTemp_1 || !state->fileNameTemp_2) {
        LoggingState_Free(state);
        return NULL;
    }

    // build a mask for the generated file names
    state->fileMask = malloc(strlen(fileName) + 4);
    if (!state->fileMask) {
        LoggingState_Free(state);
        return NULL;
    }
    strcpy(state->fileMask, state->fileName);
    tmp = strrchr(state->fileName, '.');
    if (tmp) {
        strcpy(state->fileMask + (tmp - state->fileName), ".%d");
        strcat(state->fileMask, tmp);
    } else {
        strcat(state->fileMask, ".%d");
    }

    // open the file, if necessary and write any initial messages
    if (LoggingState_OnCreate(state) < 0) {
        LoggingState_Free(state);
        return NULL;
    }

    return state;
}


//-----------------------------------------------------------------------------
// LoggingState_SetLevel()
//   Set the level for the logging state.
//-----------------------------------------------------------------------------
static int LoggingState_SetLevel(
    LoggingState *state,                // state on which to change level
    unsigned long newLevel)             // new level to set
{
    if (WritePrefix(state, LOG_LEVEL_NONE) < 0)
        return -1;
    if (fputs("switched logging level from ", state->fp) == EOF)
        return -1;
    if (WriteLevel(state, state->level) < 0)
        return -1;
    if (fputs(" to ", state->fp) == EOF)
        return -1;
    if (WriteLevel(state, newLevel) < 0)
        return -1;
    if (fputs("\n", state->fp) == EOF)
        return -1;
    if (fflush(state->fp) == EOF)
        return -1;
    state->level = newLevel;
    return 0;
}


//-----------------------------------------------------------------------------
// PythonLoggingState_Free()
//   Called when a Python logging state variable is freed.
//-----------------------------------------------------------------------------
void PythonLoggingState_Free(
    udt_LoggingState *self)             // object being freed
{
    if (self->state) {
        LoggingState_Free(self->state);
        LogMessage(LOG_LEVEL_INFO, "stopping logging for Python thread");
    }
    Py_TYPE(self)->tp_free((PyObject*) self);
}


// define logging state Python type
static PyTypeObject gPythonLoggingStateType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "cx_Logging.LoggingState",          // tp_name
    sizeof(udt_LoggingState),           // tp_basicsize
    0,                                  // tp_itemsize
    (destructor) PythonLoggingState_Free,
                                        // tp_dealloc
    0,                                  // tp_print
    0,                                  // tp_getattr
    0,                                  // tp_setattr
    0,                                  // tp_compare
    0,                                  // tp_repr
    0,                                  // tp_as_number
    0,                                  // tp_as_sequence
    0,                                  // tp_as_mapping
    0,                                  // tp_hash
    0,                                  // tp_call
    0,                                  // tp_str
    0,                                  // tp_getattro
    0,                                  // tp_setattro
    0,                                  // tp_as_buffer
    Py_TPFLAGS_DEFAULT,                 // tp_flags
    0,                                  // tp_doc
    0,                                  // tp_traverse
    0,                                  // tp_clear
    0,                                  // tp_richcompare
    0,                                  // tp_weaklistoffset
    0,                                  // tp_iter
    0,                                  // tp_iternext
    0,                                  // tp_methods
    0,                                  // tp_members
    0,                                  // tp_getset
    0,                                  // tp_base
    0,                                  // tp_dict
    0,                                  // tp_descr_get
    0,                                  // tp_descr_set
    0,                                  // tp_dictoffset
    0,                                  // tp_init
    0,                                  // tp_alloc
    0,                                  // tp_new
    0,                                  // tp_free
    0,                                  // tp_is_gc
    0                                   // tp_bases
};


//-----------------------------------------------------------------------------
// GetThreadStateDictionary()
//   Return the thread state dictionary and sets an exception if it is not
// available.
//-----------------------------------------------------------------------------
static PyObject* GetThreadStateDictionary(void)
{
    PyObject *dict;

    dict = PyThreadState_GetDict();
    if (!dict) {
        LogMessage(LOG_LEVEL_WARNING, "no thread state dictionary");
        return NULL;
    }
    return dict;
}


//-----------------------------------------------------------------------------
// StartLogging()
//   Start logging to the specified file.
//-----------------------------------------------------------------------------
CX_LOGGING_API int StartLogging(
    const char *fileName,               // name of file to write to
    unsigned long level,                // level to use for logging
    unsigned long maxFiles,             // maximum number of files to have
    unsigned long maxFileSize,          // maximum size of each file
    const char *prefix)                 // prefix to use in logging
{
    LoggingState *loggingState, *origLoggingState;

    loggingState = LoggingState_New(NULL, fileName, level, maxFiles,
            maxFileSize, prefix);
    if (!loggingState)
        return -1;
    ACQUIRE_LOCK(gLoggingStateLock);
    origLoggingState = gLoggingState;
    gLoggingState = loggingState;
    RELEASE_LOCK(gLoggingStateLock);
    if (origLoggingState)
        LoggingState_Free(origLoggingState);
    return 0;
}


//-----------------------------------------------------------------------------
// StartLoggingForPythonThread()
//   Start logging to the specified file for the given Python thread. It is
// assumed at this point that the Python interpreter lock is held.
//-----------------------------------------------------------------------------
CX_LOGGING_API int StartLoggingForPythonThread(
    const char *fileName,               // name of file to write to
    unsigned long level,                // level to use for logging
    unsigned long maxFiles,             // maximum number of files to have
    unsigned long maxFileSize,          // maximum size of each file
    const char *prefix)                 // prefix to use in logging
{
    udt_LoggingState *loggingState;
    int result;

    // create a new logging state object
    loggingState = (udt_LoggingState*)
            gPythonLoggingStateType.tp_alloc(&gPythonLoggingStateType, 0);
    if (!loggingState)
        return -1;
    INITIALIZE_LOCK(loggingState->lock);
    loggingState->state = LoggingState_New(NULL, fileName, level, maxFiles,
            maxFileSize, prefix);
    if (!loggingState->state) {
        Py_DECREF(loggingState);
        PyErr_SetFromErrno(PyExc_OSError);
        return -1;
    }

    // set the logging state
    if (LogMessage(LOG_LEVEL_INFO, "starting logging for Python thread") < 0 ||
            LogMessageV(LOG_LEVEL_INFO, "    fileName => %s",
                    loggingState->state->fileName) < 0 ||
            LogMessageV(LOG_LEVEL_INFO, "    level => %d",
                    loggingState->state->level) < 0 ||
            LogMessageV(LOG_LEVEL_INFO, "    maxFiles => %d",
                    loggingState->state->maxFiles) < 0 ||
            LogMessageV(LOG_LEVEL_INFO, "    maxFileSize => %d",
                    loggingState->state->maxFileSize) < 0) {
        Py_DECREF(loggingState);
        PyErr_SetFromErrno(PyExc_OSError);
        return -1;
    }

    result = SetLoggingState(loggingState);
    Py_DECREF(loggingState);
    return result;
}


//-----------------------------------------------------------------------------
// StartLoggingStderr()
//   Start logging to stderr.
//-----------------------------------------------------------------------------
CX_LOGGING_API int StartLoggingStderr(
    unsigned long level,                // level to use for logging
    const char *prefix)                 // prefix to use for logging
{
    LoggingState *loggingState, *origLoggingState;

    loggingState = LoggingState_New(stderr, "<stderr>", level, 1, 0, prefix);
    if (!loggingState)
        return -1;
    ACQUIRE_LOCK(gLoggingStateLock);
    origLoggingState = gLoggingState;
    gLoggingState = loggingState;
    RELEASE_LOCK(gLoggingStateLock);
    if (origLoggingState)
        LoggingState_Free(origLoggingState);
    return 0;
}


//-----------------------------------------------------------------------------
// StartLoggingStdout()
//   Start logging to stdout.
//-----------------------------------------------------------------------------
CX_LOGGING_API int StartLoggingStdout(
    unsigned long level,                // level to use for logging
    const char *prefix)                 // prefix to use for logging
{
    LoggingState *loggingState, *origLoggingState;

    loggingState = LoggingState_New(stdout, "<stdout>", level, 1, 0, prefix);
    if (!loggingState)
        return -1;
    ACQUIRE_LOCK(gLoggingStateLock);
    origLoggingState = gLoggingState;
    gLoggingState = loggingState;
    RELEASE_LOCK(gLoggingStateLock);
    if (origLoggingState)
        LoggingState_Free(origLoggingState);
    return 0;
}


//-----------------------------------------------------------------------------
// StartLoggingFromEnvironment()
//   Start logging using the parameters specified in the environment.
//-----------------------------------------------------------------------------
CX_LOGGING_API int StartLoggingFromEnvironment(void)
{
    char *fileName, *valueAsString, *temp, *prefix;
    unsigned long level, maxFiles, maxFileSize;

    fileName = getenv(ENV_NAME_FILE_NAME);
    if (!fileName)
        return -1;
    valueAsString = getenv(ENV_NAME_LEVEL);
    if (!valueAsString)
        return -1;
    level = strtol(valueAsString, &temp, 10);
    if (*temp)
        return -1;
    valueAsString = getenv(ENV_NAME_MAX_FILES);
    if (!valueAsString) {
        maxFiles = 1;
    } else {
        maxFiles = strtol(valueAsString, &temp, 10);
        if (*temp)
            return -1;
    }
    valueAsString = getenv(ENV_NAME_MAX_FILE_SIZE);
    if (!valueAsString) {
        maxFileSize = DEFAULT_MAX_FILE_SIZE;
    } else {
        maxFileSize = strtol(valueAsString, &temp, 10);
        if (*temp)
            return -1;
    }
    prefix = getenv(ENV_NAME_PREFIX);
    if (!prefix)
        prefix = DEFAULT_PREFIX;
    return StartLogging(fileName, level, maxFiles, maxFileSize, prefix);
}


//-----------------------------------------------------------------------------
// StopLogging()
//   Stop logging to the specified file.
//-----------------------------------------------------------------------------
CX_LOGGING_API void StopLogging(void)
{
    LoggingState *loggingState;

    ACQUIRE_LOCK(gLoggingStateLock);
    loggingState = gLoggingState;
    gLoggingState = NULL;
    RELEASE_LOCK(gLoggingStateLock);
    if (loggingState)
        LoggingState_Free(loggingState);
}


//-----------------------------------------------------------------------------
// StopLoggingForPythonThread()
//   Stop logging to a different file for the current Python thread.
//-----------------------------------------------------------------------------
CX_LOGGING_API void StopLoggingForPythonThread(void)
{
    PyObject *dict;

    dict = GetThreadStateDictionary();
    if (dict && PyDict_DelItemString(dict, KEY_LOGGING_STATE) < 0) {
        PyErr_Clear();
        LogMessage(LOG_LEVEL_WARNING,
                "tried to stop logging without starting first");
    }
}


//-----------------------------------------------------------------------------
// LogMessageVaList()
//   Log a message to the log file with a variable number of argument specified
// as a va_list.
//-----------------------------------------------------------------------------
CX_LOGGING_API int LogMessageVaList(
    unsigned long level,                // level to check for
    const char *format,                 // format of message to log
    va_list arguments)                  // argument list
{
    int result = 0;

    if (gLoggingState) {
        ACQUIRE_LOCK(gLoggingStateLock);
        if (gLoggingState && level >= gLoggingState->level)
            result = WriteMessageWithFormat(gLoggingState, level, format,
                    arguments);
        RELEASE_LOCK(gLoggingStateLock);
    }

    return result;
}


//-----------------------------------------------------------------------------
// LogMessageV()
//   Log a message to the log file with a variable number of arguments.
//-----------------------------------------------------------------------------
CX_LOGGING_API int LogMessageV(
    unsigned long level,                // level at which to log
    const char *format,                 // format of message to log
    ...)                                // arguments required for format
{
    va_list arguments;
    int result;

    va_start(arguments, format);
    result = LogMessageVaList(level, format, arguments);
    va_end(arguments);
    return result;
}


//-----------------------------------------------------------------------------
// LogMessageForPythonV()
//   Log a message to the log file with a variable number of arguments. The
// Python thread state is examined first to determine if special logging for
// this thread is desired; if not, the normal LogMessageV() is invoked.
//-----------------------------------------------------------------------------
CX_LOGGING_API int LogMessageForPythonV(
    unsigned long level,                // level at which to log
    const char *format,                 // format of message to log
    ...)                                // arguments required for format
{
    udt_LoggingState *loggingState;
    va_list arguments;
    int result = 0;

    loggingState = GetLoggingState();
    va_start(arguments, format);
    if (loggingState) {
        if (level >= loggingState->state->level) {
           Py_BEGIN_ALLOW_THREADS
           ACQUIRE_LOCK(loggingState->lock);
           result = WriteMessageWithFormat(loggingState->state, level, format,
                   arguments);
           RELEASE_LOCK(loggingState->lock);
           Py_END_ALLOW_THREADS
        }
    } else result = LogMessageVaList(level, format, arguments);
    va_end(arguments);

    return result;
}


//-----------------------------------------------------------------------------
// LogMessage()
//   Log a message to the log file.
//-----------------------------------------------------------------------------
CX_LOGGING_API int LogMessage(
    unsigned long level,                // level at which to log
    const char *message)                // message to log
{
    int result = 0;

    if (gLoggingState) {
        ACQUIRE_LOCK(gLoggingStateLock);
        if (gLoggingState && level >= gLoggingState->level)
            result = WriteMessage(gLoggingState, level, message);
        RELEASE_LOCK(gLoggingStateLock);
    }

    return result;
}


//-----------------------------------------------------------------------------
// LogDebug()
//   Log a message at level LOG_LEVEL_DEBUG to the log file.
//-----------------------------------------------------------------------------
CX_LOGGING_API int LogDebug(
    const char *message)                // message to log
{
    return LogMessage(LOG_LEVEL_DEBUG, message);
}


//-----------------------------------------------------------------------------
// LogInfo()
//   Log a message at level LOG_LEVEL_INFO to the log file.
//-----------------------------------------------------------------------------
CX_LOGGING_API int LogInfo(
    const char *message)                // message to log
{
    return LogMessage(LOG_LEVEL_INFO, message);
}


//-----------------------------------------------------------------------------
// LogWarning()
//   Log a message at level LOG_LEVEL_WARNING to the log file.
//-----------------------------------------------------------------------------
CX_LOGGING_API int LogWarning(
    const char *message)                // message to log
{
    return LogMessage(LOG_LEVEL_WARNING, message);
}


//-----------------------------------------------------------------------------
// LogError()
//   Log a message at level LOG_LEVEL_ERROR to the log file.
//-----------------------------------------------------------------------------
CX_LOGGING_API int LogError(
    const char *message)                // message to log
{
    return LogMessage(LOG_LEVEL_ERROR, message);
}


//-----------------------------------------------------------------------------
// LogCritical()
//   Log a message at level LOG_LEVEL_CRITICAL to the log file.
//-----------------------------------------------------------------------------
CX_LOGGING_API int LogCritical(
    const char *message)                // message to log
{
    return LogMessage(LOG_LEVEL_CRITICAL, message);
}


//-----------------------------------------------------------------------------
// LogTrace()
//   Log a message regardless of the current logging level to the log file.
//-----------------------------------------------------------------------------
CX_LOGGING_API int LogTrace(
    const char *message)                // message to log
{
    return LogMessage(LOG_LEVEL_NONE, message);
}


//-----------------------------------------------------------------------------
// GetLoggingLevel()
//   Return the current logging level.
//-----------------------------------------------------------------------------
CX_LOGGING_API unsigned long GetLoggingLevel(void)
{
    unsigned long level = LOG_LEVEL_NONE;

    ACQUIRE_LOCK(gLoggingStateLock);
    if (gLoggingState)
        level = gLoggingState->level;
    RELEASE_LOCK(gLoggingStateLock);
    return level;
}


//-----------------------------------------------------------------------------
// SetLoggingLevel()
//   Set the current logging level.
//-----------------------------------------------------------------------------
CX_LOGGING_API int SetLoggingLevel(
    unsigned long newLevel)             // new level to use
{
    int result = 0;

    ACQUIRE_LOCK(gLoggingStateLock);
    if (gLoggingState)
        result = LoggingState_SetLevel(gLoggingState, newLevel);
    RELEASE_LOCK(gLoggingStateLock);

    return result;
}


#if defined WIN32 && !defined UNDER_CE
//-----------------------------------------------------------------------------
// LogWin32Error()
//   Log an error message from the Win32 subsystem and return -1.
//-----------------------------------------------------------------------------
CX_LOGGING_API int LogWin32Error(
    DWORD errorCode,                    // Win32 error code to log
    const char *message)                // message to log
{
    DWORD status;
    char *buffer;

    LogMessageV(LOG_LEVEL_ERROR, "Win32 error 0x%x encountered.", errorCode);
    LogMessageV(LOG_LEVEL_ERROR, "    Context: %s", message);
    status = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (void*) &buffer, 0, NULL);
    if (status == 0)
        LogMessage(LOG_LEVEL_ERROR, "    Message: FormatMessage() failed");
    else {
        LogMessageV(LOG_LEVEL_ERROR, "    Message: %s", buffer);
        LocalFree(buffer);
    }
    return -1;
}


//-----------------------------------------------------------------------------
// LogGUID()
//   Log a GUID in human readable format to the log file.
//-----------------------------------------------------------------------------
CX_LOGGING_API int LogGUID(
    unsigned long level,                // level at which to log
    const char *prefix,                 // prefix to print in front of it
    const IID *iid)                     // interface to print
{
    int strLength, result = 0;
    OLECHAR str[128];
    char outstr[128];

    ACQUIRE_LOCK(gLoggingStateLock);
    if (gLoggingState && level >= gLoggingState->level) {
        if (StringFromGUID2(iid, str, sizeof(str)) == 0) {
            result = LogMessageV(LOG_LEVEL_ERROR, "%s: huh? string too long?",
                    prefix);
        } else {
            strLength = wcslen(str) + 1;
            if (WideCharToMultiByte(CP_ACP, 0, str, strLength, outstr,
                    strLength, NULL, NULL) == 0) {
                result = LogMessageV(LOG_LEVEL_ERROR,
                        "%s: huh? can't convert string?", prefix);
            } else result = LogMessageV(level, "%s: %s", prefix, outstr);
        }
    }
    RELEASE_LOCK(gLoggingStateLock);

    return result;
}
#endif



//-----------------------------------------------------------------------------
// LogPythonObject()
//   Log the string representation of the Python object to the log file.
//-----------------------------------------------------------------------------
CX_LOGGING_API int LogPythonObject(
    unsigned long logLevel,             // log level
    const char *prefix,                 // prefix for message
    const char *name,                   // name to display
    PyObject *object)                   // object to log
{
    PyObject *stringRep;
    int result;

    if (!object) {
        result = LogMessageForPythonV(logLevel, "%s%s => NULL", prefix, name);
    } else {
        stringRep = PyObject_Str(object);
        if (stringRep) {
            result = LogMessageForPythonV(logLevel, "%s%s => %s", prefix, name,
                    PyBytes_AS_STRING(stringRep));
            Py_DECREF(stringRep);
        } else {
            result = LogMessageForPythonV(logLevel,
                    "%s%s => unable to stringify", prefix, name);
            PyErr_Clear();
        }
    }

    return result;
}


//-----------------------------------------------------------------------------
// BaseLogPythonException()
//   Base method for logging a Python exception.
//-----------------------------------------------------------------------------
static int BaseLogPythonException(
    const char *message,                // message to log
    PyObject *type,                     // exception type
    PyObject *value)                    // exception value
{
    LogMessageForPythonV(LOG_LEVEL_ERROR, "Python exception encountered:");
    LogMessageForPythonV(LOG_LEVEL_ERROR, "    Internal Message: %s", message);
    LogPythonObject(LOG_LEVEL_ERROR, "    ", "Type", type);
    LogPythonObject(LOG_LEVEL_ERROR, "    ", "Value", value);
    return -1;
}


//-----------------------------------------------------------------------------
// LogPythonExceptionNoTraceback()
//   Log a Python exception without traceback.
//-----------------------------------------------------------------------------
static int LogPythonExceptionNoTraceback(
    const char *message)                // message to log
{
    PyObject *type, *value, *traceback;

    PyErr_Fetch(&type, &value, &traceback);
    BaseLogPythonException(message, type, value);
    Py_XDECREF(type);
    Py_XDECREF(value);
    Py_XDECREF(traceback);
    return -1;
}


//-----------------------------------------------------------------------------
// SetArgumentValue()
//   Set an argument value for the call to make. All NULL values are
// transformed into None values automatically.
//-----------------------------------------------------------------------------
static void SetArgumentValue(
    PyObject *args,                     // argument tuple to modify
    int position,                       // position in tuple to modify
    PyObject *value)                    // value to place in tuple
{
    if (!value)
        value = Py_None;
    Py_INCREF(value);
    PyTuple_SET_ITEM(args, position, value);
}


//-----------------------------------------------------------------------------
// LogPythonExceptionWithTraceback()
//   Log a Python exception with traceback, if possible.
//-----------------------------------------------------------------------------
CX_LOGGING_API int LogPythonExceptionWithTraceback(
    const char *message,                // message to log
    PyObject *type,                     // exception type
    PyObject *value,                    // exception value
    PyObject *traceback)                // exception traceback
{
    PyObject *module, *method, *args, *result, *line;
    Py_ssize_t i, numElements;
    char *string;

    BaseLogPythonException(message, type, value);
    module = PyImport_ImportModule("traceback");
    if (!module)
        return LogPythonExceptionNoTraceback("get traceback module");
    method = PyObject_GetAttrString(module, "format_exception");
    Py_DECREF(module);
    if (!method)
        return LogPythonExceptionNoTraceback("get traceback method");
    args = PyTuple_New(3);
    if (!args) {
        Py_DECREF(method);
        return LogPythonExceptionNoTraceback("cannot create args tuple");
    }
    SetArgumentValue(args, 0, type);
    SetArgumentValue(args, 1, value);
    SetArgumentValue(args, 2, traceback);
    result = PyObject_CallObject(method, args);
    Py_DECREF(method);
    Py_DECREF(args);
    if (!result)
        return LogPythonExceptionNoTraceback("traceback method failed");
    numElements = PyList_Size(result);
    if (PyErr_Occurred()) {
        Py_DECREF(result);
        return LogPythonExceptionNoTraceback("cannot determine size");
    }
    for (i = 0; i < numElements; i++) {
        line = PyList_GET_ITEM(result, i);
        string = PyBytes_AsString(line);
        if (!string) {
            Py_DECREF(result);
            return LogPythonExceptionNoTraceback("cannot get string");
        }
        LogMessageForPythonV(LOG_LEVEL_ERROR, "    %s", string);
    }

    return -1;
}


//-----------------------------------------------------------------------------
// LogPythonException()
//   Log a Python exception, first attempting to do so with traceback and if
// that fails then without.
//-----------------------------------------------------------------------------
CX_LOGGING_API int LogPythonException(
    const char *message)                // message to display
{
    PyObject *type, *value, *traceback;

    PyErr_Fetch(&type, &value, &traceback);
    LogPythonExceptionWithTraceback(message, type, value, traceback);
    Py_XDECREF(type);
    Py_XDECREF(value);
    Py_XDECREF(traceback);
    return -1;
}


//-----------------------------------------------------------------------------
// LogMessageFromErrorObj()
//   Log the message from error object.
//-----------------------------------------------------------------------------
static int LogMessageFromErrorObj(
    unsigned long level,                // log level
    PyObject *errorObj)                 // error object to log
{
    PyObject *message;
    char *buffer;

    message = PyObject_GetAttrString(errorObj, "message");
    if (!message)
        return LogPythonException("no message on error object");
    buffer = PyBytes_AsString(message);
    if (!buffer) {
        Py_DECREF(message);
        return LogPythonException("message attribute is not a string");
    }
    LogMessageForPythonV(level, "    Message: %s", buffer);
    Py_DECREF(message);
    return -1;
}


//-----------------------------------------------------------------------------
// LogTemplateIdFromErrorObj()
//   Log the template id from the error object.
//-----------------------------------------------------------------------------
static int LogTemplateIdFromErrorObj(
    unsigned long level,                // log level
    PyObject *errorObj)                 // error object to examine
{
    PyObject *templateId;
    int value;

    templateId = PyObject_GetAttrString(errorObj, "templateId");
    if (!templateId)
        return LogPythonException("no templateId on error object");
    value = PyInt_AsLong(templateId);
    Py_DECREF(templateId);
    if (PyErr_Occurred())
        return LogPythonException("templateId attribute not an int");
    LogMessageForPythonV(level, "    Template Id: %d", value);
    return -1;
}


//-----------------------------------------------------------------------------
// LogArgumentsFromErrorObj()
//   Log the arguments stored on the error object.
//-----------------------------------------------------------------------------
static int LogArgumentsFromErrorObj(
    unsigned long level,                // log level
    PyObject *errorObj)                 // error object to examine
{
    PyObject *dict, *items, *item, *key, *value;
    Py_ssize_t i, size;
    char *keyString;

    dict = PyObject_GetAttrString(errorObj, "arguments");
    if (!dict)
        return LogPythonException("no arguments on error object");
    items = PyDict_Items(dict);
    Py_DECREF(dict);
    if (!items)
        return LogPythonException("cannot get items from dictionary");
    if (PyList_Sort(items) < 0) {
        Py_DECREF(items);
        return LogPythonException("cannot sort items");
    }
    size = PyList_Size(items);
    if (PyErr_Occurred()) {
        Py_DECREF(items);
        return LogPythonException("cannot get length of items");
    }
    LogMessageForPythonV(level, "    Arguments:");
    for (i = 0; i < size; i++) {
        item = PyList_GET_ITEM(items, i);
        key = PyTuple_GET_ITEM(item, 0);
        value = PyTuple_GET_ITEM(item, 1);
        keyString = PyBytes_AsString(key);
        if (!keyString) {
            Py_DECREF(items);
            return LogPythonException("key value is not a string");
        }
        LogPythonObject(level, "        ", keyString, value);
    }
    Py_DECREF(items);
    return -1;
}


//-----------------------------------------------------------------------------
// LogListOfStringsFromErrorObj()
//   Get the template id from the error object.
//-----------------------------------------------------------------------------
static int LogListOfStringsFromErrorObj(
    unsigned long level,                // log level
    PyObject *errorObj,                 // error object to examine
    char *attributeName,                // attribute name to examine
    const char *header)                 // header to log
{
    Py_ssize_t i, size;
    PyObject *list;
    char *buffer;

    list = PyObject_GetAttrString(errorObj, attributeName);
    if (!list)
        return LogPythonException("cannot get list from error object");
    size = PyList_Size(list);
    if (PyErr_Occurred()) {
        Py_DECREF(list);
        return LogPythonException("cannot get size of list");
    }
    LogMessageForPythonV(level, "    %s:", header);
    for (i = 0; i < size; i++) {
        buffer = PyBytes_AsString(PyList_GET_ITEM(list, i));
        if (!buffer) {
            Py_DECREF(list);
            return LogPythonException("value in list is not a string");
        }
        LogMessageForPythonV(level, "        %s", buffer);
    }
    Py_DECREF(list);
    return -1;
}


//-----------------------------------------------------------------------------
// LogConfiguredException()
//   Log a configured Python exception.
//-----------------------------------------------------------------------------
CX_LOGGING_API int LogConfiguredException(
    PyObject *errorObj,                 // object to log
    const char *message)                // message to prepend
{
    unsigned long logLevel;
    PyObject *logLevelObj;

    // determine the level at which to log
    logLevelObj = PyObject_GetAttrString(errorObj, "logLevel");
    if (logLevelObj) {
        logLevel = PyInt_AsLong(logLevelObj);
        if (PyErr_Occurred()) {
            logLevel = LOG_LEVEL_ERROR;
            LogPythonException("logLevel attribute is not an integer");
        }
    } else {
        logLevel = LOG_LEVEL_ERROR;
        LogMessageForPythonV(LOG_LEVEL_WARNING, "log level attribute missing");
        PyErr_Clear();
    }

    // log the exception
    LogMessageForPythonV(logLevel, message);
    LogMessageFromErrorObj(logLevel, errorObj);
    LogTemplateIdFromErrorObj(logLevel, errorObj);
    LogArgumentsFromErrorObj(logLevel, errorObj);
    LogListOfStringsFromErrorObj(logLevel, errorObj, "traceback", "Traceback");
    LogListOfStringsFromErrorObj(logLevel, errorObj, "details", "Details");
    return -1;
}


//-----------------------------------------------------------------------------
// GetLoggingState()
//   Get the current logging state for Python so that it can be retained
// across thread boundaries (such as for COM servers).
//-----------------------------------------------------------------------------
CX_LOGGING_API udt_LoggingState* GetLoggingState(void)
{
    PyObject *dict;

    dict = GetThreadStateDictionary();
    if (!dict)
        return NULL;
    return (udt_LoggingState*) PyDict_GetItemString(dict, KEY_LOGGING_STATE);
}


//-----------------------------------------------------------------------------
// SetLoggingState()
//   Set the current logging state for Python. This is intended to be used as
// the complement to GetLoggingState() so that logging state can be retained
// across thread boundaries.
//-----------------------------------------------------------------------------
CX_LOGGING_API int SetLoggingState(
    udt_LoggingState *loggingState)     // logging state to set
{
    PyObject *dict;

    dict = GetThreadStateDictionary();
    if (!dict) {
        PyErr_SetString(PyExc_RuntimeError,
                "unable to get thread state dictionary");
        return -1;
    }
    if (PyDict_SetItemString(dict, KEY_LOGGING_STATE,
            (PyObject*) loggingState) < 0)
        return LogPythonException("unable to set logging state for thread");
    return 0;
}


//-----------------------------------------------------------------------------
// IsLoggingStarted()
//   Return a boolean indicating if logging is currently started.
//-----------------------------------------------------------------------------
CX_LOGGING_API int IsLoggingStarted(void)
{
    return (gLoggingState != NULL);
}


//-----------------------------------------------------------------------------
// LogMessageForPythonWithLevel()
//   Python implementation of LogMessage() where the level is already known.
//-----------------------------------------------------------------------------
static PyObject* LogMessageForPythonWithLevel(
    unsigned long level,                // logging level
    int startingIndex,                  // starting index to look at
    PyObject *args)                     // Python arguments
{
    PyObject *tempArgs, *temp, *format;

    if (IsLoggingAtLevelForPython(level)) {
        tempArgs = PyTuple_GetSlice(args, startingIndex, startingIndex + 1);
        if (!tempArgs)
            return NULL;
        if (!PyArg_ParseTuple(tempArgs, "S", &format)) {
            Py_DECREF(tempArgs);
            return NULL;
        }
        Py_DECREF(tempArgs);
        tempArgs = PyTuple_GetSlice(args, startingIndex + 1,
                PyTuple_GET_SIZE(args));
        if (!tempArgs)
            return NULL;
#if PY_MAJOR_VERSION >= 3
        temp = PyUnicode_Format(format, tempArgs);
#else
        temp = PyString_Format(format, tempArgs);
#endif
        Py_DECREF(tempArgs);
        if (!temp)
            return NULL;
        if (WriteMessageForPython(level, PyBytes_AS_STRING(temp)) < 0) {
            Py_DECREF(temp);
            return PyErr_SetFromErrno(PyExc_OSError);
        }
        Py_DECREF(temp);
        Py_INCREF(Py_True);
        return Py_True;
    }

    Py_INCREF(Py_False);
    return Py_False;
}


//-----------------------------------------------------------------------------
// LogMessageForPython()
//   Python implementation of LogMessage() exposed through the module.
//-----------------------------------------------------------------------------
static PyObject* LogMessageForPython(
    PyObject *self,                     // passthrough argument
    PyObject *args)                     // arguments
{
    unsigned long level;
    PyObject *tempArgs;

    tempArgs = PyTuple_GetSlice(args, 0, 1);
    if (!tempArgs)
        return NULL;
    if (!PyArg_ParseTuple(tempArgs, "i", &level)) {
        Py_DECREF(tempArgs);
        return NULL;
    }
    Py_DECREF(tempArgs);
    return LogMessageForPythonWithLevel(level, 1, args);
}


//-----------------------------------------------------------------------------
// LogDebugForPython()
//   Python implementation of Debug() exposed through the module.
//-----------------------------------------------------------------------------
static PyObject* LogDebugForPython(
    PyObject *self,                     // passthrough argument
    PyObject *args)                     // arguments
{
    return LogMessageForPythonWithLevel(LOG_LEVEL_DEBUG, 0, args);
}


//-----------------------------------------------------------------------------
// LogInfoForPython()
//   Python implementation of Info() exposed through the module.
//-----------------------------------------------------------------------------
static PyObject* LogInfoForPython(
    PyObject *self,                     // passthrough argument
    PyObject *args)                     // arguments
{
    return LogMessageForPythonWithLevel(LOG_LEVEL_INFO, 0, args);
}


//-----------------------------------------------------------------------------
// LogWarningForPython()
//   Python implementation of Warning() exposed through the module.
//-----------------------------------------------------------------------------
static PyObject* LogWarningForPython(
    PyObject *self,                     // passthrough argument
    PyObject *args)                     // arguments
{
    return LogMessageForPythonWithLevel(LOG_LEVEL_WARNING, 0, args);
}


//-----------------------------------------------------------------------------
// LogErrorForPython()
//   Python implementation of Error() exposed through the module.
//-----------------------------------------------------------------------------
static PyObject* LogErrorForPython(
    PyObject *self,                     // passthrough argument
    PyObject *args)                     // arguments
{
    return LogMessageForPythonWithLevel(LOG_LEVEL_ERROR, 0, args);
}


//-----------------------------------------------------------------------------
// LogCriticalForPython()
//   Python implementation of Critical() exposed through the module.
//-----------------------------------------------------------------------------
static PyObject* LogCriticalForPython(
    PyObject *self,                     // passthrough argument
    PyObject *args)                     // arguments
{
    return LogMessageForPythonWithLevel(LOG_LEVEL_CRITICAL, 0, args);
}


//-----------------------------------------------------------------------------
// LogTraceForPython()
//   Python implementation of Trace() exposed through the module.
//-----------------------------------------------------------------------------
static PyObject* LogTraceForPython(
    PyObject *self,                     // passthrough argument
    PyObject *args)                     // arguments
{
    return LogMessageForPythonWithLevel(LOG_LEVEL_NONE, 0, args);
}


//-----------------------------------------------------------------------------
// StartLoggingForPython()
//   Python implementation of StartLogging() exposed through the module.
//-----------------------------------------------------------------------------
static PyObject* StartLoggingForPython(
    PyObject *self,                     // passthrough argument
    PyObject *args,                     // arguments
    PyObject *keywordArgs)              // keyword arguments
{
    static char *keywordList[] = {"fileName", "level", "maxFiles",
            "maxFileSize", "prefix", NULL};
    unsigned long level, maxFiles, maxFileSize;
    char *fileName, *prefix;

    maxFiles = 1;
    maxFileSize = DEFAULT_MAX_FILE_SIZE;
    prefix = DEFAULT_PREFIX;
    if (!PyArg_ParseTupleAndKeywords(args, keywordArgs, "sl|lls", keywordList,
            &fileName, &level, &maxFiles, &maxFileSize, &prefix))
        return NULL;
    if (StartLogging(fileName, level, maxFiles, maxFileSize, prefix) < 0)
        return PyErr_SetFromErrnoWithFilename(PyExc_OSError, fileName);

    Py_INCREF(Py_None);
    return Py_None;
}


//-----------------------------------------------------------------------------
// StartLoggingStderrForPython()
//   Python implementation of StartLoggingStderr() exposed through the module.
//-----------------------------------------------------------------------------
static PyObject* StartLoggingStderrForPython(
    PyObject *self,                     // passthrough argument
    PyObject *args)                     // arguments
{
    unsigned long level;
    char *prefix;

    prefix = DEFAULT_PREFIX;
    if (!PyArg_ParseTuple(args, "l|s", &level, &prefix))
        return NULL;
    StartLoggingStderr(level, prefix);

    Py_INCREF(Py_None);
    return Py_None;
}


//-----------------------------------------------------------------------------
// StartLoggingStdoutForPython()
//   Python implementation of StartLoggingStdout() exposed through the module.
//-----------------------------------------------------------------------------
static PyObject* StartLoggingStdoutForPython(
    PyObject *self,                     // passthrough argument
    PyObject *args)                     // arguments
{
    unsigned long level;
    char *prefix;

    prefix = DEFAULT_PREFIX;
    if (!PyArg_ParseTuple(args, "l|s", &level, &prefix))
        return NULL;
    StartLoggingStdout(level, prefix);

    Py_INCREF(Py_None);
    return Py_None;
}


//-----------------------------------------------------------------------------
// StartLoggingForThreadForPython()
//   Python implementation of StartLoggingForThread().
//-----------------------------------------------------------------------------
static PyObject *StartLoggingForThreadForPython(
    PyObject *self,                     // passthrough argument
    PyObject *args,                     // arguments
    PyObject *keywordArgs)              // keyword arguments
{
    static char *keywordList[] = {"fileName", "level", "maxFiles",
            "maxFileSize", "prefix", NULL};
    unsigned long level, maxFiles, maxFileSize;
    char *fileName, *prefix;

    maxFiles = 1;
    maxFileSize = DEFAULT_MAX_FILE_SIZE;
    prefix = DEFAULT_PREFIX;
    if (!PyArg_ParseTupleAndKeywords(args, keywordArgs, "sl|lls", keywordList,
            &fileName, &level, &maxFiles, &maxFileSize, &prefix))
        return NULL;
    if (StartLoggingForPythonThread(fileName, level, maxFiles, maxFileSize,
            prefix) < 0)
        return NULL;

    Py_INCREF(Py_None);
    return Py_None;
}


//-----------------------------------------------------------------------------
// StopLoggingForPython()
//   Python implementation of StopLogging() exposed through the module.
//-----------------------------------------------------------------------------
static PyObject* StopLoggingForPython(
    PyObject *self,                     // passthrough argument
    PyObject *args)                     // arguments
{
    StopLogging();
    Py_INCREF(Py_None);
    return Py_None;
}


//-----------------------------------------------------------------------------
// StopLoggingForThreadForPython()
//   Python implementation of StartLoggingForThread().
//-----------------------------------------------------------------------------
static PyObject *StopLoggingForThreadForPython(
    PyObject *self,                     // passthrough argument
    PyObject *args)                     // arguments
{
    StopLoggingForPythonThread();
    Py_INCREF(Py_None);
    return Py_None;
}


//-----------------------------------------------------------------------------
// GetLoggingLevelForPython()
//   Return the current logging level.
//-----------------------------------------------------------------------------
static PyObject* GetLoggingLevelForPython(
    PyObject *self,                     // passthrough argument
    PyObject *args)                     // arguments
{
    udt_LoggingState *loggingState;

    loggingState = GetLoggingState();
    if (loggingState)
        return PyInt_FromLong(loggingState->state->level);
    return PyInt_FromLong(GetLoggingLevel());
}


//-----------------------------------------------------------------------------
// SetLoggingLevelForPython()
//   Set the logging level.
//-----------------------------------------------------------------------------
static PyObject* SetLoggingLevelForPython(
    PyObject *self,                     // passthrough argument
    PyObject *args)                     // arguments
{
    udt_LoggingState *loggingState;
    unsigned long newLevel;

    if (!PyArg_ParseTuple(args, "l", &newLevel))
        return NULL;
    loggingState = GetLoggingState();
    if (loggingState)
        LoggingState_SetLevel(loggingState->state, newLevel);
    else SetLoggingLevel(newLevel);
    Py_INCREF(Py_None);
    return Py_None;
}


//-----------------------------------------------------------------------------
// GetLoggingFileForPython()
//   Return the current logging file as a Python object.
//-----------------------------------------------------------------------------
static PyObject* GetLoggingFileForPython(
    PyObject *self,                     // passthrough argument
    PyObject *args)                     // arguments
{
    udt_LoggingState *loggingState;
    PyObject *fileObj;

    loggingState = GetLoggingState();
    if (loggingState)
#if PY_MAJOR_VERSION >= 3
        return PyFile_FromFd(fileno(loggingState->state->fp),
                loggingState->state->fileName, "w", -1, NULL, NULL, NULL, 0);
#else
        return PyFile_FromFile(loggingState->state->fp,
                loggingState->state->fileName, "w", NULL);
#endif
    ACQUIRE_LOCK(gLoggingStateLock);
    if (gLoggingState)
#if PY_MAJOR_VERSION >= 3
        fileObj = PyFile_FromFd(fileno(gLoggingState->fp),
                gLoggingState->fileName, "w", -1, NULL, NULL, NULL, 0);
#else
        fileObj = PyFile_FromFile(gLoggingState->fp,
                gLoggingState->fileName, "w", NULL);
#endif
    else {
        Py_INCREF(Py_None);
        fileObj = Py_None;
    }
    RELEASE_LOCK(gLoggingStateLock);
    return fileObj;
}


//-----------------------------------------------------------------------------
// GetLoggingFileNameForPython()
//   Return the current logging file name.
//-----------------------------------------------------------------------------
static PyObject* GetLoggingFileNameForPython(
    PyObject *self,                     // passthrough argument
    PyObject *args)                     // arguments
{
    udt_LoggingState *loggingState;
    PyObject *fileNameObj;

    loggingState = GetLoggingState();
    if (loggingState)
        return PyBytes_FromString(loggingState->state->fileName);
    ACQUIRE_LOCK(gLoggingStateLock);
    if (gLoggingState)
        fileNameObj = PyBytes_FromString(gLoggingState->fileName);
    else {
        Py_INCREF(Py_None);
        fileNameObj = Py_None;
    }
    RELEASE_LOCK(gLoggingStateLock);
    return fileNameObj;
}


//-----------------------------------------------------------------------------
// GetLoggingStateForPython()
//   Return the current logging state for the thread.
//-----------------------------------------------------------------------------
static PyObject* GetLoggingStateForPython(
    PyObject *self,                     // passthrough argument
    PyObject *args)                     // arguments
{
    PyObject *loggingState;

    loggingState = (PyObject*) GetLoggingState();
    if (!loggingState)
        loggingState = Py_None;
    Py_INCREF(loggingState);
    return loggingState;
}


//-----------------------------------------------------------------------------
// SetLoggingStateForPython()
//   Set the current logging state with the state acquired earlier by a call to
// GetLoggingStateForPython().
//-----------------------------------------------------------------------------
static PyObject* SetLoggingStateForPython(
    PyObject *self,                     // passthrough argument
    PyObject *args)                     // arguments
{
    udt_LoggingState *loggingState;

    if (!PyArg_ParseTuple(args, "O!", &gPythonLoggingStateType,
            &loggingState))
        return NULL;
    if (SetLoggingState(loggingState) < 0)
        return NULL;

    Py_INCREF(Py_None);
    return Py_None;
}


//-----------------------------------------------------------------------------
// SetExceptionInfoForPython()
//   Set base exception information for use when logging exceptions in
// Python.
//-----------------------------------------------------------------------------
static PyObject* SetExceptionInfoForPython(
    PyObject *self,                     // passthrough argument
    PyObject *args)                     // arguments
{
    PyObject *baseClass, *message, *builder, *dict;

    message = builder = NULL;
    if (!PyArg_ParseTuple(args, "O|OS", &baseClass, &builder, &message))
        return NULL;

    dict = GetThreadStateDictionary();
    if (!dict) {
        PyErr_SetString(PyExc_RuntimeError,
                "unable to set thread state dictionary");
        return NULL;
    }
    if (PyDict_SetItemString(dict, KEY_EXC_BASE_CLASS, baseClass) < 0)
        return NULL;
    if (message && PyDict_SetItemString(dict, KEY_EXC_MESSAGE, message) < 0)
        return NULL;
    if (builder && PyDict_SetItemString(dict, KEY_EXC_BUILDER, builder) < 0)
        return NULL;

    Py_INCREF(Py_None);
    return Py_None;
}


//-----------------------------------------------------------------------------
// LogExceptionForPython()
//   Set the current logging state with the state acquired earlier by a call to
// GetLoggingStateForPython().
//-----------------------------------------------------------------------------
static PyObject* LogExceptionForPython(
    PyObject *self,                     // passthrough argument
    PyObject *args)                     // arguments
{
    PyObject *value, *configuredExcBaseClass, *excBuilder, *dict, *messageObj;
    int isConfigured = 1, isBuilt = 0;
    PyThreadState *threadState;
    char *message = NULL;

    // parse arguments
    value = configuredExcBaseClass = NULL;
    if (!PyArg_ParseTuple(args, "|OO", &value, &configuredExcBaseClass))
        return NULL;

    // determine which base class to use for comparisons
    threadState = PyThreadState_GET();
    dict = GetThreadStateDictionary();
    if (dict && !configuredExcBaseClass)
        configuredExcBaseClass = PyDict_GetItemString(dict,
                KEY_EXC_BASE_CLASS);

    // now determine if base class is configured or not
    if (!value || PyBytes_Check(value)) {
        isConfigured = 0;
        if (value)
            message = PyBytes_AS_STRING(value);
        else {
            value = threadState->exc_value;
            if (configuredExcBaseClass && value) {
                isConfigured = PyObject_IsInstance(value,
                        configuredExcBaseClass);
                if (isConfigured < 0)
                    return NULL;
            }
        }
    }

    // if class is not configured but we have a way of building one,
    // build it now
    if (!isConfigured && dict && threadState->exc_type &&
            threadState->exc_value && threadState->exc_traceback) {
        excBuilder = PyDict_GetItemString(dict, KEY_EXC_BUILDER);
        if (excBuilder) {
            value = PyObject_CallFunctionObjArgs(excBuilder,
                    threadState->exc_type, threadState->exc_value,
                    threadState->exc_traceback, NULL);
            if (!value)
                return NULL;
            isConfigured = 1;
            isBuilt = 1;
        }
    }

    // define message to display if not already defined
    if (!message) {
        if (dict) {
            messageObj = PyDict_GetItemString(dict, KEY_EXC_MESSAGE);
            if (messageObj)
                message = PyBytes_AS_STRING(messageObj);
        }
        if (!message)
            message = "Python exception encountered:";
    }

    // display message
    if (isConfigured) {
        LogConfiguredException(value, message);
    } else {
        LogPythonExceptionWithTraceback(message, threadState->exc_type,
                threadState->exc_value, threadState->exc_traceback);
    }

    // return value is a configured exception if possible; otherwise None
    if (isBuilt)
        return value;
    else if (isConfigured) {
        Py_INCREF(value);
        return value;
    }
    Py_INCREF(Py_None);
    return Py_None;
}


//-----------------------------------------------------------------------------
//  declaration of methods supported by the internal module
//-----------------------------------------------------------------------------
static PyMethodDef gLoggingModuleMethods[] = {
    { "Debug", (PyCFunction) LogDebugForPython, METH_VARARGS },
    { "Info", (PyCFunction) LogInfoForPython, METH_VARARGS },
    { "Warning", (PyCFunction) LogWarningForPython, METH_VARARGS },
    { "Error", (PyCFunction) LogErrorForPython, METH_VARARGS },
    { "Critical", (PyCFunction) LogCriticalForPython, METH_VARARGS },
    { "Log", (PyCFunction) LogMessageForPython, METH_VARARGS },
    { "Trace", (PyCFunction) LogTraceForPython, METH_VARARGS },
    { "StartLogging", (PyCFunction) StartLoggingForPython,
            METH_VARARGS | METH_KEYWORDS},
    { "StartLoggingForThread", (PyCFunction) StartLoggingForThreadForPython,
            METH_VARARGS | METH_KEYWORDS},
    { "StartLoggingStderr", (PyCFunction) StartLoggingStderrForPython,
            METH_VARARGS },
    { "StartLoggingStdout", (PyCFunction) StartLoggingStdoutForPython,
            METH_VARARGS },
    { "StopLogging", (PyCFunction) StopLoggingForPython, METH_NOARGS },
    { "StopLoggingForThread", (PyCFunction) StopLoggingForThreadForPython,
            METH_NOARGS },
    { "GetLoggingLevel", (PyCFunction) GetLoggingLevelForPython, METH_NOARGS },
    { "SetLoggingLevel", (PyCFunction) SetLoggingLevelForPython, METH_VARARGS },
    { "GetLoggingFile", (PyCFunction) GetLoggingFileForPython, METH_NOARGS },
    { "GetLoggingFileName", (PyCFunction) GetLoggingFileNameForPython,
            METH_NOARGS },
    { "GetLoggingState", (PyCFunction) GetLoggingStateForPython, METH_NOARGS },
    { "SetLoggingState", (PyCFunction) SetLoggingStateForPython,
            METH_VARARGS },
    { "SetExceptionInfo", (PyCFunction) SetExceptionInfoForPython,
            METH_VARARGS },
    { "LogException", (PyCFunction) LogExceptionForPython, METH_VARARGS },
    { NULL }
};


#if PY_MAJOR_VERSION >= 3
//-----------------------------------------------------------------------------
//   Declaration of module definition for Python 3.x.
//-----------------------------------------------------------------------------
static struct PyModuleDef g_ModuleDef = {
    PyModuleDef_HEAD_INIT,
    "cx_Logging",
    NULL,
    -1,
    gLoggingModuleMethods,              // methods
    NULL,                               // m_reload
    NULL,                               // traverse
    NULL,                               // clear
    NULL                                // free
};
#endif


//-----------------------------------------------------------------------------
// Module_Initialize()
//   Initialize the logging module.
//-----------------------------------------------------------------------------
static PyObject *Module_Initialize(void)
{
    PyObject *module;

    // initialize module and types
    PyEval_InitThreads();
#if PY_MAJOR_VERSION >= 3
    module = PyModule_Create(&g_ModuleDef);
#else
    module = Py_InitModule("cx_Logging", gLoggingModuleMethods);
#endif
    if (!module)
        return NULL;
    if (PyType_Ready(&gPythonLoggingStateType) < 0)
        return NULL;

    // add version and build time for easier support
    if (PyModule_AddStringConstant(module, "version",
            BUILD_VERSION_STRING) < 0)
        return NULL;
    if (PyModule_AddStringConstant(module, "buildtime",
            __DATE__ " " __TIME__) < 0)
        return NULL;

    // set constants
    if (PyModule_AddIntConstant(module, "CRITICAL", LOG_LEVEL_CRITICAL) < 0)
        return NULL;
    if (PyModule_AddIntConstant(module, "ERROR", LOG_LEVEL_ERROR) < 0)
        return NULL;
    if (PyModule_AddIntConstant(module, "WARNING", LOG_LEVEL_WARNING) < 0)
        return NULL;
    if (PyModule_AddIntConstant(module, "INFO", LOG_LEVEL_INFO) < 0)
        return NULL;
    if (PyModule_AddIntConstant(module, "DEBUG", LOG_LEVEL_DEBUG) < 0)
        return NULL;
    if (PyModule_AddIntConstant(module, "NONE", LOG_LEVEL_NONE) < 0)
        return NULL;
    if (PyModule_AddStringConstant(module, "ENV_NAME_FILE_NAME",
            ENV_NAME_FILE_NAME) < 0)
        return NULL;
    if (PyModule_AddStringConstant(module, "ENV_NAME_LEVEL",
            ENV_NAME_LEVEL) < 0)
        return NULL;
    if (PyModule_AddStringConstant(module, "ENV_NAME_MAX_FILES",
            ENV_NAME_MAX_FILES) < 0)
        return NULL;
    if (PyModule_AddStringConstant(module, "ENV_NAME_MAX_FILE_SIZE",
            ENV_NAME_MAX_FILE_SIZE) < 0)
        return NULL;
    if (PyModule_AddStringConstant(module, "ENV_NAME_PREFIX",
            ENV_NAME_PREFIX) < 0)
        return NULL;

    return module;
}


//-----------------------------------------------------------------------------
// Start routine for the module.
//-----------------------------------------------------------------------------
#if PY_MAJOR_VERSION >= 3
PyMODINIT_FUNC PyInit_cx_Logging(void)
{
    return Module_Initialize();
}
#else
void initcx_Logging(void)
{
    Module_Initialize();
}
#endif

#ifdef WIN32
//-----------------------------------------------------------------------------
// StartLoggingW()
//   Start logging to the specified file.
//-----------------------------------------------------------------------------
CX_LOGGING_API int StartLoggingW(
    const OLECHAR *fileName,            // name of file to write to
    unsigned long level,                // level to use for logging
    unsigned long maxFiles,             // maximum number of files to have
    unsigned long maxFileSize,          // maximum size of each file
    const OLECHAR *prefix)              // prefix to use in logging
{
    char *tempFileName, *tempPrefix;
    unsigned size;

    size = wcslen(fileName) + 1;
    tempFileName = alloca(size);
    if (!tempFileName)
        return -1;
    wcstombs(tempFileName, fileName, size);

    size = wcslen(prefix) + 1;
    tempPrefix = alloca(size);
    if (!prefix)
        return -1;
    wcstombs(tempPrefix, prefix, size);

    return StartLogging(tempFileName, level, maxFiles, maxFileSize,
            tempPrefix);
}


//-----------------------------------------------------------------------------
// LogMessageW()
//   Log a message to the log file.
//-----------------------------------------------------------------------------
CX_LOGGING_API int LogMessageW(
    unsigned long level,                // level at which to log
    const OLECHAR *message)             // message to log
{
    char *tempMessage;
    unsigned size;

    size = wcslen(message) + 1;
    tempMessage = alloca(size);
    if (!tempMessage)
        return -1;
    wcstombs(tempMessage, message, size);
    return LogMessage(level, tempMessage);
}


//-----------------------------------------------------------------------------
// LogDebugW()
//   Log a message at level LOG_LEVEL_DEBUG to the log file.
//-----------------------------------------------------------------------------
CX_LOGGING_API int LogDebugW(
    const OLECHAR *message)             // message to log
{
    return LogMessageW(LOG_LEVEL_DEBUG, message);
}


//-----------------------------------------------------------------------------
// LogInfoW()
//   Log a message at level LOG_LEVEL_INFO to the log file.
//-----------------------------------------------------------------------------
CX_LOGGING_API int LogInfoW(
    const OLECHAR *message)             // message to log
{
    return LogMessageW(LOG_LEVEL_INFO, message);
}


//-----------------------------------------------------------------------------
// LogWarningW()
//   Log a message at level LOG_LEVEL_WARNING to the log file.
//-----------------------------------------------------------------------------
CX_LOGGING_API int LogWarningW(
    const OLECHAR *message)             // message to log
{
    return LogMessageW(LOG_LEVEL_WARNING, message);
}


//-----------------------------------------------------------------------------
// LogErrorW()
//   Log a message at level LOG_LEVEL_ERROR to the log file.
//-----------------------------------------------------------------------------
CX_LOGGING_API int LogErrorW(
    const OLECHAR *message)             // message to log
{
    return LogMessageW(LOG_LEVEL_ERROR, message);
}


//-----------------------------------------------------------------------------
// LogCriticalW()
//   Log a message at level LOG_LEVEL_CRITICAL to the log file.
//-----------------------------------------------------------------------------
CX_LOGGING_API int LogCriticalW(
    const OLECHAR *message)             // message to log
{
    return LogMessageW(LOG_LEVEL_CRITICAL, message);
}


//-----------------------------------------------------------------------------
// LogTraceW()
//   Log a message regardless of the current logging level to the log file.
//-----------------------------------------------------------------------------
CX_LOGGING_API int LogTraceW(
    const OLECHAR *message)             // message to log
{
    return LogMessageW(LOG_LEVEL_NONE, message);
}


//-----------------------------------------------------------------------------
// DllMain()
//   Main routine for the DLL in Windows.
//-----------------------------------------------------------------------------
BOOL WINAPI DllMain(
    HINSTANCE instance,                 // handle to DLL
    DWORD reason,                       // reason for call
    LPVOID reserved)                    // reserved for future use
{
    if (reason == DLL_PROCESS_ATTACH)
        INITIALIZE_LOCK(gLoggingStateLock);
    else if (reason == DLL_PROCESS_DETACH)
        StopLogging();
    return TRUE;
}
#else
//-----------------------------------------------------------------------------
// Initialize()
//   Called when the library is initialized.
//-----------------------------------------------------------------------------
void __attribute__ ((constructor)) Initialize(void)
{
    INITIALIZE_LOCK(gLoggingStateLock);
}


//-----------------------------------------------------------------------------
// Finalize()
//   Called when the library is finalized.
//-----------------------------------------------------------------------------
void __attribute__ ((destructor)) Finalize(void)
{
    StopLogging();
}
#endif

