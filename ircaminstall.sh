#!/bin/bash

# These have to be done manually
# sudo apt-get install -y realvnc-vnc-server
# sudo apt-get install -y gedit
# sudo apt-get install -y rclone
# sudo apt-get install -y unclutter
# sudo apt-get install -y libgtk-3-dev
# sudo apt-get install -y libi2c-dev
# sudo apt-get install -y libjpeg-dev
# sudo apt-get install -y libpng-dev

cp -R Desktop/* ~/Desktop/

piv=$(cat /proc/cpuinfo | grep Revision)
if [[ ${piv:11:6} < "a020d3" ]]
then 
	sed -i 's/\-lbcm2835//g' makefile
fi

make

echo "kneads utdape" > ~/srcmd5

ln -s /home/pi/ctrlr /home/pi/Desktop/ctrlr
echo "sudo pkill raspi" > /home/pi/Desktop/kill_overlay.sh
# echo "cd ~\n~/swupd.sh" > /home/pi/Desktop/software update.sh

sudo chmod +x *.sh
sudo chmod +x kill_overlay.sh
sudo cp *.sh ~/
sudo cp *.png ~/

# Not certain of this step yet.
# sudo cp boot/config.txt /boot/config.txt

#write out current crontab
sudo crontab -l > /tmp/mycron
#echo new cron into cron file
echo "06,13,19,28,31,35,43,49,58 * * * * /bin/bash ~/gdbkp.sh >> ~/cron.log" >> /tmp/mycron
echo "04 * * * * tail -n 3000 ~/cron.log > ~/cron.log" >> /tmp/mycron
echo "08 * * * * tail -n 3000 ~/act.log > ~/act.log" >> /tmp/mycron
echo "16 * * * * tail -n 3000 ~/bkp.log > ~/bkp.log" >> /tmp/mycron
#install new cron file
sudo crontab /tmp/mycron
rm /tmp/mycron

echo "tmpfs /tmp tmpfs nodev,nosuid,size=128M 0 0" | sudo tee -a /etc/fstab

# This has to be done manually
# ~/ctrlr thc

echo "@xset s off" | sudo tee -a /etc/xdg/lxsession/LXDE-pi/autostart
echo "@xset -dpms" | sudo tee -a /etc/xdg/lxsession/LXDE-pi/autostart
echo "@xset s noblank" | sudo tee -a /etc/xdg/lxsession/LXDE-pi/autostart
echo "@/bin/bash /home/pi/c1.sh" | sudo tee -a /etc/xdg/lxsession/LXDE-pi/autostart

# sudo mkdir -p /home/pi/.config/lxsession/LXDE
# cat /etc/xdg/lxsession/LXDE-pi/autostart | sudo tee /home/pi/.config/lxsession/LXDE/autostart
# echo "@/bin/bash /home/pi/c1.sh" | sudo tee -a /home/pi/.config/lxsession/LXDE/autostart

cd ~
# wget https://raw.githubusercontent.com/adafruit/Raspberry-Pi-Installer-Scripts/master/adafruit-pitft.sh
chmod +x adafruit-pitft.sh
sudo ./adafruit-pitft.sh

sudo reboot



