# GPingTraceroute
my own implementation of Linux command ping and traceroute

## environment

gcc-7.1.0 + macOS 10.12.6

## usage

```shell
./GPingTraceroute -p [hostname/IP address] # for ping
./GPingTraceroute -t [hostname/IP address] # for traceroute
```

## recompile

put `GPingTraceroute.c` and `GPingTraceroute.h` in one same folder, and then enter that folder in terminal using `cd` command or other methods, then simply type the following command and you are good to go:

```shell
gcc GPingTraceroute.c -o GPingTraceroute
```
## todo

- [x] add traceroute implementation

- [x] translate comments into English🤣
