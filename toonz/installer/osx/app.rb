#!/usr/bin/env ruby
# -*- coding: utf-8 -*-
require 'FileUtils'
if ARGV.size != 4 then
  puts "usage: ./app.rb [BUILD_DIR] [SRC_STUFF_DIR] [SRC_SCRIPTS_DIR] [VERSION(float)]"
  exit 1
end

def exec_with_assert(cmd)
    result = `#{cmd}`
    if $? != 0 then
        puts "Execution '#{cmd}' failed."
        exit 1
    end
    puts "Execution '#{cmd}' succeed."
end

# Constant group
BUILD_DIR = ARGV[0]
SRC_STUFF_DIR = ARGV[1]
SRC_SCRIPTS_DIR = ARGV[2] 
VERSION = ARGV[3]
VIRTUAL_ROOT = "#{BUILD_DIR}/VirtualRoot"
APP_BUNDLE = "#{BUILD_DIR}/Tahoma2D.app"
APP = "Applications"

PKG_ID = "io.github.tahoma2d"
PKG_TMP = "Tahoma2DBuild.pkg"
FINAL_PKG = "#{BUILD_DIR}/Tahoma2D-install-osx.pkg"

# Installation in VirtualRoot
# Delete existing and install
if File.exist? VIRTUAL_ROOT then
    exec_with_assert "rm -rf #{VIRTUAL_ROOT}"
end
exec_with_assert "mkdir -p #{VIRTUAL_ROOT}/#{APP}"
exec_with_assert "cp -r #{APP_BUNDLE} #{VIRTUAL_ROOT}/#{APP}"

# Generate the plist if it doesn't exist and apply the required changes
PKG_PLIST = "#{BUILD_DIR}/app.plist"
unless File.exist? PKG_PLIST then
    exec_with_assert "pkgbuild --root #{VIRTUAL_ROOT} --analyze #{PKG_PLIST}"
    exec_with_assert "gsed -i -e \"14i <key>BundlePreInstallScriptPath</key>\" #{PKG_PLIST}"
    exec_with_assert "gsed -i -e \"15i <string>preinstall-script.sh</string>\" #{PKG_PLIST}"
    exec_with_assert "gsed -i -e \"14i <key>BundlePostInstallScriptPath</key>\" #{PKG_PLIST}"
    exec_with_assert "gsed -i -e \"15i <string>postinstall-script.sh</string>\" #{PKG_PLIST}"
end

# Preparing stuff
if File.exist? "#{BUILD_DIR}/scripts" then
    exec_with_assert "rm -rf #{BUILD_DIR}/scripts"
end
exec_with_assert "cp -r #{SRC_SCRIPTS_DIR} #{BUILD_DIR}/."

# Delete the existing one, tar it and put it in scripts
if File.exist? "#{BUILD_DIR}/scripts/stuff.tar.bz2" then
    exec_with_assert "rm #{BUILD_DIR}/scripts/*.tar.bz2"
end
exec_with_assert "cp -r #{SRC_STUFF_DIR} #{BUILD_DIR}/stuff"
exec_with_assert "tar cjvf #{BUILD_DIR}/scripts/stuff.tar.bz2 -C #{BUILD_DIR} stuff"

# Generating a pkg using a plist
exec_with_assert "pkgbuild --root #{VIRTUAL_ROOT} --component-plist #{PKG_PLIST} --scripts #{BUILD_DIR}/scripts --identifier #{PKG_ID} --version #{VERSION} #{PKG_TMP}"

# Generate if distribution.xml does not exist
DIST_XML = "#{BUILD_DIR}/distribution.xml"
unless File.exists? DIST_XML then
    exec_with_assert "productbuild --synthesize --package #{PKG_TMP} #{DIST_XML}"
    exec_with_assert "gsed -i -e \"3i <title>Tahoma2D</title>\" #{DIST_XML}"
end

# Generate final pkg
exec_with_assert "productbuild --distribution #{DIST_XML} --package-path #{PKG_TMP} --resources . #{FINAL_PKG}"

# Remove temporary product
`rm #{PKG_TMP}`
`rm -rf #{BUILD_DIR}/stuff`
`rm -rf #{BUILD_DIR}/scripts`
`rm -rf #{VIRTUAL_ROOT}`
`rm #{DIST_XML}`
`rm #{PKG_PLIST}`
