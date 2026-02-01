#!/bin/bash
#
# Usage: releaseAll.sh

BASE_DIR=$(cd `dirname "$0"`; pwd)

./release.sh brazilian-portuguese 'Português(Brasil)'
if [ "$?" != "0" ]
then
   echo "ERROR ENCOUNTERED!!!!"
   exit 1
fi

./release.sh chinese 中文
if [ "$?" != "0" ]
then
   echo "ERROR ENCOUNTERED!!!!"
   exit 1
fi

./release.sh czech Čeština
if [ "$?" != "0" ]
then
   echo "ERROR ENCOUNTERED!!!!"
   exit 1
fi

./release.sh french Français
if [ "$?" != "0" ]
then
   echo "ERROR ENCOUNTERED!!!!"
   exit 1
fi

./release.sh german Deutsch
if [ "$?" != "0" ]
then
   echo "ERROR ENCOUNTERED!!!!"
   exit 1
fi

./release.sh italian Italiano
if [ "$?" != "0" ]
then
   echo "ERROR ENCOUNTERED!!!!"
   exit 1
fi

./release.sh japanese 日本語
if [ "$?" != "0" ]
then
   echo "ERROR ENCOUNTERED!!!!"
   exit 1
fi

./release.sh korean 한국어
if [ "$?" != "0" ]
then
   echo "ERROR ENCOUNTERED!!!!"
   exit 1
fi

./release.sh russian Русский
if [ "$?" != "0" ]
then
   echo "ERROR ENCOUNTERED!!!!"
   exit 1
fi

./release.sh spanish Español
if [ "$?" != "0" ]
then
   echo "ERROR ENCOUNTERED!!!!"
   exit 1
fi
