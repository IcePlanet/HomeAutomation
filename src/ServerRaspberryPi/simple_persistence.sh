#!/bin/bash
# This file is to be located in /usr/local/bin/ and chown to openhab
# Change owner: sudo chown openhab:openhab simple_persistence.sh
# Change rights: sudo chmod 754 simple_persistence.sh


# Create dir: sudo mkdir /var/local/openhab_simple_persistence
# Change owner: sudo chown openhab:openhab openhab_simple_persistence
# Chmod: sudo chmod 755 openhab_simple_persistence

# Simple persistence file for OpenHab, it always keeps only last version of attribute

# Please setup location of persistence file below

PERSISTENCE_FILE="/var/local/openhab_simple_persistence/openhab_persistence.data"
BACKUP_FOR_CLEANUP="/var/local/openhab_simple_persistence/openhab_persistence.old"

usage() {
    # Description of usage
    echo "Usage: $0 -W -a <attribute> -v <value>"
    echo "Usage: $0 -R -a <attribute>"
    echo "Usage: $0 -h"
    echo "Usage: $0 -C"
    echo
    echo "Simple peristence for open hab based on one shell script, options are processed in order below, where h causes stop of further processing"
    echo
    echo "Options:"
    echo "    -h                    Displays this text"
    echo "    -W                    Write <value> of given <attribute> to persistence file"
    echo "    -R                    Read value of given <attribute>"
    echo "    -C                    Cleanup of file, remove any old values"
    echo 
    exit 2
}

READ_ATTRIBUTE=false
WRITE_ATTRIBUTE=false
FILE_CLEANUP=false
ATTRIBUTE=""
VALUE=""
# Reading input from command line
while getopts hRWCa:v: FLAG; do
    case $FLAG in
        h) usage ;;
        H) usage ;;
        a) ATTRIBUTE="${OPTARG}" ;;
        v) VALUE="${OPTARG}" ;;
        W) WRITE_ATTRIBUTE=true ;;
        R) READ_ATTRIBUTE=true ;;
        C) FILE_CLEANUP=true ;;
        *) usage
           ;;
    esac
done
if [ ${WRITE_ATTRIBUTE} = true ]; then
  if [ -z "${ATTRIBUTE}" ]; then  
    echo 
    echo "ERROR: Missing attribute for write"
    echo
    usage
  else
    if [ -z "${VALUE}" ]; then  
      echo 
      echo "ERROR: Missing value for attribute ${ATTRIBUTE}"
      echo
      usage
    else
      echo "${ATTRIBUTE}=${VALUE}" >> ${PERSISTENCE_FILE}
      logger -plocal7.info "INFO `date +%Y%m%d%H%M%S` Written ${ATTRIBUTE}=${VALUE}"
      if [ `cat "${PERSISTENCE_FILE}" | wc -l` -gt 4567 ]; then FILE_CLEANUP=true; fi
    fi
  fi
fi
if [ ${FILE_CLEANUP} = true ]; then
  #echo "Persistence file (${PERSISTENCE_FILE} `wc -l ${PERSISTENCE_FILE}` ) cleanup started at `date +%Y%m%d%H%M%S`"
  if [ -f "${PERSISTENCE_FILE}" ]; then
    logger -plocal7.info "INFO `date +%Y%m%d%H%M%S` Persistence file (`wc -l ${PERSISTENCE_FILE}` ) cleanup started at `date +%Y%m%d%H%M%S`"
    mv ${PERSISTENCE_FILE} ${BACKUP_FOR_CLEANUP} && tac ${BACKUP_FOR_CLEANUP} | sort -s -t = -k 1,1 -u > ${PERSISTENCE_FILE}
    #echo "Persistence file (${PERSISTENCE_FILE} `wc -l ${PERSISTENCE_FILE}` ) cleanup finished at `date +%Y%m%d%H%M%S`"
    logger -plocal7.info "INFO `date +%Y%m%d%H%M%S` Persistence file (`wc -l ${PERSISTENCE_FILE}` ) cleanup finished at `date +%Y%m%d%H%M%S`"
  else 
    logger -plocal7.warning "WARN `date +%Y%m%d%H%M%S` Persistence file (${PERSISTENCE_FILE}) can not be found"
  fi
fi
if [ ${READ_ATTRIBUTE} = true ]; then
  if [ -z "${ATTRIBUTE}" ]; then  
    echo 
    echo "ERROR: Missing attribute to read"
    echo
    usage
  else
    if [ -f "${PERSISTENCE_FILE}" ]; then
      VALUE="$(grep "${ATTRIBUTE}" ${PERSISTENCE_FILE} | tail -n 1 | sed -r 's/^'"${ATTRIBUTE}"'=(.*$)/\1/g')"
      if [ -z "${VALUE}" ]; then
        logger -plocal7.info "INFO `date +%Y%m%d%H%M%S` ${ATTRIBUTE} not read"
      else
        logger -plocal7.info "INFO `date +%Y%m%d%H%M%S` Read ${ATTRIBUTE}=${VALUE}"
        echo "${VALUE}"
      fi
    else 
      logger -plocal7.warning "WARN `date +%Y%m%d%H%M%S` Persistence file (${PERSISTENCE_FILE}) can not be found, attribute ${ATTRIBUTE} not read"
    fi
  fi
fi
