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
        python3 \
        python3-pip

# Install python packages
RUN pip3 install --no-cache \
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
        RISE \
        jupyter-server-proxy \
        jupyterlab \
        lavavu-osmesa

# Create user with a home directory
ENV USER=jovyan UID=1000
ENV HOME=/home/${USER}
RUN adduser --disabled-password \
    --gecos "Default user" \
    --uid ${UID} \
    ${USER}

COPY ./notebooks ${HOME}
RUN chown -R jovyan:jovyan ${HOME}

# Copy notebooks to ${HOME}
USER ${USER}
WORKDIR ${HOME}

# delete some unnecessary files,
# trust included notebooks,
# add a notebook profile.
RUN cd ~ && \
    find . -name \*.ipynb  -print0 | xargs -0 jupyter trust && \
    mkdir .jupyter && \
    echo "c.ServerApp.ip = '0.0.0.0'" >> .jupyter/jupyter_notebook_config.py && \
    echo "c.ServerApp.token = ''" >> .jupyter/jupyter_notebook_config.py

# launch notebook
CMD ["jupyter", "notebook", "--ip=0.0.0.0", "--NotebookApp.token=''", "--no-browser"]

