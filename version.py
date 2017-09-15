#!/usr/bin/env python
import os

version = os.popen("git describe --tags --always").read()
version = version.rstrip('\r\n')

f = open('src/version.cpp', 'a+')
content = f.read()

if not version in content:
    f.close()
    f = open('src/version.cpp', 'w')
    print "Writing new version"
    f.write('#include "version.h"\nconst std::string version = "%s";\n' % version)
else:
    print "Version matches: " + version

f.close()

