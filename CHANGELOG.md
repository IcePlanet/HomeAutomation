# Change Log

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