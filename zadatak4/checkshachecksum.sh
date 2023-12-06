#!/bin/bash

echo "Comparing checksums..."

snap_system=$(sha256sum /usr/bin/snap | cut -d ' ' -f 1)

snap_local=$(sha256sum ./snap | cut -d ' ' -f 1)

if [ "$snap_system" == "$snap_local" ]; then
  echo "SUCCESS"
else
  echo "FAILED"
  echo "original: $snap_system"
  echo "local:    $snap_local"
fi

