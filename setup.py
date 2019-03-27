import os
import sys
from setuptools import setup
from setuptools.command.install import install
from setuptools.command.develop import develop
from setuptools.command.egg_info import egg_info
from distutils.command.build import build
import distutils
import subprocess
from multiprocessing import cpu_count
from ctypes.util import find_library
from setuptools import Extension
import platform
import glob

#Current version
version = "1.3.4"

"""
To release a new verison:

    1) Edit the version number above, then commit the change!

    2) Tag the release with git

    >>> python setup.py tag

    3) Rebuild to update version in library and rebuild docs

    >>> make
    >>> make docs

    4) Publish the release to PyPi,
    ensure this is done in a clean checkout with no other source changes!

    >>> python setup.py publish

    (If this fails, check ~/.pypirc and try upgrading pip: pip install -U pip setuptools)

TODO:
    Move to twine for uploads https://pypi.org/project/twine/
    Build and upload wheels for major platforms
"""

#Run with "tag" arg to create a release tag
if sys.argv[-1] == 'tag':
    os.system("git tag -a %s -m 'version %s'" % (version, version))
    os.system("git push --tags")
    sys.exit()

#Run with "publish" arg to upload the release
if sys.argv[-1] == 'publish':
    os.system("python setup.py sdist upload")
    sys.exit()

#From https://stackoverflow.com/a/28949827/866759
def check_libraries(libraries, headers):
    """check if the C module can be built by trying to compile a small
    program against the passed library with passed headers"""

    import tempfile
    import shutil

    import distutils.sysconfig
    import distutils.ccompiler
    from distutils.errors import CompileError, LinkError

    # write a temporary .c file to compile
    c_code = "int main(int argc, char* argv[]) { return 0; }"
    #Add headers
    for header in headers:
        c_code = "#include <" + header + ">\n" + c_code

    tmp_dir = tempfile.mkdtemp(prefix = 'tmp_comp__')
    bin_file_name = os.path.join(tmp_dir, 'test_comp')
    file_name = bin_file_name + '.c'
    with open(file_name, 'w') as fp:
        fp.write(c_code)

    # and try to compile it
    compiler = distutils.ccompiler.new_compiler()
    assert isinstance(compiler, distutils.ccompiler.CCompiler)
    distutils.sysconfig.customize_compiler(compiler)

    try:
        compiler.link_executable(
            compiler.compile([file_name]),
            bin_file_name,
            libraries=libraries,
        )
    except CompileError:
        print('Libraries ' + str(libraries) + ' test compile error')
        ret_val = False
    except LinkError:
        print('Libraries ' + str(libraries) + ' test link error')
        ret_val = False
    else:
        print('Libraries ' + str(libraries) + ' found and passed compile test')
        ret_val = True
    shutil.rmtree(tmp_dir)
    return ret_val

_debug = False
defines = [('USE_FONTS', '1'), ('USE_ZLIB', '1')]
cflags = ['-std=c++0x']
srcs = ['src/LavaVuPython_wrap.cxx'] + glob.glob('src/*.cpp') + glob.glob('src/Main/*.cpp') + glob.glob('src/jpeg/*.cpp') + glob.glob('src/png/*.cpp') + glob.glob('src/sqlite3/*.c')
libs = []
ldflags = []
inc_dirs = []
lib_dirs = []

if _debug:
    defines += [('CONFIG', 'debug')]
# Optional external libraries - check if installed
if find_library('png') and check_libraries(['png'], ['png.h']):
    defines += [('LIBPNG', 1)]
if find_library('tiff') and check_libraries(['tiff'], ['tiffio.h']):
    defines += [('TIFF', 1)]
if (find_library('avcodec') and find_library('avformat')
    and find_library('avutil') and find_library('swscale')
    and check_libraries(['avcodec', 'avformat', 'avutil', 'swscale'],
        ['libavformat/avformat.h', 'libavcodec/avcodec.h', 'libavutil/mathematics.h',
         'libavutil/imgutils.h', 'libswscale/swscale.h'])):
    defines += [('VIDEO', 1)]

#OS Specific
P = platform.system()
if P == 'Linux':
    #Linux X11 or EGL
    defines += [('HAVE_X11', '1')]
    libs += ['GL', 'dl', 'pthread', 'm', 'z', 'X11']
    #EGL for offscreen OpenGL without X11/GLX
    #if find_library('OpenGL') and find_library('EGL') and check_libraries(['OpenGL', 'EGL'], ['GL/gl.h']):
    #    defines += [('EGL', 1)]
elif P == 'Darwin':
    #Mac OS X with Cocoa + CGL
    defines += [('HAVE_CGL', '1')]
    srcs += ['src/Main/CocoaViewer.mm']
    cflags += ['-undefined suppress', '-flat_namespace'] #Swig, necessary?
    cflags += ['-Wno-unknown-warning-option', '-Wno-c++14-extensions', '-Wno-shift-negative-value']
    cflags += ['-FCocoa', '-FOpenGL', '-stdlib=libc++']
    libs += ['c++', 'dl', 'pthread',  'objc', 'm', 'z']
    ldflags += ['-framework Cocoa', '-framework Quartz', '-framework OpenGL']
elif P == 'Windows':
    defines += [('HAVE_SDL', '1')]

lv = Extension('_LavaVuPython',
                define_macros = defines,
                include_dirs = inc_dirs,
                libraries = libs,
                library_dirs = lib_dirs,
                extra_compile_args = cflags,
                extra_link_args = ldflags,
                sources = srcs)

if __name__ == "__main__":

    setup(name = 'lavavu',
          author            = "Owen Kaluza",
          author_email      = "owen.kaluza@monash.edu",
          url               = "https://github.com/OKaluza/LavaVu",
          version           = version,
          license           = "LGPL-3",
          description       = "Python interface to LavaVu OpenGL 3D scientific visualisation utilities",
          long_description  = 'See https://github.com/OKaluza/LavaVu/wiki for more info',
          packages          = ['lavavu'],
          install_requires  = ['numpy', 'jupyter-server-proxy;python_version>"2.7"'],
          platforms         = ['any'],
          scripts           = ['LV'],
          package_data      = {'lavavu': glob.glob('lavavu/shaders/*.*') + glob.glob('lavavu/html/*.*') + ['lavavu/font.bin', 'lavavu/dict.json']},
          data_files        = [('lavavu', ['lavavu/font.bin', 'lavavu/dict.json'])],
          include_package_data = True,
          classifiers = [
            'Intended Audience :: Developers',
            'Intended Audience :: Science/Research',
            'License :: OSI Approved :: GNU Lesser General Public License v3 (LGPLv3)',
            'Operating System :: Unix',
            'Operating System :: POSIX :: Linux',
            'Operating System :: MacOS',
            'Environment :: X11 Applications',
            'Environment :: MacOS X :: Cocoa',
            'Programming Language :: C++',
            'Topic :: Multimedia :: Graphics :: 3D Rendering',
            'Topic :: Scientific/Engineering :: Visualization',
            'Development Status :: 4 - Beta',
            'Programming Language :: Python :: 2.7',
            'Programming Language :: Python :: 3',
            'Programming Language :: Python :: 3.4',
            'Programming Language :: Python :: 3.5',
            'Programming Language :: Python :: 3.6',
          ],
          ext_modules = [lv]
          )

