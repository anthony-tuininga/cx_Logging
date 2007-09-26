"""Distutils script for cx_Logging.

Windows platforms:
    python setup.py build --compiler=mingw32 install

Unix platforms
    python setup.py build install

"""

import distutils.command.build_ext
import distutils.command.install_data
import distutils.util
import os
import sys

from distutils.core import setup
from distutils.extension import Extension
from distutils import sysconfig

BUILD_VERSION = "1.4b1"

# define class to ensure that the import library is created properly
class build_ext(distutils.command.build_ext.build_ext):
    user_options = distutils.command.build_ext.build_ext.user_options + [
            ('build-implib=', None,
             'directory for import library')
    ]

    def build_extension(self, ext):
        self.mkpath(self.build_implib)
        self.importLibraryName = os.path.join(self.build_implib,
                "lib%s.a" % ext.name)
        if sys.platform == "win32":
            extraLinkArgs = ext.extra_link_args = []
            extraLinkArgs.append("-Wl,--add-stdcall-alias")
            extraLinkArgs.append("-Wl,--enable-stdcall-fixup")
            extraLinkArgs.append("-Wl,--out-implib=%s" % \
                    self.importLibraryName)
            ext.libraries = ["ole32"]
        distutils.command.build_ext.build_ext.build_extension(self, ext)

    def finalize_options(self):
        distutils.command.build_ext.build_ext.finalize_options(self)
        if self.build_implib is None:
            dir = "implib.%s-%s" % \
                    (distutils.util.get_platform(), sys.version[:3])
            self.build_implib = os.path.join("build", dir)

    def initialize_options(self):
        distutils.command.build_ext.build_ext.initialize_options(self)
        self.build_implib = None


# define class to ensure that the import library is installed properly
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

# define the list of files to be included as documentation
dataFiles = None
if sys.platform in ("win32", "cygwin"):
    baseName = "cx_Logging-doc"
    dataFiles = [ (baseName, [ "LICENSE.txt", "HISTORY.txt", "README.txt" ]) ]
    htmlFiles = [os.path.join("html", n) for n in os.listdir("html") \
            if not n.startswith(".")]
    dataFiles.append( ("%s/%s" % (baseName, "html"), htmlFiles) )
docFiles = "LICENSE.txt HISTORY.txt README.txt html"
options = dict(bdist_rpm = dict(doc_files = docFiles))

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

# setup the extension
extension = Extension(
        name = "cx_Logging",
        define_macros = defineMacros,
        sources = ["cx_Logging.c"],
        depends = ["cx_Logging.h"])

# perform the setup
setup(
        name = "cx_Logging",
        cmdclass = dict(build_ext = build_ext, install_data = install_data),
        version = BUILD_VERSION,
        description = "Python and C interfaces for logging",
        license = "See LICENSE.txt",
        data_files = dataFiles,
        long_description = "Python and C interfaces for logging",
        author = "Anthony Tuininga",
        author_email = "anthony.tuininga@gmail.com",
        maintainer = "Anthony Tuininga",
        maintainer_email = "anthony.tuininga@gmail.com",
        url = "http://cx-logging.sourceforge.net",
        keywords = "logging",
        classifiers = classifiers,
        license = "Python Software Foundation License",
        ext_modules = [extension],
        options = options)

