; ---------------------------------------------------------------------------
; VAADIVASAL GAME CONSTANTS (constants.inc)
; ---------------------------------------------------------------------------

; --- GAMEPLAY CONFIGURATION ---
MAX_LIVES           = 3
WIN_SCORE           = 25
QTE_GOAL            = 5         ; How many buttons to press to catch bull
BASE_QTE_TIME       = 60        ; Frames allowed to press button
MIN_QTE_TIME        = 25        ; Fastest speed for QTE
BASE_VELOCITY       = 64        ; Sub-pixel velocity start
MAX_VELOCITY        = 140       ; Cap for bull speed

; --- TIMERS (Assuming 60 FPS) ---
CHASE_DURATION      = 300       ; 5 Seconds
TIRED_DURATION      = 180       ; 3 Seconds
HIT_DURATION        = 120       ; 2 Seconds invincibility/stun
FEEDBACK_TIME       = 30        ; 0.5 Seconds to show Check/Cross
WIN_DELAY           = 120       ; Delay before Win Screen

; --- BOUNDARIES (Physics) ---
; Player (24x64)
WALL_MIN_X          = 6
WALL_MAX_X          = 228
WALL_MIN_Y          = 58
WALL_MAX_Y          = 162

; Bull (40x24)
BULL_MIN_X          = 16
BULL_MAX_X          = 200
BULL_MIN_Y          = 72
BULL_MAX_Y          = 216

PLAYER_WIDTH        = 24
BULL_WIDTH          = 40

; --- GRAPHICS & TILES ---
TILE_EMPTY          = $0F
TILE_DUST           = $8C

; --- GAME STATES ---
STATE_CHASE         = 0
STATE_TIRED         = 1
STATE_QTE           = 2
STATE_FEEDBACK      = 3
STATE_GAMEOVER      = 4
STATE_HIT           = 5
STATE_ESCAPE        = 6
STATE_SPAWN_WAIT    = 7
STATE_WON_WAIT      = 8
STATE_PAUSED        = 9
STATE_TITLE         = 10
STATE_WIN           = 11
STATE_LOADING       = $FF

; --- AUDIO SONGS (Famistudio IDs) ---
SONG_TITLE          = 0
SONG_START          = 1
SONG_WIN            = 2
SONG_GAMEOVER       = 3
SONG_QTE_OK         = 4
SONG_QTE_FAIL       = 5
SONG_HIT            = 6
SONG_CATCH          = 7