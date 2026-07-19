# Downloadd

downloadd is a Linux file system monitoring daemon written in C++ using the Linux `inotify` API. It recursively watches a directory tree and logs file system events such as file creation, deletion, modification, moves, and renames with timestamps.

## Features

- Recursive directory monitoring
- File and directory event detection
- Detects:
  - File/Directory creation
  - Deletion
  - Modification
  - Move events
  - Rename detection (using inotify cookies)
- Timestamped logging
- Modern C++17 implementation
- Uses `std::filesystem` for path handling

## Requirements

- Linux
- GCC or Clang with C++17 support
- `inotify` (included in the Linux kernel)

## Build

```bash
g++ -std=c++17 main.cpp -o d
```

## Run

```bash
./download-daemon
```

Modify the `pathToWatch` variable in `main.cpp` to monitor a different directory.

## Example Output

```
CREATE: file.txt
MODIFY: file.txt
DELETE: old.txt
RENAMED FROM: notes.txt -> notes_old.txt
```

## Log Format

```
2026-07-20 18:34:12 | [FILE] CREATE | Path: /home/user/Downloads/file.txt
```

## Future Improvements

- Store browser links for files downloaded
