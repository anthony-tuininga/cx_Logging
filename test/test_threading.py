import cx_Logging
import os
import sys
import threading

if len(sys.argv) > 1:
    num_threads = int(sys.argv[1])
else:
    num_threads = 5
if len(sys.argv) > 2:
    num_iters = int(sys.argv[2])
else:
    num_iters = 1000


def run(thread_num):
    cx_Logging.Debug("Thread-%d: starting", thread_num)
    iters_left = num_iters
    while iters_left > 0:
        num_files = len(os.listdir("."))
        cx_Logging.Debug(
            "Thread-%d: counted %d files, %d iterations left",
            thread_num,
            num_files,
            iters_left,
        )
        iters_left -= 1


cx_Logging.StartLogging(
    "test_threading.log",
    level=cx_Logging.DEBUG,
    maxFiles=10,
    maxFileSize=5 * 1024 * 1024,
    prefix="[%i] %t",
)
cx_Logging.Debug("Testing logging with %s threads.", num_threads)
threads = []
for i in range(num_threads):
    thread = threading.Thread(target=run, args=(i + 1,))
    threads.append(thread)
    thread.start()
for thread in threads:
    thread.join()
