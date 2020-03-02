# IP Setup (/etc/dhcpcd.conf or network/interfaces)
# Static IP address for eth0
interface eth0
#static ip_address=192.168.1.222/24 # FINAL configuration
static ip_address=192.168.1.222/26 # Temporary solution due to IP conflict
#static routers=192.168.1.1 # FINAL configuration
static routers=192.168.1.193 # Temporary solution due to IP conflict
#static domain_name_servers=192.168.1.1 # FINAL configuration
static domain_name_servers=192.168.1.193 # Temporary solution due to IP conflict

# ZULU JAVA
sudo apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv-keys 0x219BD9C9
sudo echo 'deb http://repos.azulsystems.com/debian stable main' | sudo tee /etc/apt/sources.list.d/zulu.list
sudo apt-get remove --purge oracle-java8-jdk openjdk-7-jre oracle-java7-jdk openjdk-8-jre
sudo apt-get update -qq
sudo apt-get install zulu-embedded-8
java -version
update-alternatives --config java

# ADD OPENHAB repo
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

# 