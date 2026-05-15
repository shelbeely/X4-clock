#!/usr/bin/env bash
# deploy_sd.sh — Copy all X4 apps and faces to an SD card mount point.
#
# Usage:
#   bash scripts/deploy_sd.sh /Volumes/SD
#   bash scripts/deploy_sd.sh /media/$USER/SD
#
# The script must be run from the repository root.

set -euo pipefail

if [ $# -ne 1 ]; then
  echo "Usage: $0 <SD_MOUNT_POINT>" >&2
  exit 1
fi

SD="$1"

if [ ! -d "$SD" ]; then
  echo "Error: '$SD' is not a directory or is not mounted." >&2
  exit 1
fi

# Create target directories
mkdir -p "$SD/apps"
mkdir -p "$SD/faces"

# Copy example apps
cp apps/clock.js            "$SD/apps/clock.js"
cp apps/hello.js            "$SD/apps/hello.js"
cp apps/stopwatch.js        "$SD/apps/stopwatch.js"
cp apps/countdown.js        "$SD/apps/countdown.js"
cp apps/battery_monitor.js  "$SD/apps/battery_monitor.js"

# Copy clock faces
cp apps/faces/digital.js     "$SD/faces/digital.js"
cp apps/faces/minimal.js     "$SD/faces/minimal.js"
cp apps/faces/seconds.js     "$SD/faces/seconds.js"
cp apps/faces/status.js      "$SD/faces/status.js"
cp apps/faces/roman.js       "$SD/faces/roman.js"
cp apps/faces/world_clock.js "$SD/faces/world_clock.js"

echo "Done. Files copied to $SD"
