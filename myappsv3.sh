sudo apt install mc -y
sudo apt install build-essential -y
sudo apt install liballeggl4 -y
sudo apt install liballegro-acodec5-dev -y
sudo apt install liballegro-audio5-dev -y
sudo apt install liballegro-dialog5-dev -y
sudo apt install liballegro-image5-dev -y
sudo apt install liballegro-ttf5-dev -y
sudo apt install liballegro4-dev -y
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
sudo apt install clamav-daemon
sudo apt install clamtk -y

sudo systemctl stop clamav-freshclam.service
sudo freshclam


sudo systemctl enable clamav-freshclam.service
sudo systemctl start clamav-freshclam.service

echo "alias cls='clear'" > .bash_aliases
echo "alias ll='ls -l --color'" >> .bash_aliases
each "alias l='ls --color'" >> .bash_aliases

sudo add-apt-repository ppa:bashtop-monitor/bashtop
sudo apt update
sudo apt install bashtop






