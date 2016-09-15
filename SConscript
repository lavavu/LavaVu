import os, platform
Import('env')

#
# Need to make a copy because SCons uses the environment
# at it's final state, so StGermain ends up depending on
# StgDomain, etc.
#

origenv = env
env = env.Clone()

#############################################
# Switch env to LavaVu base!
values = {}
execfile("graphics.cfg", globals(), values)
env._dict.update(values)
#############################################

env['CURR_PROJECT'] = 'LavaVu'

# Need the module name, which is just the directory.
d = "."
mod_name = env['ESCAPE']('"' + ''.join(d.split('/')) + '"')
# Binary install dir and rpath
if env['prefix'] == env.GetLaunchDir():
   bin_dir = os.path.join(env['prefix'], env['build_dir'], "bin");
   rpath = [os.path.join(env['prefix'], env['build_dir'], "lib")]
else:
   bin_dir = os.path.join(env['prefix'], "bin");
   rpath = [os.path.join(env['prefix'], "lib")]

cpp_defs = [('CURR_MODULE_NAME', mod_name)] + [('SHADER_PATH', '"\\"' + bin_dir + '/\\""')] + [('USE_FONTS')]
#cpp_defs = [('CURR_MODULE_NAME', mod_name)] + [('SHADER_PATH', '"\\"' + bin_dir + '/\\""')] + [('USE_FONTS')] + [('USE_ZLIB')]
env['CPPDEFINES'] += cpp_defs

# Setup where to look for files.
src_dir = 'src'
inc_dir = 'include/LavaVu/Viewer/'

# Install the shaders
env.Install('bin/', Glob(src_dir + '/shaders/*.vert'))
env.Install('bin/', Glob(src_dir + '/shaders/*.frag'))
# Install the html source
env.Install('bin/html/', Glob(src_dir + '/html/*.js'))
env.Install('bin/html/', Glob(src_dir + '/html/*.css'))
this_sconscript_file = (lambda x:x).func_code.co_filename
code_base = os.path.dirname(this_sconscript_file)
env.Command(bin_dir + '/html/index.html', 'src/html/index.html', code_base + "/build-index.sh $SOURCE $TARGET gLucifer/Viewer/src/shaders")

# Build our source files.
# C++ sources
srcs = Glob(src_dir + '/*.cpp')
srcs += [src_dir + '/mongoose/mongoose.c']
srcs += [src_dir + '/miniz/miniz.c']
srcs += [src_dir + '/jpeg/jpge.cpp']
srcs += [src_dir + '/jpeg/jpgd.cpp']
srcs += Glob(src_dir + '/Main/*Viewer.cpp')
if platform.system() == 'Darwin':
  srcs += Glob(src_dir + '/Main/CocoaViewer.mm')
objs = env.SharedObject(srcs)
#build SQLite3 source (named object prevents clash)
sqlite3 = env.SharedObject('sqlite3-c', ['src/sqlite3/sqlite3.c'])

#Create renderer shared library
#Add pthreads & dl (required for sqlite3)
libs = ['pthread', 'dl'] + env.get('LIBS', [])
#libs = ['pthread', 'dl', 'z'] + env.get('LIBS', [])
l = env.SharedLibrary('lib/LavaVu', objs + sqlite3, LIBS=libs)

# Build LavaVu viewer (interactive version)
env['CPPDEFINES'] += cpp_defs
main = env.SharedObject('main', Glob(src_dir + '/Main/main.cpp'))
#Set search paths for libraries
env['RPATH'] += rpath
build_lib = os.path.join(env['build_dir'], "lib")
if build_lib[0] != "/":
   build_lib = "#" + build_lib
env['LIBPATH'] += [build_lib]
#Add the renderer library
libs = ['LavaVu'] + env.get('LIBS', [])
#Build the executable
env.Program('bin/LavaVu', main, LIBS=libs)

#Build as a shared library (Experimental)
main = env.SharedObject('lvlib', Glob(src_dir + '/Main/main.cpp'), CPPDEFINES=env['CPPDEFINES'])
