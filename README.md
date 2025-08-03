# Linux System Programming Project 2: ssu-cleanupd
`ssu-cleanupd` is a Linux daemon program that **automatically monitors user-specified directories** and **organizes new files by extension**.


## Implementation
- Interactive shell with built-in commands: `add`, `modify`, `remove`, `show`, `help`, and `exit`.
- Daemon processes are created using following the standard 7 steps of daemonization.
- Each daemon:
  - Monitors a target directory periodically (`time_interval`, default 10 seconds).
  - Copies newly added files into an output directory.
  - Categorizes files by extension.
  - Writes detailed logs (`ssu_cleanupd.log`) and config files (`ssu_cleanupd.config`).
- Configuration and logs are safely accessed using `fcntl()` file locking to support concurrent daemon access.
- Enforces constraints to avoid nested or overlapping monitored paths.
- Operates only within the user's `$HOME` directory.


## Commands
### `add <DIR_PATH> [OPTIONS]`
Start monitoring a directory. Supported options:
- `-d <output_path>`: output directory (default: `<DIR_PATH>_arranged`)
- `-i <time_interval>`: monitoring interval in seconds (default: 10 seconds)
- `-l <max_log_lines>`: max lines in the log file
- `-x <exclude_paths>`: skip specific subdirectories
- `-e <extensions>`: only include specific extensions Handles name conflicts
- `-m <mode>`: duplicate handling mode (`1`: newest, `2`: oldest, `3`: ignore)


### `modify <DIR_PATH> [OPTIONS]`
Change the configuration of an existing daemon using the same options as `add`.


### `remove <DIR_PATH>`
Terminate the daemon monitoring the given directory.


### `show`
Interactively lists active daemons and lets the user inspect config and log details for each.


### `help`
Display command usage information.


### `exit`
Terminate the shell interface.


## File Structure
- `~/.ssu_cleanupd/current_daemon_list`: Tracks active daemons.
- `<DIR_PATH>/ssu_cleanupd.config`: Stores daemon settings.
- `<DIR_PATH>/ssu_cleanupd.log`: Logs of file operations.


## Build
- Compile
```
$ make
```
- Execute
```
$ ./ssu_cleanupd
20232372>
20232372> exit
$
```
- Remove
```
$ make clean
```
