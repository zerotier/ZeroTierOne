# vim: ft=dockerfile

FROM ubuntu:20.04

RUN apt-get update -qq && apt-get install python2.7 -y

COPY apkg-tools.py /apkg-tools.py

COPY entrypoint.sh /entrypoint.sh
ENTRYPOINT ["/entrypoint.sh"]
