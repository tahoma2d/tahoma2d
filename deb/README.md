DEBIAN CONTROL STRUCTURE FOR THE GENERATION OF A DEB STANDARD PACKAGE

Instructions for making a Tahoma2D deb package:

1) Unpack the official .appimage with ./Tahoma2D.AppImage --appimage-extract

2) Create a folder to save the content of the resulting squashfs-root inside the folder /deb/tahoma2d_1.2_amd64/opt/tahoma2d

3) Create the package with the command dpkg -b ./deb/tahoma2d_1.2_amd64 
