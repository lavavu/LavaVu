"""
LavaVu python interface: vis utils
"""
import sys

def is_ipython():
    """
    Detects if running within IPython environment

    Returns
    -------
    boolean
        True if IPython detected
        Does not necessarrily indicate running within a browser / notebook context
    """
    try:
        if __IPYTHON__:
            return True
        else:
            return False
    except:
        return False

def is_notebook():
    """
    Detects if running within an interactive IPython notebook environment

    Returns
    -------
    boolean
        True if IPython detected and browser/notebook display capability detected
    """
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


