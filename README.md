# Home automation

This project is for home automation (mainly roller shutter / jalousie) using Raspberry PI as server, OpenHab as core and UI, NRF21 modules as remote and also 433 for livolo switches control.

Details are described in specific sections in `doc` folder. This document contains basic information.

There is no install script and the make script is also very primitive.

## License

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details. 

You should have received a copy of the GNU General Public License along with this program.  If not, see <https://www.gnu.org/licenses/>.

## WARNING

**Keep yourself and others safe when designing and making electronic projects. Mains voltage electricity is extremely dangerous. There is a significant risk of death through electrocution if mains voltage electricity is allowed to pass through the body. There can also be a risk of fire and explosion if electricity is not cabled and fused correctly. Therefore precautions must be taken when using mains electricity or similar. Do not try this if you are not 100% sure what you are doing and how to do it properly and correctly.**

## General description and operation

User controls functions via openHAB UI (paper UI), main logic is within openHAB rules. Additionally and there is supporting daemon for communication to jalousie, receiving sensor values and updating them in openHAB etc... This daemon is also receiving information from sensors and distributes the information as needed. Jalousie communicate via Livolo switches (only listen) or via NRF21L01 modules which run the motors via relay board and also connect sensors as needed and sends measured value(s) to daemon.

Complete functionality is based on Raspberry PI, arduino pro mini, relay boards, light and temperature sensors, livolo switches, 433 radios, NRF24L01.

Each jalousie is moved based on several pre-set values like if there are plants or not, if the jalousie is covering door, schema setup...

## Modules

- OpenHab `src/OpenHabConfiguration` - main command center and UI to interact with user using open source project openHAB 2 (runs on raspberry pi)
- PiServer `src/ServerRaspberyPi` - server section of 2.GHz and 433MHz communication (runs on raspberry pi also), includes also all additional sections (e.g. logrotate settings, support batch files...)
- Arduino `src/Arduino` - computer managing one or more jalousies, reporting light values...

### Modules interaction

Main command module is **OpenHab** which is running together with PiServer on the same HW (not necessarily). All inputs are processed by rules and outputs are exported to file. Inputs are fed by user using UI or by PiServer using API. Output file is again processed by PiServer. In UI you can manipulate the jalousie manually, or adjust basic settings.
Rules are 'heart' of logic, they take into account all the input values (usually sensors), preferences, setup and from this create orders for jalousie (driven by Livolo or by arduino).

**PiServer** `home_automation.server` is communicating with arduinos and Livolo switches. With Livolo switches the communication is only one way and there is no possibility to get any status update. With arduinos there is response confirming receiving of command. Used engines are not reporting position so it is not possible to give any information of real jalousie position. Inputs are taken from 2 possible sources

- file where OpenHab writes what (command) must be send where (target), input are send to arduinos or to Livolo switches based on configuration, the OpenHab module is not aware what method will be used
- arduino values send to sever, these values are internally processed and if evaluated as needed the (processed) value is then send to openHAB server via API

**Arduino** is collection of sources for Arduino modules. In general each arduino has one NRF21 communication board (not mandatory), one or more relay boards (can also have none) and additional sensors (e.g. temperature). Arduino is always sleeping for 8 seconds and then checks cca 30ms for commands, because of this each command is transmitted from PiServer for at least 8 seconds. Measured values are directly on arduino processed only in limited way to prevent unnecessary transfers.

## Few caveats

There is no configuration in one place. The configuration is always hardcoded in given module. This is not correct, but currently do not know of more universal way due to different 'targets' for configuraiton (e.g. Raspberry pi, arduino). Some kind of pre-processor/deployment tooling will be needed like chef... to allow centralizing of configuration.

Communication with Livolo switches is very sensitive as they require precise timing. **Timing is CRITICAL** pay attention to this fact. Values in source code are OK for Raspberry PI 2B. Also if you have correctly setup communication there is NO WAY to identify if Livolo has recived the command. Consequently there is NO WAY to send stop command as stop is simply send as repeat of last command, but what if the command signal was not received by Livolo switch...

Some config values depends on each other and if you setup them incorrectly the system will not work (e.g. if you setup `voltage_ignore_range` in receiver to higher value than 1/2 of `sensor_default_hyst` in rules the switching will ignore the setup hysteresis range, but because `voltage_ignore_range` is in one direction only you need to setup it **MAX 1/4** from `sensor_default_hyst` to make sure rules will correctly fire and switch. Always understand in advance, what, why and how you are changing and most importantly what consequences it has.

[Pin numbering on raspberry PI](https://pinout.xyz/) can get confusing, be carefull about this. For example physical pin `8` is referred to as `PIN 15`, `TX` or `GPIO14`.

If something does not work first check the HW. What to look for:

- Bad solder joints or shorts
- Pin numbering confusion
- Other noise e.g.: WiFi (observed when NRF module is closer than 20cm to the WiFi antenna), weather station, speakers, metal plates...
- Some NRF24L01 generate flood of incomming traffic (I have seen mainly on models with antenna), the actual version of software is not resistant to this behaviour and will cause hang of receiver
- Antenna rotation/direction
- Sufficient capacitance to cover current spikes (significant when using low power high efficinency power sources)

The 433 module I'm using is creating lot of noise. To fix this the 433 module is powered via transistor switch only when needed. I really recommend this approach.

## Versions you can build

There are 3 versions planned (1 available, 1 prototyped, 1 idea), in general everything you read it for MAINS version, if there are any specifics for SUPERCAP or BATTERY version it will be explicitly mentioned, to not mix the version names with electrical components, the version is always written in uppercase.

### MAINS (Standard)

This is the common version, it is povered from AC voltage, you do not need to take care about battery or power consumption, but continuously it takes cca 0.5 to 1W of energy beacuse of transformation from 220AC to 5V DC. **This is the only version existing in production**

### SUPERCAP version

This version is based on supercapacitor charged via solar panel, currently exists in prototype version, but due to size (SMD) of components it can not be used in production without own PCB board. It consumes in average cca 2uA so supercapacitor is sufficient. In case of solar failure, or longer dark period where charge will be not sufficient there is backup switch to AC, the same backup switch is used also for operating jalousie as from supercap you can not drive relays or jalousie engines.

### BATTERY version

In this version LiIon battery is charged via solar panel allowing mains-free work also in extended periods of darkness (e.g. solar covered by snow for days), but I do not like the idea of LiIon battery as it miht fail catastrophically when not properly used and also there is some kind of lifetime monitoring needed. This version does not exist at all, several modules have been tested, but no complete working solution.

## Note on security

There is absolutelly 0 security on Livolo switches and the same is valid for arduino. Never use this for any kind of security. In my usage model the attacker can move the jalousie freely. This is maybe not nice, but did not create any harm as the jalousie on my house are not intended as security measure.
Livolo switches use very simple protocol and basic reply attack will work on thse. The same is valid for arduino modules.
There is no possibility that attacker will get access to server via Livolo or Arduino. Livolo can not send at all and arduinos can send only very limited set of commands. Incomming interface filters this completelly and anything not matching protocol is thrown away.
Depending where your Raspberry PI is located (or other Linux server), pay attention to securing of this server, as it is fully working Linux server that can be used at base for attacking of your whole network. I recommend to limit access on need to know basis, and if communication outside of your network is needed use VPN or other form of security.

## Side notes

If you are changing anything, try to understand first what you are doing.

Selection of Livolo switches presents compromise between usability, ease of use, design on positive side and no information if command was received, no possibility to send 'STOP' command, no status reporting on the negative side.

If you wish there are many mods of these switches, you can be inspired here (for now I do not plan to do modding of Livolo):
- https://forum.mysensors.org/topic/7151/livolo-firmware-mod (https://github.com/jamarju/livolo-firmware)
- https://forum.mysensors.org/topic/5697/livolo-alike-switch-mod
- https://www.openhardware.io/view/306/RFM69-Livolo-2-channels-1-way-EU-switchVL-C700X-1-Ver-B8 https://www.openhardware.io/view/763/NRF52832-2-channel-control-plate-for-in-wall-switch
- https://forum.mysensors.org/topic/2775/livolo-glass-panel-touch-light-wall-switch-arduino-433mhz/21
