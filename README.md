# ME56PS2 emulator
This software emulates ME56PS2 (PlayStation 2 compatible modem; manufactured by Omron) and performs communication via the Internet.

By using this software, game software that supports modem communication can be used via the Internet.

## Requirements
- Linux with USB Raw Gadget driver
  - Linux 4.14+ with [xairy/raw-gadget](https://github.com/xairy/raw-gadget), or Linux 5.7+
- Hardware with USB Device Controller
  - e.g. Raspberry Pi (Zero W/2 W, 4 Model B), NanoPi NEO2

## Usage
### Prepare
```shell
$ git clone https://github.com/msawahara/me56ps2-emulator.git
$ cd me56ps2-emulator
$ make rpi4  # for Raspberry Pi 4
```

If you use a different board than the Raspberry Pi 4, use below.
```shell
$ make rpi-zero  # for Raspberry Pi Zero W
$ make rpi-zero2  # for Raspberry Pi Zero 2 W
$ make nanopi-neo2  # for NanoPi NEO2, Lichee Zero
```

### Run
Requires root privileges to use the USB Raw Gadget.
Run as root user or use sudo if necessary.

#### Run as a server
When listening on port 10023
```shell
$ sudo ./me56ps2 -s 0.0.0.0 10023
```

In the game software, operate as the waiting side (or "RECEIVE SIDE") when running as a server.

#### Run as a client
When connecting to a server with address 203.0.113.1 and port 10023
```shell
$ sudo ./me56ps2 203.0.113.1 10023
```

In the game software, operate as the connecting side (or "SEND SIDE") when running as a client.

#### PTY mode
When no IP address and port are given, the emulator starts in PTY-only mode.
Dialing `ATD100` from the game software opens a PTY slave device (e.g. `/dev/pts/1`)
whose path is printed to the console. Any program (such as `minicom` or `socat`) can
then connect to that slave device to exchange data with the emulator.

```shell
$ sudo ./me56ps2
```

After the game dials `ATD100`, the slave device path is printed:

```
pty_dev: PTY slave device: /dev/pts/1
```

PTY mode can also be combined with socket mode by supplying ip_addr and port as usual:

```shell
$ sudo ./me56ps2 203.0.113.1 10023
```

In that case, `ATD100` uses PTY while any other `ATD` address uses the TCP socket.

## Notes
- "PlayStation" and "PS2" are registered trademarks of Sony Interactive Entertainment Inc.
- This software is NOT created by Sony Interactive Entertainment Inc. or OMRON SOCIAL SOLUTIONS CO., LTD., and has nothing to do with them. Please do not make inquiries about this software to each company.

## License
The MIT License is applied to this software.
