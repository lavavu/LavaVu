#!/usr/bin/env python
import os

version = os.popen("git describe --tags --always 2> /dev/null").read()
if len(version) == 0:
    #Just report the current release version
    import setup
    version = setup.version
    print(version)
    exit()

#Strip trailing newline
version = version.rstrip('\r\n')
#Replace first - with .
version = version.replace('-', '.', 1)

if __name__ == "__main__":
    # Only update version.cpp when not called via 'import'
    f = open('src/version.cpp', 'a+')
    content = f.read()

    if not version in content:
        f.close()
        f = open('src/version.cpp', 'w')
        print("Writing new version: " + version)
        f.write('#include "version.h"\nconst std::string version = "%s";\n' % version)
    else:
        print("Version matches: " + version)

    f.close()

