# docker build  -f scripts/cpychecker.Dockerfile -t cpychecker scripts/
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
    && rm -rf /var/lib/apt/lists/*

RUN git clone -b psycopg2 https://github.com/dvarrazzo/gcc-python-plugin.git
WORKDIR /gcc-python-plugin
ENV PYTHON=python2.7
ENV PYTHON_CONFIG=python2.7-config
ENV CC=gcc-6
RUN make PYTHON_CONFIG=$PYTHON_CONFIG plugin

# expected to be mounted
WORKDIR /psycopg2
ENV CC_FOR_CPYCHECKER=$CC
ENV CC=/gcc-python-plugin/gcc-with-cpychecker

CMD ["python2.7", "setup.py", "build"]
# CMD ["sleep", "infinity"]
