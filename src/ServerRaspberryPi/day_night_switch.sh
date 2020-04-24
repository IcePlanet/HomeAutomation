#!/bin/bash
# This file is to be located in /usr/local/bin/ and chown to openhab sudo cp /usr/src/HomeAutomation_Novaky/day_night_switch.sh /usr/local/bin/day_night_switch.sh
# Change owner: sudo chown openhab:openhab /usr/local/bin/day_night_switch.sh
# Change rights: sudo chmod 754 /usr/local/bin/day_night_switch.sh

usage() {
    # Description of usage
    echo "Usage: $0 -S <value>"
    echo
    echo "Simple script to set day or night, used by cron records"
    echo
    echo "Options:"
    echo "    -h                    Displays this text"
    echo "    -S                    Set <value> of day/night possible values are ON=day OFF=night"
    echo 
    exit 2
}

VALUE=""
# Reading input from command line
while getopts hHS: FLAG; do
    case $FLAG in
        h) usage ;;
        H) usage ;;
        S) VALUE="${OPTARG}" ;;
        *) usage
           ;;
    esac
done
curl -X POST --header "Content-Type: text/plain" --header "Accept: application/json" -d "${VALUE}" "http://192.168.32.133:8080/rest/items/DAY_NIGHT" >> /tmp/day_night_switch_out.log
