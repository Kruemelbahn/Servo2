	list p=PIC16F690
;**************************************************************
;*  	Pinbelegung
;*	-------------------------------------------------------	
;*	PORTA 	0 In  AN0, Endlage2 rechts / grün
;*		1 In  AN1, Endlage2 links / rot
;*	 	2 In  AN2, Endlage1 rechts / grün
;*		3 In  Taster IBN
;*              bei internem Oszillator ohne XCHG_SERVO:
;*			4 Out Impulsausgang für SERVO_2
;*			5 Out Impulsausgang für SERVO_1
;*              bei internem Oszillator mit XCHG_SERVO:
;*			4 Out Impulsausgang für SERVO_1
;*			5 Out Impulsausgang für SERVO_2
;*              bei externem Oszillator:
;*			4 Out
;*      PORTB   4 In  Taster1 rechts
;*      	5 In  Taster1 links	
;*      	6 In  Taster2 rechts	
;*      	7 In  Taster2 links
;*	PORTC	0 In  AN4, Endlage1 links / rot
;*              bei internem Oszillator:
;*			1 Out IBN ist ein
;*			2 In  1 = Servoabschaltung aktiv
;*                0 = Servoabschaltung deaktiviert
;*              bei externer Taktquelle:
;*			1 Out Impulsausgang für SERVO_1
;*			2 Out Impulsausgang für SERVO_2
;*		3 Out LED_LI_2	
;*		4 Out LED_RE_2	
;*		5 Out LED_LI_1	
;*		6 Out LED_RE_1	
;*		7 Out POWER_ON	
;*	
;**************************************************************
;
; Hauptdatei = DSA-2.ASM
;
; M.Zimmermann 03.06.2020
;
; ServoAnsteuerung : einfache Ansteuerelektronik
;                    für bis zu zwei Servomotoren
;
; Prozessor PIC 16F690
;
; Prozessor-Takt 4MHz (wählbar)
;
; zu diesem Project gehören auch die Dateien:
;    DSA-2.inc, servo_isr.inc
;
; das HEX-File kann erstellt werden durch: build.bat
;
;**************************************************************
;
; Signalform für Servo:
;
;  +--+          +--+
;  |  |          |  |
;  |  |          |  |
; -+  +----------+  +-
;  |<-- ~20ms -->|
;  |  |
;   ^
;   |
;   +-- Breite = 1..2ms (LONG_PULS: 0,5..2,5ms)
;                1(0,5) ms  = links
;                1,5    ms = mitte
;                2(2,5) ms  = rechts
;
; Pulsabstand 15..30ms möglich
;
; Testservo: Dymond D90eco
;            11.8 * 22.8 * 20.6 mm, Gewicht 9g
;            Stellkraft 15NCm
;            Stellzeit  0.09sec/45°
;            Anschluß: GND | +5V | Imp.
;                       sw | rt  | ws
;
; Programmablauf:
; - Interrupt alle 10ms über Timer abwechselnd für Servo1 und Servo2:
; 	~ setze Ausgang auf High
;	~ warte Grundbreite 1ms (LONG_PULS: 0,5ms)
;	~ Wartezeit 0..1ms (LONG_PULS: 0..2ms)je nach Potistellung
;		Potistellung 0..5V = 0..1023 ./. 4 = 0..255
;	~ setze Ausgang auf Low
;
;**************************************************************
; History:
;	1 Portierung von Servoansteuerung Version 4
;	2 flexible Taktquellenauswahl
;	  write to eeprom benutzt ISR
;	  IBN-Routine: Positionsvorgaben aus dem EEPROM
;	  Servoausgänge getauscht wg. Potizuordnung
;	3 keine Pullups bei invertierten Eingängen an PortB
;	4 neues Timing für ISR
;	5 Servospannungsversorgung nach Erreichen der Position abschaltbar
;	6 Abschaltung Servospannungsversorgung durch PORTC, 2
;
;**************************************************************
; *  Copyright (c) 2018 Michael Zimmermann <http://www.kruemelsoft.privat.t-online.de>
; *  All rights reserved.
; *
; *  LICENSE
; *  -------
; *  This program is free software: you can redistribute it and/or modify
; *  it under the terms of the GNU General Public License as published by
; *  the Free Software Foundation, either version 3 of the License, or
; *  (at your option) any later version.
; *  
; *  This program is distributed in the hope that it will be useful,
; *  but WITHOUT ANY WARRANTY; without even the implied warranty of
; *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; *  GNU General Public License for more details.
; *  
; *  You should have received a copy of the GNU General Public License
; *  along with this program. If not, see <http://www.gnu.org/licenses/>.
; *
;**************************************************************
;
;

#define VERSION_NO	.6

;.........................................................................
;--- Compiler(Hardware)-Optionen, können 0 oder 1 sein, NICHT änderbar mit ServoAnsteuerung.exe ---
; Taktquelle einstellen - nur EINE von den drei Variablen darf 1 sein!
#define INT_OSC		.1
#define EXT_CST		.0
#define EXT_OSC		.0

; schreiben ins EEPROM mit ISR
#define WRITE_EEPROM_INT .0

; IBN-Routine und EEPROM-Position verwenden
#define USE_IBN		.1

#define LONG_PULS	.1	; Impulsbreite 0,5...2,5ms
#define XCHG_SERVO	.1	; Anschluss Servo 1 mit Servo 2 tauschen
#define SWITCH_POW_OFF	.1	; Abschalten der Spannungsversorgung nach Positionierung
;.........................................................................
;--- Software-Optionen, können 0 oder 1 sein, änderbar mit ServoAnsteuerung.exe ---
#define INPUT_INV	.0	; Eingänge sind invertiert (aktiv high)
#define LED_BLINK	.0	; LED blinken, wenn ohne Position
#define OVER_L_1	.0	; for later use
#define OVER_R_1	.0	; for later use
#define OVER_L_2	.0	; for later use
#define OVER_R_2	.0	; for later use
#define SWITCH_OFF	.1	; Abschalten der Spannungsversorgung nach Positionierung

; sinnvolle Wertepaare:	INC_DEC, INC_20 bzw. DEC_DEC, DEC_20
;	Signal		5	1
;	Schranke	1	3 (bzw. 1 2)
#define INC_DEC		.1	; Rampensummand: je größer desto schneller
#define	INC_20		.3	; Summand jede ...te 20ms-Schleife
				; je größer desto langsamer
#define DEC_DEC		.1	; Rampensummand: je größer desto schneller
#define	DEC_20		.3	; Summand jede ...te 20ms-Schleife
				; je größer desto langsamer

#include DSA-2.inc

; end of file
