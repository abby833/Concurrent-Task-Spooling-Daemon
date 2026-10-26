# Request Spooler (`spoold`)

A lightweight daemon that watches a directory for job requests, dispatches them to parallel worker processes, and reports their execution progress atomically. Built as a C systems programming project exploring processes (`fork`), inter-process communication (pipes, FIFOs), signal handling, and atomic filesystem operations.

##  Directory Structure
When `spoold` starts, it automatically creates the following workspace in the current directory:
```text
spool/
├── inbox/          # New incoming requests (<id>.request)
├── work/           # Requests currently claimed by the daemon
├── status/         # Live progress reports (<id>.request.<pid>.status)
└── control.fifo    # Named pipe for runtime configuration
```

## Compile the project:
gcc -Wall -Wextra spoold.c status.c -o spoold

## Start the daemon:
./spoold

## Graceful Shutdown & Signals
The daemon intercepts Ctrl+C (SIGINT) and SIGTERM to shut down safely without corrupting data or leaving zombie processes.

    First Ctrl+C (Graceful Drain): Prints a draining notice, stops accepting new requests from the inbox/, allows all currently running children to finish naturally, and then exits.

    Second Ctrl+C (Force Abort): If pressed again while draining, the daemon immediately sends SIGTERM to all running children, marks their status as ABORTED, reaps them, and exits.
