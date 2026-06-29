# LogIngestor

A small Unix IPC log collector written in C++. Multiple producer processes
send log lines through a named pipe (FIFO), and a single server process
ingests them, stamps each line with a timestamp and the producer's PID, and
writes them to a rotating log file.

The project is a hands-on exploration of the systems-programming details that
a real log pipeline has to get right: non-blocking I/O, event-driven reads,
message framing over a byte stream, and graceful shutdown.

## Architecture

```
 producer 1 ┐
 producer 2 ┤── write() ──▶  /tmp/music_fifo.log  ──▶  epoll loop  ──▶  framer  ──▶  app.log
 producer N ┘                     (FIFO)              (log_server)    (reassemble)   (rotating)
```

Many short-lived **log_client** producers write framed lines into a single
FIFO. The long-lived **log_server** reads the raw byte stream, reassembles
complete messages, prefixes each with a timestamp and the producer's PID, and
appends them to `app.log`, rotating the file as it grows.

## Components

- **log_server** — Creates the FIFO, reads continuously using non-blocking
  `read()` driven by `epoll`, reassembles complete lines from the raw byte
  stream, prefixes each with a timestamp and producer PID, and writes to
  `app.log`. Rotates to `app.log.1` … `app.log.5` when the active file passes
  1 MB, keeping the last five. Shuts down cleanly on SIGINT/SIGTERM via
  `signalfd` and flushes the log on exit.

- **log_client** — Opens the FIFO for writing and emits synthetic log lines
  (`LEVEL=INFO msg="..." seq=123`) at a configurable rate. Flags: `--rate`
  (lines/sec), `--count` (total lines), `--level` (INFO/WARN/ERROR),
  `--pipe` (FIFO path).

## Key implementation details

- **Persistent reader, no EOF churn** — the server holds a keepalive write
  descriptor open so the FIFO never signals EOF, letting one long-lived
  `epoll` loop serve many short-lived producers.
- **Framing** — reads return arbitrary byte chunks, so an accumulator
  reassembles newline-delimited messages across read boundaries.
- **Atomic writes** — clients keep each line under `PIPE_BUF` (4 KB) and emit
  it in a single `write()` so concurrent producers don't interleave.
- **Event-driven shutdown** — SIGINT/SIGTERM are routed into the same `epoll`
  loop via `signalfd`, avoiding the EINTR/`SA_RESTART` pitfalls of classic
  signal handlers.

## Build & Run

The project is **Linux only** (uses `epoll` and `signalfd`). You can build it
natively on a Linux machine, or — on macOS/Windows — inside Docker.

### Option A — Native (Linux)

Requires a C++17 compiler and CMake.

```bash
cmake -B build && cmake --build build

./build/log_server &
./build/log_client --rate 100 --count 10000
```

### Option B — Docker (any host, including macOS/Windows)

The image compiles both binaries during the build, so you get a ready-to-run
container. This is the recommended route on macOS, where `epoll`/`signalfd`
aren't available natively.

```bash
# build the image (installs cmake, compiles log_server + log_client)
docker build -t logingestor .

# start a container and drop into a shell
docker run --rm -it --name logingestor logingestor

# inside the container:
./build/log_server &
./build/log_client --rate 100 --count 10000
```

To send traffic from a second terminal while the server runs, open another
shell into the same container:

```bash
docker exec -it logingestor bash
./build/log_client --rate 100 --count 5000 --level WARN
```

### One-command build & run

A helper script wraps the Docker build-and-run into a single command:

```bash
chmod +x run.sh   # once, after cloning
./run.sh
```

It builds the image and drops you into a container shell, ready to launch
`log_server` and `log_client`.

#### How the Docker build works

The `Dockerfile` is based on the `gcc` image (a Debian system with a C++
toolchain), installs CMake, copies the source in, and runs the same
`cmake -B build && cmake --build build` step the native path uses — so the
compiled binaries already exist inside the image. Because the build happens in
the image, the host doesn't need a compiler, CMake, or Linux: Docker provides
the Linux kernel the program's `epoll`/`signalfd` calls require.

## Verifying it works

Each client line carries an incrementing `seq=` field, so you can confirm no
messages were lost. After a run of `--count 10000`, the log should contain
10,000 ingested lines:

```bash
grep -c 'seq=' app.log          # should match the total --count sent
```

Run several clients at once to see PID-tagged interleaving from multiple
producers:

```bash
./build/log_client --rate 100 --count 5000 --level INFO  &
./build/log_client --rate 100 --count 5000 --level WARN  &
./build/log_client --rate 100 --count 5000 --level ERROR &
```

Trigger rotation by sending enough volume to push `app.log` past 1 MB, then
check the rotated files:

```bash
ls -lh app.log app.log.*        # app.log.1 … app.log.5, oldest dropped
```

## Project layout

```
.
├── LogServer/      # log_server sources (server.h, server.cpp, messageframer.*)
├── LogClient/      # log_client sources (client.h, client.cpp)
├── CMakeLists.txt  # builds both binaries
├── Dockerfile      # gcc + cmake build image
├── run.sh          # one-command Docker build & run
└── README.md
```

## Requirements

- Linux kernel (for `epoll` and `signalfd`), or Docker on any host
- C++17 compiler (GCC 8+ / Clang 7+)
- CMake 3.x

## License

MIT