#Based on https://github.com/underworldcode/underworld2/tree/master/docs/development/docker/
FROM lavavu/base:v0.1
LABEL maintainer="owen.kaluza@monash.edu"
LABEL repo="https://github.com/OKaluza/LavaVu"

# set working directory to /root
WORKDIR /root

# setup environment
ENV PYTHONPATH $PYTHONPATH:/root/LavaVu

# get underworld, compile, delete some unnecessary files, trust notebooks, copy to workspace
RUN git clone --branch "master" --single-branch https://github.com/OKaluza/LavaVu && \
    cd LavaVu && \
    make LIBPNG=1 TIFF=1 VIDEO=1 -j4 && \
    rm -fr tmp
RUN find /root/LavaVu/notebooks -name \*.ipynb  -print0 | xargs -0 jupyter trust && \
    mkdir /workspace && \
    rsync -av /root/LavaVu/notebooks /workspace
RUN apt-get update -qq && \
    DEBIAN_FRONTEND=noninteractive apt-get install -yq --no-install-recommends \
    gdal-bin \
    python-gdal

# expose notebook port
EXPOSE 8888
# expose LavaVu port
EXPOSE 9999

# setup space for working in
VOLUME /workspace/user_data
WORKDIR /workspace

# launch notebook
CMD ["jupyter", "notebook", " --no-browser", "--allow-root", "--ip=0.0.0.0"]

