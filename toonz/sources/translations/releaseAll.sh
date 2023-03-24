#!/bin/bash
#
# Usage: releaseAll.sh

BASE_DIR=$(cd `dirname "$0"`; pwd)

./release.sh brazilian-portuguese 'Português(Brasil)'
./release.sh chinese 中文
./release.sh czech Čeština
./release.sh french Français
./release.sh german Deutsch
./release.sh italian Italiano
./release.sh japanese 日本語
./release.sh korean 한국어
./release.sh russian Русский
./release.sh spanish Español
