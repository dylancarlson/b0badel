;       pexec.asm for Zortech C

include MACROS.ASM

begcode

public	_pexec
_pexec	proc	far
;
;	pexec(mode, program, tail, environment)
;
;	stack on entry to the procedure:
;
;				|-------|----------------|
envseg	equ	[bp+18]	;	|  +18	| seg environment|
envofs	equ	[bp+16]	;	|  +16	| ofs environment|
tailseg	equ	[bp+14]	;	|  +14	| seg tail       |
tailofs	equ	[bp+12]	;	|  +12	| ofs tail       |
prgseg	equ	[bp+10]	;	|  +10	| seg program    |
prgofs	equ	[bp+8]	;	|  + 8	| ofs program    |
mode	equ	[bp+6]	;	|  + 6	| mode           |
;				|-------|----------------|
;
	push	bp
	mov	bp, sp
	push	bx
	push	cx
	push	di
	push	si
	push	ds
	push	es

	mov	ax, ss
	mov	cs:save_ss, ax
	mov	ax, sp
	mov	cs:save_sp, ax

	mov	ax, tailseg		; move address of command tail into
	mov	cs:param+4, ax		; the parameter block
	mov	ax, tailofs
	mov	cs:param+2, ax

	mov	ax, envofs		; move the segment address of the
	mov	cs:param+0, ax		; environment into the parameter
					; block
	mov	ax, cs			; load segment part of fcb addresses
	mov	cs:param+12, ax
	mov	cs:param+16, ax

	mov	ax, cs			; es:bx has address of parameter
	mov	es, ax			; block.
	mov	bx, offset cs:param

	mov	ax, prgseg		; ds:dx has address of program
	mov	ds, ax
	mov	dx, prgofs

	xor	ax, ax			; do the interrupt with mode = 0
	mov	ah, 4bh
	int	21h
	jc	drat			; carry set so it failed

	mov	bx, cs:save_ss		; pexec worked, so all registers are
	mov	ss, bx			; trashed.  Restore the stack before
	mov	bx, cs:save_sp		; we do anything else.
	mov	sp, bx

	mov	ah, 4dh			; do a wait and get the program
	int	21h			; status into ax (sign bit is clear)
	xor	dx, dx
	jmp	done

drat:	mov	dx, 0ffffh		; mark dx for error
					; then fall into exit code
done:	pop	es
	pop	ds
	pop	si
	pop	di
	pop	cx
	pop	bx
	pop	bp
	ret
_pexec	endp

param	dw	0
	dw	0
	dw	0
	dw	0
	dw	0
	dw	offset fcb0
	dw	(?)
	dw	offset fcb1
	dw	(?)

save_ss	dw	(?)
save_sp	dw	(?)

fcb0	dw	40 dup (?)
fcb1	dw	40 dup (?)

endcode

	end
