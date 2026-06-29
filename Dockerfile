FROM gcc:latest
WORKDIR /LogIngestor

# install cmake once (cached unless this line changes)
RUN apt-get update && apt-get install -y cmake && rm -rf /var/lib/apt/lists/*

# copy sources and build
COPY . .
RUN cmake -B build && cmake --build build

CMD ["/bin/bash"]