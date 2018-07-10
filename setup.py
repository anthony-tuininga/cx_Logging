"""
Distutils script for cx_Logging.

python setup.py build install
"""

import distutils.command.build_ext
import distutils.command.install_data
import distutils.util
import os
import sys

# if setuptools is detected, use it to add support for eggs
try:
    from setuptools import setup, Extension
except:
    from distutils.core import setup
    from distutils.extension import Extension

BUILD_VERSION = "2.2"

# define class to ensure that linking against the library works for normal
# C programs while maintaining the name that Python expects
class build_ext(distutils.command.build_ext.build_ext):
    import distutils
    import os
    import sys
    if sys.platform == "win32":
        user_options = distutils.command.build_ext.build_ext.user_options + [
                ('build-implib=', None,
                'directory for import library')
        ]

    def build_extension(self, ext):
        import distutils.command.build_ext
        import os
        import sys
        extraLinkArgs = ext.extra_link_args = []
        if sys.platform == "win32":
            self.mkpath(self.build_implib)
            if self.compiler.compiler_type == "msvc":
                self.importLibraryName = os.path.join(self.build_implib,
                        "%s.lib" % ext.name)
                extraLinkArgs.append("/IMPLIB:%s" % self.importLibraryName)
            else:
                self.importLibraryName = os.path.join(self.build_implib,
                        "lib%s.a" % ext.name)
                extraLinkArgs.append("-Wl,--add-stdcall-alias")
                extraLinkArgs.append("-Wl,--enable-stdcall-fixup")
                extraLinkArgs.append("-Wl,--out-implib=%s" % \
                        self.importLibraryName)
            ext.libraries = ["ole32"]
        else:
            fileName = self.get_ext_filename(ext.name)
            if sys.platform.startswith("aix"):
                extraLinkArgs.append("-Wl,-so%s" % fileName)
            else:
                extraLinkArgs.append("-Wl,-soname,%s" % fileName)
        distutils.command.build_ext.build_ext.build_extension(self, ext)

    def finalize_options(self):
        import distutils.command.build_ext
        distutils.command.build_ext.build_ext.finalize_options(self)
        import os
        import sys
        if sys.platform == "win32" and self.build_implib is None:
            dir = "implib.%s-%s" % \
                    (distutils.util.get_platform(), sys.version[:3])
            self.build_implib = os.path.join("build", dir)

    def initialize_options(self):
        import distutils.command.build_ext
        distutils.command.build_ext.build_ext.initialize_options(self)
        import sys
        if sys.platform == "win32":
            self.build_implib = None


# define class to ensure that the import library (Windows) is installed
# properly; this is not relevant on other platforms
class install_data(distutils.command.install_data.install_data):

    def run(self):
        distutils.command.install_data.install_data.run(self)
        if sys.platform == "win32":
            command = self.get_finalized_command("build_ext")
            dir = os.path.join(self.install_dir, "Libs")
            self.mkpath(dir)
            baseName = os.path.basename(command.importLibraryName)
            targetFileName = os.path.join(dir, baseName)
            self.copy_file(command.importLibraryName, targetFileName)
            self.outfiles.append(targetFileName)


# setup macros
defineMacros = [
        ("CX_LOGGING_CORE",  None),
        ("BUILD_VERSION", BUILD_VERSION)
]

# define the classifiers for the project
classifiers = [
        "Development Status :: 5 - Production/Stable",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: Python Software Foundation License",
        "Natural Language :: English",
        "Operating System :: OS Independent",
        "Programming Language :: C",
        "Programming Language :: Python",
        "Topic :: Software Development :: Libraries :: Python Modules",
        "Topic :: Utilities"
]

exportSymbols = [
        "StartLogging",
        "StartLoggingEx",
        "StartLoggingForPythonThread",
        "StartLoggingForPythonThreadEx",
        "StartLoggingStderr",
        "StartLoggingStderrEx",
        "StartLoggingStdout",
        "StartLoggingStdoutEx",
        "StartLoggingFromEnvironment",
        "StopLogging",
        "StopLoggingForPythonThread",
        "LogMessage",
        "LogMessageV",
        "LogMessageVaList",
        "LogMessageForPythonV",
        "WriteMessageForPython",
        "LogDebug",
        "LogInfo",
        "LogWarning",
        "LogError",
        "LogCritical",
        "LogTrace",
        "GetLoggingLevel",
        "SetLoggingLevel",
        "LogPythonObject",
        "LogPythonException",
        "LogPythonExceptionWithTraceback",
        "LogConfiguredException",
        "GetLoggingState",
        "SetLoggingState",
        "IsLoggingStarted",
        "IsLoggingAtLevelForPython"
]

if sys.platform == "win32":
    exportSymbols.extend([
            "LogWin32Error",
            "LogGUID",
            "StartLoggingW",
            "StartLoggingExW",
            "LogMessageW",
            "LogDebugW",
            "LogInfoW",
            "LogWarningW",
            "LogErrorW",
            "LogCriticalW",
            "LogTraceW"
    ])

# setup the extension
extension = Extension(
        name = "cx_Logging",
        define_macros = defineMacros,
        export_symbols = exportSymbols,
        sources = ["src/cx_Logging.c"],
        depends = ["src/cx_Logging.h"])

# perform the setup
setup(
        name = "cx_Logging",
        cmdclass = dict(build_ext = build_ext, install_data = install_data),
        version = BUILD_VERSION,
        description = "Python and C interfaces for logging",
        data_files = [ ("cx_Logging-doc", ["LICENSE.txt", "README.txt"]) ],
        long_description = "Python and C interfaces for logging",
        author = "Anthony Tuininga",
        author_email = "anthony.tuininga@gmail.com",
        maintainer = "Anthony Tuininga",
        maintainer_email = "anthony.tuininga@gmail.com",
        url = "http://cx-logging.sourceforge.net",
        keywords = "logging",
        classifiers = classifiers,
        license = "Python Software Foundation License",
        ext_modules = [extension])

