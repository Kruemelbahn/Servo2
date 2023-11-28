;**************************************************************
;*  	Servo_isr.inc
;*	-------------------------------------------------------	
;*	Macro zur Verwendung in der ISR zur Ansteuerung eines 
;*	Servo's.	
;*	
;**************************************************************
;
; M.Zimmermann 22.05.2020
;
;**************************************************************
;
; wird verwendet von: DSA-2.inc, ServoAnsteurungCore.inc
;
;**************************************************************

servo_nr = 0
IsrForServo	macro
servo_nr++			; increment count of instantations
				; irgendeine Richtung für Servo gewählt?
	btfsc	IMove_L_#v(servo_nr)
	  goto	SERVO_ok_#v(servo_nr)
	btfsc	IMove_R_#v(servo_nr)
	  goto	SERVO_ok_#v(servo_nr)

				; '''''''''''''''''''''''''''''''''''''''''''
				; Nein: keine Richtung gesetzt = LED ein/aus
	btfss	Blink_LED
	  goto	switch_to_#v(3-servo_nr)_no_power

        decfsz  counter_#v(servo_nr),F	; 0,5s vorbei? 
	  goto	switch_to_#v(3-servo_nr)_no_power

	movlw	LOOP_LED_#v(servo_nr)
	movwf	counter_#v(servo_nr)

	btfsc	LED_flg_#v(servo_nr)
	  goto	LED_#v(servo_nr)_inv
	bsf	LED_LI_#v(servo_nr)
	bcf	LED_RE_#v(servo_nr)
	bsf	LED_flg_#v(servo_nr)
	goto	switch_to_#v(3-servo_nr)_no_power
	
LED_#v(servo_nr)_inv:
	bcf	LED_LI_#v(servo_nr)
	bsf	LED_RE_#v(servo_nr)
	bcf	LED_flg_#v(servo_nr)
	goto	switch_to_#v(3-servo_nr)_no_power
	
				; '''''''''''''''''''''''''''''''''''''''''''
				; Richtung vorhanden: Normalbetrieb		
SERVO_ok_#v(servo_nr):		; SERVO ansteuern
	bcf	T1CON, TMR1ON	; Timer 1 stoppen
	movlw	.255
	movwf	TMR1H
#if LONG_PULS==0
	clrc
	rrf	ist_pos_#v(servo_nr), 0	; geteilt durch 2
	addlw	OFFSET_05	; + 0,5ms
 	skpnc
	  decf	TMR1H, 1
#else
	movfw	ist_pos_#v(servo_nr)
#endif 	
	addlw	OFFSET_05	; + 0,5ms
	xorlw	.255
	movwf	TMR1L
 	skpnc
	  decf	TMR1H, 1
	bsf	T1CON, TMR1ON	; Timer 1 starten
	bsf	SERVO_#v(servo_nr); Impuls ein

#if USE_IBN==1
	btfsc	IBN_Modus
	  goto	mov_ziel_#v(servo_nr)
	; Zielposition vorgeben
	movfw	srvpos_#v(servo_nr)_l
	btfss	IMove_L_#v(servo_nr)
	  movfw	srvpos_#v(servo_nr)_r
	movwf	ziel_pos_#v(servo_nr)
mov_ziel_#v(servo_nr):
#endif
	movfw	ziel_pos_#v(servo_nr)

				; Position über Rampe anfahren
	decfsz  inc__20_1, F
	  goto	SERVO_istpos#v(servo_nr)

	movfw	inc__20fix_1
	movwf	inc__20_1
	movfw	ziel_pos_#v(servo_nr)

	subwf	ist_pos_#v(servo_nr),0	; w(ziel_pos) = ist_pos - ziel_pos
	skpnz
	  goto	reached_#v(servo_nr)
				; Position noch nicht erreicht
	bcf	LED_LI_#v(servo_nr)
	bcf	LED_RE_#v(servo_nr)
	bcf	EndPos_SV#v(servo_nr)
	bcf	PowOffTimeStart
	movfw	inc__dec_1	; w enthält jetzt den Rampensummand

	skpnc
	  goto	decrement_#v(servo_nr)

increment_#v(servo_nr):
				; C=1 -> Wert erhöhen
	addwf	ist_pos_#v(servo_nr),0
	skpnc			; mehr als 255 geht nicht
	  goto	reached_#v(servo_nr)
	movwf	ist_pos_#v(servo_nr)
	subwf	ziel_pos_#v(servo_nr),0
	skpnz
	  goto	reached_#v(servo_nr)
	skpc
	  goto	reached_#v(servo_nr)
	movfw	ist_pos_#v(servo_nr)
	goto	SERVO_time#v(servo_nr)

decrement_#v(servo_nr):
				; C=0 -> Wert erniedrigen
	subwf	ist_pos_#v(servo_nr),0
	skpnz
	  goto	reached_#v(servo_nr)	; weniger als 0 geht nicht
	skpc
	  goto	reached_#v(servo_nr)	; weniger als 0 geht nicht
	movwf	ist_pos_#v(servo_nr)
	subwf	ziel_pos_#v(servo_nr),0
	skpnz
	  goto	reached_#v(servo_nr)	; weniger als 0 geht nicht
	skpnc
	  goto	reached_#v(servo_nr)	; weniger als 0 geht nicht

SERVO_istpos#v(servo_nr):
	movfw	ist_pos_#v(servo_nr)
	goto	SERVO_time#v(servo_nr)

reached_#v(servo_nr):
	movfw	ziel_pos_#v(servo_nr)
	movwf	ist_pos_#v(servo_nr)	; ziel_pos -> ist_pos
				; LED einschalten
	btfsc	IMove_L_#v(servo_nr)
	  goto	set_L_#v(servo_nr)
	bsf	LED_RE_#v(servo_nr)
	bsf	EndPos_SV#v(servo_nr)
	goto	SERVO_time#v(servo_nr)
set_L_#v(servo_nr):
	bsf	LED_LI_#v(servo_nr)
	bsf	EndPos_SV#v(servo_nr)

SERVO_time#v(servo_nr):
	bsf	Power_ON_#v(servo_nr)	; ISR wurde für diesen Servo wenigstens einmal durchlaufen
switch_to_#v(3-servo_nr)_no_power:
		endm
