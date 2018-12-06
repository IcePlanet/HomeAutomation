set PSCP_PATH="c:\Program Files (x86)\Putty\PSCP.EXE"
set "SOURCE=d:\Ice\Private\HomeAutomation\HomeAutomationGit\"
set "TARGET=192.168.2.222"

%PSCP_PATH% -r %SOURCE%src\ServerRaspberryPi\* ice@%TARGET%:/usr/src/HomeAutomation_Novaky
%PSCP_PATH% -r %SOURCE%src\OpenHabConfiguration\* ice@%TARGET%:/etc/openhab2