# Version 4.1



# All this stuff is for me to build with scons and c++
sudo apt install build-essential -y
sudo apt install liballegro-acodec5-dev -y
sudo apt install liballegro-audio5-dev -y
sudo apt install liballegro-dialog5-dev -y
sudo apt install liballegro-image5-dev -y
sudo apt install liballegro-ttf5-dev -y
sudo apt install liballeggl4-dev -y
sudo apt install liballegro5-dev -y
sudo apt install libsdl-gfx1.2-dev -y
sudo apt install libsdl-image1.2-dev -y
sudo apt install libsdl-mixer1.2-dev -y
sudo apt install libsdl-sound1.2-dev -y
sudo apt install libsdl-ttf2.0-dev -y
sudo apt install libsdl1.2-dev -y
sudo apt install libsdl2-dev -y
sudo apt install libsdl2-gfx-dev -y
sudo apt install libsdl2-image-dev -y
sudo apt install libsdl2-mixer-dev -y
sudo apt install libsdl2-ttf-dev -y

# this is the end of Dev Stuff
echo "Installing all Dev tools SDL1, SDL2, Allegro4, Allegro5, Scons"
sudo apt install mc -y
sudo apt install scons -y
sudo apt install gedit -y
sudo apt install samba -y
sudo apt install htop -y
sudo apt install vim -y
sudo apt install remmina -y
sudo apt install remmina-common -y
sudo apt install remmina-plugin-rdp -y
sudo apt install git -y
sudo apt install clamav -y
sudo apt install clamav-daemon -y
sudo apt install clamtk -y
sudo apt install openssh-server -y

echo "Installing VNC Stuff"

#VNC Stuff
sudo apt-get install x11vnc net-tools -y
sudo echo "Enter VNC password"
sudo hmod 766 ~/.vnc/passwd
x11vnc -storepasswd 

sudo cp x11vnc.conf /etc/init/x11vnc.conf
sudo cp x11vnc.service /lib/systemd/system/x11vnc.service

cp .bash_aliases ~/.bash_aliases:wq

sudo systemctl daemon-reload
sudo systemctl enable x11vnc
sudo systemctl start x11vnc

echo "Installing and downloading latest Virus Defenitions"
sudo systemctl stop clamav-freshclam.service
sudo freshclam
sudo systemctl enable clamav-freshclam.service
sudo systemctl start clamav-freshclam.service

# For Linux Mint remove this line so you can use snap
sudo mv /etc/apt/preferences.d/nosnap.pref ~/Documents/nosnap.backup

sudo add-apt-repository ppa:bashtop-monitor/bashtop
sudo apt update
sudo apt install snapd
sudo apt install bashtop -y
chmod +x ~/linuxapps/*






