; ===========================================================================
; VAADIVASAL (NES)
; Main Game Engine
; Developer: Dinesh Richard
; ===========================================================================

; --- FAMISTUDIO CONFIG ---
FAMISTUDIO_CFG_EXTERNAL       = 1
FAMISTUDIO_CFG_EN_NTSC        = 1
FAMISTUDIO_CFG_EN_PAL         = 0
FAMISTUDIO_CFG_THREAD         = 1 
FAMISTUDIO_CFG_SFX_SUPPORT    = 0 
FAMISTUDIO_CFG_SFX_STREAMS    = 2
FAMISTUDIO_CFG_DPCM_SUPPORT   = 0 
FAMISTUDIO_USE_FAMITRACKER_TEMPO = 1 

.define FAMISTUDIO_CA65_ZP_SEGMENT    ZEROPAGE
.define FAMISTUDIO_CA65_RAM_SEGMENT   BSS       
.define FAMISTUDIO_CA65_CODE_SEGMENT  CODE

; --- INCLUDES ---
.p02
.include "header.s"
.include "../include/nes.inc"
.include "../include/constants.inc"

; ---------------------------------------------------------------------------
; RAM VARIABLES (ZEROPAGE & BSS)
; ---------------------------------------------------------------------------
.segment "ZEROPAGE"
sound_ready:      .res 1 
nmi_done:         .res 1 
frame_counter:    .res 1
rng_seed:         .res 1
game_state:       .res 1
state_backup:     .res 1 

; Cheat Codes
cheat_index:      .res 1 
cheat_active:     .res 1 

; Game Physics & Logic
player_x:         .res 1
player_y:         .res 1
player_lives:     .res 1
player_score:     .res 1
buttons:          .res 1
prev_buttons:     .res 1
facing_left:      .res 1
walking:          .res 1
anim_frame:       .res 1
anim_timer:       .res 1

; Bull AI
bull_x:           .res 1
bull_y:           .res 1
bull_move_acc:    .res 1 
bull_velocity:    .res 1 
bull_state_timer: .res 2
bull_anim_timer:  .res 1
bull_frame_idx:   .res 1
bull_facing_left: .res 1

; QTE System
qte_needed_btn:   .res 1 
qte_current_cnt:  .res 1
qte_timer:        .res 1
qte_max_time:     .res 1 
qte_palette:      .res 1 
qte_result_ok:    .res 1 

; Rendering & UI
hit_timer:        .res 1
heart_update_req: .res 1 
score_update_req: .res 1 
current_pal_add:  .res 1 
shake_timer:      .res 1 
soft_ppu_mask:    .res 1 

; Drawing Pointers
oam_ptr:          .res 1
temp_x:           .res 1
temp_y:           .res 1
sprite_width:     .res 1
meta_ptr_lo:      .res 1
meta_ptr_hi:      .res 1
map_ptr_lo:       .res 1
map_ptr_hi:       .res 1

; ---------------------------------------------------------------------------
; RESET VECTOR
; ---------------------------------------------------------------------------
.segment "CODE"
RESET:
    sei                             ; Disable interrupts
    cld                             ; Disable decimal mode
    ldx #$40
    stx JOYPAD2                     ; Disable APU frame IRQ
    ldx #$FF
    txs                             ; Set stack pointer
    inx
    stx PPU_CTRL                    ; Disable NMI
    stx PPU_MASK                    ; Disable Rendering
    stx APU_DMC_FREQ                ; Disable DMC IRQ

    lda #0
    sta sound_ready
    sta cheat_active 
    sta cheat_index

vblankwait1:
    bit PPU_STATUS
    bpl vblankwait1

    ; Clear RAM
    lda #$00
    ldx #$00
clrmem:
    sta $0000, x
    sta $0100, x
    sta $0200, x
    sta $0300, x
    sta $0400, x
    sta $0500, x
    sta $0600, x
    sta $0700, x
    inx
    bne clrmem

vblankwait2:
    bit PPU_STATUS
    bpl vblankwait2

    ; Initialize PPU
    lda PPU_STATUS
    lda #$3F
    sta PPU_ADDR
    lda #$10
    sta PPU_ADDR
    ldx #$00
LoadSprPal:
    lda pal_sprites, x
    sta PPU_DATA
    inx
    cpx #16
    bne LoadSprPal

    jsr GoToTitle

; ---------------------------------------------------------------------------
; MAIN GAME LOOP
; ---------------------------------------------------------------------------
ForeverLoop:
    jsr WaitForNMI 
    jsr ReadController
    jsr RandomUpdate
    jsr CheckPauseInput 

    lda game_state
    cmp #STATE_PAUSED
    bne @CheckTitle
    jmp RunPaused        

@CheckTitle:
    cmp #STATE_TITLE
    bne @CheckGameOver
    jmp RunTitle

@CheckGameOver:
    cmp #STATE_GAMEOVER
    bne @CheckWin
    jmp RunGameOver

@CheckWin:
    cmp #STATE_WIN
    bne @CheckSpawn
    jmp RunWin

@CheckSpawn:
    cmp #STATE_SPAWN_WAIT
    bne @CheckChase
    jmp RunSpawnWait

@CheckChase:
    cmp #STATE_CHASE
    bne @CheckTired
    jmp RunChase

@CheckTired:
    cmp #STATE_TIRED
    bne @CheckQTE
    jmp RunTired

@CheckQTE:
    cmp #STATE_QTE
    bne @CheckFeedback
    jmp RunQTE

@CheckFeedback:
    cmp #STATE_FEEDBACK
    bne @CheckHit
    jmp RunFeedback

@CheckHit:
    cmp #STATE_HIT
    bne @CheckEscape
    jmp RunHit

@CheckEscape:
    cmp #STATE_ESCAPE
    bne @CheckWonWait
    jmp RunEscape

@CheckWonWait:
    cmp #STATE_WON_WAIT
    bne @Draw
    jmp RunWonWait

@Draw:
    jmp DrawEverything 

; --- STATE HANDLERS ---
RunTitle:
    ; Konami Code Logic
    lda buttons
    eor prev_buttons
    and buttons
    sta temp_x       

    lda temp_x
    beq @TitleDone  

    ldy cheat_index
    lda KonamiSequence, y
    cmp temp_x
    bne @WrongInput

    inc cheat_index
    lda cheat_index
    cmp #11          
    beq @CheatSuccess
    jmp @TitleDone

@WrongInput:
    lda #0
    sta cheat_index
    lda temp_x
    cmp #%00010000  ; Start Button
    beq @StartNormal
    jmp @TitleDone

@CheatSuccess:
    lda #1
    sta cheat_active
    jsr InitGame 
    jmp @TitleDone

@StartNormal:
    lda #0
    sta cheat_active
    jsr InitGame
    
@TitleDone:
    jmp ForeverLoop 

RunGameOver:
    lda buttons
    and #%00010000
    beq @GODone
    lda prev_buttons
    and #%00010000
    bne @GODone
    jsr GoToTitle
@GODone:
    jmp ForeverLoop

RunWin:
    lda buttons
    and #%00010000
    beq @WinDone
    lda prev_buttons
    and #%00010000
    bne @WinDone
    jsr GoToTitle
@WinDone:
    jmp ForeverLoop

RunPaused:
    jmp DrawEverything

RunSpawnWait:
    jsr UpdateSpawnWait
    jsr UpdatePlayerMove 
    jmp DrawEverything

RunChase:
    jsr UpdatePlayerMove
    jsr UpdateBullChase
    jsr CheckCollisionChase
    jmp DrawEverything

RunTired:
    jsr UpdatePlayerMove
    jsr UpdateBullTired
    jsr CheckCollisionTired
    jmp DrawEverything

RunQTE:
    jsr UpdateQTEInput
    jmp DrawEverything

RunFeedback:
    jsr UpdateFeedback
    jmp DrawEverything

RunHit:
    jsr UpdatePlayerHit
    jmp DrawEverything

RunEscape:
    jsr UpdateBullEscape
    jmp DrawEverything

RunWonWait:
    jsr UpdateWonWait
    jmp DrawEverything

DrawEverything:
    jsr PrepareOAMBUFFER
    jmp ForeverLoop

; ---------------------------------------------------------------------------
; NMI HANDLER (VBlank)
; ---------------------------------------------------------------------------
NMI:
    pha
    txa
    pha
    tya
    pha
    
    lda #$00
    sta OAM_ADDR
    lda #>OAM_RAM
    sta OAM_DMA

    lda game_state
    cmp #STATE_LOADING
    beq @NMIDone

    lda soft_ppu_mask
    sta PPU_MASK

    lda game_state
    cmp #STATE_TITLE
    beq @DoBlink
    cmp #STATE_GAMEOVER
    beq @DoBlink
    cmp #STATE_WIN
    beq @DoBlink
    
    jmp @CheckGameUI

@DoBlink:
    jsr UpdateTitleBlink
    jmp @FinishPPU

@CheckGameUI:
    lda heart_update_req
    beq @CheckScore
    jsr RemoveHeartUI_Safe
    lda #0
    sta heart_update_req

@CheckScore:
    lda score_update_req
    beq @FinishPPU
    jsr UpdateScoreUI_Safe
    lda #0
    sta score_update_req
    
@FinishPPU:
    lda shake_timer
    beq @NormalScroll

    lda game_state
    cmp #STATE_PAUSED
    beq @NormalScroll

    lda frame_counter
    and #$03        
    beq @NormalScroll 

    lda #4
    sta PPU_SCROLL        
    lda #4
    sta PPU_SCROLL        
    jmp @DoAudio

@NormalScroll:
    lda #0
    sta PPU_SCROLL
    sta PPU_SCROLL

@DoAudio:
    lda sound_ready
    beq @SkipAudio
    
    lda game_state
    cmp #STATE_PAUSED
    beq @SkipAudio
    
    jsr famistudio_update
@SkipAudio:

@NMIDone:
    inc nmi_done
    inc frame_counter
    
    pla
    tay
    pla
    tax
    pla
    rti 

WaitForNMI:
    lda nmi_done
@loop:
    cmp nmi_done
    beq @loop
    rts

; ---------------------------------------------------------------------------
; GAMEPLAY SUBROUTINES
; ---------------------------------------------------------------------------

CheckPauseInput:
    lda game_state
    cmp #STATE_TITLE
    beq @NoPause
    cmp #STATE_GAMEOVER
    beq @NoPause
    cmp #STATE_WIN
    beq @NoPause
    cmp #STATE_LOADING
    beq @NoPause

    lda buttons
    and #%00010000
    beq @NoPause
    lda prev_buttons
    and #%00010000
    bne @NoPause

    lda game_state
    cmp #STATE_PAUSED
    beq @Unpause

@DoPause:
    lda game_state
    sta state_backup
    lda #STATE_PAUSED
    sta game_state
    lda #$0F                ; Dim Screen
    sta soft_ppu_mask
    lda #$00
    sta APU_STATUS
    rts

@Unpause:
    lda state_backup
    sta game_state
    lda #$1E
    sta soft_ppu_mask
    lda #$0F
    sta APU_STATUS
    rts

@NoPause:
    rts

; --- INITIALIZATION ROUTINES ---

GoToTitle:
    lda #0
    sta PPU_CTRL
    sta PPU_MASK        
    
    bit PPU_STATUS
@WaitV:
    bit PPU_STATUS
    bpl @WaitV

    lda #STATE_LOADING
    sta game_state
    
    lda #<map_title
    sta map_ptr_lo
    lda #>map_title
    sta map_ptr_hi
    jsr LoadNametable
    
    lda #<pal_arena
    sta map_ptr_lo
    lda #>pal_arena
    sta map_ptr_hi
    jsr LoadBgPalette

    jsr ClearSprites 
    
    ldx #<music_data_vaadivasal_music
    ldy #>music_data_vaadivasal_music
    lda #1 
    jsr famistudio_init

    lda #SONG_TITLE
    jsr famistudio_music_play
    
    lda #1
    sta sound_ready

    lda #%10000000      ; NMI Enable, Base settings
    sta PPU_CTRL
    jsr WaitForNMI
    
    lda #STATE_TITLE
    sta game_state
    
    lda #0
    sta PPU_SCROLL
    sta PPU_SCROLL
    lda #%10010000      ; NMI on, BG at $1000
    sta PPU_CTRL
    lda #%00011110
    sta soft_ppu_mask 
    sta PPU_MASK
    
    lda #0
    sta cheat_index
    sta cheat_active
    rts

GoToGameOver:
    jsr WaitForNMI
    lda #0
    sta PPU_CTRL
    sta PPU_MASK
    
    lda #STATE_LOADING
    sta game_state
    
    lda #<map_gameover
    sta map_ptr_lo
    lda #>map_gameover
    sta map_ptr_hi
    jsr LoadNametable

    lda #<pal_arena
    sta map_ptr_lo
    lda #>pal_arena
    sta map_ptr_hi
    jsr LoadBgPalette
    
    jsr DrawFinalScore
    jsr ClearSprites 
    
    lda #SONG_GAMEOVER
    jsr famistudio_music_play

    lda #%10000000
    sta PPU_CTRL
    jsr WaitForNMI
    
    lda #STATE_GAMEOVER
    sta game_state
    
    lda #0
    sta PPU_SCROLL
    sta PPU_SCROLL
    lda #%10010000
    sta PPU_CTRL
    lda #%00011110
    sta soft_ppu_mask
    sta PPU_MASK
    rts

GoToWin:
    jsr WaitForNMI
    lda #0
    sta PPU_CTRL
    sta PPU_MASK
    
    lda #STATE_LOADING
    sta game_state
    
    lda #<map_win
    sta map_ptr_lo
    lda #>map_win
    sta map_ptr_hi
    jsr LoadNametable

    lda #<pal_arena
    sta map_ptr_lo
    lda #>pal_arena
    sta map_ptr_hi
    jsr LoadBgPalette
    
    jsr DrawFinalScore
    jsr ClearSprites 
    
    lda #SONG_WIN
    jsr famistudio_music_play

    lda #%10000000
    sta PPU_CTRL
    jsr WaitForNMI
    
    lda #STATE_WIN
    sta game_state
    
    lda #0
    sta PPU_SCROLL
    sta PPU_SCROLL
    lda #%10010000
    sta PPU_CTRL
    lda #%00011110
    sta soft_ppu_mask
    sta PPU_MASK
    rts

InitGame:
    lda #0
    sta sound_ready
    jsr famistudio_music_stop
    lda #$00
    sta APU_STATUS
    
    jsr WaitForNMI
    lda #0
    sta PPU_CTRL
    sta PPU_MASK
    
    lda #STATE_LOADING
    sta game_state
    
    lda #MAX_LIVES
    sta player_lives
    lda #0
    sta player_score
    lda #BASE_VELOCITY
    sta bull_velocity
    lda #0
    sta bull_move_acc
    lda #BASE_QTE_TIME
    sta qte_max_time
    lda #0
    sta shake_timer 
    
    lda #<map_arena
    sta map_ptr_lo
    lda #>map_arena
    sta map_ptr_hi
    jsr LoadNametable

    lda cheat_active
    beq @HeartsNormal
    
    lda PPU_STATUS
    lda #$23
    sta PPU_ADDR
    lda #$66
    sta PPU_ADDR
    lda #TILE_EMPTY
    sta PPU_DATA

    lda PPU_STATUS
    lda #$23
    sta PPU_ADDR
    lda #$68
    sta PPU_ADDR
    lda #TILE_EMPTY
    sta PPU_DATA
    
    lda PPU_STATUS
    lda #$23
    sta PPU_ADDR
    lda #$6A
    sta PPU_ADDR
    lda #TILE_EMPTY
    sta PPU_DATA
@HeartsNormal:

    lda #<pal_arena
    sta map_ptr_lo
    lda #>pal_arena
    sta map_ptr_hi
    jsr LoadBgPalette
   
    lda PPU_STATUS
    lda #$23
    sta PPU_ADDR
    lda #$7D
    sta PPU_ADDR
    ldx #0
    lda ScoreTileTable, x
    sta PPU_DATA 
    sta PPU_DATA 
    
    jsr SpawnPlayer
    jsr SpawnBull
    
    ldx #<music_data_vaadivasal_music
    ldy #>music_data_vaadivasal_music
    lda #1 
    jsr famistudio_init
    
    lda #$0F
    sta APU_STATUS
    lda #$40
    sta JOYPAD2
    
    lda #SONG_START
    jsr famistudio_music_play
    lda #1
    sta sound_ready 
    
    lda #%10000000
    sta PPU_CTRL
    jsr WaitForNMI
    
    lda #0
    sta PPU_SCROLL
    sta PPU_SCROLL
    lda #%10010000
    sta PPU_CTRL
    lda #%00011110
    sta soft_ppu_mask 
    sta PPU_MASK
    rts

; --- UTILITY SUBROUTINES ---

LoadNametable:
    lda PPU_STATUS
    lda #$20
    sta PPU_ADDR
    lda #$00
    sta PPU_ADDR
    ldx #4      
    ldy #0
@LoadLoop:
    lda (map_ptr_lo), y
    sta PPU_DATA
    iny
    bne @LoadLoop
    inc map_ptr_hi
    dex
    bne @LoadLoop
    rts

LoadBgPalette:
    lda PPU_STATUS
    lda #$3F
    sta PPU_ADDR
    lda #$00
    sta PPU_ADDR
    ldy #0
@PalLoop:
    lda (map_ptr_lo), y
    sta PPU_DATA
    iny
    cpy #16
    bne @PalLoop
    rts

ClearSprites:
    ldx #0
    lda #$FF    
@ClearLoop:
    sta OAM_RAM, x
    inx
    inx
    inx
    inx
    bne @ClearLoop
    rts

DrawFinalScore:
    lda PPU_STATUS
    lda #$22
    sta PPU_ADDR
    lda #$54        
    sta PPU_ADDR
    lda player_score
    ldx #0
@CalcLoop:
    cmp #10
    bcc @Write
    sbc #10
    inx
    jmp @CalcLoop
@Write:
    pha              
    lda ScoreTileTable, x
    sta PPU_DATA
    pla
    tax
    lda ScoreTileTable, x
    sta PPU_DATA
    rts

; --- UPDATE LOGIC ---

UpdatePlayerMove:
    lda #0
    sta walking
    lda buttons
    and #%00001000
    beq NoUp
    lda player_y
    cmp #WALL_MIN_Y
    bcc NoUp
    dec player_y
    inc walking
NoUp:
    lda buttons
    and #%00000100
    beq NoDown
    lda player_y
    cmp #WALL_MAX_Y
    bcs NoDown
    inc player_y
    inc walking
NoDown:
    lda buttons
    and #%00000010
    beq NoLeft
    lda player_x
    cmp #WALL_MIN_X
    bcc NoLeft
    dec player_x
    lda #1
    sta facing_left
    inc walking
NoLeft:
    lda buttons
    and #%00000001
    beq NoRight
    lda player_x
    cmp #WALL_MAX_X
    bcs NoRight
    inc player_x
    lda #0
    sta facing_left
    inc walking
NoRight:
    lda walking
    bne DoAnimate
    lda #0
    sta anim_frame
    rts

DoAnimate:
    inc anim_timer
    lda anim_timer
    cmp #8
    bne AnimDone
    lda #0
    sta anim_timer
    inc anim_frame
    lda anim_frame
    cmp #3
    bne AnimDone
    lda #0
    sta anim_frame
AnimDone:
    rts

UpdateSpawnWait:
    dec bull_state_timer
    bne @Wait
    jsr BullRecover 
@Wait:
    jsr AnimateBull 
    rts

UpdateBullChase:
    lda bull_move_acc
    clc
    adc bull_velocity
    sta bull_move_acc
    bcc ChaseTimer   
    
    lda player_x
    cmp bull_x
    bcc BullTryLeft
    
    lda bull_x
    cmp #BULL_MAX_X     
    bcs CheckBullY
    inc bull_x
    lda #0
    sta bull_facing_left
    jmp CheckBullY

BullTryLeft:
    lda bull_x
    cmp #BULL_MIN_X     
    bcc CheckBullY
    dec bull_x
    lda #1
    sta bull_facing_left

CheckBullY:
    lda player_y
    clc
    adc #20             
    cmp bull_y
    bcc BullTryUp
    
    lda bull_y
    cmp #BULL_MAX_Y     
    bcs ChaseTimer
    inc bull_y
    jmp ChaseTimer

BullTryUp:
    lda bull_y
    cmp #BULL_MIN_Y     
    bcc ChaseTimer
    dec bull_y

ChaseTimer:
    lda bull_state_timer
    sec
    sbc #1
    sta bull_state_timer
    lda bull_state_timer+1
    sbc #0
    sta bull_state_timer+1
    lda bull_state_timer
    ora bull_state_timer+1
    bne ChaseAnim
    
    lda #STATE_TIRED
    sta game_state
    lda #<TIRED_DURATION
    sta bull_state_timer
    lda #>TIRED_DURATION
    sta bull_state_timer+1
ChaseAnim:
    jsr AnimateBull
    rts

CheckCollisionChase:
    jsr AABB_Check
    bne HitDetected
    rts
HitDetected:
    lda #SONG_HIT
    jsr famistudio_music_play

    lda cheat_active
    bne @SkipDamage     
    
    dec player_lives
    lda #1
    sta heart_update_req
@SkipDamage:

    lda #STATE_HIT
    sta game_state
    lda #HIT_DURATION
    sta hit_timer
    
    lda #30          
    sta shake_timer

    lda #0
    sta walking
    sta anim_frame
    rts

UpdatePlayerHit:
    lda shake_timer
    beq @SkipShakeDec
    dec shake_timer
@SkipShakeDec:

    lda bull_y
    cmp #240
    bcs BullGoneHit
    inc bull_y
    inc bull_y
    jsr AnimateBull
    jmp HitTimerCheck
BullGoneHit:
    lda #$FF
    sta bull_y
HitTimerCheck:
    dec hit_timer
    bne HitWait
    lda player_lives
    beq @DoGameOver
    jsr SpawnBull
    rts
@DoGameOver:
    jsr GoToGameOver
    rts
HitWait:
    rts

UpdateBullEscape:
    lda bull_y
    cmp #240
    bcs BullGoneEscape
    inc bull_y
    inc bull_y
    jsr AnimateBull
    rts
BullGoneEscape:
    jsr SpawnBull
    rts

UpdateBullTired:
    lda bull_state_timer
    sec
    sbc #1
    sta bull_state_timer
    lda bull_state_timer+1
    sbc #0
    sta bull_state_timer+1
    lda bull_state_timer
    ora bull_state_timer+1
    bne TiredStill
    
    jsr BullRecover
TiredStill:
    rts

CheckCollisionTired:
    jsr AABB_Check
    bne StartQTE
    rts
StartQTE:
    lda #STATE_QTE
    sta game_state
    lda #0
    sta qte_current_cnt
    jsr PickNewQTEButton
    rts

UpdateQTEInput:
    dec qte_timer
    beq QTE_Fail
    lda buttons
    eor prev_buttons
    and buttons
    beq QTE_Wait
    
    ldy qte_needed_btn
    lda QTE_BitMasks, y
    and buttons
    bne QTE_Success
    jmp QTE_Fail

QTE_Success:
    lda #2
    sta qte_palette
    lda #1
    sta qte_result_ok

    lda qte_current_cnt
    cmp #4               
    beq @FinalHitSound
    
    lda #SONG_QTE_OK
    jsr famistudio_music_play
    jmp GoToFeedback

@FinalHitSound:
    lda #SONG_CATCH
    jsr famistudio_music_play
    jmp GoToFeedback

QTE_Fail:
    lda #3
    sta qte_palette
    lda #0
    sta qte_result_ok

    lda #SONG_QTE_FAIL
    jsr famistudio_music_play

    jmp GoToFeedback
QTE_Wait:
    rts

GoToFeedback:
    lda #STATE_FEEDBACK
    sta game_state
    lda #FEEDBACK_TIME
    sta bull_state_timer
    rts

UpdateFeedback:
    dec bull_state_timer
    bne FeedbackWait
    lda qte_result_ok
    beq FeedbackFail
    
    inc qte_current_cnt
    lda qte_current_cnt
    cmp #QTE_GOAL
    beq WinRound
    
    lda #STATE_QTE
    sta game_state
    jsr PickNewQTEButton
    rts

FeedbackFail:
    lda #STATE_ESCAPE
    sta game_state
    rts

WinRound:
    inc player_score
    lda #1
    sta score_update_req
    
    lda bull_velocity
    cmp #MAX_VELOCITY
    bcs @SkipSpeed
    clc
    adc #2
    sta bull_velocity
@SkipSpeed:
    lda qte_max_time
    cmp #MIN_QTE_TIME
    bcc @SkipQTE
    dec qte_max_time
@SkipQTE:
    lda player_score
    cmp #WIN_SCORE
    bcc @ContinueGame   
    lda #STATE_WON_WAIT
    sta game_state
    lda #WIN_DELAY
    sta bull_state_timer
    rts
@ContinueGame:
    jsr SpawnBull
    rts

UpdateWonWait:
    dec bull_state_timer
    bne FeedbackWait
    jsr GoToWin
    rts
FeedbackWait:
    rts

PickNewQTEButton:
@ReRoll:
    jsr RandomUpdate
    lda rng_seed
    and #$07 ; 0-7
    cmp #6
    bcs @ReRoll ; If 6 or 7, roll again
    sta qte_needed_btn
    
    lda qte_max_time
    sta qte_timer
    lda #1
    sta qte_palette
    rts

SpawnPlayer:
    lda #120
    sta player_x
    lda #144
    sta player_y
    lda #0
    sta walking
    rts
SpawnBull:
    lda #110
    sta bull_x
    
    lda #64
    sta bull_y
    
    lda #STATE_SPAWN_WAIT
    sta game_state
    
    lda player_score
    cmp #16
    bcs @CheckTier2
    lda #60
    jmp @SetTimer
@CheckTier2:
    cmp #21
    bcs @Tier3
    lda #90
    jmp @SetTimer
@Tier3:
    lda #120
@SetTimer:
    sta bull_state_timer
    rts

BullRecover:
    lda #STATE_CHASE
    sta game_state
    lda #<CHASE_DURATION
    sta bull_state_timer
    lda #>CHASE_DURATION
    sta bull_state_timer+1
    rts

AABB_Check:
    lda player_x
    clc
    adc #24
    cmp bull_x
    bcc NoHit
    lda bull_x
    clc
    adc #40
    cmp player_x
    bcc NoHit
    lda player_y
    clc
    adc #60
    cmp bull_y
    bcc NoHit
    lda bull_y
    clc
    adc #30
    cmp player_y
    bcc NoHit
    lda #1
    rts
NoHit:
    lda #0
    rts

RandomUpdate:
    lda rng_seed
    asl a
    asl a
    clc
    adc frame_counter
    sta rng_seed
    rts

ReadController:
    lda buttons
    sta prev_buttons
    lda #$01
    sta JOYPAD1
    lda #$00
    sta JOYPAD1
    ldx #$08
ReadLoop:
    lda JOYPAD1
    lsr a
    rol buttons
    dex
    bne ReadLoop
    rts

AnimateBull:
    inc bull_anim_timer
    lda bull_anim_timer
    cmp #8
    bne BullAnimRet
    lda #0
    sta bull_anim_timer
    lda bull_frame_idx
    eor #1
    sta bull_frame_idx
BullAnimRet:
    rts

; --- DRAWING ROUTINES ---

PrepareOAMBUFFER:
    ldx #0
    stx oam_ptr
    lda #1
    sta current_pal_add 
    
    lda game_state
    cmp #STATE_TITLE
    beq @HideAll
    cmp #STATE_GAMEOVER
    beq @HideAll
    cmp #STATE_WIN
    beq @HideAll
    lda game_state
    cmp #STATE_QTE
    beq DrawBtn
    cmp #STATE_FEEDBACK
    beq DrawFeedbackIcon
    jmp CheckZSort
@HideAll:
    jsr ClearSprites
    rts

DrawBtn:
    lda bull_y
    sec
    sbc #24
    sta temp_y
    lda bull_x
    clc
    adc #12
    sta temp_x
    ldy qte_needed_btn 
    lda BtnTableLo, y
    sta meta_ptr_lo
    lda BtnTableHi, y
    sta meta_ptr_hi
    jsr DrawMetaSpriteQTE
    jmp CheckZSort

DrawFeedbackIcon:
    lda bull_y
    sec
    sbc #24
    sta temp_y
    lda bull_x
    clc
    adc #12
    sta temp_x
    lda qte_result_ok
    bne ShowCheck
    ldy #7 ; Cross
    jmp DrawResult
ShowCheck:
    ldy #6 ; Checkmark
DrawResult:
    lda BtnTableLo, y
    sta meta_ptr_lo
    lda BtnTableHi, y
    sta meta_ptr_hi
    jsr DrawMetaSpriteQTE 
    jmp CheckZSort

CheckZSort:
    lda game_state
    cmp #STATE_HIT
    bne CheckEscape
    
    lda frame_counter
    and #$03 
    sta current_pal_add
    
    jmp DrawNormally
CheckEscape:
    lda game_state
    cmp #STATE_ESCAPE
    bne CheckTiredBlink
    jmp DrawNormally
CheckTiredBlink:
    lda game_state
    cmp #STATE_TIRED
    bne DrawNormally
    lda frame_counter
    and #$08
    bne DrawPlayerOnly
    jmp DrawNormally

DrawPlayerOnly:
    jsr DrawPlayerSprite
    jmp CleanOAM

DrawNormally:
    lda player_y
    cmp bull_y
    bcc PlayerIsBehind
    
    jsr DrawPlayerSprite
    
    lda #1
    sta current_pal_add
    jsr DrawBullSprite
    jmp CleanOAM
PlayerIsBehind:
    lda #1
    sta current_pal_add
    jsr DrawBullSprite
    
    lda game_state
    cmp #STATE_HIT
    bne @NoFlash
    lda frame_counter
    and #$03
    sta current_pal_add
    jmp @DrawP
@NoFlash:
    lda #1
    sta current_pal_add
@DrawP:
    jsr DrawPlayerSprite
    jmp CleanOAM

CleanOAM:
    ldx oam_ptr
HideLoop:
    cpx #0
    beq FinishOAM
    lda #$FF
    sta OAM_RAM, x
    inx
    inx
    inx
    inx
    jmp HideLoop
FinishOAM:
    rts

DrawPlayerSprite:
    lda player_y
    cmp #240
    bcc @DoDrawP
    rts

@DoDrawP:
    lda game_state
    cmp #STATE_QTE
    beq @SkipDust

    lda walking
    beq @SkipDust
    
    lda frame_counter
    and #$04        
    bne @SkipDust   
    
    ldx oam_ptr
    
    lda facing_left
    bne @DustFacingLeft
    
    jsr DrawOneDustHelper
    lda player_x
    clc
    adc #0          
    sta OAM_RAM, x
    inx
    
    jsr DrawOneDustHelper
    lda player_x
    clc
    adc #12         
    sta OAM_RAM, x
    inx
    jmp @FinishDust

@DustFacingLeft:
    jsr DrawOneDustHelper
    lda player_x
    clc
    adc #4          
    sta OAM_RAM, x
    inx
    
    jsr DrawOneDustHelper
    lda player_x
    clc
    adc #16         
    sta OAM_RAM, x
    inx

@FinishDust:
    stx oam_ptr

@SkipDust:
    ldy anim_frame
    lda PlayerFrameLo, y
    sta meta_ptr_lo
    lda PlayerFrameHi, y
    sta meta_ptr_hi
    lda player_x
    sta temp_x
    lda player_y
    sta temp_y
    lda facing_left
    bne @DrawFlipped
    jsr DrawMetaSpriteRaw
    rts
@DrawFlipped:
    lda #PLAYER_WIDTH
    sta sprite_width
    jsr DrawMetaSpriteFlipped
    rts

DrawBullSprite:
    lda bull_y
    cmp #240
    bcc @DoDrawB
    rts

@DoDrawB:
    lda game_state
    cmp #STATE_CHASE
    beq @BullDustOn
    cmp #STATE_ESCAPE
    beq @BullDustOn
    jmp @SkipBullDust

@BullDustOn:
    lda frame_counter
    and #$04
    bne @SkipBullDust
    
    ldx oam_ptr
    
    lda bull_facing_left
    bne @BullDustFacingLeft
    
    jsr DrawBullDustHelper
    lda bull_x
    clc
    adc #00          
    sta OAM_RAM, x
    inx
    
    jsr DrawBullDustHelper
    lda bull_x
    clc
    adc #24         
    sta OAM_RAM, x
    inx
    jmp @FinishBullDust

@BullDustFacingLeft:
    jsr DrawBullDustHelper
    lda bull_x
    clc
    adc #4          
    sta OAM_RAM, x
    inx
    
    jsr DrawBullDustHelper
    lda bull_x
    clc
    adc #28         
    sta OAM_RAM, x
    inx

@FinishBullDust:
    stx oam_ptr

@SkipBullDust:
    ldy bull_frame_idx
    lda BullFrameTableLo, y
    sta meta_ptr_lo
    lda BullFrameTableHi, y
    sta meta_ptr_hi
    lda bull_x
    sta temp_x
    lda bull_y
    sta temp_y
    lda bull_facing_left
    bne DrawBullFlipped
    jsr DrawMetaSpriteRaw
    rts

DrawBullFlipped:
    lda #BULL_WIDTH
    sta sprite_width
    jsr DrawMetaSpriteFlipped
    rts

; --- META SPRITE RENDERING ---

DrawMetaSpriteRaw:
    ldy #0
MetaL:
    lda (meta_ptr_lo), y
    cmp #$80
    beq MetaEnd
    clc
    adc temp_y
    bcs @HideTile        
    cmp #240
    bcs @HideTile        
    ldx oam_ptr
    sta OAM_RAM, x
    iny
    inx
    lda (meta_ptr_lo), y
    sta OAM_RAM, x
    iny
    inx
    lda (meta_ptr_lo), y
    ora current_pal_add 
    sta OAM_RAM, x
    iny
    inx
    lda (meta_ptr_lo), y
    clc
    adc temp_x
    sta OAM_RAM, x
    iny
    inx
    stx oam_ptr
    jmp MetaL
@HideTile:
    lda #$FF
    ldx oam_ptr
    sta OAM_RAM, x
    iny
    inx
    iny
    inx
    iny
    inx
    iny
    inx
    stx oam_ptr
    jmp MetaL
MetaEnd:
    rts

DrawMetaSpriteQTE:
    ldy #0
MetaQ:
    lda (meta_ptr_lo), y
    cmp #$80
    beq MetaEndQ
    clc
    adc temp_y
    ldx oam_ptr
    sta OAM_RAM, x
    iny
    inx
    lda (meta_ptr_lo), y
    sta OAM_RAM, x
    iny
    inx
    lda (meta_ptr_lo), y
    and #$FC
    ora qte_palette
    sta OAM_RAM, x
    iny
    inx
    lda (meta_ptr_lo), y
    clc
    adc temp_x
    sta OAM_RAM, x
    iny
    inx
    stx oam_ptr
    jmp MetaQ
MetaEndQ:
    rts

DrawMetaSpriteFlipped:
    ldy #0
MetaL_F:
    lda (meta_ptr_lo), y
    cmp #$80
    beq MetaEnd_F
    clc
    adc temp_y
    bcs @HideTileF      
    cmp #240
    bcs @HideTileF      
    ldx oam_ptr
    sta OAM_RAM, x
    iny
    inx
    lda (meta_ptr_lo), y
    sta OAM_RAM, x
    iny
    inx
    lda (meta_ptr_lo), y
    ora current_pal_add  
    eor #%01000000 
    sta OAM_RAM, x
    iny
    inx
    lda sprite_width
    sec
    sbc #8
    sec
    sbc (meta_ptr_lo), y 
    clc
    adc temp_x              
    sta OAM_RAM, x
    iny
    inx
    stx oam_ptr
    jmp MetaL_F
@HideTileF:
    lda #$FF
    ldx oam_ptr
    sta OAM_RAM, x
    iny
    inx
    iny
    inx
    iny
    inx
    iny
    inx
    stx oam_ptr
    jmp MetaL_F
MetaEnd_F:
    rts

DrawOneDustHelper:
    lda player_y
    clc
    adc #62          
    sta OAM_RAM, x
    inx
    lda #TILE_DUST    
    sta OAM_RAM, x
    inx
    lda #0            
    sta OAM_RAM, x
    inx
    rts

DrawBullDustHelper:
    lda bull_y
    clc
    adc #32         
    sta OAM_RAM, x
    inx
    lda #TILE_DUST    
    sta OAM_RAM, x
    inx
    lda #0            
    sta OAM_RAM, x
    inx
    rts

UpdateTitleBlink:
    lda frame_counter
    and #%00100000
    beq @DrawText   
    jmp @DrawEmpty  
@DrawText:
    lda PPU_STATUS
    lda #$22
    sta PPU_ADDR
    lda #$E8
    sta PPU_ADDR
    ldx #0
@LoopT1:
    lda map_title + 744, x
    sta PPU_DATA
    inx
    cpx #16
    bne @LoopT1
    lda PPU_STATUS
    lda #$23
    sta PPU_ADDR
    lda #$08
    sta PPU_ADDR
    ldx #0
@LoopT2:
    lda map_title + 776, x
    sta PPU_DATA
    inx
    cpx #16
    bne @LoopT2
    lda PPU_STATUS
    lda #$23
    sta PPU_ADDR
    lda #$28
    sta PPU_ADDR
    ldx #0
@LoopT3:
    lda map_title + 808, x
    sta PPU_DATA
    inx
    cpx #16
    bne @LoopT3
    rts
@DrawEmpty:
    lda PPU_STATUS
    lda #$22
    sta PPU_ADDR
    lda #$E8
    sta PPU_ADDR
    ldx #0
    lda #TILE_EMPTY
@LoopE1:
    sta PPU_DATA
    inx
    cpx #16
    bne @LoopE1
    lda PPU_STATUS
    lda #$23
    sta PPU_ADDR
    lda #$08
    sta PPU_ADDR
    ldx #0
    lda #TILE_EMPTY
@LoopE2:
    sta PPU_DATA
    inx
    cpx #16
    bne @LoopE2
    lda PPU_STATUS
    lda #$23
    sta PPU_ADDR
    lda #$28
    sta PPU_ADDR
    ldx #0
    lda #TILE_EMPTY
@LoopE3:
    sta PPU_DATA
    inx
    cpx #16
    bne @LoopE3
    rts

RemoveHeartUI_Safe:
    lda PPU_STATUS 
    lda #$23
    sta PPU_ADDR
    ldx player_lives
    cpx #2
    beq ClearH3 
    cpx #1
    beq ClearH2 
    cpx #0
    beq ClearH1 
    rts
ClearH3:
    lda #$6A
    jmp DoClear
ClearH2:
    lda #$68
    jmp DoClear
ClearH1:
    lda #$66
    jmp DoClear
DoClear:
    sta PPU_ADDR        
    lda #TILE_EMPTY 
    sta PPU_DATA
    rts

UpdateScoreUI_Safe:
    lda PPU_STATUS
    lda #$23
    sta PPU_ADDR
    lda #$7D
    sta PPU_ADDR
    lda player_score
    ldx #0
@DivLoop:
    cmp #10
    bcc @DivDone
    sbc #10
    inx
    jmp @DivLoop
@DivDone:
    pha
    lda ScoreTileTable, x
    sta PPU_DATA
    pla
    tax
    lda ScoreTileTable, x
    sta PPU_DATA
    rts

; ---------------------------------------------------------------------------
; READ-ONLY DATA
; ---------------------------------------------------------------------------
.segment "RODATA"
pal_sprites:
    .incbin "../assets/gfx/sprites.pal"

KonamiSequence:
    .byte $08, $08, $04, $04, $02, $01, $02, $01, $40, $80, $10

map_arena:      .incbin "../assets/maps/arena.nam"
pal_arena:      .incbin "../assets/gfx/arena.pal"
map_title:      .incbin "../assets/maps/title.nam"
map_gameover:   .incbin "../assets/maps/gameover.nam"
map_win:        .incbin "../assets/maps/win.nam"

ScoreTileTable:
    .byte $28, $29, $2A, $2B, $2C, $2D, $2E, $2F, $38, $39

; --- ANIMATION DATA ---
PlayerFrameLo:
    .byte <PFrame0, <PFrame1, <PFrame2
PlayerFrameHi:
    .byte >PFrame0, >PFrame1, >PFrame2
PFrame0:
    .byte 0, $00, 0, 0,   0, $01, 0, 8,   0, $02, 0, 16
    .byte 8, $10, 0, 0,   8, $11, 0, 8,   8, $12, 0, 16
    .byte 16, $20, 0, 0,  16, $21, 0, 8,  16, $22, 0, 16
    .byte 24, $30, 0, 0,  24, $31, 0, 8,  24, $32, 0, 16
    .byte 32, $40, 0, 0,  32, $41, 0, 8,  32, $42, 0, 16
    .byte 40, $50, 0, 0,  40, $51, 0, 8,  40, $52, 0, 16
    .byte 48, $60, 0, 0,  48, $61, 0, 8,  48, $62, 0, 16
    .byte 56, $70, 0, 0,  56, $71, 0, 8,  56, $72, 0, 16
    .byte $80
PFrame1:
    .byte 0, $03, 0, 0,   0, $04, 0, 8,   0, $05, 0, 16
    .byte 8, $13, 0, 0,   8, $14, 0, 8,   8, $15, 0, 16
    .byte 16, $23, 0, 0,  16, $24, 0, 8,  16, $25, 0, 16
    .byte 24, $33, 0, 0,  24, $34, 0, 8,  24, $35, 0, 16
    .byte 32, $43, 0, 0,  32, $44, 0, 8,  32, $45, 0, 16
    .byte 40, $53, 0, 0,  40, $54, 0, 8,  40, $55, 0, 16
    .byte 48, $63, 0, 0,  48, $64, 0, 8,  48, $65, 0, 16
    .byte 56, $73, 0, 0,  56, $74, 0, 8,  56, $75, 0, 16
    .byte $80
PFrame2:
    .byte 0, $06, 0, 0,   0, $07, 0, 8,   0, $08, 0, 16
    .byte 8, $16, 0, 0,   8, $17, 0, 8,   8, $18, 0, 16
    .byte 16, $26, 0, 0,  16, $27, 0, 8,  16, $28, 0, 16
    .byte 24, $36, 0, 0,  24, $37, 0, 8,  24, $38, 0, 16
    .byte 32, $46, 0, 0,  32, $47, 0, 8,  32, $48, 0, 16
    .byte 40, $56, 0, 0,  40, $57, 0, 8,  40, $58, 0, 16
    .byte 48, $66, 0, 0,  48, $67, 0, 8,  48, $68, 0, 16
    .byte 56, $76, 0, 0,  56, $77, 0, 8,  56, $78, 0, 16
    .byte $80

BullFrameTableLo:
    .byte <BullFrame1, <BullFrame2
BullFrameTableHi:
    .byte >BullFrame1, >BullFrame2
BullFrame1:
    .byte 0, $80, 1, 0,   0, $81, 1, 8,   0, $82, 1, 16,  0, $83, 1, 24,  0, $84, 1, 32
    .byte 8, $90, 1, 0,   8, $91, 1, 8,   8, $92, 1, 16,  8, $93, 1, 24,  8, $94, 1, 32
    .byte 16, $A0, 1, 0,  16, $A1, 1, 8,  16, $A2, 1, 16, 16, $A3, 1, 24, 16, $A4, 1, 32
    .byte 24, $B0, 1, 0,  24, $B1, 1, 8,  24, $B2, 1, 16, 24, $B3, 1, 24, 24, $B4, 1, 32
    .byte $80
BullFrame2:
    .byte 0, $C0, 1, 0,   0, $C1, 1, 8,   0, $C2, 1, 16,  0, $C3, 1, 24,  0, $C4, 1, 32
    .byte 8, $D0, 1, 0,   8, $D1, 1, 8,   8, $D2, 1, 16,  8, $D3, 1, 24,  8, $D4, 1, 32
    .byte 16, $E0, 1, 0,  16, $E1, 1, 8,  16, $E2, 1, 16, 16, $E3, 1, 24, 16, $E4, 1, 32
    .byte 24, $F0, 1, 0,  24, $F1, 1, 8,  24, $F2, 1, 16, 24, $F3, 1, 24, 24, $F4, 1, 32
    .byte $80

BtnTableLo:
    .byte <BtnA_Spr, <BtnB_Spr, <BtnUp_Spr, <BtnRight_Spr, <BtnDown_Spr, <BtnLeft_Spr, <BtnCheck_Spr, <BtnCross_Spr
BtnTableHi:
    .byte >BtnA_Spr, >BtnB_Spr, >BtnUp_Spr, >BtnRight_Spr, >BtnDown_Spr, >BtnLeft_Spr, >BtnCheck_Spr, >BtnCross_Spr

BtnA_Spr:
    .byte 0, $86, 2, 0, 0, $87, 2, 8
    .byte 8, $96, 2, 0, 8, $97, 2, 8
    .byte $80
BtnB_Spr:
    .byte 0, $A6, 2, 0, 0, $A7, 2, 8
    .byte 8, $B6, 2, 0, 8, $B7, 2, 8
    .byte $80
BtnUp_Spr:
    .byte 0, $A8, 2, 0, 0, $A9, 2, 8
    .byte 8, $B8, 2, 0, 8, $B9, 2, 8
    .byte $80
BtnRight_Spr:
    .byte 0, $AA, 2, 0, 0, $AB, 2, 8
    .byte 8, $BA, 2, 0, 8, $BB, 2, 8
    .byte $80
BtnDown_Spr:
    .byte 0, $AC, 2, 0, 0, $AD, 2, 8
    .byte 8, $BC, 2, 0, 8, $BD, 2, 8
    .byte $80
BtnLeft_Spr:
    .byte 0, $AE, 2, 0, 0, $AF, 2, 8
    .byte 8, $BE, 2, 0, 8, $BF, 2, 8
    .byte $80

BtnCheck_Spr:
    .byte 0, $88, 3, 0, 0, $89, 3, 8
    .byte 8, $98, 3, 0, 8, $99, 3, 8
    .byte $80
BtnCross_Spr:
    .byte 0, $8A, 2, 0, 0, $8B, 2, 8
    .byte 8, $9A, 2, 0, 8, $9B, 2, 8
    .byte $80

QTE_BitMasks:
    .byte $80, $40, $08, $01, $04, $02

; --- VECTORS & AUDIO INCLUDES ---

.segment "VECTORS"
    .word NMI
    .word RESET
    .word 0

.segment "TILES"
    .incbin "../assets/gfx/tiles.chr"

.segment "CODE"
.include "famistudio_ca65.s"  
.include "Vaadivasal_Sounds.s"