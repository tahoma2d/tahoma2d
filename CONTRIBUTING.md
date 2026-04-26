# Contributing to Ztoryc

Ztoryc is currently in active development by Matitanimata.

## Bug reports

If you find a bug, please open a [GitHub issue](https://github.com/matitanimata/ztoryc/issues)
with as much detail as possible: operating system, steps to reproduce, screenshots or video.

## Pull requests

Contributions are welcome. If you fixed or improved something, open a pull request.
We will review it and either accept it, request changes, or decline with an explanation.

### Workflow

1. Fork the repository on GitHub.
2. Clone your fork and create a branch:
git checkout -b fix/your-fix-name 
3. Make your changes and test them.
4. Apply clang-format using `toonz/sources/.clang-format`:
cd toonz/sources
./beautification.sh 5. Commit with a clear message and push your branch.
6. Open a pull request against the `main` branch.

## Upstream contributions

Ztoryc is a fork of [Tahoma2D](https://github.com/tahoma2d/tahoma2d).
If your fix is relevant to the base engine and not Ztoryc-specific,
consider also submitting it upstream to Tahoma2D or OpenToonz.

## Translations

Translation `.ts` files are in `toonz/sources/translations`.
Use [Qt Linguist](http://doc.qt.io/qt-5.6/linguist-translators.html) to edit them.
Submit updated `.ts` and generated `.qm` files via pull request.
