DEBIAN CONTROL STRUCTURE FOR THE GENERATION OF A DEB STANDARD PACKAGE

Instructions for making a Tahoma2D deb package:

1) Clone this repository.

2) Download https://github.com/tahoma2d/tahoma2d/releases/download/v1.2/Tahoma2D-linux.tar.gz

3) Unpack the official .appimage with ./Tahoma2D.AppImage --appimage-extract

4) Create a folder to save the content of the resulting squashfs-root inside the folder /deb/tahoma2d_1.2_amd64/opt/tahoma2d

5) Create the package with the command dpkg -b ./deb/tahoma2d_1.2_amd64 
