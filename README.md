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
sudo apt install git iptables-persistent dnsmasq

# compile raw-gadget
git clone https://github.com/xairy/raw-gadget.git ~/raw-gadget
make -C ~/raw-gadget/raw_gadget
# sudo insmod ~/raw-gadget/raw_gadget/raw_gadget.ko

# install raw_gadget
sudo mkdir -p /lib/modules/$(uname -r)/extra
sudo cp ~/raw-gadget/raw_gadget/raw_gadget.ko /lib/modules/$(uname -r)/extra/
sudo depmod -a
sudo modprobe raw_gadget
grep -qxF dwc2 /etc/modules || echo dwc2 | sudo tee -a /etc/modules
grep -qxF raw_gadget /etc/modules || echo raw_gadget | sudo tee -a /etc/modules

# register client credentials
echo "user * pass *"     | sudo tee    /etc/ppp/pap-secrets
echo "mmducp1 * cpcm1 *" | sudo tee -a /etc/ppp/pap-secrets
echo "mmducp2 * cpcm2 *" | sudo tee -a /etc/ppp/pap-secrets
echo "mmducp3 * cpcm3 *" | sudo tee -a /etc/ppp/pap-secrets
echo "mmducp4 * cpcm4 *" | sudo tee -a /etc/ppp/pap-secrets
echo "mmducp5 * cpcm5 *" | sudo tee -a /etc/ppp/pap-secrets

# DNS redirect
echo "interface=ppp0"                       | sudo tee    /etc/dnsmasq.conf
echo "bind-dynamic"                         | sudo tee -a /etc/dnsmasq.conf
echo "address=/ca1201.mmcp6/192.168.100.14" | sudo tee -a /etc/dnsmasq.conf
echo "address=/ca1202.mmcp6/192.168.100.14" | sudo tee -a /etc/dnsmasq.conf
echo "address=/ca1203.mmcp6/192.168.100.14" | sudo tee -a /etc/dnsmasq.conf
echo "address=/ca1204.mmcp6/192.168.100.14" | sudo tee -a /etc/dnsmasq.conf
sudo systemctl restart dnsmasq

# compile me56ps2-emulator
git clone https://github.com/msawahara/me56ps2-emulator.git ~/me56ps2-emulator
make -C ~/me56ps2-emulator
sudo ~/me56ps2-emulator/me56ps2 -s 0.0.0.0 10023
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

## PC drivers
- Omron Viaggio (ME56PS2)
  - Windows: https://web.archive.org/web/20050309011724/http://www.omron.co.jp/ped-j/download/me56ps2ws/me56ps2ws.htm
  - Linux: `sudo modprobe ftdi_sio ; echo 0590 001a | sudo tee /sys/bus/usb-serial/drivers/ftdi_sio/new_id`
- Suntac OnlineStation (MS56KPS2)
  - Windows: https://www.sun-denshi.co.jp/sc/suntac/download/modem/ms56kps2/firmup.html
  - Linux 2.4: https://x68trap.no.coocan.jp/linux/suntacucp.html
  - BSD: https://github.com/openbsd/src/blob/ee05ec4a571e94e17ba0246deda48b72b7b89aef/sys/dev/usb/uvscom.c
- Conexant SmartSCM-USB (P2GATE DFML-560/P2 / ASC-1605M56 / PV-PS200 / IGM-UB56PS2C)
  - Linux: https://github.com/Florin9doi/linux-smartscm-usb :wink:
- Multi-Tech MultiMobile (MT5634MU)
  - Windows: https://google.com/search?q=%225634USB.INF%22
  - Linux: OOTB

## Notes
- "PlayStation" and "PS2" are registered trademarks of Sony Interactive Entertainment Inc.
- This software is NOT created by Sony Interactive Entertainment Inc. or OMRON SOCIAL SOLUTIONS CO., LTD., and has nothing to do with them. Please do not make inquiries about this software to each company.

## License
The MIT License is applied to this software.
