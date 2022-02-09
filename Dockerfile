FROM ubuntu:21.04 as base
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get -y upgrade


FROM base as builder
RUN apt-get install -y curl zip unzip tar pkg-config wget
RUN apt-get install -y build-essential libssl-dev cmake git

WORKDIR /benchmark

COPY ./install_dependencies.sh install_dependencies.sh
RUN ./install_dependencies.sh

COPY . .
RUN ./build.sh


FROM base as benchmark
WORKDIR /benchmark
COPY --from=builder /benchmark/build/benchmark ./
CMD ["/benchmark/benchmark"]
