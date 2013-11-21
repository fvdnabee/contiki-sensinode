#!/bin/bash
# Script to flash the nodes in the demo suite case, you can either specify
# which nodes you want to flash or flash all nodes by specifing an asterisk
# Note: this script requires bash 4.0

# Process command line arguments
if [[ $# -eq 0 ]] ; then
  echo "Usage: sudo ./flash.bash all|<bathroom,bedroom,driveway,kitchen-living,kitchen,living,toilet>"
  exit 1
fi

mote=()
if [ "$1" == "all" ]; then
  echo "test"
  mote=( "bathroom" "bedroom" "driveway" "kitchen-living" "kitchen" "living" "toilet")
else
  for var in "$@"
  do
  if [[ "$var" == "bathroom" || "$var" == "bedroom" || "$var" == "driveway" || "$var" == "kitchen-living" || "$var" == "kitchen" || "$var" == "living" || "$var" == "toilet" ]]; then
    mote+=("$var") # append element to array
  else
    echo "Invalid argument $var, exiting"
    exit 1
  fi
  done
fi

declare -A moteids
# Mote IDs of the different sensor boards in the suit case:
moteids["driveway"]=Z1RC2496
moteids["bedroom"]=Z1RC2209
moteids["bathroom"]=Z1RC2488
moteids["toilet"]=Z1RC2742
moteids["living"]=Z1RC2703
moteids["kitchen"]=Z1RC2055
moteids["kitchen-living"]=Z1RC2181

for var in "${mote[@]}"
do
  tmp=`sudo make TARGET=z1 motelist | grep -n ${moteids[$var]} | cut -f1 -d:`
  let MOTENR=tmp-4
  echo -e "\n\e[00;31mFlashing $var node on MOTE=$MOTENR (ID=${moteids[$var]})\e[00m"
  sudo make TARGET=z1 MOTE=$MOTENR demo-$var-server.upload
done

