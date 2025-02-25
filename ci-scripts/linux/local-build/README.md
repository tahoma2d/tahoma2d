# Linux Local Build Scripts

Interactive and multi-language scripts designed to simplify the local compilation of **Tahoma2D**.

- ✅ **Tested on**: Debian, Devuan, Ubuntu-based distributions.  
- ⚠️ **Experimental on**: Fedora, Arch, and OpenSuse.  

---

## 📌 Important Information

🔹 This is an **interactive script** designed **only for local builds on Linux**.  
🔹 If you need **unattended scripts** for automated CI/CD processes, please check the root scripts linux folder:  

📂 `tahoma2d/ci-scripts/linux`  

## Native Location of These Scripts  

The native location of these scripts is:  
`tahoma2d/local-build/tahoma2d/ci-scripts/linux/local-build`  

### Script Behavior  

The scripts **linux_depends.sh** and **linux_build.sh** are aware of this location and will look for the **tahoma2d** folder three levels up from their own location (`/../../..`).  

If, for any reason, you run these scripts from a different location, they will attempt to find the **tahoma2d** folder at the same level as their current directory.  

### Downloading the Repository  

If **linux_depends.sh** and **linux_build.sh** do not find the **tahoma2d** folder, they will attempt to download it from the URL specified in:  
`/local-build/assets/var (REPO_URL)`.  

Default path being:  
[https://github.com/tahoma2d/tahoma2d.git](https://github.com/tahoma2d/tahoma2d.git).

## 🚀 Usage

### 0️⃣ Grant Permissions

```
chmod +x ./linux_depends.sh sudo ./linux_depends.sh
```
### 1️⃣ Install dependencies  
Run the `linux_depends.sh` script **with root permissions** to install the necessary dependencies:

```
./linux_depends.hs
```

#### ⚙️ Available Parameters

If after build any functionality fails (e.g., webcam usage), you can try compiling with linux_depends.sh the required libraries locally by using parameters.

The `linux_depends.sh` script accepts the following parameters:

| Parameter | Description |
|-----------|-------------|
| `-o`, `--opencv` | Locally compile and install **OpenCV**. |
| `-f`, `--ffmpeg` | Locally compile and install **FFmpeg**. |
| `-g`, `--gphoto` | Locally compile and install **GPhoto**. |
| `-m`, `--mpaint` | Locally compile and install **MyPaint**. |
| `-c`, `--clear` | **Deletes check files and the local** `tahoma2d` **repository directory**  (if it is at the same level as the script). |

##### 🔹 Example Usage:

```
./linux_build.sh -o -f
```

### 2️⃣ Build Tahoma2D  
Run the `linux_build.sh` script **without root permissions**:

```
./linux_build.hs
```
---

## 📜 License & Credits  

**© 2019-2025 Charlie Martínez** – All Rights Reserved.  
**License**: BSD 3-Clause "New" or "Revised" License.  

✅ **Authorized and unauthorized uses of the Quirinux trademark**:  
🔗 [See Legal Quirinux Notice](https://www.quirinux.org/aviso-legal)  

**Tahoma2D and OpenToonz** are registered trademarks of their respective owners.  
Any third-party projects mentioned or affected by this code belong to their respective owners and follow their respective licenses.  
