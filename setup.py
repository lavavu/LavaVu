import os
import sys
from setuptools import setup
from setuptools.command.install import install
from setuptools.command.develop import develop
from setuptools.command.egg_info import egg_info
from setuptools import Extension
from setuptools import find_packages
import distutils
from distutils.command.build import build
import distutils.sysconfig
import distutils.ccompiler
from distutils.errors import CompileError, LinkError
import subprocess
from multiprocessing import cpu_count
from ctypes.util import find_library
import platform
import glob
import shutil


try: 
    from distutils.command import bdist_conda
except ImportError:
    pass


#Current version
#(must be of the form X.Y.Z to trigger wheel builds)
version = "1.6.1"

"""
To release a new verison:

    1) Edit the version number above, then run below command (in a fresh checkout!).
    This will rebuild lib and docs in release mode with the new version
    (may need to reset notebooks to documentation versions)
    (docs repo is at: git@github.com:lavavu/Documentation.git)
    (may need: `cd docs/src; pip install -r requirements.txt`)

    >>> python setup.py check

    2) Check the docs, and make any further commits necessary to get them in order,
       and run above command again until docs are ready.

    Now ready for release, run this to commit and push changes to docs,
    then tag, commit and push new version.

    >>> python setup.py release

NOTE:
    To use particular libraries, set LV_LIB_DIRS and LV_INC_DIRS on command line before running, eg:
    LV_LIB_DIRS=${MESA_PATH/lib/x86_64-linux-gnu LV_INC_DIRS=${MESA_PATH}/include python setup.py install

"""

def write_version():
    """
    Writes version info to version.cpp
    """
    content = ""
    try:
        with open('src/version.cpp', 'r') as f:
            content = f.read()
    except:
        pass
    if not version in content:
        with open('src/version.cpp', 'w') as f:
            print("Writing new version: " + version)
            f.write('#include "version.h"\nconst std::string version = "%s";\n' % version)
    else:
        print("Version matches: " + version)

#Run with "new" to prepare for a new version after changing above
if sys.argv[-1] == 'check':
    write_version()
    os.system("LV_RELEASE=1 make -j$(nproc)")
    os.system("LV_RELEASE=1 make docs -B")
    import webbrowser
    webbrowser.open("docs/index.html")
    sys.exit()
#Run with "tag" arg to create a release tag
if sys.argv[-1] == 'release':
    #Commit and push docs
    wd = os.getcwd()
    os.chdir("docs")
    os.system("git add -u")
    os.system('git commit -m "Docs rebuilt for release"')
    os.system("git push")
    #Commit main repo
    os.chdir(wd)
    os.system("git add -u")
    os.system('git commit -m "Release version {0}"'.format(version))

    #Tag and push
    os.system("git tag -a {0} -m 'version {0}'".format(version))
    os.system("git push origin %s" % version)
    os.system("git push")
    sys.exit()

#Run with "publish" arg to upload the release
if sys.argv[-1] == 'publish':
    os.system("python setup.py sdist")
    os.system("twine upload dist/lavavu-%s.tar.gz" % version)
    sys.exit()


#Get extra lib and include dirs
inc_dirs = []
lib_dirs = []
if 'LV_LIB_DIRS' in os.environ:
    lib_dirs += os.environ['LV_LIB_DIRS'].split(':')
if 'LV_INC_DIRS' in os.environ:
    inc_dirs += os.environ['LV_INC_DIRS'].split(':')

def build_sqlite3(sqlite3_path):
    """Builds sqlite3 from included submodule
    """
    compiler = distutils.ccompiler.new_compiler()
    assert isinstance(compiler, distutils.ccompiler.CCompiler)
    distutils.sysconfig.customize_compiler(compiler)

    #Add any include dirs
    compiler.add_include_dir(sqlite3_path)

    try:
        obj = compiler.compile([os.path.join(sqlite3_path, 'sqlite3.c')], 'build')
        return obj
    except CompileError:
        print('sqlite3 compile error')
        raise

#From https://stackoverflow.com/a/28949827/866759
def check_libraries(libraries, headers, extra_lib_dirs=[], extra_inc_dirs=[]):
    """check if the C module can be built by trying to compile a small
    program against the passed library with passed headers"""

    import tempfile
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

    #Add any extra library or include dirs
    for l in lib_dirs + extra_lib_dirs:
        compiler.add_library_dir(l)
    for p in inc_dirs + extra_inc_dirs:
        compiler.add_include_dir(p)


    try:
        compiler.link_executable(
            compiler.compile([file_name]),
            bin_file_name,
            libraries=libraries,
        )
    except (CompileError) as e:
        print('Libraries ' + str(libraries) + ' test compile error', e)
        ret_val = False
    except (LinkError) as e:
        print('Libraries ' + str(libraries) + ' test link error', e)
        ret_val = False
    else:
        print('Libraries ' + str(libraries) + ' found and passed compile test')
        ret_val = True
    shutil.rmtree(tmp_dir)
    return ret_val

if __name__ == "__main__":
    print("Running setup with args:", sys.argv)
    #Update version.cpp
    write_version()

    sqlite3_path = os.path.join('src', 'sqlite3')
    if not os.path.exists(os.path.join(sqlite3_path, 'sqlite3.c')):
        #Attempt to get sqlite3 source submodule if not checked out
        os.system("git submodule update --init")

    _debug = False
    #_debug = True
    srcs = [os.path.join('src', 'LavaVuPython_wrap.cxx')]
    srcs += glob.glob(os.path.join('src', '*.cpp'))
    srcs += glob.glob(os.path.join('src', 'Main', '*.cpp'))
    srcs += glob.glob(os.path.join('src', 'jpeg', '*.cpp'))
    defines = [('USE_FONTS', '1')]
    cflags = []
    libs = []
    ldflags = []
    rt_lib_dirs = []
    extra_objects = []
    try:
        import numpy
    except:
        subprocess.check_call([sys.executable, '-m', 'pip', 'install', 'numpy>=1.11.0'])
        try:
            import numpy
        except:
            subprocess.check_call([sys.executable, '-m', 'pip3', 'install', 'numpy>=1.11.0'])
            import numpy
    inc_dirs += [numpy.get_include()]

    P = platform.system()
    if _debug:
        print("DEBUG BUILD ENABLED")
        defines += [('DEBUG', '1')]
        if P == 'Windows':
            cflags += ['/Zi']
            ldflags += ['/DEBUG']
        else:
            cflags += ['-g', '-O0']

    #Parallel build
    num_jobs = 1
    if not 'arm' in platform.machine():
        try:
            num_jobs = os.cpu_count()
        except AttributeError:
            import multiprocessing
            num_jobs = multiprocessing.cpu_count()

    if num_jobs > 1:
        try:
            from numpy.distutils.ccompiler import CCompiler_compile
            import distutils.ccompiler
            distutils.ccompiler.CCompiler.compile = CCompiler_compile
            os.environ['NPY_NUM_BUILD_JOBS'] = str(num_jobs)
            print('Using parallel build, jobs: ', num_jobs)
        except ImportError:
            print("Numpy not found, parallel compile not available")

    #OS Specific
    if P == 'Windows':
        #Windows - includes all dependencies

        #VC++ can handle C or C++ without issues clang has,
        #so just throw sqlite source in with the rest
        inc_dirs += [sqlite3_path]
        srcs += [os.path.join(sqlite3_path, 'sqlite3.c')]

        #32 or 64 bit python interpreter?
        if sys.maxsize > 2**32:
            arc = '64'
        else:
            arc = '32'

        #Download, extract and install ffmpeg files
        ffmpeg_dlls = []
        ffmpeg_libs = []
        try:
            sys.path.append('lavavu')
            import vutils
            import zipfile
            src = 'https://ffmpeg.zeranoe.com/builds/win' + arc
            fn1 = 'ffmpeg-4.2.1-win' + arc + '-shared'
            fn2 = 'ffmpeg-4.2.1-win' + arc + '-dev'
            LIBS = 'lib' + arc
            outfn = vutils.download(src + '/shared/' + fn1 + '.zip')
            with zipfile.ZipFile(outfn, 'r') as zip_ref:
                zip_ref.extractall('.')
            outfn = vutils.download(src + '/dev/' + fn2 + '.zip')
            with zipfile.ZipFile(outfn, 'r') as zip_ref:
                zip_ref.extractall('.')
            #Now copy into windows build dirs
            libs = ['avformat', 'avcodec', 'avutil', 'swscale']
            for lib in libs:
                #Headers - move entire directories
                dst = os.path.join('src', 'windows', 'inc', 'lib' + lib)
                if not os.path.exists(dst):
                    shutil.move(os.path.join(fn2, 'include', 'lib' + lib), dst)
                #Lib file
                dst = os.path.join('src', 'windows', LIBS)
                shutil.copy(os.path.join(fn2, 'lib', lib + '.lib'), dst)
                ffmpeg_libs += [lib]
            #Just grab all the dlls
            ffmpeg_dlls = glob.glob(os.path.join(fn1, 'bin', '*.dll'))

            #If we got this far, enable video
            defines += [('HAVE_LIBAVCODEC', '1'), ('HAVE_SWSCALE', '1')]

        except (Exception) as e:
            print("ffmpeg download/extract failed", str(e))
            ffmpeg_dlls = []
            ffmpeg_libs = []
            pass

        srcs += [os.path.join('src', 'png', 'lodepng.cpp')]
        srcs += [os.path.join('src', 'miniz', 'miniz.c')]
        defines += [('HAVE_GLFW', '1')]
        #defines += [('HAVE_LIBPNG', 1)]
        #inc_dirs += [os.path.join(os.getcwd(), 'src', 'windows', 'inc')]
        #lib_dirs += [os.path.join(os.getcwd(), 'src', 'windows', LIBS)]
        inc_dirs += [os.path.join('src', 'windows', 'inc')]
        lib_dirs += [os.path.join('src', 'windows', LIBS)]
        #ldflags += ['/LIBPATH:' + os.path.join(os.getcwd(), 'src', 'windows', LIBS)]
        ldflags += ['/LIBPATH:' + os.path.join('src', 'windows', LIBS)]
        libs += ['opengl32', 'pthreadVC2', 'glfw3dll'] + ffmpeg_libs
        dlls = [os.path.join('src', 'windows', LIBS, 'pthreadVC2.dll'),
                os.path.join('src', 'windows', LIBS, 'glfw3.dll')] + ffmpeg_dlls
        #Copy dlls into ./lavavu so can be found by package_data
        for d in dlls:
            shutil.copy(d, 'lavavu')
    else:
        #POSIX only - find external dependencies
        cflags += ['-std=c++0x']
        #Use external sqlite3 if available, otherwise build
        if not (find_library('sqlite3') and check_libraries(['sqlite3'], ['sqlite3.h'], extra_inc_dirs=[sqlite3_path])):
            #Don't compile when just running 'setup.py egg_info'
            if len(sys.argv) < 2 or sys.argv[1] == 'egg_info':
                sqlite3 = 'build/src/sqlite3/sqlite3.o'
            else:
                sqlite3 = build_sqlite3(sqlite3_path)
            inc_dirs += [sqlite3_path]
            extra_objects = sqlite3
        else:
            libs += ['sqlite3']

        # Optional external libraries - check if installed
        if find_library('png') and check_libraries(['png', 'z'], ['png.h', 'zlib.h']):
            defines += [('HAVE_LIBPNG', 1), ('USE_ZLIB', '1')]
            libs += ['png', 'z']
        else:
            srcs += ['src/png/lodepng.cpp']
            if find_library('z') and check_libraries(['z'], ['zlib.h']):
                defines += [('USE_ZLIB', '1')]
                libs += ['z']
            else:
                srcs += ['src/miniz/miniz.c']

        if find_library('tiff') and check_libraries(['tiff'], ['tiffio.h']):
            defines += [('HAVE_LIBTIFF', 1)]
            libs += ['tiff']

        inc_dirs += ['/usr/include/ffmpeg'] #Required for centos/redhat ffmpeg-devel
        if (find_library('avcodec') and find_library('avformat') and find_library('avutil')
            and check_libraries(['avcodec', 'avformat', 'avutil'],
                ['libavformat/avformat.h', 'libavcodec/avcodec.h', 'libavutil/mathematics.h',
                 'libavutil/imgutils.h'])):
            defines += [('HAVE_LIBAVCODEC', 1)]
            libs += ['avcodec', 'avformat', 'avutil']
            if find_library('swscale') and check_libraries(['swscale'], ['libswscale/swscale.h']):
                defines += [('HAVE_SWSCALE', 1)]
                libs += ['swscale']

        if P == 'Linux':
            #Linux GLFW, X11, EGL or OSMesa
            #To force EGL or OSMesa, set LV_EGL=1 or LV_OSMESA=1
            if ("LV_EGL" in os.environ and find_library('OpenGL') and find_library('EGL')
            and check_libraries(['OpenGL', 'EGL'], ['GL/gl.h', 'EGL/egl.h'])):
                #EGL for offscreen OpenGL without X11/GLX - works only with NVidia currently
                defines += [('HAVE_EGL', '1')]
                libs += ['OpenGL', 'EGL']
            elif ("LV_OSMESA" in os.environ and find_library('OSMesa')
            and check_libraries(['OSMesa'], ['GL/osmesa.h'])):
                #OSMesa for software rendered offscreen OpenGL
                defines += [('HAVE_OSMESA', '1')]
                libs += ['OSMesa']
            #elif find_library('glfw') and check_libraries(['glfw'], ['GLFW/glfw3.h']):
            #    #Use GLFW if installed
            #    defines += [('HAVE_GLFW', '1')]
            #    libs += ['GL', 'glfw']
            else:
                #Fallback - X11
                defines += [('HAVE_X11', '1')]
                libs += ['GL', 'X11']

            #Runtime libraries: set rpath: $ORIGIN includes current dir in search
            rt_lib_dirs += ['$ORIGIN'] + lib_dirs

        elif P == 'Darwin':
            #Mac OS X with Cocoa + CGL
            #srcs += ['src/Main/CocoaViewer.mm']
            #This hack is because setuptools can't handle .mm extension
            shutil.copyfile('src/Main/CocoaViewer.mm', 'src/Main/CocoaViewer.m')
            srcs += ['src/Main/CocoaViewer.m']
            cflags += ['-ObjC++'] #Now have to tell compiler it's objective c++ as .m file indicates objective c
            defines += [('HAVE_CGL', '1'), ('GL_SILENCE_DEPRECATION','1')]
            cflags += ['-undefined suppress', '-flat_namespace'] #Swig, necessary?
            cflags += ['-Wno-unknown-warning-option', '-Wno-c++14-extensions', '-Wno-shift-negative-value']
            cflags += ['-FCocoa', '-FOpenGL', '-stdlib=libc++']
            libs += ['c++', 'objc']
            #Can't use extra_link_args because they are appended by setuptools but frameworks must come first
            os.environ['LDFLAGS'] = '-framework Cocoa -framework Quartz -framework OpenGL'
            #Runtime library dirs doesn't work on mac, so set rpath manually
            for l in lib_dirs:
                ldflags.append('-Wl,-rpath,'+l)

        #Other posix libs
        libs += ['dl', 'pthread', 'm']

    lv = Extension('lavavu._LavaVuPython',
                    define_macros = defines,
                    include_dirs = inc_dirs,
                    libraries = libs,
                    library_dirs = lib_dirs,
                    runtime_library_dirs = rt_lib_dirs,
                    extra_compile_args = cflags,
                    extra_link_args = ldflags,
                    extra_objects=extra_objects,
                    sources = srcs)

    with open('requirements.txt') as f:
        requirements = f.read().splitlines()

    #Binary package data for wheels
    #Package_data works for wheels(binary) only - so add everything we need for the wheels here
    #(MANIFEST.in contents will be included with sdist only unless include_package_data is enabled)
    package_data = {'lavavu': ['font.bin', 'dict.json', 'shaders/*', 'html/*']}
    if P == 'Windows':
        package_data['lavavu'] += ['*.dll']

    setup(name = 'lavavu',
          author            = "Owen Kaluza",
          author_email      = "owen.kaluza@monash.edu",
          url               = "https://github.com/lavavu/LavaVu",
          version           = version,
          license           = "LGPL-3",
          description       = "Python interface to LavaVu OpenGL 3D scientific visualisation utilities",
          long_description  = 'See https://github.com/lavavu/LavaVu for more info',
          packages          = ['lavavu'],
          package_dir       = {'lavavu': 'lavavu'},
          package_data      = package_data,
          install_requires  = requirements,
          platforms         = ['any'],
          entry_points      = {
              'gui_scripts': [
                  'LV = lavavu.__main__:main',
                  'LavaVu = lavavu.__main__:main'
              ]
          },
          #Use below to include files from MANIFEST.in or specify package_data, not both
          #include_package_data = True,
          classifiers = [
            'Intended Audience :: Developers',
            'Intended Audience :: Science/Research',
            'License :: OSI Approved :: GNU Lesser General Public License v3 (LGPLv3)',
            'Operating System :: POSIX :: Linux',
            'Operating System :: MacOS',
            'Operating System :: Microsoft :: Windows',
            'Environment :: X11 Applications',
            'Environment :: MacOS X :: Cocoa',
            'Environment :: Win32 (MS Windows)',
            'Topic :: Multimedia :: Graphics :: 3D Rendering',
            'Topic :: Scientific/Engineering :: Visualization',
            'Development Status :: 5 - Production/Stable',
            'Programming Language :: C++',
            'Programming Language :: Python :: 3.6',
            'Programming Language :: Python :: 3.7',
            'Programming Language :: Python :: 3.8',
            'Framework :: Jupyter',
            'Framework :: IPython',
          ],
          ext_modules = [lv]
          )

