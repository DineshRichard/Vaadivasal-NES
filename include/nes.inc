; ---------------------------------------------------------------------------
; NES HARDWARE DEFINITIONS (nes.inc)
; ---------------------------------------------------------------------------

; --- PPU (Picture Processing Unit) ---
PPU_CTRL        = $2000     ; VPHB SINN (NMI enable, Master/Slave, Sprite Height, Bg Table, Spr Table, Increment, NameTable)
PPU_MASK        = $2001     ; BGRs bMmG (Color emphasis, Sprite enable, BG enable, Left col hide)
PPU_STATUS      = $2002     ; VSO- ---- (VBlank, Sprite 0 Hit, Sprite Overflow)
OAM_ADDR        = $2003     ; OAM Read/Write Address
OAM_DATA        = $2004     ; OAM Data
PPU_SCROLL      = $2005     ; Fine Scroll Position (Write X, then Y)
PPU_ADDR        = $2006     ; VRAM Address (Write MSB, then LSB)
PPU_DATA        = $2007     ; VRAM Data

; --- APU (Audio Processing Unit) & I/O ---
APU_PULSE1_VOL  = $4000
APU_PULSE1_SWEEP= $4001
APU_PULSE1_LO   = $4002
APU_PULSE1_HI   = $4003
APU_PULSE2_VOL  = $4004
APU_PULSE2_SWEEP= $4005
APU_PULSE2_LO   = $4006
APU_PULSE2_HI   = $4007
APU_TRI_LINEAR  = $4008
APU_TRI_LO      = $400A
APU_TRI_HI      = $400B
APU_NOISE_VOL   = $400C
APU_NOISE_LO    = $400E
APU_NOISE_HI    = $400F
APU_DMC_FREQ    = $4010
APU_DMC_RAW     = $4011
APU_DMC_START   = $4012
APU_DMC_LEN     = $4013

OAM_DMA         = $4014     ; Sprite DMA Transfer (Upload 256 bytes to OAM)
APU_STATUS      = $4015     ; Sound Channel Enable/Disable
JOYPAD1         = $4016     ; Joystick 1 Data / Strobe
JOYPAD2         = $4017     ; Joystick 2 Data / Frame Counter Mode

; --- MEMORY MAP CONSTANTS ---
OAM_RAM         = $0200     ; Location of Shadow OAM in RAM