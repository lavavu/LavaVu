FROM python:3.7-slim

LABEL maintainer="owen.kaluza@monash.edu"
LABEL repo="https://github.com/lavavu/LavaVu"

# install things
RUN apt-get update -qq && \
    DEBIAN_FRONTEND=noninteractive apt-get install -yq --no-install-recommends \
        bash-completion \
        build-essential \
        ssh \
        curl \
        libpng-dev \
        libtiff-dev \
        mesa-utils \
        libavcodec-dev \
        libavformat-dev \
        libavutil-dev \
        libswscale-dev \
        zlib1g-dev \
        rsync \
        wget \
        xauth

#Add deb-src repo
RUN echo "deb-src http://deb.debian.org/debian buster main" >> /etc/apt/sources.list

# install mesa deps
RUN apt-get update -qq && \
    DEBIAN_FRONTEND=noninteractive apt-get -y build-dep mesa

# Add Tini
ENV TINI_VERSION v0.18.0
ADD https://github.com/krallin/tini/releases/download/${TINI_VERSION}/tini /tini
RUN chmod +x /tini

# install the notebook package
RUN pip install --no-cache --upgrade pip && \
    pip install --no-cache notebook

RUN pip install setuptools
RUN pip install \
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

#Setup RISE for notebook slideshows
RUN jupyter-nbextension install rise --py --sys-prefix
RUN jupyter nbextension enable rise --py --sys-prefix

ENV NB_USER jovyan
ENV NB_UID 1000
ENV HOME /home/${NB_USER}

# create user with a home directory
ARG NB_USER
ARG NB_UID
ENV USER ${NB_USER}
ENV HOME /home/${NB_USER}

RUN adduser --disabled-password \
    --gecos "Default user" \
    --uid ${NB_UID} \
    ${NB_USER}
WORKDIR ${HOME}

# Make sure the contents of our repo are in ${HOME}
COPY . ${HOME}
USER root
RUN chown -R ${NB_UID} ${HOME}
USER ${NB_USER}

#Build OSMesa
RUN wget ftp://ftp.freedesktop.org/pub/mesa/mesa-19.1.7.tar.xz && \
    tar xvf mesa-19.1.7.tar.xz  && \
    rm mesa-19.1.7.tar.xz && \
    cd mesa-19.1.7 && \
    meson build/ -Dosmesa=gallium -Dgallium-drivers=swrast -Ddri-drivers= -Dvulkan-drivers= -Degl=false -Dgbm=false -Dgles1=false -Dgles2=false && \
    meson configure build/ -Dprefix=${PWD}/build/install && \
    ninja -C build/ -j5 && \
    ninja -C build/ install

#Build LavaVu
# setup environment
ENV PYTHONPATH $PYTHONPATH:${HOME}

ENV LV_LIB_DIRS $HOME/mesa-19.1.7/build/install/lib/x86_64-linux-gnu/
ENV LV_INC_DIRS $HOME/mesa-19.1.7/build/install/include/

# Compile, delete some unnecessary files
RUN cd ~ && \
    make OSMESA=1 LIBPNG=1 TIFF=1 VIDEO=1 -j$(nproc) && \
    rm -fr tmp

#Trust included notebooks
RUN cd ~ && \
    find notebooks -name \*.ipynb  -print0 | xargs -0 jupyter trust

# Add a notebook profile.
RUN cd ~ && \
    mkdir .jupyter && \
    echo "c.NotebookApp.ip = '0.0.0.0'" >> .jupyter/jupyter_notebook_config.py && \
    echo "c.NotebookApp.token = ''" >> .jupyter/jupyter_notebook_config.py

ENTRYPOINT ["/tini", "--"]

# launch notebook
# CMD scripts/run-jupyter.sh
CMD ["jupyter", "notebook", "--ip='0.0.0.0'", "--NotebookApp.token='' ", "--no-browser"]

