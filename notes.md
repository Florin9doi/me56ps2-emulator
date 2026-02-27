- Raspberry PI Zero 2W
```shell
sudo apt install git iptables-persistent

# compile raw-gadget
git clone https://github.com/xairy/raw-gadget.git ~/raw-gadget
make -C ~/raw-gadget/raw_gadget
# sudo insmod ~/raw-gadget/raw_gadget/raw_gadget.ko

# install raw_gadget
sudo mkdir -p /lib/modules/$(uname -r)/extra
sudo cp ~/raw-gadget/raw_gadget/raw_gadget.ko /lib/modules/$(uname -r)/extra/
sudo depmod -a
sudo modprobe raw_gadget
grep -qxF raw_gadget /etc/modules || echo raw_gadget | sudo tee -a /etc/modules

# register client credentials
echo "user * pass *" | sudo tee /etc/ppp/pap-secrets

# compile me56ps2-emulator
git clone https://github.com/msawahara/me56ps2-emulator.git ~/me56ps2-emulator
make -C ~/me56ps2-emulator rpi-zero2
sudo ~/me56ps2-emulator/me56ps2 -s 0.0.0.0 10023
```
- Manual setup (deprecated)
```shell
sudo pppd /dev/pts/3 115200 local nodetach debug 10.0.0.1:10.0.0.2 ms-dns 8.8.8.8 proxyarp

sudo sysctl -w net.ipv4.ip_forward=1

sudo iptables -F
sudo iptables -t nat -A POSTROUTING -o wlan0 -j MASQUERADE
sudo iptables -A FORWARD -i ppp+ -o wlan0 -j ACCEPT
sudo iptables -A FORWARD -i wlan0 -o ppp+ -m state --state RELATED,ESTABLISHED -j ACCEPT
sudo iptables -L -v -n
```
- Debug/misc
```shell
sudo modprobe usbmon

sudo modprobe ftdi_sio
echo 0590 001a | sudo tee /sys/bus/usb-serial/drivers/ftdi_sio/new_id

rsync -avzh ~/Desktop/modem/me56ps2-emulator/ florin@Florin-RPI.local:~/me56ps2-emulator
ssh florin@Florin-RPI.local "make -C ~/me56ps2-emulator rpi-zero2"

echo -e -n "AT\r\n" > /dev/ttyUSB0
```
