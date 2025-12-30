#include <snes.h>
#include "soundbank.h"

// #define ADRSPRITE 0x2100
#define ADRSPRITEPLAYER 0x4000
#define ADRSPRITELASER  0x4500
#define ADRSPRITEALIEN  0x5000 

#define PALETTESPRSIZE (16 * 2)         // We are using words for palette entry

#define LASER_OFF_SCREEN 276            // Put the laser off the screen 256 + 20
#define ALIEN_OFF_SCREEN 280            // Put the alien off screen

#define TIMER_ALIEN_DELTA 100           // time between 2 aliens

extern char SOUNDBANK__;

// Splash screen
extern char back, back_end;
extern char palette;
extern char map;

extern char patterns_splash, patterns_splash_end;
extern char palette_splash, palette_splash_end;
extern char map_splash, map_splash_end;

// Font
extern char tilfont, palfont;

// Player
extern char gfxplayer, gfxplayer_end;
extern char palplayer, palplayer_end;

// Laser
extern char gfxlaser, gfxlaser_end;
extern char pallaser, pallaser_end;

// Alien
extern char gfxalien, gfxalien_end;
extern char palalien, palalien_end;

// Wave sound
extern char soundbrr, soundbrrend;
brrsamples lasersound;

// Screen dimensions SCREEN_TOP = 0, SCREEN_BOTTOM = 224, SCREEN_LEFT = 0, SCREEN_RIGHT = 256
enum {SCREEN_TOP = 2, SCREEN_BOTTOM = 208, SCREEN_LEFT = 2, SCREEN_RIGHT = 238};

// Game state
enum {INIT = 0, TAKE_OFF_1 = 1, TAKE_OFF_2 = 2, TAKE_OFF_3 = 3, PLAY = 4, ALIEN_KILL = 5, GAME_OVER = 6};

// Sprite structure
typedef struct
{
    short x, y;
    int gfx_frame;
    int state;
    int anim_frame;
    int flipx;
} Sprite;

u8 ticks;
u8 playerSpeed = 2;                 // Player movement speed
u8 alienSpeed = 0;                  // Alien movement speed
u8 keyapressed = 0;                 // 1 if key A is actually pressed
u8 keySelectPressed = 0;            // 1 if key Select is actually pressed
u8 shoot = 0;                       // Laser animation
u8 gameState = TAKE_OFF_1;          // Current game state

void splashScreen()
{
    unsigned short pad0;

    setFadeEffect(FADE_IN);

    // Initialize text console with our font
    consoleSetTextMapPtr(0x6000);
    consoleSetTextGfxPtr(0x3000);
    consoleInitText(1, 16 * 2, &tilfont, &palfont);

    bgSetGfxPtr(1, 0x3000);
    bgSetMapPtr(1, 0x6000, SC_32x32);
    bgInitTileSet(0, &patterns_splash, &palette_splash, 0, (&patterns_splash_end - &patterns_splash), (&palette_splash_end - &palette_splash), BG_16COLORS, 0x2000);
    bgSetMapPtr(0, 0x6800, SC_64x32);
    bgInitMapSet(0, &map_splash, (&map_splash_end - &map_splash), SC_32x32, 0x0000);

    // Now Put in 16 color mode and disable BG3
    setMode(BG_MODE1, 0);
    bgSetDisable(2);

    setScreenOn();
    
    // Game loop
    while (1)
    {
        ticks++;
        if (ticks == 33)
            ticks=1;

        pad0 = padsCurrent(0);
        if (pad0)
        {
            if(pad0 & KEY_START)
            {
                consoleDrawText(10,8,"            ");
                break;
            }
    }

    // "press button" blink
    if( ticks < 15 )
        consoleDrawText(10,8,"PRESS START");
    else
        consoleDrawText(10,8,"            ");

    WaitForVBlank();
    }

    // //Fade out to start game
    setFadeEffect(FADE_OUT);
    //setFadeEffectEx(FADE_OUT,2);

    // Mosaic effect test
    // setMosaicEffect(MOSAIC_OUT, MOSAIC_BG1);
    // WaitForVBlank();

    // not blinking
    setBrightness(0);    
}

void game()
{
    unsigned short pad0;                                            // Controller player 1
    u8 playMusic = 1;                                               // 1 if music is played
    Sprite player = {0,180};                                        // Player structure
    Sprite laser = {LASER_OFF_SCREEN,LASER_OFF_SCREEN};             // Laser structure
    Sprite alien = {ALIEN_OFF_SCREEN,ALIEN_OFF_SCREEN};             // Alien structure

    // I use u16 because of an unresolved bug
    u16 score = 0;                                                  // Player score
    u16 timerAlien = 0;                                             // When to show the next alien
    u16 startTimerAlien = 0;                                        // 1 if timer is running
    u16 timerPressStart = 1000;                                     // When to show the next alien
    u16 startTimerPressStart = 0;                                   // 1 if timer is running

    // Music: Initialize sound engine (take some time)
    spcBoot();

    // Wav: allocate around 10K of sound ram (39 256-byte blocks)
    spcAllocateSoundRegion(39);

    // Music: Set give soundbank
    spcSetBank(&SOUNDBANK__);
    // Music: Load music
    spcLoad(MOD_AXELF);

    // player:  Init Sprites gfx and palette with default size of 32x32
    oamInitGfxSet(&gfxplayer, (&gfxplayer_end - &gfxplayer), &palplayer, (&palplayer_end - &palplayer), 0, ADRSPRITEPLAYER, OBJ_SIZE16_L32);

    // player: Define sprites parameters
    oamSet(0, player.x, player.y, 3, 0, 0, 0, 0); // Put sprite in x,y, with maximum priority 3 from tile entry 0, palette 0
    oamSetEx(0, OBJ_SMALL, OBJ_SHOW);

    // laser: load laser sprite with palette
    // NOTE: There is a bug, the palette used by the laser is the player's palette. To solve this problem I use the same palette for each sprite bmp file.
    dmaCopyVram(&gfxlaser, ADRSPRITELASER, (&gfxlaser_end - &gfxlaser));
    dmaCopyCGram(&pallaser, (&pallaser_end - &pallaser), PALETTESPRSIZE);

    // alien: load alien sprite with palette
    // NOTE: There is a bug, the palette used by the laser is the player's palette. To solve this problem I use the same palette for each sprite bmp file.
    dmaCopyVram(&gfxalien, ADRSPRITEALIEN, (&gfxalien_end - &gfxalien));
    dmaCopyCGram(&palalien, (&palalien_end - &palalien), PALETTESPRSIZE);

    // Read tiles to VRAM
    bgInitTileSet(0, &back, &palette, 0, (&back_end - &back), 16 * 2 * 2, BG_16COLORS, 0x1000);

    // Copy Map to VRAM
    bgInitMapSet(0, &map, 64 * 32 * 2, SC_64x32, 0x0000);

    // Initialize text console with our font
    consoleSetTextMapPtr(0x6000);
    consoleSetTextGfxPtr(0x3000);
    consoleInitText(1, 16 * 2, &tilfont, &palfont);

    // Now Put in 16 color mode and disable other BGs except 1st one
    setMode(BG_MODE1, 0);
    //bgSetDisable(1);
    bgSetDisable(2);
    setScreenOn();

    // Fade IN effect test
    //setFadeEffect(FADE_IN);

    // Mosaic effect test (restore screen to normal)
    // setMosaicEffect(MOSAIC_IN, MOSAIC_BG1);
    // WaitForVBlank();

    // Music: Play file from the beginning
    spcPlay(0);

    // Init HDMA table
    // 72 lines with 1 scrolling effect (top of screen)
    // Next 88 lines with 2 scrolling effect (middle of screen)
    // Next 64 lines with 4 scrolling effect (end of screen)
    HDMATable16[0] = 72;
    *(u16 *)&(HDMATable16 + 1) = 1;
    HDMATable16[3] = 88;
    *(u16 *)&(HDMATable16 + 4) = 2;
    HDMATable16[6] = 64;
    *(u16 *)&(HDMATable16 + 7) = 4;
    HDMATable16[9] = 0x00; // end of hdma table

    // Load wav
    spcSetSoundEntry(15, 15, 4, &soundbrrend - &soundbrr, &soundbrr, &lasersound);

    // Player: initial posiyion
    oamSet(0,  player.x, player.y, 3, player.flipx, 0, player.gfx_frame, 0);

    // Wait for key to change gradient
    while (1)
    {
        // Music: Update music / sfx stream and wait vbl
        spcProcess();

        // in fact, it's not necessary to display the text at each iteration, but if I don't do it I have slowdowns on emulator (need to check)
        // Draw score
        consoleDrawText(28, 1, "%u", score);

        pad0 = padsCurrent(0);
        if (pad0  && (gameState == PLAY))
        {
            // Music: L = stop / R = play
            if((pad0 & KEY_L) && (playMusic == 1))
            {
                playMusic = 0;
                spcStop();
            }
            if((pad0 & KEY_R) && (playMusic == 0))
            {
                playMusic = 1;
                spcPlay(0);
            }
            
            // Sprite: Update sprite with current pad
            if(pad0 & KEY_UP )
            {
                if(player.y >= SCREEN_TOP)
                    player.y -= playerSpeed;
                //player.state = W_UP;
                //player.flipx = 0;
            }
            if(pad0 & KEY_LEFT)
            {
                if(player.x >= SCREEN_LEFT)
                    player.x -= playerSpeed;
                //player.state = W_LEFT;
                //player.flipx = 0;
            }
            if(pad0 & KEY_RIGHT)
            {
                if(player.x <= SCREEN_RIGHT)
                    player.x += playerSpeed;
                //player.state = W_LEFT;
                //player.flipx = 0;
            }
            if(pad0 & KEY_DOWN)
            {
                if(player.y <= SCREEN_BOTTOM)
                    player.y += playerSpeed;
                //player.state = W_DOWN;
                //player.flipx = 0;
            }

            // Shoot Key a, b, x, y 
            if ((padsCurrent(0) & KEY_A) || (padsCurrent(0) & KEY_B) || (padsCurrent(0) & KEY_X) || (padsCurrent(0) & KEY_Y))
            {
                if ((keyapressed == 0) && (gameState == PLAY))
                {
                    if (shoot == 0)
                    {
                        laser.x = player.x;
                        laser.y = player.y;
                        keyapressed = 1;
                        shoot = 1;
                        // Play effect
                        spcPlaySound(0);
                    }
                }
            }
            else
                keyapressed = 0;

            // Change player speed
            if (padsCurrent(0) & KEY_SELECT)
            {
                if (keySelectPressed == 0)
                {
                    keySelectPressed = 1;
                    // speed
                    switch(playerSpeed)
                    {
                        case 2: 
                            playerSpeed = 4;    // Medium speed
                            break;
                        case 4: 
                            playerSpeed = 6;    // Fast speed
                            break;
                        case 6: 
                            playerSpeed = 2;    // Low speed
                            break;
                        default:
                            playerSpeed = 2;
                            break;
                    }
                }
            }
            else
                keySelectPressed = 0;
        }
        else
        {
            keyapressed = 0;
            keySelectPressed = 0;
        }

        // Update scrolling
        setParallaxScrolling(0);

        // Change scrolling inside HDMA table
        *(u16 *)&(HDMATable16 + 1) += 1;
        *(u16 *)&(HDMATable16 + 4) += 2;
        *(u16 *)&(HDMATable16 + 7) += 4;

        switch (gameState)
        {
            case INIT:
                if (startTimerPressStart)
                {
                    consoleDrawText(10,8,"PRESS START");
                    if (snes_vblank_count > timerPressStart)
                    {
                        startTimerPressStart = 0; // stop timer
                        consoleDrawText(10,8,"            ");   // remove Press Start
                    }
                }

                if (padsCurrent(0) & KEY_START)
                {
                    startTimerPressStart = 0; // stop timer
                    consoleDrawText(10,8,"            ");
                    gameState = TAKE_OFF_1;
                }
                break;

            case TAKE_OFF_1:
                if (player.x < 100)
                    player.x += 1;
                else
                    gameState = TAKE_OFF_2;
                break;

            case TAKE_OFF_2:
                if (player.x < 225)
                {
                    player.x += 1;
                    player.y -= 1;
                }
                else
                    gameState = TAKE_OFF_3;
                break;

            case TAKE_OFF_3:
                if (player.x > 20)
                    player.x -= 1;
                else
                {
                    // Start game
                    gameState = PLAY;
                    // Random Alien position
                    alien.y = rand() % 150;
                    alienSpeed = 1;
                    score = 0;
                    consoleDrawText(28,1,"            ");   // Clear score text
                }        
                break;

            case PLAY:
                if (startTimerAlien)
                {
                    if (snes_vblank_count > timerAlien)
                    {
                        startTimerAlien = 0; // stop timer
                        //random Alien position
                        alien.y = rand() % 150;
                    }
                }
                else
                {
                    // Alien: move
                    alien.x -= alienSpeed;
                    // loose
                    if (alien.x < -16)
                    {
                        // init game
                        gameState = INIT;
                        player.x = 30;
                        player.y = 180;
                        laser.x = LASER_OFF_SCREEN;
                        laser.y = LASER_OFF_SCREEN;
                        alien.x = ALIEN_OFF_SCREEN;
                        alien.y = ALIEN_OFF_SCREEN;
                        //score = 0;
                        timerAlien = 0;
                        startTimerAlien = 0;
                        playerSpeed = 2;
                        alienSpeed = 0;
                        keyapressed = 0;
                        keySelectPressed = 0;
                        shoot = 0;
                        timerPressStart = snes_vblank_count + 1000;   // Update timer press start
                        startTimerPressStart = 1;

                        // Laser: update 
                        oamSet(4, laser.x, laser.y, 3, 0, 0, 0x0050, 0);
                    }
                }
                break;

            default:
                break;
        }

        // Player: update
        oamSet(0,  player.x, player.y, 3, player.flipx, 0, player.gfx_frame, 0);

        // Laser: update
        if (shoot)
        {
            oamSet(4, laser.x, laser.y, 3, 0, 0, 0x0050, 0);
            laser.x += 20;
            if (laser.x > LASER_OFF_SCREEN)
                shoot = 0;

            // Alien is hit
            if (laser.y < alien.y + 8 && laser.y > alien.y - 8 && laser.x < alien.x + 25 + alienSpeed && laser.x > alien.x)
            {
                // Every 10 points we increase the speed of Alien
                score++;
                if ((score % 10) == 0)
                    alienSpeed++;
                
                // Put Alien off screen
                alien.x = ALIEN_OFF_SCREEN;
                alien.y = ALIEN_OFF_SCREEN;
                timerAlien = snes_vblank_count + TIMER_ALIEN_DELTA;   // Update timer limit
                startTimerAlien = 1;
            }
        }

        // Alien: update
        oamSet(8,  alien.x, alien.y, 3, alien.flipx, 0, 0x0100, 0);
        
        //consoleDrawText(10, 10, "COUNTER=%u", snes_vblank_count);
        WaitForVBlank();
    }
}

int main(void)
{
    //consoleInit();
    splashScreen();
    game();
    return 0;
}
