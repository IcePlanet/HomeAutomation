set PSCP_PATH="c:\Program Files (x86)\Putty\PSCP.EXE"
set "SOURCE=d:\Ice\Private\HomeAutomation\HomeAutomationGit\"
set "TARGET=192.168.32.133"
rem set "TARGET=192.168.1.222"

%PSCP_PATH% -r %SOURCE%src\ServerRaspberryPi\* ice@%TARGET%:/usr/src/HomeAutomation_Novaky
%PSCP_PATH% -r %SOURCE%src\OpenHabConfiguration\* ice@%TARGET%:/etc/openhab2

%PSCP_PATH% -r %SOURCE%src\ServerRaspberryPi\simple_persistence.sh ice@%TARGET%:/tmp/