# yutools

```
make configure PREFIX=/path/to/install # default: /usr/local
make
make install
```

## yuproc

Read procfs and list process information, including open file descriptors with their modes, offsets, and paths.

```
Usage: ./build/release/yuproc [PIDS...]

Options:
  -t, --tree    Show process tree
  -r, --regular Show only regular files
  -p, --prune   Prune processes with no children or fds
  -c, --color   Indicate file offset as a color bar
  -l, --loop=N  Loop every N seconds
  -h, --help    Show this help message
```

If no PIDS are specified, list all user processes.

Example output:

```
$ yuproc -trpc
- pid: 123
  cmd: "sshd: staff@pts/0"
  children:
    - pid: 456
      cmd: "-bash"
      children:
        - pid: 789
          cmd: "xargs md5sum"
          children:
            - pid: 1234
              cmd: "md5sum largefile"
              fds:
                3: r_______________   1.1% (  11.0GiB /    1.0TiB) /path/to/largefile
```

- `pid`: Process ID
- `cmd`: Command line of the process
- `children`: List of child processes
- `fds`: List of opened files
    - `{n}: {mode} {offset_percent} % ({offset} / {size}) {path}`
        - `n` File descriptor number
        - `mode`: File mode
            - `r`: `O_RDONLY`
            - `w`: `O_WRONLY`
            - `c`: `O_CREAT`
            - `x`: `O_EXCL`
            - `T`: `O_NOCTTY`
            - `t`: `O_TRUNC`
            - `a`: `O_APPEND`
            - `n`: `O_NONBLOCK`
            - `s`: `O_DSYNC`
            - `f`: `FASYNC`
            - `d`: `O_DIRECT`
            - `l`: `O_LARGEFILE`
            - `Y`: `O_DIRECTORY`
            - `F`: `O_NOFOLLOW`
            - `A`: `O_NOATIME`
            - `e`: `O_CLOEXEC`
        - `offset_percent`: Percentage of the offset relative to the file size
            - If `-c` option is used, background color indicates the percentage of the offset
        - `offset`: Offset in bytes of the file
        - `size`: Size of the file
        - `path`: Path to the file
