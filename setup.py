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

#Current version
version = "1.2.52"

"""
To release a new verison:

    1) Edit the version number above

    2) Tag the release with git

    >>> python setup.py tag

    3) Rebuild to update version in library and rebuild docs

    >>> make
    >>> make docs

    4) Publish the release to PyPi,
    ensure this is done in a clean checkout with no other source changes!

    >>> python setup.py publish

    (If this fails, check ~/.pypirc and try upgrading pip: pip install -U pip setuptools)
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

#Class to do the custom library build with make
class LVBuild(build):
    def run(self):
        # Run original build code
        build.run(self)

        # Build with make
        installdir = os.path.join(self.build_lib, 'lavavu')
        cmd = [
            'make',
            'OPATH=' + os.path.abspath(self.build_temp),
            'PREFIX=' + installdir,
            'PYTHON=' + sys.executable,
            'PYINC=-I' + distutils.sysconfig.get_python_inc(),
            'PYLIB=-L' + distutils.sysconfig.get_python_lib(),
        ]

        try:
            cmd.append('-j%d' % cpu_count())
        except:
            pass

        # Optional external libraries
        if find_library('png') and check_libraries(['png'], ['png.h']):
            cmd.append('LIBPNG=1')
        if find_library('tiff') and check_libraries(['tiff'], ['tiffio.h']):
            cmd.append('TIFF=1')
        if (find_library('avcodec') and find_library('avformat')
            and find_library('avutil') and find_library('swscale')
            and check_libraries(['avcodec', 'avformat', 'avutil', 'swscale'],
                ['libavformat/avformat.h', 'libavcodec/avcodec.h', 'libavutil/mathematics.h',
                 'libavutil/imgutils.h', 'libswscale/swscale.h'])):
            cmd.append('VIDEO=1')

        #Debug build
        #cmd.append('CONFIG=debug')

        def compile():
            try:
                output = subprocess.check_output(cmd, cwd=os.path.dirname(os.path.abspath(__file__)),
                                                 stderr=subprocess.STDOUT, universal_newlines=True)
            except subprocess.CalledProcessError as e:
                print("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n")
                print("Build Failed!\n")
                print("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n")
                print("{}\n".format(e.output))
                print("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n")
                raise e

        self.execute(compile, [], 'Compiling LavaVu')

class LVInstall(install):
    def run(self):
        install.run(self)

class LVDevelop(develop):
    def run(self):
        develop.run(self)

class LVEggInfo(egg_info):
    def run(self):
        egg_info.run(self)

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
          install_requires  = ['numpy'],
          platforms         = ['any'],
          scripts           = ['LV'],
          package_data      = {'lavavu': ['lavavu/shaders/*.*', 'lavavu/html/*.*', 'lavavu/font.bin', 'lavavu/dict.json']},
          data_files        = [('lavavu', ['lavavu/font.bin', 'lavavu/dict.json'])],
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
          cmdclass = {'build': LVBuild, 'install': LVInstall, 'develop': LVDevelop, 'egg_info': LVEggInfo},
          )

