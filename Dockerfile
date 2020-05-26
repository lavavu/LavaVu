# A Dockerfile for LavaVu
# Using Ubuntu 20.04 we have a new Mesa build that supports LLVMpipe OSMesa
# No longer need to build our own mesa
# Test with:
# https://mybinder.org/v2/gh/lavavu/LavaVu/master?filepath=notebooks
FROM ubuntu:20.04

LABEL maintainer="owen.kaluza@monash.edu"
LABEL repo="https://github.com/lavavu/LavaVu"

# Install build dependencies
RUN apt-get update -qq && \
    DEBIAN_FRONTEND=noninteractive apt-get install -yq --no-install-recommends \
        build-essential \
        python3 \
        python3-pip \
        python3-setuptools \
        libpython3.8-dev \
        libpng-dev \
        libtiff-dev \
        mesa-utils \
        libglu1-mesa-dev \
        libosmesa6-dev \
        libavcodec-dev \
        libavformat-dev \
        libavutil-dev \
        libswscale-dev \
        zlib1g-dev

# Install python packages
RUN pip3 install --no-cache --upgrade pip \
        packaging \
        appdirs \
        numpy \
        jupyter \
        notebook \
        matplotlib \
        runipy \
        pillow \
        scipy \
        h5py \
        rise \
        jupyter-server-proxy \
        jupyterlab

# Create user with a home directory
ENV USER=jovyan UID=1000 HOME=/home/${USER}
RUN adduser --disabled-password \
    --gecos "Default user" \
    --uid ${UID} \
    ${USER}

# Make sure the contents of our repo are in ${HOME}
WORKDIR ${HOME}
COPY . ${HOME}
RUN chown -R ${UID} ${HOME}

# Build LavaVu
# delete some unnecessary files,
# trust included notebooks,
# add a notebook profile.
# setup RISE for notebook slideshows
USER ${USER}
RUN cd ~ && \
    LV_OSMESA=1 python3 setup.py install --user && \
    rm -fr tmp && \
    find notebooks -name \*.ipynb  -print0 | xargs -0 jupyter trust && \
    mkdir .jupyter && \
    echo "c.NotebookApp.ip = '0.0.0.0'" >> .jupyter/jupyter_notebook_config.py && \
    echo "c.NotebookApp.token = ''" >> .jupyter/jupyter_notebook_config.py && \
    jupyter nbextension install rise --user --py && \
    jupyter nbextension enable rise --user --py

# launch notebook
CMD ["jupyter", "notebook", "--ip='0.0.0.0'", "--NotebookApp.token='' ", "--no-browser"]

