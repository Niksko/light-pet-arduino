FROM python:alpine3.6

RUN apk add --update make protobuf
# Symlink python to python3, since apparently there's a dependency on the python binary somewhere
RUN ln -s /usr/bin/python3 /usr/bin/python
RUN pip3 install protobuf
RUN mkdir /nanopb
RUN wget -P /nanopb http://jpa.kapsi.fi/nanopb/download/nanopb-0.3.9-linux-x86.tar.gz
RUN tar xvfz /nanopb/nanopb-0.3.9-linux-x86.tar.gz -C /nanopb

WORKDIR /nanopb/nanopb-0.3.9-linux-x86

ADD Makefile .
ADD configuration.json .
VOLUME ["/src", "/tools"]

CMD make