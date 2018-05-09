FROM ubuntu:18.04

COPY docker/setup-yael.sh /dep/setup-yael.sh
RUN /dep/setup-yael.sh
