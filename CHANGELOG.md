# Change Log

## [2.0.0] - 2022-12-12

- NON WORKING VERSION !!!
- Port to openhab3 and Raspberry 3B

## [1.2.8] - 2022-10-21

- Bugfix on freeze when multiple items triggered in parallel, delays in main threads watch for locks and only when lock is free next update is triggered, this check can not be part of update as they can occupy all threads available in openhab, causing Arduino send to hang and because all other threads wait for lock, that can not be released because the arduino can not start from the thread having lock acquired
- Arduino configuration put to separate file for each arduino improving overview and handling of multiple arduinos
- Added items and rules for winter garden

## [1.2.7] - 2021-03-09

- Bugfix in time measure during sending

## [1.2.6] - 2021-03-09

- Changed delays processing on sending, removed fixed delay after send, new delay is calculated based on real difference between previous and actual send command (including media to send 433 or nrf)

## [1.2.4] - 2021-01-03

- Changed delays in processing of file to get faster response (increases risk of 'missed' commands in case the write handle was still open)
- After file processed some cycles (faster_loop_count) use reduced delays to speed up processing of consecutive commands, also number of cycles in receive loop is decreased during this time
- Delay after sending (433 and also NRF) changed to micro seconds (was seconds until now), but configured as miliseconds (multipled by 1000 directly in usleep)

## [1.2.3] - 2020-11-12

- Changed 433 send count from 100 to 75 as solution to stoping in movement (configuration value, read comment directly in code)
- Documentation update

## [1.2.2] - 2020-09-26

- Fixed comparing to null of local automation in case global automation is OFF
- Changed order of commands sending, radio 433 is send first and then is NRF 21 send
- Trigger queue does not accept duplicates (in case too many dark/light events)

## [1.2.1] - 2020-08-31

- Added remote 433 day night switching (not yet used in real life)
- Radio 433 is powered on only in case sending is needed
- Fix for packet logging as unknown after successfully decoded
- Fix of NULL comparison and double triggering in rules

## [1.1.0] - 2020-05-03

- Added possibility to define 433 and nrf in parallel
- General on/off switch for 433 sending `r433_enabled`
- Created new arduino code for 433 sender (as sending 433 from raspberry pi is not reliable
- Added .gitignore because of kicad in another branch
- Added pins for up to 3 engines in receiver
- ARDUINO 52 burned (winter garden)

## [1.0.0] - 2020-03-30

- Versioning and change log created