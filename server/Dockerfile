# Сборка сервера с ядром игры. Из корня репозитория: docker build -f server/Dockerfile -t lanternfall-server .
FROM golang:1.24-bookworm AS build
RUN apt-get update -q && apt-get install -y -q cmake g++ && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY engine/src engine/src
COPY engine/CMakeLists.txt engine/CMakeLists.txt
COPY server server
RUN server/build_core.sh && cd server && go build -trimpath -o /out/lanternfall-server .

FROM debian:bookworm-slim
COPY --from=build /out/lanternfall-server /usr/local/bin/lanternfall-server
VOLUME /data
EXPOSE 8080
CMD ["lanternfall-server", "-addr", ":8080", "-db", "/data/lanternfall.db"]
