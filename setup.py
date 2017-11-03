import os
import sys
from setuptools import setup
from setuptools.command.install import install
from setuptools.command.develop import develop
from setuptools.command.egg_info import egg_info
from distutils.command.build import build
from subprocess import call
from multiprocessing import cpu_count
from ctypes.util import find_library

if sys.argv[-1] == 'tag':
    os.system("git tag -a %s -m 'version %s'" % (version, version))
    os.system("git push --tags")
    sys.exit()

if sys.argv[-1] == 'publish':
    os.system("python setup.py sdist upload")
    sys.exit()

class LVBuild(build):
    def run(self):
        # Run original build code
        build.run(self)

        # Build with make
        cmd = [
            'make',
            'OPATH=' + os.path.abspath(self.build_temp),
            'PREFIX=' + os.path.join(self.build_lib, 'lavavu'),
        ]

        try:
            cmd.append('-j%d' % cpu_count())
        except:
            pass

        # Optional external libraries
        if find_library('png'):
            cmd.append('LIBPNG=1')
        if find_library('tiff'):
            cmd.append('TIFF=1')
        if find_library('avcodec') and find_library('avformat') and find_library('avutil') and find_library('swscale'):
            cmd.append('VIDEO=1')

        #Debug build
        #cmd.append('CONFIG=debug')

        def compile():
            call(cmd, cwd= os.path.dirname(os.path.abspath(__file__)))

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
          version           = "1.2.13",
          license           = "LGPL-3",
          description       = "Python interface to LavaVu OpenGL 3D scientific visualisation utilities",
          long_description  = 'See https://github.com/OKaluza/LavaVu/wiki for more info',
          packages          = ['lavavu'],
          install_requires  = ['numpy'],
          platforms         = ['any'],
          scripts           = ['LV'],
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
          ],
          cmdclass = {'build': LVBuild, 'install': LVInstall, 'develop': LVDevelop, 'egg_info': LVEggInfo},
          )

