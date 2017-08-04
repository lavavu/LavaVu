#Based on https://github.com/underworldcode/underworld2/tree/master/docs/development/docker/
#FROM debian:jessie
FROM ubuntu:16.04
LABEL maintainer="owen.kaluza@monash.edu"
LABEL repo="https://github.com/OKaluza/LavaVu"

# install things
RUN apt-get update -qq && \
    DEBIAN_FRONTEND=noninteractive apt-get install -yq --no-install-recommends \
        bash-completion \
        build-essential \
        git \
        python \
        python-dev \
        python-pip \
        xorg-dev \
        ssh \
        curl \
        libfreetype6-dev \
        libpng12-dev \
        libtiff-dev \
        libxft-dev \
        xvfb \
        freeglut3 \
        freeglut3-dev \
        libgl1-mesa-dri \
        libgl1-mesa-glx \
        libavcodec-dev \
        libavformat-dev \
        libavutil-dev \
        libswscale-dev \
        rsync \
        vim \
        less \
        xauth && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*
RUN pip install --upgrade pip
RUN pip install setuptools
RUN pip install \
        packaging \
        appdirs \
        numpy \
        jupyter \
        matplotlib \
        runipy \
        pillow \
        pyvirtualdisplay \
        scipy
RUN pip install 'ipython==4.2.0' --force-reinstall

# ^^^ Note we choose an older version of ipython because it's tooltips work better.
#     Also, the system six is too old, so we upgrade for the pip version


# Install Tini.. this is required because CMD (below) doesn't play nice with notebooks for some reason: https
#NOTE: If you are using Docker 1.13 or greater, Tini is included in Docker itself. This includes all versions of Docker CE. To enable Tini, just pass the --init flag to docker run.
RUN curl -L https://github.com/krallin/tini/releases/download/v0.6.0/tini > tini && \
    echo "d5ed732199c36a1189320e6c4859f0169e950692f451c03e7854243b95f4234b *tini" | sha256sum -c - && \
    mv tini /usr/local/bin/tini && \
chmod +x /usr/local/bin/tini

# script for xvfb-run.  all docker commands will effectively run under this via the entrypoint
RUN printf "#\041/bin/sh \n rm -f /tmp/.X99-lock && xvfb-run -s '-screen 0 1600x1200x16' \$@" >> /usr/local/bin/xvfbrun.sh && \
    chmod +x /usr/local/bin/xvfbrun.sh

# Add a notebook profile.
RUN mkdir -p -m 700 /root/.jupyter/ && \
    echo "c.NotebookApp.ip = '*'" >> /root/.jupyter/jupyter_notebook_config.py && \
    echo "c.NotebookApp.token = ''" >> /root/.jupyter/jupyter_notebook_config.py

WORKDIR /

# expose notebook port
EXPOSE 8888

# note we also use xvfb which is required for viz
ENTRYPOINT ["/usr/local/bin/tini", "--", "xvfbrun.sh"]

