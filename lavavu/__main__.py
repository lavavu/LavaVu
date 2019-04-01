#!/usr/bin/env python
def main():
    #Run interactive with args via python
    from . import lavavu
    import sys
    import platform
    if platform.system() == 'Darwin':
        #CocoaViewer is not capable of running in threaded mode
        lv = lavavu.Viewer(arglist=sys.argv[1:], interactive=True, hidden=False, port=0)
    else:
        lv = lavavu.Viewer(arglist=sys.argv[1:], interactive=True, hidden=False)

if __name__ == '__main__':
    main()

