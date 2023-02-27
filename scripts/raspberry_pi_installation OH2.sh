# Even when named as linux shell script it is NOT! Copy and paste commands, but first read description and check if you do not want to modify!

# Next command exits the script in case someone tries to run as command
echo "This is not real bash script, read inside"
exit 0

# Get basic information about raspberry pi https://ozzmaker.com/check-raspberry-software-hardware-version-command-line/
cat /proc/cpuinfo #H W info
cat /etc/debian_version # Debian version
cat /etc/os-release # Debian release
uname -a # kernel version

# CREATE USER (not described below as usually created during image writing)
# CREATE AUTHORIZED KEY ~/.ssh/authorized_keys (not described below as usually created during image writing)

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
# Commands for raspberry pi 2B:
sudo apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv-keys 0x219BD9C9
sudo echo 'deb http://repos.azulsystems.com/debian stable main' | sudo tee /etc/apt/sources.list.d/zulu.list
sudo apt-get remove --purge oracle-java8-jdk openjdk-7-jre oracle-java7-jdk openjdk-8-jre
sudo apt-get update -qq
sudo apt-get install zulu-embedded-8
# Check commands
java -version
update-alternatives --config java

# ADD OPENHAB repo FOR RASPBERRY PI 2B OH2
wget -qO - 'https://bintray.com/user/downloadSubjectPublicKey?username=openhab' | sudo apt-key add -
sudo apt-get install apt-transport-https
echo 'deb https://dl.bintray.com/openhab/apt-repo2 stable main' | sudo tee /etc/apt/sources.list.d/openhab2.list
sudo apt-get update; sudo sudo apt-get install openhab2; sudo apt-get install openhab2-addons; sudo apt-get clean

# INSTALL OpenHab
sudo systemctl start openhab2.service
sudo systemctl status openhab2.service
sudo systemctl daemon-reload
sudo systemctl enable openhab2.service

# If problem with lock manager try to change ownership of files
sudo chown -R openhab:openhab /var/lib/openhab2
sudo chown -R openhab:openhab /usr/share/openhab2
sudo chown -R openhab:openhab /var/log/openhab2
sudo chown -R openhab:openhab /etc/openhab2

# Uninstall home builder (via paper UI)

# Routes
route delete 0.0.0.0
route add 192.168.2.0 MASK 255.255.255.0 192.168.2.1 METRIC 50 IF 17
route add 192.168.21.0 MASK 255.255.255.0 192.168.21.129 METRIC 50 IF 17
route add 0.0.0.0 MASK 0.0.0.0 192.168.1.1 METRIC 30 IF 13

# Set time zone
sudo timedatectl set-timezone Europe/Bratislava

# Disable user login with password

# Install goodwe support in python (need python 3.7 or newer)
# Based on source from https://github.com/marcelblijleven/goodwe and guide from https://community.openhab.org/t/connecting-goodwe-solar-panel-inverter-directly-to-openhab/130982
sudo pip3 install goodwe

# Regedit change on windows machines to allow smooth operation of GitHub
# \src\WindowsEnableTLS\EnableTLS.reg

# Update python version from source (to get sufficient python for goodwe)
python3 -V # to check actual version
cd /tmp
wget https://www.python.org/ftp/python/3.11.0/Python-3.11.0.tgz # download source
tar -xzvf Python-3.11.0.tgz # extract source
sudo apt-get update
sudo apt install build-essential zlib1g-dev libncurses5-dev libgdbm-dev libnss3-dev # install build tools
cd Python-3.11.0/
./configure --enable-optimizations
sudo make altinstall
cd /usr/bin
python3 --version
sudo rm python3
sudo ln -s /usr/local/bin/python3.11 python3
python3 --version
