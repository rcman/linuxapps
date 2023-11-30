# Version 4.3



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
sudo apt install gkrellm
echo "Installing VNC Stuff"

#VNC Stuff
sudo apt-get -y remove vino
sudo apt-get -y install x11vnc
sudo mkdir /etc/x11vnc

sudo echo "Enter VNC password"
sudo chmod 766 ~/.vnc/passwd

sudo x11vnc --storepasswd /etc/x11vnc/vncpwd
sudo cp x11vnc.service /lib/systemd/system/x11vnc.service

cp .bash_aliases ~/.bash_aliases

sudo systemctl daemon-reload
sudo systemctl enable x11vnc.service
sudo systemctl start x11vnc.service

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
sudo chmod +x ~/linuxapps/*.sh

#setup Xscreensaver

gsettings set org.cinnamon.desktop.session idle-delay 0 && gsettings set org.cinnamon.desktop.screensaver idle-activation-enabled false && gsettings set org.cinnamon.desktop.screensaver lock-enabled false
sudo apt install xscreensaver xscreensaver-data-extra xscreensaver-gl xscreensaver-gl-extra
sudo ln -sf /usr/bin/xscreensaver-command /usr/bin/cinnamon-screensaver-command

echo "Make sure you setup startup apps. Name: xscreensaver    command: xscreensaver     Comment: whatever you want     STart Delay: 0    amd Save"



