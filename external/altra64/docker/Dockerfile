
# to build and test:
# docker build -t altra64-dev . && docker run --rm -v "$(pwd):/build" -it altra64-dev

# to use to compile altra64 (or other n64 stuff I guess)
# docker run --rm -v "$(pwd):/build" altra64-dev make

#FROM ubuntu:18.04
FROM ubuntu:16.04

RUN mkdir /build

COPY setup-linux.sh /usr/bin/setup-linux.sh

# run it then delete all build artifacts
RUN /usr/bin/setup-linux.sh && rm -rf /root/*

ENV N64_INST=/usr/local/libdragon
ENV PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/local/libdragon/bin

VOLUME [ "/build" ]

WORKDIR /build

CMD ["bash"]
