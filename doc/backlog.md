# Backlog and TO-DO list

- Create own board to host all needed HW
- Make file cleanup and change
- Settings to be exported in separate file and load from there (currently hardcoded on various places)
- Solar powering of arduinos (3.3V arduinos are 5V tolerant, but radio modules not, idea is to put LDO linear regulator in front of radio as radio is 5V tolerant on data and only 3.3V tolerant on Vin, reason for not using LDO for whole arduino is because of power consumption) the uPover version to consume less than 2uA in stanbe and power source is ultracapacitor
- Solar powering of arduinos normla power via LiIon batteries
- Common adding of new jalousie, as now they need to be added on multiple places