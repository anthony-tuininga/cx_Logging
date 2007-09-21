"""Distutils script for cx_Logging.

Windows platforms:
    python setup.py build --compiler=mingw32 install

Unix platforms
    python setup.py build install

"""

import os
import sys

from distutils.core import setup
from distutils.extension import Extension
from distutils import sysconfig

BUILD_VERSION = "HEAD"

# setup extra compilation and linking args
libs = []
extraLinkArgs = []
if sys.platform == "win32":
    extraLinkArgs.append("-Wl,--add-stdcall-alias")
    extraLinkArgs.append("-Wl,--enable-stdcall-fixup")
    importLibraryDir = os.environ.get("CX_LOGGING_LIB_DIR")
    if importLibraryDir is not None:
        importLibraryName = os.path.join(importLibraryDir, "libcx_Logging.a")
        extraLinkArgs.append("-Wl,--out-implib=%s" % importLibraryName)
    libs.append("ole32")
defineMacros = [
        ("CX_LOGGING_CORE",  None),
        ("BUILD_VERSION", BUILD_VERSION)
]

# define the list of files to be included as documentation for Windows
dataFiles = None
if sys.platform in ("win32", "cygwin"):
    baseName = "cx_Logging-doc"
    dataFiles = [ (baseName, [ "LICENSE.TXT", "HISTORY.TXT", "README.TXT" ]) ]
    htmlFiles = [os.path.join("html", n) for n in os.listdir("html") \
            if not n.startswith(".")]
    dataFiles.append( ("%s/%s" % (baseName, "html"), htmlFiles) )

# setup the extension
extension = Extension(
        name = "cx_Logging",
        define_macros = defineMacros,
        extra_link_args = extraLinkArgs,
        libraries = libs,
        sources = ["cx_Logging.c"])

# perform the setup
setup(
        name = "cx_Logging",
        version = BUILD_VERSION,
        description = "Python interface for logging",
        license = "See LICENSE.txt",
        data_files = dataFiles,
        long_description = "Python interface for logging",
        author = "Anthony Tuininga",
        author_email = "anthony.tuininga@gmail.com",
        url = "http://starship.python.net/crew/atuining",
        ext_modules = [extension])

