echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
echo PYTHON $PYTHON
echo SP_DIR $SP_DIR
echo STDLIB_DIR $STDLIB_DIR
#export LV_LIB_DIRS=${MESA_PATH/lib/x86_64-linux-gnu
#export LV_INC_DIRS=${MESA_PATH}/include python setup.py install
echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
python setup.py install -vv
echo "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
#WHEEL=$(python setup.py wheelname)
#echo Using wheel: $WHEEL
#python -m pip install -vv https://pypi.debian.net/lavavu/${WHEEL}

