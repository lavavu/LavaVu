echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
echo PYTHON $PYTHON
echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
ls $PREFIX/lib
export LV_LIB_DIRS=$PREFIX/lib
export LV_INC_DIRS=$PREFIX/include
#export LV_GLFW=1
$PYTHON setup.py install -vv
echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
#WHEEL=$(python setup.py wheelname)
#echo Using wheel: $WHEEL
#python -m pip install -vv https://pypi.debian.net/lavavu/${WHEEL}

