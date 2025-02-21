# Linux Local Build Scripts

Interactive and multi-language scripts created solely to facilitate the local compilation of Tahoma2D on Debian-based distributions, Ubuntu, Fedora, Arch, and OpenSuse.

## About Cloning:

You don’t need to download the entire repository. Just download these only two scripts. linux_build.sh will handle downloading the repository for you.

## Usage

1) Run the `linux_depends.sh` script with root permissions to install the necessary dependencies:

   ```bash
   chmod +x ./linux_depends.sh
   sudo ./linux_depends.sh
   ```

2) Run the `linux_build.sh` script **without** root permissions:

   ```bash
   chmod +x ./linux_build.sh
   ./linux_build.sh
   ```

## Important Information

The first time `linux_build.sh` is executed, the script will clone the Git repository and store it in a folder named `tahoma2d`. In subsequent executions, if the `tahoma2d` folder has not been deleted, the script will ask whether you want to overwrite it.

Answering "no" is useful when you need to test changes in local builds, as it allows you to modify files and recompile them. Otherwise, the repository will be downloaded again, overwriting your local folder.


___________________

Copyright (c) 2019-2025 Charlie Martínez. All Rights Reserved.  
License: BSD 3-Clause "New" or "Revised" License
Authorized and unauthorized uses of the Quirinux trademark:  
See https://www.quirinux.org/aviso-legal  

Tahoma2D and OpenToonz are registered trademarks of their respective owners, and any other third-party projects that may be mentioned or affected by this code belong to their respective owners and follow their respective licenses.
