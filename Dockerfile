FROM ubuntu as base
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y git

FROM base as builder
RUN apt-get install -y curl zip unzip tar pkg-config wget
RUN apt-get install -y build-essential libssl-dev cmake git
RUN apt-get install -y python3 pip
RUN python3 -m pip install gutenbergpy

WORKDIR /benchmark

COPY ./scripts/installDependencies.sh ./scripts/downloadGutenberg.py ./scripts/
RUN chmod +x ./scripts/installDependencies.sh
# COPY ./vcpkg_overlay_ports ./vcpkg_overlay_ports
RUN ./scripts/installDependencies.sh

COPY . .
RUN chmod +x ./scripts/build.sh


FROM base as benchmark
WORKDIR /benchmark
COPY --from=builder /benchmark/build/benchmark ./
COPY --from=builder /benchmark/randomText ./randomText

CMD ["/benchmark/benchmark"]
