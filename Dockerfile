FROM ubuntu:20.04

RUN apt-get update && apt-get install -y \
    build-essential \
    gcc \
    make \
    man-db \
    libpthread-stubs0-dev \
    libssl-dev \
    libpcap-dev \
    netcat \
    vim \
    && rm -rf /var/lib/apt/lists/*

COPY . /usr/src/myapp

WORKDIR /usr/src/myapp

RUN make clean && make

CMD ["./myapp"]
