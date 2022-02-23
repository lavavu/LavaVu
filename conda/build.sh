#!/bin/bash
echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
echo PYTHON $PYTHON
echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
ls $PREFIX/lib
export LV_LIB_DIRS=$PREFIX/lib:$MESA_DIR/lib/x86_64-linux-gnu
export LV_INC_DIRS=$PREFIX/include:$MESA_DIR/include
#export LV_GLFW=1
#$PYTHON setup.py install -vv
#$PYTHON -m pip install --no-deps .
echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
#WHEEL=$(python setup.py wheelname)
#echo Using wheel: $WHEEL
#python -m pip install -vv https://pypi.debian.net/lavavu/${WHEEL}

# install using pip from the whl file
$PYTHON -m pip install lavavu

