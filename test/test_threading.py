import cx_Logging
import os
import sys
import threading

if len(sys.argv) > 1:
    numThreads = int(sys.argv[1])
else:
    numThreads = 5
if len(sys.argv) > 2:
    numIterations = int(sys.argv[2])
else:
    numIterations = 1000

def Run(threadNum):
    cx_Logging.Debug("Thread-%d: starting", threadNum)
    iterationsLeft = numIterations
    while iterationsLeft > 0:
        numFiles = len(os.listdir("."))
        cx_Logging.Debug("Thread-%d: counted %d files, %d iterations left",
                threadNum, numFiles, iterationsLeft)
        iterationsLeft -= 1

cx_Logging.StartLogging("test_threading.log", level = cx_Logging.DEBUG,
        maxFiles = 10, maxFileSize = 5 * 1024 * 1024, prefix = "[%i] %t")
cx_Logging.Debug("Testing logging with %s threads.", numThreads)
threads = []
for i in range(numThreads):
    thread = threading.Thread(target = Run, args = (i + 1,))
    threads.append(thread)
    thread.start()
for thread in threads:
    thread.join()

