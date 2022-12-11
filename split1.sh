IN="bla@some.com;john@home.com"

devices=$(ls /media/pi | tr ";" "\n")

for usbd in $devices
do
    echo "$usbd"
    destdir = "/media/pi/$usbd/infrared"
    
    mkdir -p $destdir
    
    cp -n "/home/pi/Pictures/*.png" $destdir
    
    break
done

