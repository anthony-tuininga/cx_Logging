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
except ImportError:
    from distutils.core import setup
    from distutils.extension import Extension

BUILD_VERSION = "3.1"

# define class to ensure that linking against the library works for normal
# C programs while maintaining the name that Python expects
class build_ext(distutils.command.build_ext.build_ext):
    if sys.platform == "win32":
        user_options = distutils.command.build_ext.build_ext.user_options + [
                ('build-implib=', None,
                'directory for import library')
        ]

    def build_extension(self, ext):
        extra_link_args = ext.extra_link_args = []
        if sys.platform == "win32":
            self.mkpath(self.build_implib)
            if self.compiler.compiler_type == "msvc":
                self.import_library_name = os.path.join(self.build_implib,
                        "%s.lib" % ext.name)
                extra_link_args.append("/IMPLIB:%s" % self.import_library_name)
            else:
                self.import_library_name = os.path.join(self.build_implib,
                        "lib%s.a" % ext.name)
                extra_link_args.append("-Wl,--add-stdcall-alias")
                extra_link_args.append("-Wl,--enable-stdcall-fixup")
                extra_link_args.append("-Wl,--out-implib=%s" % \
                        self.import_library_name)
            ext.libraries = ["ole32"]
        else:
            file_name = self.get_ext_filename(ext.name)
            if sys.platform.startswith("aix"):
                extra_link_args.append("-Wl,-so%s" % file_name)
            else:
                extra_link_args.append("-Wl,-soname,%s" % file_name)
        distutils.command.build_ext.build_ext.build_extension(self, ext)

    def finalize_options(self):
        distutils.command.build_ext.build_ext.finalize_options(self)
        if sys.platform == "win32" and self.build_implib is None:
            target_dir = "implib.%s-%s" % \
                    (distutils.util.get_platform(), sys.version[:3])
            self.build_implib = os.path.join("build", target_dir)

    def initialize_options(self):
        distutils.command.build_ext.build_ext.initialize_options(self)
        if sys.platform == "win32":
            self.build_implib = None


# define class to ensure that the import library (Windows) is installed
# properly; this is not relevant on other platforms
class install_data(distutils.command.install_data.install_data):

    def run(self):
        distutils.command.install_data.install_data.run(self)
        if sys.platform == "win32":
            command = self.get_finalized_command("build_ext")
            target_dir = os.path.join(self.install_dir, "Libs")
            self.mkpath(target_dir)
            base_name = os.path.basename(command.import_library_name)
            target_file_name = os.path.join(target_dir, base_name)
            self.copy_file(command.import_library_name, target_file_name)
            self.outfiles.append(target_file_name)


# setup macros
define_macros = [
        ("CX_LOGGING_CORE",  None),
        ("BUILD_VERSION", BUILD_VERSION)
]

export_symbols = [
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
    export_symbols.extend([
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
        name="cx_Logging",
        define_macros=define_macros,
        export_symbols=export_symbols,
        sources=["src/cx_Logging.c"],
        depends=["src/cx_Logging.h"])

# perform the setup
setup(
        cmdclass=dict(build_ext=build_ext, install_data=install_data),
        version=BUILD_VERSION,
        ext_modules=[extension])
