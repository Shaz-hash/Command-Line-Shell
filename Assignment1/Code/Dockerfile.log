FROM ubuntu:latest

RUN apt-get update && apt-get install -y build-essential
RUN apt-get install -y valgrind binutils libreadline-dev git python3 python3-pip

RUN pip3 install pexpect rich

RUN rm -f /usr/bin/echo
RUN rm -f /usr/bin/pwd
