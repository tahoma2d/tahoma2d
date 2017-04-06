#!/bin/bash
brew update
brew install glew lz4 lzo libusb
brew tap tcr/tcr
brew install clang-format
# revert to the previous version of the formula to get qt5.6.1-1
curl -O https://raw.githubusercontent.com/Homebrew/homebrew-core/fdfc724dd532345f5c6cdf47dc43e99654e6a5fd/Formula/qt5.rb
brew install ./qt5.rb
