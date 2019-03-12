"""
LavaVu main module

"""

import sys
import os
sys.path.append(os.path.dirname(__file__))

from .lavavu import *

from vutils import is_ipython, is_notebook

__version__ = lavavu.version

