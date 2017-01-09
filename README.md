# irRemote
An Arduino program to send and receive infrared remote signals

## Abstract
This is a program which runs on Atmega328 5V 16Mhz based Arduino (Uno, Nano, etc.). 
It gets commands from the serial port and send infrared remote signals. 
It also sends infrared remote signals to the serial port. 
It supports sending and recieving AHEA, NEC and Sony formats and sending Ohm's OCR-04 and OCR-05 signals.

The program is written to acheive my purpose, so modify it so as to acheive your purpose. 
Part of this program is based on codes by kenkenpa (http://hello-world.blog.so-net.ne.jp/2011-05-19).

## Circuits and Serial
Connect IR remote recieving module to pin 2 and IR LED to pin 3. The speed of serial port is 115.2kbps.

## Serial commands
- Axxxxxxxx...xx¥n
Send AHEA infrared signal.
- H
Send stop heating infrared siganal for a Mitsubishi's airconditioner. 
- h
Send start heating infrared siganal for a Mitsubishi's airconditioner. 
- k
Send NEC formated B8009D infrared signal (Kenwood RC-RP0702 power button).
- Nxxyyzz¥n
Send NEC formated xxyyzz infrared signal. 
- NxxyyzzZZ¥n
Send NEC formated xxyyzzZZ infrared signal.
- O
Send Ohm's OCR-04 OFF remote signal.
- o
Send Ohm's OCR-04 ON remote signal.
- P
Send Ohm's OCR-04 OFF remote signal.
- p
Send Ohm's OCR-04 ON remote signal.
- Sppqqrrss¥n
Send Sony formated ppqqrrss infrared signal.
- V
Verbose mode off (default)
- v
verbose mode on

## Recieved signals
Recieved infrared signals are sent to serial port. 
It starts with 'A', 'N' or 'S' if the signal is AHEA, NEC or Sony format accordingly, and ends with '¥n'.

## Recieving and sending signals
It sends infrared signals when it recieve some infrared signals. Then it services as a remote control converter.
It is programed to send OCR-04 ON signal if it recieves NEC formated 00FF08F7 code and so on. 
I am using a remote controller (https://www.amazon.co.jp/dp/B01LXDLPTC/) and I uses it instead of multiple controllers.
