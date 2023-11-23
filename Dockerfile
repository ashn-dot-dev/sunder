# Build the linux/amd64 Sunder image.
# $ docker buildx build --platform=linux/amd64 --tag sunder .
#
# Create a Sunder development environment.
# $ docker run --rm --interactive --tty --volume "$(pwd)":/sunder sunder
#
# Remove the Sunder image.
# $ docker image rm sunder

FROM debian:stable-slim

ARG DEBIAN_FRONTEND=noninteractive
RUN apt update -y && apt upgrade -y && apt update -y
RUN apt install -y build-essential clang clang-format
RUN mkdir -p /sunder

WORKDIR /sunder
VOLUME ["/sunder"]

CMD bash -c 'SUNDER_HOME=/sunder; source /sunder/env; exec bash'
