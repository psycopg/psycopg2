# docker build -f scripts/cpychecker.Dockerfile --build-arg PYTHON=python2.7 -t cpychecker scripts/
# docker run --rm -ti --name cpychecker --volume `pwd`:/psycopg2 cpychecker

FROM ubuntu:18.04

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    ca-certificates \
    g++-6 \
    gcc \
    gcc-6-plugin-dev \
    git \
    libpq-dev \
    make \
    python-lxml \
    python-pygments \
    python-setuptools \
    python-six \
    python2.7 \
    python2.7-dev \
    python3-lxml \
    python3-pygments \
    python3-setuptools \
    python3-six \
    python3.6 \
    python3.6-dev \
    && rm -rf /var/lib/apt/lists/*

RUN git clone -b psycopg2 https://github.com/dvarrazzo/gcc-python-plugin.git
WORKDIR /gcc-python-plugin
RUN git fetch origin 08d0e644cadd0e6e2d2008ee286e2ea05326148c
RUN git reset --hard FETCH_HEAD

ARG PYTHON
ENV PYTHON=$PYTHON

ENV CC=gcc-6
RUN make PYTHON="$PYTHON" PYTHON_CONFIG="$PYTHON-config" plugin

# expected to be mounted
WORKDIR /psycopg2
ENV CC_FOR_CPYCHECKER=$CC
ENV CC=/gcc-python-plugin/gcc-with-cpychecker

CMD $PYTHON setup.py build
# CMD ["sleep", "infinity"]
