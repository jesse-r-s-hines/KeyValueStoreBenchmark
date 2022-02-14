FROM ubuntu:21.04 as base
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get -y upgrade


FROM base as builder
RUN apt-get install -y curl zip unzip tar pkg-config wget
RUN apt-get install -y build-essential libssl-dev cmake git

WORKDIR /benchmark

COPY ./scripts/installDependencies.sh ./scripts/installDependencies.sh
COPY ./vcpkg_overlay_ports ./vcpkg_overlay_ports
RUN ./scripts/installDependencies.sh

COPY . .
RUN ./scripts/build.sh


FROM base as benchmark
WORKDIR /benchmark
COPY --from=builder /benchmark/build/benchmark ./
CMD ["/benchmark/benchmark"]
