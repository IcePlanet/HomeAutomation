# Even when named as linux shell script it is NOT! Copy and paste commands, but first read description and check if you do not want to modify!

# Next command exits the script in case someone tries to run as command
echo "This is not real bash script, read inside"
exit 0

# Get basic information about raspberry pi https://ozzmaker.com/check-raspberry-software-hardware-version-command-line/
cat /proc/cpuinfo #H W info
cat /etc/debian_version # Debian version
cat /etc/os-release # Debian release
uname -a # kernel version
vcgencmd measure_temp | sed  -r 's/temp/GPU/g' &&cpu=$(</sys/class/thermal/thermal_zone0/temp) && echo "CPU=$((cpu/1000))'C" # temperature of CPU and GPU


# CREATE USER (not described below as usually created during image writing)
# CREATE AUTHORIZED KEY ~/.ssh/authorized_keys (not described below as usually created during image writing) authorized keys must have rw for user as only rights

# Disable IPv6
sudo vi /etc/sysctl.conf
# IPv6 DISABLE add following lines to that file
net.ipv6.conf.all.disable_ipv6 = 1
net.ipv6.conf.default.disable_ipv6 = 1
net.ipv6.conf.lo.disable_ipv6 = 1

# IP Setup (/etc/dhcpcd.conf or /etc/network/interfaces - this one used)
# Static IP address for eth0
interface eth0
auto eth0
iface eth0 inet static
address 192.168.32.133
netmask 255.255.255.192
broadcast 192.168.32.191
network 192.168.32.128
gateway 192.168.32.129

# ZULU JAVA
# Commands for raspberry pi 3B
sudo apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv-keys 0xB1998361219BD9C9
curl -O https://cdn.azul.com/zulu/bin/zulu-repo_1.0.0-3_all.deb
sudo apt-get install ./zulu-repo_1.0.0-3_all.deb
sudo apt-get update
sudo apt-get install zulu18
# Check commands
java -version
update-alternatives --config java

# System update/upgrade
sudo apt update && sudo apt upgrade

# ADD OPENHAB repo FOR RASPBERRY PI 3B and OH3
curl -fsSL "https://openhab.jfrog.io/artifactory/api/gpg/key/public" | gpg --dearmor > openhab.gpg
sudo mkdir /usr/share/keyrings
sudo mv openhab.gpg /usr/share/keyrings
sudo chmod u=rw,g=r,o=r /usr/share/keyrings/openhab.gpg
sudo chown -R root:root /usr/share/keyrings/openhab.gpg
sudo apt-get install apt-transport-https
echo 'deb [signed-by=/usr/share/keyrings/openhab.gpg] https://openhab.jfrog.io/artifactory/openhab-linuxpkg stable main' | sudo tee /etc/apt/sources.list.d/openhab.list
sudo apt-get update && sudo apt-get install openhab && sudo apt-get install openhab-addons

# Now we have user openhab and can add your local user for checks, deployment etc... to openhab group
sudo usermod -a -G openhab <youruser>

# START/CHECK/STOP OpenHab
sudo systemctl start openhab.service
sudo systemctl status openhab.service
sudo systemctl daemon-reload
sudo systemctl enable openhab.service
sudo systemctl stop openhab.service

# OPENHAB version
openhab-cli info

# If problem with lock manager try to change ownership of files
sudo chown -R openhab:openhab /var/lib/openhab
sudo chown -R openhab:openhab /usr/share/openhab
sudo chown -R openhab:openhab /var/log/openhab
sudo chown -R openhab:openhab /etc/openhab

# Uninstall home builder (via paper UI)

# WINDOWS Routes for local testing your network setup might differ !!!
route delete 0.0.0.0
route add 192.168.2.0 MASK 255.255.255.0 192.168.2.1 METRIC 50 IF 17
route add 192.168.21.0 MASK 255.255.255.0 192.168.21.129 METRIC 50 IF 17
route add 0.0.0.0 MASK 0.0.0.0 192.168.1.1 METRIC 30 IF 13
# route add 192.168.54.217 MASK 255.255.255.255 192.168.54.193 IF 14

# Regedit change on WINDOWS machines to allow smooth operation of GitHub
# \src\WindowsEnableTLS\EnableTLS.reg

# Set time zone
sudo timedatectl set-timezone Europe/Bratislava

# Create working directory
mkdir /usr/src/HomeAutomation_Novaky
cd /usr/src
sudo chgrp openhab HomeAutomation_Novaky/
sudo chmod 775 HomeAutomation_Novaky/
cd /etc/openhab
sudo chmod 775 *

# Create persistence directory
sudo mkdir /var/local/openhab_simple_persistence
cd /var/local/
sudo chown openhab:openhab openhab_simple_persistence
sudo chmod 755 openhab_simple_persistence 

# Install persistence file
sudo cp /usr/src/HomeAutomation_Novaky/simple_persistence.sh /usr/local/bin/
sudo chown openhab:openhab /usr/local/bin/simple_persistence.sh
sudo chmod 744 /usr/local/bin/simple_persistence.sh

# Install enqueue script
sudo cp /usr/src/HomeAutomation_Novaky/enqueue.sh /usr/local/bin/
sudo chown openhab:openhab /usr/local/bin/enqueue.sh
sudo chmod 744 /usr/local/bin/enqueue.sh

# Rsyslog installation for server
sudo cp /usr/src/HomeAutomation_Novaky/home_automation.conf /etc/rsyslog.d/ #rsyslog records
sudo cp /usr/src/HomeAutomation_Novaky/home_automation /etc/logrotate.d/ #logrotate

# Day night switching script
sudo cp /usr/src/HomeAutomation_Novaky/day_night_switch.sh /usr/local/bin/day_night_switch.sh 
sudo chown openhab:openhab /usr/local/bin/day_night_switch.sh 
sudo chmod 754 /usr/local/bin/day_night_switch.sh 

# Cron for sleeping times (not necessary if you use switch in application)
sudo cp /usr/src/HomeAutomation_Novaky/home_automation.cron /etc/cron.d/home_automation
sudo chmod 644 /etc/cron.d/home_automation
sudo systemctl restart cron.service

# Allow SPI headers
# In file /boot/config.txt uncomment text dtparam=spi=on
sudo shutdown -r now
ls -ltra /dev/ | grep spi # you should now see spidev

# Install wiring pi from http://wiringpi.com/news/ does not work on 64bit systems
# sudo apt install libc6
# cd /tmp
# wget https://project-downloads.drogon.net/wiringpi-latest.deb
# sudo dpkg -i wiringpi-latest.deb

# Install wiringpi from https://github.com/WiringPi/WiringPi.git
cd
git clone https://github.com/WiringPi/WiringPi.git
cd WiringPi
./build
# TODO there are warnings about analog read, maybe it will not work 100%, but for purpose of this project analogue read is not needed

# Add /usr/local/lib to /etc/ld.so.conf 
# sudo vi /etc/ld.so.conf
# There is include of subdir /etc/ld.so.conf.d and libc.conf contains /usr/local/lib, no need to add
sudo ldconfig # just to be sure

# Build RF24
cd
wget https://raw.githubusercontent.com/nRF24/.github/main/installer/install.sh
chmod +x install.sh
./install.sh # select wiring pi
# Do not try to build pyrf24 package as raspberry pi has not sufficient memory

# Build and install the server code, server will not start correctly if there is no wiring done for 422 and 2.1 radios!
# sudo systemctl stop home_automation_server.service # Only necessary if server already running
cd /usr/src/HomeAutomation_Novaky
make pimain
#sudo cp /usr/local/bin/home_automation.server /usr/local/bin/home_automation.server.ok.`date "+%Y%m%d%H%M%S"` # Make backup of old binary if you want
sudo cp /usr/src/HomeAutomation_Novaky/pimain /usr/local/bin/home_automation.server
sudo chown root:root /usr/local/bin/home_automation.server
sudo chmod 750 /usr/local/bin/home_automation.server

# Create service
sudo cp /usr/src/HomeAutomation_Novaky/home_automation_server.service /usr/lib/systemd/system/home_automation_server.service 
sudo chown root:root /usr/lib/systemd/system/home_automation_server.service 
sudo chmod 640 /usr/lib/systemd/system/home_automation_server.service 
sudo systemctl start home_automation_server.service
sudo systemctl status home_automation_server.service
sudo systemctl enable home_automation_server # If it has started correctly you can enable

# Login to IP:8080 and set admin username/password
# As admin install exec binding
# As admin install RegEx Transformation
# All commands must be whitelisted
sudo vi /etc/openhab/misc/exec.whitelist # and add following lines
  /usr/local/bin/enqueue.sh %2$s
  /usr/local/bin/simple_persistence.sh %2$s
  echo %2$s


# Disable user login with password
sudo passwd -l root # Lock root account, no login with password, before this check that your <working user account> is in sudoers
sudo passswd <working user account> # Have strong password for your user account

# PYTHON INSTALL/ VERIFY
python3 -V
sudo apt install python3-pip
sudo pip3 install goodwe
python3 -V
python --version
# Based on source from https://github.com/marcelblijleven/goodwe and guide from https://community.openhab.org/t/connecting-goodwe-solar-panel-inverter-directly-to-openhab/130982

# Change network interfaces, first set fixed IP for wired interface, then disable WiFi
sudo vi /etc/network/interfaces.d/home_automation_fixed_ip # and paste follwoing code:
  # Home automation server
  # Fixed IP set

  auto eth0
  iface eth0 inet static
  address 192.168.32.133
  netmask 255.255.255.192
  broadcast 192.168.32.191
  network 192.168.32.128
  gateway 192.168.32.129
sudo vi /etc/resolv.conf # Here you need to change DNS server
sudo systemctl disable dhcpcd.service ; sudo systemctl enable networking # Disable DHCP client and to make sure enable networking afterwards
sudo vi /etc/profile.d/wifi-check.sh # and add exit 0 as first line (2nd line of file) after opening bracket (1st line of file) to disable wifi check as we will be complatelly disabling wifi in next step
sudo vi /boot/config.txt # and add following lines to the file
  # Disable WiFi and Bluetooth
  dtoverlay=disable-wifi
  dtoverlay=disable-bt