#pragma once

/* Fighter sprite sheet IDs */
typedef enum FighterAsset {
    ASSET_FIGHTER_RYU_IDLE = 0,
    ASSET_FIGHTER_RYU_WALK,
    ASSET_FIGHTER_RYU_ATTACKS,
    ASSET_FIGHTER_RYU_SPECIALS,
    ASSET_FIGHTER_KEN_IDLE,
    ASSET_FIGHTER_KEN_WALK,
    ASSET_FIGHTER_KEN_ATTACKS,
    ASSET_FIGHTER_KEN_SPECIALS,
    ASSET_FIGHTER_COUNT
} FighterAsset;

/* Stage assets */
typedef enum StageAsset {
    ASSET_STAGE_TEMPLE_BG0 = 0,
    ASSET_STAGE_TEMPLE_BG1,
    ASSET_STAGE_ARENA_BG0,
    ASSET_STAGE_ARENA_BG1,
    ASSET_STAGE_COUNT
} StageAsset;

/* UI assets */
typedef enum UIAsset {
    ASSET_UI_HUD_ATLAS = 0,
    ASSET_UI_MENU_BG,
    ASSET_UI_BUTTONS,
    ASSET_UI_FONT,
    ASSET_UI_COUNT
} UIAsset;

/* Touch controller assets */
typedef enum TouchAsset {
    ASSET_TOUCH_JOYSTICK_BASE = 0,
    ASSET_TOUCH_JOYSTICK_KNOB,
    ASSET_TOUCH_BUTTONS,
    ASSET_TOUCH_COUNT
} TouchAsset;
