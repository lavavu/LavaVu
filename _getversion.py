#!/usr/bin/env python
import os
import setup

version = os.popen("git describe --tags --always 2> /dev/null").read()
if len(version) == 0:
    #Just report the current release version
    import setup
    version = setup.version
    #Still need to write if file doesn't exist
    if os.path.exists('src/version.cpp'):
        print(version)
        exit()

#Strip trailing newline
version = version.rstrip('\r\n')
#Replace first - with .
version = version.replace('-', '.', 1)

if __name__ == "__main__":
    setup.version = version
    setup.write_version()
