.segment "HEADER"
    .byte "NES", $1A    ; 1-4: Signature
    .byte 2             ; 5:   2x 16KB PRG ROM (32KB total)
    .byte 1             ; 6:   1x 8KB CHR ROM
    .byte $01           ; 7:   Mapper 0 (NROM), Vertical Mirroring (Better for Side-View)
    .byte $00           ; 8:   Mapper 0 (Upper Nybble)
    .byte $00           ; 9:   
    .byte $00           ; 10:  
    .byte $00           ; 11:  
    .byte $00, $00, $00, $00, $00 ; 12-16: Padding