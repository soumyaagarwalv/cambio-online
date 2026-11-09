# Builds and runs the Cambio server (game WebSocket + web page on one port).
FROM ubuntu:24.04 AS build
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake libboost-dev && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY . .
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j --target cambio_server

FROM ubuntu:24.04
RUN apt-get update && apt-get install -y --no-install-recommends libstdc++6 && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY --from=build /app/build/cambio_server /app/cambio_server
COPY web /app/web
ENV PORT=8080
EXPOSE 8080
CMD ["/app/cambio_server"]
