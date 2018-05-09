FROM ubuntu:18.04

COPY . /yael
WORKDIR /yael
RUN ./docker/setup-yael.sh
WORKDIR /yael/build
