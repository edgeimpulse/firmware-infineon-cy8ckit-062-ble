FROM --platform=linux/x86_64 ubuntu:20.04

WORKDIR /app

ARG DEBIAN_FRONTEND=noninteractive

ENV MODUSTOOLBOX_INSTALLER=ModusToolbox_2.4.0.5972-linux-install.tar.gz

RUN apt update && \
    apt install -y make perl wget \
    libssl-dev libkrb5-dev libglib2.0-dev libgl1-mesa-glx

RUN cd /tmp/ && \
    wget --progress=bar:force:noscroll https://cdn.edgeimpulse.com/build-system/${MODUSTOOLBOX_INSTALLER} && \
    tar -xf /tmp/${MODUSTOOLBOX_INSTALLER} --directory /root && \
    rm /tmp/${MODUSTOOLBOX_INSTALLER}

CMD make build
