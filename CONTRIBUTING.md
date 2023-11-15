# How to contribute

This document describes some points about the contribution process for Tahoma2D.

## Pull-requests

I love any kind of contributions, such as fixing typos and code refactoring.
If you fixed or added something useful to Tahoma2D, please send pull-requests to me.
I will first review the request, then I'll accept it, add comments for rework, or decline it.

### Workflow

1. `fork` Tahoma2D to your GitHub account from `tahoma2d/tahoma2d`.
  - (use the `fork` button at the https://github.com/tahoma2d/tahoma2d)
2. `clone` the repository.
  - `git clone git@github.com:your-github-account/tahoma2d.git`
  - `git remote add upstream https://github.com/tahoma2d/tahoma2d.git`, additionally.
3. modify the codes.
  - `git checkout -b your-branch-name`
    - `your-branch-name` is a name of your modifications, for example,
      `fix/fatal-bugs`, `feature/new-useful-gui` and so on.
  - fix codes, then test them.
  - `git commit` them with good commit messages.
4. `pull` the latest changes from the `master` branch of the upstream.
  - `git pull upstream master` or `git pull --rebase upstream master`.
  - apply [clang-format](http://clang.llvm.org/docs/ClangFormat.html) with `toonz/sources/.clang-format`.
    - `cd toonz/sources`
    - `./beautification.sh` or `beautification.bat`.
  - `git commit` them.
  - `git push origin your-branch-name`.
5. make a pull request.

## Bugs

If you find bugs, please report details about them using [issues](https://github.com/tahoma2d/tahoma2d/issues).
Please include information needed to reproduce the bug, including the operating system 
and information directly relating to the issue. Links to screen captures of what is 
observed on screen or video of specific steps to produce the problem are very helpful.  
Then we will try to reproduce the bugs and fix them.
Unfortunately, bugs can sometimes only be reproduced in your own environment, 
so we cannot reproduce them.
If you believe you can fix the bug, please submit a pull request.

## Features

If you had an idea about a new feature, please implement it and send a pull request to us.
If you cannot implement the feature, please make a request here.


## Translations

Translation source (`.ts`) files for Tahoma2D GUI are located in `toonz/sources/translations`.
If you create new `.ts` files for your language or update an existing one,
please send us those modifications as pull-requests.
[Qt Linguist](http://doc.qt.io/qt-5.6/linguist-translators.html) is useful for translating them.

Please send us Qt message (`.qm`) files with `.ts` files if you can make the following modifications.

Tahoma2D uses `.qm` files generated from `.ts` files.
You can generate `.qm` files by using [Qt Linguist](http://doc.qt.io/qt-5.6/linguist-translators.html).
Please locate generated `.qm` files in `stuff/config/loc`.
