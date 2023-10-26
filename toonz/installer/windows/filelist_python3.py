# list up files in program and stuff
# for python version 3.x (converted with 2to3.py from filelist.py)

import os
import codecs
import sys

currentPath=""

if len(sys.argv) > 1:
    currentPath=sys.argv[1] + "\\"

fout = codecs.open("files.iss", "w", "utf_8_sig")

sourceDir=currentPath + 'program'

for path, dirs, files in os.walk(sourceDir):
    for file in files:
        print("""Source: "%s"; DestDir: "{app}%s"; Flags: ignoreversion""" % (
            os.path.join(path, file),
            path[len(sourceDir):]), file=fout)

print()

sourceDir=currentPath + 'stuff'

for path, dirs, files in os.walk(sourceDir):
    for file in files:
        print("""Source: "%s"; DestDir: "{code:GetStuffDir}%s"; Flags: uninsneveruninstall; Check: IsOverwiteStuffCheckBoxChecked""" % (
            os.path.join(path, file),
            path[len(sourceDir):]), file=fout)
        print("""Source: "%s"; DestDir: "{code:GetStuffDir}%s"; Flags: onlyifdoesntexist uninsneveruninstall; Check: not IsOverwiteStuffCheckBoxChecked""" % (
            os.path.join(path, file),
            path[len(sourceDir):]), file=fout)

fout.close()
