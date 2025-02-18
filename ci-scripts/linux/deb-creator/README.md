# Tahoma2D .deb Creator

## Usage with Parameters

The script can be executed with the following parameters for non-interactive mode:

```bash
./deb_creator.sh -p <package_version> -v <program_version> -t <deb_template_path> -c <compressed_file>
```

Example:

```bash
./deb_creator.sh -p 1.5.1 -v 1.5.1 -t /path/to/deb-template -c /path/to/Tahoma2D-linux.tar.gz
```

### Parameter Details:

- `-p` or `--package`: Defines the package version (e.g., `1.5.1`).
- `-v` or `--version`: Defines the program version (e.g., `1.5.1`).
- `-t` or `--template`: Specifies the path to the `deb-template` directory.
- `-c` or `--compressed`: Provides the path to the compressed Tahoma2D archive (`.tar.gz` or `.zip`).*

* This is the file that contains the .AppImage and folders, not the source code.
  
![example](https://github.com/user-attachments/assets/9d1e1b8d-4270-459e-aef9-741192a2701e)

## Description

`deb_creator.sh` is a Bash script designed to generate a functional `.deb` package from an `.AppImage` file using the compressed archive of Tahoma2D, which can be downloaded from the developer's GitHub profile: [https://github.com/tahoma2d/tahoma2d](https://github.com/tahoma2d/tahoma2d).  

This script was originally developed for Quirinux 2.0 but is compatible with Debian and its derivatives.

The main goal is to facilitate the integration of Tahoma2D into Debian-based systems, allowing for a simpler and more consistent installation process within the system's package management.

## Features

- Converts an `.AppImage` file or a compressed Tahoma2D archive into an installable `.deb` package.
- Supports multiple languages (Spanish, English, Italian, German, French, Galician, and Portuguese).
- Checks for required files before creating the package.
- Automatically generates sequential package versions.
- Restores the control file in case of an interruption.
- Provides detailed status and error messages for easier debugging.

### Differences from the original AppImage

- Manages permissions to avoid `777`.
- Unlike the AppImage, the package is stored in the home directory of each user running it, enhancing security.
- Like the AppImage, it integrates the necessary libraries to avoid dependency issues.

## Requirements

- A Debian-based operating system (e.g., Ubuntu, Devuan, Quirinux, etc.).
- Bash installed (usually available by default on most Linux distributions).
- A compressed Tahoma2D archive containing the `.AppImage`.
- A `deb-template` folder with the structure of a Debian package.

## Usage

1. Store into **deb-creator** folder the compressed Linux file containing the AppImage, downloaded from the official Tahoma2D GitHub releases. For example:  
   [https://github.com/tahoma2d/tahoma2d/releases/download/v1.5.1/Tahoma2D-linux.tar.gz](https://github.com/tahoma2d/tahoma2d/releases/download/v1.5.1/Tahoma2D-linux.tar.gz)  **(Do not download the source code.)**
2. Ensure the script has execution permissions:
   ```bash
   chmod +x deb_creator.sh
   ```
3. Run the script:
   ```bash
   ./deb_creator.sh
   ```
4. If the script does not find the `deb-template` folder, it will prompt you to provide the path manually.
5. You will be asked to select the `.AppImage` file or the compressed Tahoma2D archive.
6. The package version will be checked, and you will be prompted to keep or change it.
7. Once confirmed, the `.deb` package will be generated in the working directory.

## Notes

- The generated `.deb` package version is assigned automatically and may not match the program version exactly.
- If the process is interrupted, the original control file will be restored.
- The compressed archive format must be compatible (e.g., `.tar.gz`, `.zip`).

## License

This project is licensed under the BSD 3-Clause License. For more details, see the `LICENSE` file.

## Credits

Developed by **Charlie Martínez®** ([cmartinez@quirinux.org](mailto:cmartinez@quirinux.org)) for Quirinux GNU/Linux.

## Disclaimers

- **Tahoma2D and OpenToonz** are registered trademarks of their respective owners.
- This script is not officially affiliated with the developers of Tahoma2D or OpenToonz.
- For more information regarding authorized use of the Quirinux brand, visit: [Legal Notice](https://www.quirinux.org/aviso-legal)
