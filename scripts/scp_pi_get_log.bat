set PSCP_PATH="c:\Program Files (x86)\Putty\PSCP.EXE"
set "TARGET=r:"
rem set "TARGET=192.168.54.217"
set "SOURCE=192.168.32.133"
rem set "TARGET=192.168.1.222"

%PSCP_PATH% -r ice@%SOURCE%:/var/log/openhab/*.log %TARGET%
rem %PSCP_PATH% -r %SOURCE%test_code\Pi\* ice@%TARGET%:/usr/src/HomeAutomation_Novaky
rem %PSCP_PATH% -r %SOURCE%src\OpenHabConfiguration\* ice@%TARGET%:/etc/openhab
rem %PSCP_PATH% -r %SOURCE%src\ServerGoodwe\* ice@%TARGET%:/tmp
rem %PSCP_PATH% -r %SOURCE%src\ServerRaspberryPi\simple_persistence.sh ice@%TARGET%:/tmp/