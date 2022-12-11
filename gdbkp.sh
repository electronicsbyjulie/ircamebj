#!/bin/bash

sudo renice -n 10 $$

if pgrep -x "imgcomb" > /dev/null
then
    echo "ok" > /dev/null
else
    sudo rm /tmp/*.jpg
    sudo rm /tmp/*.bmp
fi

amonline=$(/sbin/ifconfig | grep "inet " | grep "cast ")
if [[ -z $amonline ]]; then exit; fi

# sudo pkill rclone

if pgrep -x "rclone" > /dev/null
then
    d="$(date '+%Y%m%d %H%M%S')"
    echo "$d rclone already running" | sudo tee -a /home/pi/bkp.log
    exit
fi

d="$(date '+%Y%m%d %H%M%S')"
echo "$d gdbkp begin" | sudo tee -a /home/pi/bkp.log

# source code
/home/pi/gdsrc.sh

sudo pkill rclone

rems=$(sudo rclone listremotes)

for remm in $rems
do
    Remote="";
    for morceau in $(echo $remm | tr ":" "\n")
    do
        Remote=$morceau
        break
    done
    
    echo "Sync'ing to $Remote..."
    
    # today's files
    d="$(date +%Y%m%d)"
    f="${d:0:7}"
    sudo rclone copy -v --include "$d.*.*" /home/pi/Pictures/ $Remote:computers/$HOSTNAME/pix/$d | sudo tee -a /home/pi/bkp.log
    # sudo rclone copy -v --include "*.bmp" /home/pi/Pictures/ $Remote:computers/$HOSTNAME/pix/$d | sudo tee -a /home/pi/bkp.log
    sudo rclone copy -v --include "$d.*.*" /home/pi/Videos/ $Remote:computers/$HOSTNAME/pix/$d | sudo tee -a /home/pi/bkp.log
done

lastgdb=$(cat /home/pi/lastgdb)
# echo "Lastgdb is $lastgdb"
# thisgdb=$(date +%Y%m%d)
# if [ $thisgdb -gt $lastgdb ]
# then
#     echo "yep!"
# fi


# the past month's files
for dayz in {1..30}
do
    # d="$(date -d 'yesterday 13:00' '+%Y%m%d')"
    d="$(date -d "$dayz days ago 13:00" '+%Y%m%d')"
    
    if [ $d -lt $lastgdb ]
    then
        echo "Try date $d is less than last run $lastgdb"
        break
    fi

    for remm in $rems
    do
        Remote="";
        for morceau in $(echo $remm | tr ":" "\n")
        do
            Remote=$morceau
            break
        done
        
        echo "Sync'ing to $Remote..."
        echo "Backing up $d"
        sudo rclone copy -v --include "$d.*.*" /home/pi/Pictures/ $Remote:computers/$HOSTNAME/pix/$d | sudo tee -a /home/pi/bkp.log
        sudo rclone copy -v --include "$d.*.*" /home/pi/Videos/ $Remote:computers/$HOSTNAME/pix/$d | sudo tee -a /home/pi/bkp.log
    done
done

lastgdb=$(date +%Y%m%d)
echo $lastgdb > /home/pi/lastgdb

d="$(date '+%Y%m%d %H%M%S')"
echo "$d gdbkp end" | sudo tee -a /home/pi/bkp.log

