# Data structure

## Data transfer

Data are transferred using one big int 32bits, all transfers from server to client that are not broadcast are confirmed by client using ack message. In the ack message there is only one bit changed, the rules for source and target address did not apply to ack packets.

Structure of the big int transfered: by radio:

```
0000 0000   0000 0000   0000 0000   0000 0000
^^^^^^^^^   ^^^^^^^^^   ^^^^^^^^^   ^^^^^^^^^
Target      Payload 1   Payload 2   Orders
```

Source system sends data to `target` in a way that target system id is written in the `target` field, required data or identification of payload is written into `orders`, bits 5 to 0 are available to be combined by OR to include up to 2 payloads, the payloads are written in the order of bits (e.g. if payload contains voltage and light `0010 0010`the voltage is in Payload 1 and Light is in Payload 2). Specific details are included by order details.  

### Orders

For the `orders` following bit numbering is applied:

```
0000 0000
7654 3210
```

Bits on `orders` part are split in 2 groups, they are joined together in final `orders` part:

* **bits 7-0**: only one command is here, all stop, this command is sent only by master, if set to be re-transferred the re-transfer nodes will repeat this command 3 times, efectivelly blocking any other commands to be executed
  * **`0000 0000`** Stop all ongoing actions
  * **`0100 0000`** Stop all ongoing actions with re-transfer request
* **bits 3-0**: These bits are first part of order; they can be combined by OR to for any kind of request:
  * **`0001` (1)** Temp
  * **`0010` (2)** Light
  * **`0100` (4)** Rain
  * **`1000` (8)** Button/connect/PIR sensor
* **bits 5-4**: 
  * **`0001` (16)** Engines
  * **`0010` (32)** Voltage
* **bit 6**: Re-transfer request if set to `1` (64)
* **bit 7**: ACK bit, if set to `1` (128) the message is ACK

Empty data are not send so there can not be misinterpretation of all stop vs empty data

#### Orders: ENGINES

For engines only transfer from server to client is possible

Payload 1 is engine number: Engines are numbered from 1 to number of engines (configured on client), if engine number 0 is used the payload 2 must be also 0 and means ALL STOP (internally on arduino engines are numbered from 0 and transition to this numbering is done on arduino when processing orders `process_orders` function when calling `engine_change`)
Payload 2 is required action:

* **0**: STOP (for switch=OFF and also for jalousie motor=STOP)
* **1**: DOWN (only for jalousie motor)
* **2**: UP (only for jalousie motor)
* **3**: ON (only for switch)

### Target

Target `1111 1111` indicates broadcast

## Locking and variables with special meaning

Lock variable:

* **`0000 0000`**: Unlocked
* **`0000 0001`**: Orders to be processed
* **`0000 0010`**: Engine running
* **`0000 0100`**: Payload to be send
* **`0000 1000`**: Power on for radio
* **`0001 0000`**: Power on 240 V AC
* **`0010 0000`**: switch/PIR/connection


