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
; M.Zimmermann 22.05.2020
;
;**************************************************************
;
; Signalform für SERVO:
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
; Programmablauf:
; - Interrupt alle 10ms über Timer0 abwechselnd für Servo1 und Servo2:
; 	~ setze Ausgang auf High
;	~ Wartezeit über Timer1:
;               Grundbreite 1ms (LONG_PULS: 0,5ms) + 0..1ms (LONG_PULS: 0..2ms)
;		je nach Potistellung 0..5V = 0..1023 ./. 4 = 0..255
;	~ setze Ausgang auf Low
;
;**************************************************************

; Includedatei für den 16F690 einbinden

	#include <P16F690.INC>

	ERRORLEVEL      -302   	;SUPPRESS BANK SELECTION MESSAGES

;**************************************************************
#if ((INT_OSC + EXT_OSC + EXT_CST)!=1)
	error "fehlerhafte Oszillator/Taktauswahl"
#endif

; Configuration festlegen: 
#if INT_OSC==1
	; RA5 & RA4 als I/O verwendbar
	; Power up Timer, kein Watchdog, int-Oscillator, kein Brown out 
	__CONFIG	_MCLRE_OFF & _PWRTE_OFF & _WDT_OFF & _INTRC_OSC_NOCLKOUT
#endif
#if EXT_CST==1
	; RA5 & RA4 für ext Resonator verwendet
	; Power up Timer, kein Watchdog, ext-Resonator, kein Brown out 
	__CONFIG	_MCLRE_OFF & _PWRTE_OFF & _WDT_OFF & _XT_OSC
#endif
#if EXT_OSC==1
	; RA5 für ext Oszillator verwendet
	; RA4 als I/O verwendbar
	; Power up Timer, kein Watchdog, ext-Oscillator, kein Brown out 
	__CONFIG	_MCLRE_OFF & _PWRTE_OFF & _WDT_OFF & _EC_OSC
#endif
;**************************************************************
#define OFFSET_05	.62	; Timer-Offset für 0,5ms
#define LOOP_CNT	.100	; Wert für TMR0 (ISR alle 10ms)
#define LOOP_LED_1	.25	; 500ms/20ms
#define LOOP_LED_2	.25	; 500ms/20ms
;**************************************************************

; Variablen
;**************************************************************
; EEPROM
#define eeprom_start	2100h

		org	eeprom_start
sw_kennung:	de	"MZ", .19
version:	de	VERSION_NO

; Geschwindigkeitskennungen Servo 1 speed1
incdec:		de	INC_DEC
inc20:		de	INC_20

; zur Unterscheidung der Versionen (Softwareidentifikation):
		de	LONG_PULS + (.2*USE_IBN) + (.4*XCHG_SERVO) + (.8*WRITE_EEPROM_INT) + (.64*SWITCH_POW_OFF)

; Einstellung verschiedener Verhaltensmodi (wird nach 'ba_modus' kopiert):
modus:		de	INPUT_INV + (.2*LED_BLINK) + (.4*OVER_L_1) + (.8*OVER_R_1) + (.16*OVER_L_2) + (.32*OVER_R_2) + (.64*SWITCH_OFF)

#if USE_IBN==1
; Geschwindigkeitskennungen Servo 1 speed2
decdec:		de	DEC_DEC		; reserved for later use
dec20:		de	DEC_20		; reserved for later use

; Endlagenüberschwinger Servo 1
overshoot_1_l:	de	.10		; reserved for later use
overshoot_1_r:	de	.10		; reserved for later use

; Zielpositionen Servo 1 im EEPROM nach IBN
esrvpos_1_l	de	.0
esrvpos_1_r	de	.0

; Geschwindigkeitskennungen Servo 2 speed1
incdec_2:	de	INC_DEC		; reserved for later use
inc20_2:	de	INC_20		; reserved for later use

; Geschwindigkeitskennungen Servo 2 speed2
decdec_2:	de	DEC_DEC		; reserved for later use
dec20_2:	de	DEC_20		; reserved for later use

; Endlagenüberschwinger Servo 2
overshoot_2_l:	de	.10		; reserved for later use
overshoot_2_r:	de	.10		; reserved for later use

; Zielpositionen Servo 2 im EEPROM nach IBN
esrvpos_2_l	de	.0
esrvpos_2_r	de	.0

; Wartezeit für PowerOff (Wert * 10ms), wird nach 'time_pow_off' kopiert
etime_pow_off	de	.100
#endif

eflags:		de	.0

;**************************************************************
	CBLOCK		H'20'
w_save		: 1	; ISR-Zwischenspeicher
status_save	: 1	; ISR-Zwischenspeicher
counter_1	: 1	; ISR-Zähler für LED_1-Blinken
counter_2	: 1	; ISR-Zähler für LED_2-Blinken

loops		: 1	; interner Zähler für wait
loop_adc	: 1	; interner Zähler für wait adc

flags		: 1	; Richtungsauswahl
flags_2		: 1
flags_3		: 1
cflags		: 1	; Zustand der Tasten (Kopie für EEPROM)
tflags		: 1	; Zustand der Tasten
ziel_pos_1	: 1	; Position 1 Poti links / rechts
ist_pos_1	: 1	; aktuelle Position 1 links / rechts
ziel_pos_2	: 1	; Position 2 Poti links / rechts
ist_pos_2	: 1	; aktuelle Position 2 links / rechts

inc__dec_1	: 1	; Rampensummand
inc__20_1	: 1	; Rampensummand jede ...te 20ms
inc__20fix_1: 1	; Rampensummand jede ...te 20ms

ba_modus	: 1	; Betriebsmodus, copy from eeprom

loops2		: 1	; interner Zähler für wait

loop_50		: 1	; interner Zähler für wait 50ms

mflags		: 1	; Richtungsauswahl: Kopie für ISR
pclath_save	: 1	; ISR-Zwischenspeicher

srvpos_1_l	: 1	; Zielpositionen aus EEPROM
srvpos_1_r	: 1
srvpos_2_l	: 1
srvpos_2_r	: 1

time_pow_off	: 1	; copy from eeprom
time_pow_off_c	: 1

	ENDC

#define IMove_L_1	mflags,0
#define IMove_R_1	mflags,1
#define IMove_L_2	mflags,3
#define IMove_R_2	mflags,4

#define Move_L_1	flags,0
#define Move_R_1	flags,1
#define Move_L_2	flags,3
#define Move_R_2	flags,4

#define Power_ON_1	flags_2,0	; sind beide Flags gesetzt, wird
#define Power_ON_2	flags_2,1	; die Servospannung POWER_ON 
					; eingeschaltet
#define LED_flg_1	flags_2,2	; Blinkflag
#define LED_flg_2	flags_2,5	; Blinkflag
#define IBN_Modus	flags_2,6	; IBN-Modus aktiv
#define ISR_for_Servo2	flags_2,7

#define EndPos_SV1	flags_3,0
#define EndPos_SV2	flags_3,1
#define PowOffTimeStart flags_3,7

; only for reading:
#define Invert_Input	ba_modus,0
#define Blink_LED	ba_modus,1
#define PowSwitchOff	ba_modus,6

;**************************************************************

#define END_LI_1	PORTC,0	; Input
#define END_RE_1	PORTA,2	; Input
#define TA_LI_1		tflags,5; Input copy from PORTB,5
#define	TA_RE_1		tflags,4; Input copy from PORTB,4
#define LED_LI_1	PORTC,5	; Output 1 = links
#define LED_RE_1	PORTC,6	; Output 1 = rechts
#if INT_OSC==1
  #if XCHG_SERVO==0
    #define SERVO_1	PORTA,5	; Output
  #else
    #define SERVO_1	PORTA,4	; Output
  #endif
#else
  #define SERVO_1	PORTC,2	; Output
#endif

#define END_LI_2	PORTA,1	; Input
#define END_RE_2	PORTA,0	; Input
#define TA_LI_2		tflags,7; Input copy from PORTB,7
#define	TA_RE_2		tflags,6; Input copy from PORTB,6
#define LED_LI_2	PORTC,3	; Output 2 = links
#define LED_RE_2	PORTC,4	; Output 2 = rechts
#if INT_OSC==1
  #if XCHG_SERVO==0
    #define SERVO_2	PORTA,4	; Output
  #else
    #define SERVO_2	PORTA,5	; Output
  #endif
#else
  #define SERVO_2	PORTC,1	; Output
#endif

#define POWER_ON	PORTC,7	; Output, wenn ISR bereit
#define PWR_ON_EXT	PORTC,2 ; Input

#if USE_IBN==1
  #define TA_IBN	PORTA,3	; Input
  #if INT_OSC==1
    #define LED_IBN	PORTC,1 ; Output
  #endif
#endif

;### M A C R O's ##############################################
DisableInt	macro
		bcf 	INTCON, GIE 	; Disable INTs.
		btfsc	INTCON, GIE	; see AN576
	  	  goto 	$-2
		endm
EnableInt	macro
		bsf 	INTCON, GIE 	; Enable INTs.
		endm
; . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 
EnsureWnonZero	macro
		iorlw	.0		; setzt ZeroFlag
		skpnz
		  addlw	.1		; W darf nicht 0 sein!
		endm
; . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 
ADC_Ch0		macro
		bcf	ADCON0,2
		bcf	ADCON0,3
		bcf	ADCON0,4
		endm		
ADC_Ch1		macro
		bsf	ADCON0,2
		bcf	ADCON0,3
		bcf	ADCON0,4
		endm		
ADC_Ch2		macro
		bcf	ADCON0,2
		bsf	ADCON0,3
		bcf	ADCON0,4
		endm		
ADC_Ch4		macro
		bcf	ADCON0,2
		bcf	ADCON0,3
		bsf	ADCON0,4
		endm
; . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 
adc_nr	= 0
DoTheADC	macro
adc_nr++			; increment count of instantations
	bsf	ADCON0,1	; Wandlung starten
adc_loop#v(adc_nr):
	btfsc	ADCON0,1
	  goto	adc_loop#v(adc_nr)	; warten bis fertig

        movlw   .5		; Zeitkonstante für 20µs 
	movwf   loop_adc 
Waitadc#v(adc_nr):
	nop  
        decfsz  loop_adc, F 	; Zeit vorbei? 
          goto	Waitadc#v(adc_nr)		; nein, noch nicht 

	movfw	ADRESH
		endm
; . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .

#include "servo_isr.inc"

;##############################################################

	ORG	0x0000
	GOTO	Init

;***ISR********************************************************
	org     0x04 		; InterruptServiceVector 
	movwf   w_save      ; save W 
	swapf   STATUS,W 
	;===> 0
	clrf    STATUS
        movwf   status_save 
	movf    PCLATH,W
	movwf   pclath_save
	clrf    PCLATH		; Bank 0

;--------------------------------------------------------------
	btfss	PIR1, TMR1IF	; TMR1IF = 1 = INT durch Timer 1?
	  goto	check__eeprom	; nein!

	bcf	PIR1, TMR1IF	; reset Interruptflag
	bcf	T1CON, TMR1ON	; Timer 1 stoppen
	bcf	SERVO_1
	bcf	SERVO_2

;--------------------------------------------------------------
check__eeprom:
#if WRITE_EEPROM_INT==1
	btfss	PIR2, EEIF	; EEIF = 1 = INT durch eeprom write?
	  goto	check_T0	; nein!
		
	bcf	PIR2, EEIF	; reset Interruptflag
	;===> 3
	banksel	EECON1		; select bank 3	
	bcf 	EECON1, WREN	; Disable write
	;===> 0
	banksel	PIR1		; select bank 0
check_T0:
#endif
;--------------------------------------------------------------
	btfss	INTCON, T0IF	; INT durch Timer 0?
	  goto	end_isr		; nein!

	bcf     INTCON, T0IF	; reset Interruptflag
	btfss PowOffTimeStart
		goto ISR_WHICH_SERVO	; 0 = Wartezeit nicht gestartet
		
	movfw	time_pow_off_c	; PowerOff-Time bearbeiten
	iorlw	.0		; setzt ZeroFlag
	skpz
	  decf	time_pow_off_c,1	; weniger als 0 geht nicht

ISR_WHICH_SERVO:
	btfsc	ISR_for_Servo2	; ISR für Servo2?
	  goto	ISR_SERVO2	; ja :-)
;..............................................................
ISR_SERVO1:			; Servo1
	IsrForServo
	bsf	ISR_for_Servo2	; das nächste mal ist Servo2 dran
	goto	load_cnt
;..............................................................
ISR_SERVO2:			; Servo2
	IsrForServo
	bcf	ISR_for_Servo2	; das nächste mal ist Servo1 dran
;..............................................................
load_cnt:
	movlw	LOOP_CNT
	; Vorbelegung von TMR0 mit 156 -> ISR alle 10ms
	movwf	TMR0
;--------------------------------------------------------------
end_isr:
        ; End ISR, restore context and return to the main program 
	movf    pclath_save,W
	movwf   PCLATH
	swapf   status_save,W 
	movwf   STATUS 
	swapf   w_save,f	; restore W without corrupting STATUS 
	swapf   w_save,w 
	retfie 

;***ISR end****************************************************
Init:
	; Initialisierung des Timer0 
        ; TIMER0 muß eingestellt sein!  
        ;   64:1 bei 4 MHz -> 15,625 kHz 
        ;   Überlauf nach 16,384 ms
        ;   61 int in 1 Sekunde
	;   Vorbelegung von TMR0 mit 156 -> ISR alle 10ms
	;   Vorteiler 64:1 am Timer0, interner Takt/4
	;===> 1
	banksel	PIE1		; select bank 1
	movlw	B'00010101'	; 64:1
	movwf	OPTION_REG

	; Analogeingänge vorbereiten:
	movlw	B'00010000'	; Fosc /8
	movwf	ADCON1
	;===> 2
	banksel	ANSEL		; select bank 2
	movlw	B'00010111'	; ANS0, ANS1, ANS2, ANS4 
	movwf	ANSEL
	clrf	ANSELH

	; Portinitialisierung:
	movlw	B'11110000'	; input = 1, output = 0
	clrf	IOCB		; no interrupt on change on Port B
	;===> 1
	banksel	PIE1		; select bank 1
	movwf	TRISB
#if INT_OSC==1
	movlw	B'00000101'	; input = 1, output = 0
#else
	movlw	B'00000001'	; input = 1, output = 0
#endif
	movwf	TRISC

	movlw	B'00001111'	; input = 1, output = 0
	movwf	TRISA

	clrf	IOCA
	movlw	B'00001000'	; input = 1, output = 0
	movwf	WPUA		; internal Pull-up on
	movlw	B'00000001'	; enable ISR for Timer 1 (TMR1IE)
	movwf	PIE1		; disable all other
	clrf	PIE2		;   interrupts

	;===> 0
	banksel	PIR1		; select bank 0
	clrf	PIR1
	clrf	PORTA
	clrf	PORTB
	clrf	PORTC
	clrf	flags_2
	clrf	flags_3

	movlw	B'00110000'	; prescaler 8:1 for Timer 1
	movwf	T1CON

#if USE_IBN==0
	movlw	B'00000001'	; Enable ADC
	movwf	ADCON0
#else
	clrf	ADCON0
#endif

	movlw	LOOP_CNT
	; Vorbelegung von TMR0 mit 156 -> ISR alle 10ms
	movwf	TMR0
	movlw	LOOP_LED_1
	movwf	counter_1

	movlw   incdec-eeprom_start
	call 	read_eeprom
	EnsureWnonZero
	movwf	inc__dec_1

	movlw   inc20-eeprom_start
	call 	read_eeprom
	EnsureWnonZero
	movwf	inc__20_1
	movwf	inc__20fix_1

	movlw   modus-eeprom_start
	call 	read_eeprom
	movwf	ba_modus

	movlw   eflags-eeprom_start
	call 	read_eeprom
	andlw	B'01111111'	; Start immer mit Servo1
	movwf	flags
	movwf	mflags

#if USE_IBN==1
	movlw   esrvpos_1_l-eeprom_start
	call 	read_eeprom
	EnsureWnonZero
	movwf	srvpos_1_l

	movlw   esrvpos_1_r-eeprom_start
	call 	read_eeprom
	EnsureWnonZero
	movwf	srvpos_1_r

	movlw   esrvpos_2_l-eeprom_start
	call 	read_eeprom
	EnsureWnonZero
	movwf	srvpos_2_l

	movlw   esrvpos_2_r-eeprom_start
	call 	read_eeprom
	EnsureWnonZero
	movwf	srvpos_2_r

	movlw   etime_pow_off-eeprom_start
	call 	read_eeprom
	movwf	time_pow_off
#endif

	btfsc	Invert_Input
	  goto	look_ta_ibn	; Flag = 1 = kein Pull-up

	;===> 3
	banksel	WPUB		; select bank 3
	movlw	B'11110000'	; input = 1, output = 0
	movwf	WPUB		; internal Pull-up on
	;===> 0
	banksel	PIR1		; select bank 0

look_ta_ibn:
#if USE_IBN==1
	btfsc	TA_IBN
	  goto	continue_init	; Eingang = 1 = Taste nicht gedrückt
	bsf	IBN_Modus
	movlw	B'00000001'	; ADC aktivieren nur wenn erforderlich!
	movwf	ADCON0

	; warte bis Taste losgelassen:
	call	wait_debounce
  #if INT_OSC==1
	bsf	LED_IBN
  #endif
continue_init:
#endif

	movlw	.0
	movwf	ziel_pos_1	; ziel_pos_1 == 0
	call	do_adc_1	; in W steht ziel_pos_1
	movwf	ist_pos_1	; ist_pos_1 = ziel_pos_1

	movlw	.0
	movwf	ziel_pos_2	; ziel_pos_2 == 0
	call	do_adc_2	; in W steht ziel_pos_2
	movwf	ist_pos_2	; ist_pos_2 = ziel_pos_2

        ; Interrupt freigeben nach der ersten ADC-Wandlung
	movlw	B'11100000'  	; GIE, PIEE (für Timer 1), T0IE
        movwf	INTCON

;**************************************************************
; Hauptschleife

main:	
	movfw	PORTB
	btfsc	Invert_Input
	  xorlw	B'11111111'	; done when inverting is required
	movwf	tflags

	; Tasten prüfen:
	btfsc	TA_LI_1
	  goto	test_r_1	; 1 = nicht gedrückt
	bsf	Move_L_1	; Links! (dominant)
	bcf	Move_R_1
	bcf	PowOffTimeStart
	goto	test_l_2
	
test_r_1:
	btfsc	TA_RE_1
	  goto	test_l_2	; 1 = nicht gedrückt
	bsf	Move_R_1	; Rechts!
	bcf	Move_L_1
	bcf	PowOffTimeStart

test_l_2:
	btfsc	TA_LI_2
	  goto	test_r_2	; 1 = nicht gedrückt
	bsf	Move_L_2	; Links! (dominant)
	bcf	Move_R_2
	bcf	PowOffTimeStart
	goto	copy_flags
	
test_r_2:
	btfsc	TA_RE_2
	  goto	copy_flags	; 1 = nicht gedrückt
	bsf	Move_R_2	; Rechts!
	bcf	Move_L_2
	bcf	PowOffTimeStart

copy_flags:	
	DisableInt
	movfw	flags		; in Ruhe umkopieren!
	movwf	mflags		; nur mflags wird ausgewertet!
	call	writeflags
	EnableInt

	bcf	ADCON0,5
#if USE_IBN==0
	call	do_adc_1
	call	do_adc_2
#endif

#if USE_IBN==1
	btfsc	IBN_Modus
	  goto	SetupMode	; 1 = IBN-Modus

#if SWITCH_POW_OFF==1
	btfss	PowSwitchOff ; Freigabe aus 'modus'/'ba_modus'
	  goto	main_pow_on	; 0 = kein PowerOff
	
	btfss	PWR_ON_EXT ; Freigabe von PORTC,2: 1=Abschaltung aktiv, 0=keine Abschaltung
	  goto	main_pow_on	; 0 = kein PowerOff
	  
	; beide Servos müssen in Endlage sein, bevor abgeschaltet wird:
	btfss	EndPos_SV1
	  goto	main_pow_on	; 0 = Endlage noch nicht erreicht
	btfss	EndPos_SV2
	  goto	main_pow_on	; 0 = Endlage noch nicht erreicht
	  
	; Endlagen erreicht:
	btfsc	PowOffTimeStart	; wird durch Tastendruck gelöscht
	  goto	check_po_time	; 1 = Wartezeit gestartet: prüfen, ob abgelaufen
	  
	; Wartezeit für PowerOff starten
	bsf		PowOffTimeStart
	; nutzt 10ms-Takt von Timer 0
	movfw	time_pow_off
	movwf	time_pow_off_c

check_po_time:
	movfw	time_pow_off_c
	iorlw	.0		; setzt ZeroFlag
	skpz
	  goto	main_pow_on

	bcf	POWER_ON	; Wartezeit für PowerOff abgelaufen -> abschalten
	goto	main
#endif	; #if SWITCH_POW_OFF==1
#endif	; #if USE_IBN==1

main_pow_on:
	btfss	Power_ON_1	; aus ISR, wenn diese für jedes Servo...
	  goto	main
	btfss	Power_ON_2	; ...wenigstens einmal durchlaufen wurde
	  goto	main
	bsf	POWER_ON
	goto	main

#if USE_IBN==1
SetupMode:
	bsf	POWER_ON

	call	do_adc_1
	call	do_adc_2

	btfsc	TA_IBN
	  goto	main		; Eingang = 1 = Taste nicht gedrückt

	; warte bis Taste losgelassen:
	call	wait_debounce
	
	; write values to eeprom:
	movfw	ziel_pos_1
	call	write_eeprom_data

	movlw   esrvpos_1_l-eeprom_start
	btfss	IMove_L_1
	  movlw	esrvpos_1_r-eeprom_start
   
	call 	write_eeprom
	call	wait_50ms
	
	movfw	ziel_pos_2
	call	write_eeprom_data

	movlw   esrvpos_2_l-eeprom_start
	btfss	IMove_L_2
	  movlw esrvpos_2_r-eeprom_start
   
	call 	write_eeprom
	call	wait_50ms
#endif
	
	goto	main

;**************************************************************
; 		Taster		Poti		Kanal
; Ta 1 re	PORTB,4		PORTA,2		2	010
; Ta 1 li	PORTB,5		PORTC,0		4	100
; Ta 2 re	PORTB,6		PORTA,0		0	000
; Ta 2 li	PORTB,7		PORTA,1		1	001
;
;**************************************************************
; Analogwert über ADC-Wandler einlesen
; Ergebnis (1..255) steht in 'ziel_pos_1'
do_adc_1:
	btfsc	IMove_L_1
	  goto	readL		; 1 = aktiv
	btfss	IMove_R_1
	  return			; 0 = nicht aktiv
				
	; 1 Rechts...				
#if USE_IBN==1
	btfsc	IBN_Modus
	  goto	channel_2
	
	movfw	srvpos_1_r
	goto	check_1
channel_2:
#endif
	ADC_Ch2
	goto	strt_adc

readL:		; 1 Links...
#if USE_IBN==1
	btfsc	IBN_Modus
	  goto	channel_4
	
	movfw	srvpos_1_l
	goto	check_1
channel_4:
#endif
	ADC_Ch4

strt_adc:
	DoTheADC
check_1:	
	EnsureWnonZero
	movwf	ziel_pos_1
	return

;**************************************************************
; Analogwert über ADC-Wandler einlesen
; Ergebnis (1..255) steht in 'ziel_pos_2'
do_adc_2:
	btfsc	IMove_L_2
	  goto	readL_2		; 1 = aktiv
	btfss	IMove_R_2
	  return			; 0 = nicht aktiv
				
	; 2 Rechts...				
#if USE_IBN==1
	btfsc	IBN_Modus
	  goto	channel_0

	movfw	srvpos_2_r
	goto	check_2
channel_0:
#endif
	ADC_Ch0
	goto	strt_adc_2

readL_2:	; 2 Links...
#if USE_IBN==1
	btfsc	IBN_Modus
	  goto	channel_1
	
	movfw	srvpos_2_l
	  goto	check_2
channel_1:
#endif
	ADC_Ch1

strt_adc_2:
	DoTheADC
check_2:	
	EnsureWnonZero
	movwf	ziel_pos_2
	return

;**************************************************************
; Warteschleife 1 ms für einen 4MHz-PIC-Takt (1Takt = 1µs) 
; 6 * NOP = 9 Takte * 110 + 4 =  994 Takte(µs)
; 2 * NOP = 5 Takte * 199 + 4 =  999 Takte(µs)
;                     255     = 1254 Takte(µs)
; 1 * NOP = 4 Takte * 249 + 4 = 1000 Takte(µs)
WAIT_minimum:
#if LONG_PULS==1
        movlw   .125		; Zeitkonstante für 0.5ms 
#else
        movlw   .249		; Zeitkonstante für 1ms 
#endif
Wait:	movwf   loops 

Wait_l:
	nop  
#if LONG_PULS==1
        nop       		; jedes nop verlängert die Zeit
        nop        		; um ca. 0,255 ms, mit insgesamt 
        nop        		; 5 nop's wird also der Analogwert 1..255
        nop  			; auf eine Zeit von 0..2,044ms umgesetzt
#endif
        decfsz  loops, F 	; Zeit vorbei? 
          goto	Wait_l		; nein, noch nicht 
	return

;**************************************************************
writeflags:
	movfw	mflags
	subwf	cflags,0	; W = cflags - W
	skpnz
	  return

	; Statusänderung: im EEPROM merken
	; für's nächste Power-On!
#if INT_OSC==1
	btfss	IBN_Modus
#endif
	movfw	mflags
	movwf	cflags
	call	write_eeprom_data

        movlw   eflags-eeprom_start
       	call 	write_eeprom
#if INT_OSC==1
	btfss	IBN_Modus
#endif
	return

;**************************************************************
; liest ein Zeichen aus dem EEPROM-Bereich
; in W wird die EEPROM-Adresse (0..) übergeben,
; das gelesene Zeichen ist wieder in W
read_eeprom:
	
	;===> 2
	banksel	EEADR		; select bank 2
	movwf   EEADR
	;===> 3
	banksel	EECON1		; select bank 3
	bcf	EECON1, EEPGD
	bsf     EECON1, RD
	;===> 2
	banksel	EEDAT		; select bank 2
	movf    EEDAT, w
	;===> 0
	banksel	PIR1		; select bank 0
	return
 
;**************************************************************
; schreibt einen Wert in den EEPROM-Bereich
; in W wird die EEPROM-Adresse (0..) übergeben.
;
; der zu schreibende Wert (befindet sich in W) muss zuvor
;  in EEDATA geschrieben worden sein
; -> call write_eeprom_data
;
write_eeprom_data:
	;===> 2
	banksel	EEDAT		; select bank 2
	movwf	EEDAT		; Wert für EEPROM
	;===> 0
	banksel	PIR1		; select bank 0
	return

write_eeprom:
	;===> 2
	banksel EEADR		; select bank 2
	movwf	EEADR		; Adresse 
	;===> 3
	banksel	EECON1		; select bank 3
	bcf	EECON1, EEPGD
	bsf 	EECON1, WREN	; Enable write
	DisableInt

	; DON'T change the following 5 lines!
	movlw 	0x55 		;
	movwf 	EECON2 		; Write 55h
	movlw 	0xAA 		;
	movwf 	EECON2 		; Write AAh
	bsf 	EECON1,WR	; Set WR bit, begin write

#if WRITE_EEPROM_INT==1
	;===> 1
	banksel	PIE2		; select bank 1
	bsf	PIE2, EEIF	; enable ISR for eeprom write
#else
	;===> 0
	banksel	PIR2		; select bank 0
write_loop:
	btfss	PIR2, EEIF
	  goto	write_loop	; wait until write is finished

	;===> 3
	banksel	EECON1		; select bank 3
	bcf 	EECON1, WREN	; Disable write
#endif

	;===> 0
	banksel	PIR2		; select bank 0
	EnableInt
        return

#if USE_IBN==1
;**************************************************************
wait_debounce:
	btfss	TA_IBN
	  goto 	wait_debounce	; Eingang = 0 = Taste gedrückt

wait_50ms:
	movlw	.50		; debounce-time: 50ms
	movwf	loop_50
wait_1ms:			; Warteschleife 1ms für einen 4MHz-PIC-Takt 
	movlw   .110           	; Zeitkonstante für 1ms 	
	movwf   loops2 
waitloops2:	
	nop  
	nop 
	nop 
	nop  
	nop 
	nop 
   	decfsz  loops2, F	; 1 ms vorbei? 
   	  goto	waitloops2	; nein, noch nicht 

   	decfsz  loop_50, F	; Zeit vorbei? 
   	  goto	wait_1ms	; nein, noch nicht 
   	retlw	0		; das Warten hat ein Ende
#endif

	end
; end of file
