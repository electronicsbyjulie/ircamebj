# ircamebj
Software for ExJ infrared camera.
*** WARNING: *** This repository is a work in progress. It is not yet ready and probably won't work with your hardware.

# Camera Exterior

The 3D printable camera exterior can be found here: https://www.thingiverse.com/thing:4322005

# Parts List

To make a functioning IR cam using this design, the following components are required:

- Raspberry Pi 3B, 3B+, or 4, with micro SD with Raspbian installed
- Raspberry Pi NoIR v2 camera
- Pimoroni MLX90640 thermal camera breakout (note: the Adafruit breakout will NOT fit)
- Adafruit PiTFT 3.5" 480x320 capacitive touch screen (https://www.digikey.com/product-detail/en/adafruit-industries-llc/2441/1528-1348-ND/5356833)
- This specific USB battery pack: https://www.adafruit.com/product/1566
- 1000 μF capacitor
- This specific power switch: https://www.digikey.com/product-detail/en/zf-electronics/PRK22J5DBBNN/CH865-ND/1083858
- Green 525nm LED, diffused (for example https://www.digikey.com/product-detail/en/kingbright/WP7083ZGD-G/754-1890-ND/3084179)
- 470 ohm resistor
- Adafruit long 40 pin header (not the extra long) https://www.adafruit.com/product/2223
- USB-A male DIY connector
- Jumper wires to connect to pins of various components
- Roscolux #15 gel filter https://www.pnta.com/expendables/gels/roscolux/roscolux-15-deep-straw/?yotpo_token=26ffdcbcb4b3160b1ec2ede2fc4a3cf8a3b78870
- 7x #4 self tapping screw x 1/2" https://www.grainger.com/product/FABORY-4-x-1-2-Zinc-Plated-Case-Hardened-1MA79
- 10x M2 machine screw x 12mm https://www.grainger.com/product/FABORY-M2-0-40mm-Machine-Screw-54FR48
- 10x M2 hex nut https://www.grainger.com/product/FABORY-M2-0-40-Hex-Nut-26KR83
- Cotton cording

# Setup

If necessary, charge the battery (check it by pressing the button on top and peering through the battery-shaped hole in the front.
Four lights = fully charged). Connect the HDMI port to an HDMI monitor, and the USB ports to a keyboard and mouse, and power the device on.
If you have not done so yet, go through the Pi first run setup and set the locale, wifi settings, password, etc. Set the device name to
something unique to your wifi network. Disable overscan, pixel doubling, and screen blanking. If you have not enabled the camera interface,
do so now. Enable SSH, VNC, SPI, I2C, Serial port. Optionally you can set the desktop preferences: uncheck wastebasket and mounted disks,
and increase system font to about 24 pixels. In File Manager settings, select Open files with single click, Don't ask options on launch
exec, and Don't show available options for removable media. Open a terminal (Ctrl+Alt+T) and run the following commands:

```
cd ircamebj

sudo chmod +775 *.sh
```

Ensure that you have a working internet connection, and then run this command:

```
sudo rpi-update
```

Manually reboot the system. When it comes back up, open a terminal again and run:

```
sudo apt-get update

sudo apt-get install -y gedit rclone unclutter libgtk-3-dev libi2c-dev libjpeg-dev libpng-dev gpac

sudo apt-get install gstreamer1.0-plugins-ugly gstreamer1.0-tools gstreamer1.0-gl gstreamer1.0-gtk3
```

Once everything is finished, you can run `ircaminstall.sh`. It will compile the executables for the camera, then it will prompt you
for the PiTFT display setup; this is your touchscreen. When propmted, select option 4, then option 3, then wait, then answer N, Y, wait
again, and answer Y. The unit will then reboot into camera mode. Once the camera app comes up, first make sure that there is a NoIR
image shown and a thermal image overlaid on it. You can wave your hand in front of the unit to check the thermal sensor. If anything
is not working, your wiring might be incorrect. If everything works, press the Esc key and open a terminal with Ctrl+Alt+T. Place the
thermal sensor against something with a uniform temperature like the side of an empty cardboard or plastic box. (Don't rely on a wall
to be thermally uniform, as there may be conduits or pipes inside.) Run the following command and wait about 5 minutes for it to finish:

```
~/ctrlr thc
```

This stands for Thermal Calibrate. It takes one thousand consecutive thermal snapshots, averages them out, and generates a text file that
represents the necessary corrections for variances in the thermal sensor. After this, your thermal images will be calibrated, although you
may have to adjust its alignment to a known object such as a distant tree against the sky: press the W, A, S, Z keys until the optical and
thermal images align in the viewscreen. Pressing the view area takes a photo.

You can then delete the installer shell script and move the desktop icons around. You can now either reboot into the camera app, or in the
terminal type ./ctrlr and press enter. Disconnect the monitor and keyboard/mouse, and enjoy your new infrared camera!

The User Manual can be found here: https://docs.google.com/document/d/13N0HF1H3yCP-lyx0upOR_FPntTJiehe-wWmnJNSEhBQ/edit?usp=sharing
