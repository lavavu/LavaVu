"""
LavaVu python interface: vis utils
"""
import sys

def is_ipython():
    try:
        if __IPYTHON__:
            return True
        else:
            return False
    except:
        return False

def is_notebook():
    if 'IPython' not in sys.modules:
        # IPython hasn't been imported, definitely not
        return False
    try:
        from IPython import get_ipython
        from IPython.display import display,Image,HTML
    except:
        return False
    # check for `kernel` attribute on the IPython instance
    return getattr(get_ipython(), 'kernel', None) is not None


