	name	io
	title	'IO.ASM -- fossil interface drivers for citadel'

NULL    segment para public 'BEGDATA'
NULL    ends
CONST   segment word public 'CONST'
CONST   ends
_BSS    segment word public 'BSS'
_BSS    ends
_DATA	segment byte public 'DATA'
_DATA	ends

;---- FOSSIL function equates -----

SETRATE	equ	00h
WRITE	equ	01h
READ	equ	02h
STATUS	equ	03h
INIT	equ	04h
CLOSE	equ	05h
DTR	equ	06h

OFLUSH	equ	08h
OPURGE  equ     09h

IPURGE	equ	0ah
XMIT    equ     0bh     ;transmit, no wait
PEEK    equ     0ch     ;look at next char in receive buffer

FLOW    equ     0fh

BREAK	equ	1ah
WATCH   equ     14h

;----------------------------------

DGROUP  group NULL, _DATA, CONST, _BSS

        assume  cs:_TEXT, ds:DGROUP, es:nothing

_BSS    segment word public 'BSS'
	public	_byteRate
_byteRate	dw	(?)
_BSS    ends

_TEXT	segment byte public 'CODE'

port	dw	0

fossil	dw	0

sav_ofs	dw	0
sav_seg	dw	0

old_inta01	db	0

m_data	dw	0
m_stat	dw	0
l_stat	dw	0
m_ctrl	dw	0
l_ctrl	dw	0
m_ier	dw	0
vector	db	0
pic_mask	db	0


BUFSIZE	equ	108h    ;big enough for Xmodem block

combuf	db      BUFSIZE dup (?)
q_head	dw	0
q_tail	dw	0

	public	_gotcarrier
_gotcarrier	proc far
;
; gotcarrier() -- is carrier out there?
;
	push	dx
	cmp	cs:fossil, 0
	je	_gcx

	mov	dx, cs:port
	mov	ah, STATUS
	int	14h
	and	ax, 0080h
	pop	dx
	ret

_gcx:	mov	dx, cs:l_stat
	in	al, dx
	and	ax, 0080h
	pop	dx
	ret
_gotcarrier	endp


	public	_mdhit
_mdhit	proc	far
;
; mdhit() -- returns nonzero if a character is ready from the modem
;
	cmp	cs:fossil, 0
	je	_MIx

	push	dx
	mov	dx, cs:port
	mov	ah, STATUS
	int	14h
	and	ax, 0100h
	pop	dx
	ret

_MIx:	mov	ax, cs:q_head
	sub	ax, cs:q_tail
	ret
_mdhit	endp


	public	_modemOpen
_modemOpen	proc	far
;
; modemOpen() -- enable the modem
;
	push	dx
	cmp	cs:fossil, 0
	je	_mOx1

	mov	dx, cs:port
	mov	ah, DTR
	mov	al, 1
	int	14h

	mov	dx, cs:port
	mov	al, 0f0h        ;no flow control from FOSSIL, please
	mov	ah, FLOW
	int	14h

	jmp short _mOe

_mOx1:	mov	dx, cs:m_ctrl		; enable DTR
	in	al, dx			; by orring on the low bit in the
	or	al, 1			; modem control byte
	out	dx, al
	
	mov	al, cs:old_inta01
	and	al, cs:pic_mask		; c = old_inta01 & pic_mask
	out	21h, al			; outp(inta01, c)

	mov	al, 20h			; wake up the pic

_mOe:	pop	dx
	ret
_modemOpen	endp


	public	_modemClose
_modemClose	proc	far
;
; modemClose() -- disable the modem
;
	push	dx
	cmp	cs:fossil, 0
	je	_mCx

	mov	dx, cs:port
	mov	ah, DTR
	mov	al, 0
	int	14h

	jmp short _mCe

_mCx:	mov	dx, cs:m_ctrl		; disable DTR
	in	al, dx			; by anding out the low bit in
	and	al, 0feh		; the modem control byte
	out	dx, al
	
	mov	al, cs:old_inta01	; turn off the pic for this interrupt
	out	21h, al

_mCe:	pop	dx
	ret
_modemClose	endp


	public	_getraw
_getraw	proc	far
;
; getraw() -- get an eight-bit character from the modem
;
	push	dx
	cmp	cs:fossil, 0
	je	_grx

comment ~       ;orc's code could hang the system
	mov	dx, cs:port
	mov	ah, READ
	int	14h
	sub	ah, ah
	pop	dx
	ret
endcomment ~

	mov	dx, cs:port
	mov	ah, PEEK
	int	14h
        cmp     ah, 0
        jne     noRaw

	mov	dx, cs:port
	mov	ah, READ
	int	14h
	pop	dx
	ret

noRaw:  xor     ax, ax          ;return a NUL char!
        pop     dx
        ret

        ;the "no fossil" code

_grx:	push	di
	mov	dx, cs:q_tail		; busy-wait until a character is here
grl:	cmp	dx, cs:q_head
	je	grl

	mov	di, dx
	xor	ax, ax
	mov	al, cs:combuf[di]	; grab the character into al
	inc	dx
	cmp	dx, BUFSIZE
	jne	gr2
	xor	dx, dx
gr2:	mov	cs:q_tail, dx		; store the updated tail pointer

	pop	di
	pop	dx
	ret
_getraw	endp


	public	_mflush
;
; mflush() -- clear all pending input from the modem
;
_mflush		proc far
	cmp	cs:fossil, 0
	je	_mfx

	push	dx
	mov	dx, cs:port
	mov	ah, IPURGE
	int	14h
	pop	dx
	ret

_mfx:	cli
	xor	ax, ax
	mov	cs:q_head, ax
	mov	cs:q_tail, ax
	sti
	ret
_mflush		endp


    public	_modout
;
; modout() -- stuff a character out the modem
;
_modout		proc far
	push	bp
	mov	bp, sp
	push	dx
	cmp	cs:fossil, 0
	je	_mox

comment ~       ;orc's code
	mov	dx, cs:port		; ready the character for output
	mov	ah, WRITE		; (fossil may queue it up for
	mov	al, byte ptr [bp+6]	; later output, so....)
	int	14h

	mov	dx, cs:port		; don't return until the char is
	mov	ah, OFLUSH		; written out to the acu.
	int	14h			; (pray for no* flow control)
end comment ~

	mov	dx, cs:port
	mov	al, byte ptr [bp+6]
        mov     ah, XMIT
	int	14h
        test    ax, 0001h               ;was char accepted?
        jz      oweird                  ;no - something's very wrong

	mov	dx, cs:port
	mov	ah, OFLUSH              ;force the char out of the buffer
	int	14h
	pop	dx
	pop	bp
	ret

oweird:	mov	dx, cs:port
	mov	ah, OPURGE		;purge the ouput buffer
	int	14h                     
	pop	dx
	pop	bp
	ret

_mox:	mov	dx, cs:m_ctrl
	mov	al, 0bh
	out	dx, al

	mov	dx, cs:m_stat
mol:	in	al, dx
	and	al, 020h
	je	mol

	mov	dx, cs:m_data
	mov	al, byte ptr [bp+6]
	out	dx, al
	
	pop	dx
	pop	bp
	ret
_modout		endp


	public	_fixmodem
_fixmodem	proc far
;
; fixmodem() -- restore the modem to (more or less) the state it was in
;		before we started diddling with it.
;
	push	dx
	cmp	cs:fossil, 0
	je	_fmx

	mov	dx, cs:port
	mov	ah, CLOSE
	int	14h
	pop	dx
	ret

_fmx:	push	ds

	mov	al, cs:old_inta01
	out	21h, al

	mov	ds, cs:sav_seg
	mov	dx, cs:sav_ofs
	mov	ah, 25h
	mov	al, cs:vector
	int	21h

	pop	ds
	pop	dx
	ret
_fixmodem	endp


handler	proc	far
;
; handler() -- the interrupt handler.  NEVER NEVER NEVER call this yourself.
;
	sti
	push	ax
	push	bx
	push	dx
	push	ds

	mov	dx, cs:m_stat	; don't interrupt if a character isn't waiting
	in	al, dx
	and	al, 1
	je	intr2

	mov	dx, cs:m_data
	in	al, dx		; grab the character we want...
	cli			; no interrupts, please, while we update the
				; queue
	mov	bx, cs:q_head		; store the character...
        mov     cs:[bx+combuf], al      ; and update the head pointer
	inc	bx
	cmp	bx, BUFSIZE
	jne	intr0
	xor	bx,bx

intr0:	cmp	cs:q_tail, bx	; buffer overrun?
	je	intr1
	mov	cs:q_head, bx	; if not, save the new head pointer
intr1:	sti			; allow interrupts again

intr2:	mov	dx, cs:m_ctrl	; reenable the modem....
	mov	al, 0bh
	out	dx, al

	mov	al, 20h		; and tell the pic that the interrupt is over
	out	20h, al

	pop	ds
	pop	dx
	pop	bx
	pop	ax

	iret
handler	endp


	public	_enablecom
_enablecom	proc far
;
; enablecom() -- enable our modem handler
; returns 1 for fossil, 

	push	bp
	mov	bp, sp

	push	dx
	push	bx

	mov	dx, word ptr [bp+6]		; get port number
	mov	cs:port, dx			; save it for citadel use
	sub	bx, bx
	mov	ah, INIT
	int	14h

	cmp	ax, 1954h			; is a fossil out there?
	jne	loadown
	mov	ax, 1                           ; 1 flags fossil
	mov	cs:fossil, ax

	pop	bx				; if so, signal it and
	pop	dx				; leave now.
	pop	bp
	ret
;
; no fossil or cio out there - install our own driver.
;
loadown:xor	ax, ax				; return 0 for no load...
	mov	cs:fossil, ax
	push	ds
	push	es

	mov	ax, [bp+6]		; get the com port the i/o will be on
	cmp	ax, 1			; com2: ?
	jne	com1
	mov	cs:m_data,02f8h
	mov	cs:m_stat,02fdh
	mov	cs:m_ctrl,02fch
	mov	cs:l_ctrl,02fbh
	mov	cs:m_ier, 02f9h
	mov	cs:l_stat,02feh
	mov	cs:vector,11
	mov	cs:pic_mask,  0f7h
	jmp	siv1
com1:					; anything else is com1:
	mov	cs:m_data,03f8h
	mov	cs:m_stat,03fdh
	mov	cs:m_ctrl,03fch
	mov	cs:l_ctrl,03fbh
	mov	cs:m_ier, 03f9h
	mov	cs:l_stat,03feh
	mov	cs:vector,12
	mov	cs:pic_mask,  0efh

siv1:	mov	ah, 35h			; grab the address of the old handler
	mov	al, cs:vector		; and stuff it into a safe place
	int	21h
	mov	cs:sav_seg, es
	mov	cs:sav_ofs, bx
	
	call	_mflush			; flush the device buffer
	
	mov	dx, seg handler		; TRY* to load our bloody handler
	mov	ds, dx
	mov	dx, offset handler
	mov	ah, 25h
	mov	al, cs:vector
	int	21h

	mov	dx, cs:m_ctrl		; outp(mdm_ctrl, 0x0b)
	mov	al, 0bh
	out	dx, al

	mov	dx, cs:m_stat		; inp(modem_status)
	in	al, dx
	mov	dx, cs:m_stat		; inp(line_status)
	in	al, dx
	mov	dx, cs:m_data		; inp(modem_data)
	in	al, dx
	mov	dx, cs:l_ctrl		; c = inp(ln_ctrl)
	in	al, dx
	and	al, 7fh			; c &= 0x7f
	out	dx, al			; outp(ln_ctrl, c)

	mov	dx, cs:m_ier		; outp(ier, 1)
	mov	al, 1
	out	dx, al

	in	al, 21h			; old_inta01 = inp(inta01)
	mov	cs:old_inta01, al
	and	al, cs:pic_mask		; c = old_inta01 & pic_mask
	out	21h, al			; outp(inta01, c)

	mov	al, 20h			; wake up the pic
	out	20h, al

        mov     ax, 0ffffh              ; -1 flags our own driver
	pop	es
	pop	ds
	pop	bx			; fossil/cio branches reenter here.
	pop	dx

	pop	bp
	ret
_enablecom	endp


codes	db	043h		;   300 baud
	db	083h		;  1200 baud
	db	0a3h		;  2400 baud
	db	0e3h		;  9600 baud
	db	003h		; 19200 baud
	db	023h		; 38400 baud
rates	dw	  30		; the actual byterates (in byteRate)
	dw	 120
	dw	 240
	dw	 960
	dw	1920
	dw	3840

_rcl	db	80h		; low byte of baudrate divisor (300 baud)
	db	60h		; 1200 baud
	db	30h		; 2400 baud
	db	0ch		; 9600 baud
	db	06h		;19200 baud
	db	03h		;38400 baud
_rch	db	1		; high byte of baudrate divisor (300 baud)
	db	0		; 1200 baud
	db	0		; 2400 baud
	db	0		; 9600 baud
	db	0		;19200 baud
	db	0		;38400 baud

	public	_setBaud
_setBaud	proc	far
;
; setBaud() -- set the baud rate
;
	push	bp
	mov	bp, sp
	push	dx
	push	di
	mov	dx, word ptr[bp+6]	; get the desired baudrate
	mov	di, dx			; into di

	cmp	cs:fossil, 0
	je	_sBx

	mov	ah, SETRATE
	mov	al, byte ptr cs:codes[di]	; pull baud code from table
	mov	dx, cs:port			; and set up for int14 call
	int	14h
	jmp short _sBe

_sBx:	mov	dx, cs:l_ctrl			; tweak out the baud rate
	mov	al, 83h				; divisor into magic sio
	out	dx, al				; memory.
	mov	dx, cs:m_data
	mov	al, byte ptr cs:_rcl[di]
	out	dx, al
	mov	dx, cs:m_ier
	mov	al, byte ptr cs:_rch[di]
	out	dx, al
	mov	dx, cs:l_ctrl
	mov	al, 03h
	out	dx, al

_sBe:	add	di, di				; after setting baudrate, set
	mov	ax, word ptr cs:rates[di]	; up byteRate variable.
	mov	_byteRate, ax

	pop	di
	pop	dx
	pop	bp
	ret
_setBaud	endp


	public	_watchon
_watchon	proc far        ;enable FOSSIL watchdog
	push	dx
	cmp	cs:fossil, 0
	je	won
	mov	al, 1
	mov	ah, WATCH
	mov	dx, cs:port
	int	14h
won:	pop	dx
	ret
_watchon	endp


	public	_watchoff
_watchoff	proc far        ;disable FOSSIL watchdog
	push	dx
	cmp	cs:fossil, 0
	je	woff
	mov	al, 0
	mov	ah, WATCH
	mov	dx, cs:port
	int	14h
woff:	pop	dx
	ret
_watchoff	endp

_TEXT	ends

	end
