#!/usr/bin/env python
def main():
    #Run interactive with args via python
    from . import lavavu
    import sys
    import platform
    lv = lavavu.Viewer(arglist=sys.argv[1:], interactive=True, hidden=False)

if __name__ == '__main__':
    main()

