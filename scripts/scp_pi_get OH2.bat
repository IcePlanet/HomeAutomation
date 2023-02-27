set PSCP_PATH="c:\Program Files (x86)\Putty\PSCP.EXE"
set "SOURCE=d:\Ice\New\"
set "TARGET=192.168.32.133"
rem set "TARGET=192.168.1.222"

%PSCP_PATH% -r ice@%TARGET%:/etc/openhab2/rules/rollershutter.rules %SOURCE% 
%PSCP_PATH% -r ice@%TARGET%:/etc/openhab2/items/default.items %SOURCE% 
rem %PSCP_PATH% -r %SOURCE%test_code\Pi\* ice@%TARGET%:/usr/src/HomeAutomation_Novaky
rem %PSCP_PATH% -r %SOURCE%src\OpenHabConfiguration\* ice@%TARGET%:/etc/openhab2
rem %PSCP_PATH% -r %SOURCE%src\ServerGoodwe\* ice@%TARGET%:/tmp
rem %PSCP_PATH% -r %SOURCE%src\ServerRaspberryPi\simple_persistence.sh ice@%TARGET%:/tmp/