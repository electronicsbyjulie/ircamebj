#!/bin/bash

while [ 1 ]
do
    output=$(eval $"sudo /home/pi/thermcam2")
    clear
    echo -e "$output"
    
    # output=$(eval $"sudo /home/pi/thermcamh 1")
    # clear
    # echo -e "$output"
    
    sleep 0.2
done

