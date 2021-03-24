#!/usr/bin/env python
def main():
    #Run interactive with args via python
    from . import lavavu
    import sys
    import platform
    lv = lavavu.Viewer(*sys.argv[1:])
    lv.interactive()

if __name__ == '__main__':
    main()

