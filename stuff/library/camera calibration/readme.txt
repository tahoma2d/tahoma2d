### How to calibrate camera

1. Prepare the checkerboard

    - Print out "checkerboard.tif" found in "tahomastuff/library/camera calibration".
    - It would be better to paste the checkerboard pattern on some flat panel like cardboard.

2. Capture the pattern

    - In the Stop Motion Controller's "Settings" tab, enable "Calibration" box
    - Click "Start calibration".
    - Take 10 snapshots of the checkerboard pattern in various positions and angles.
    - Once the capturing is done, calibration will be automatically applied whenever the "Calibration" box is checked.

3. Export & load calibration settings

    - Calibration settings will be saved under "tahomastuff/library/camera calibration" with name "[MACHINENAME]_[CAMERANAME]_[RESOLUTION].xml"
    - The settings file will be overwritten after finishing the new calibration.
    - You can export the file for backup, or load it afterwards.
