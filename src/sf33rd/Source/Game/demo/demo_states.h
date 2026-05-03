/**
 * @file demo_states.h
 * @brief Named state constants for the demo / boot-sequence state machines.
 *
 * Each function in the demo module has its own enum so that the numeric
 * indices carry a meaningful label that describes what the state does.
 *
 * Part of the demo module.
 */

#ifndef DEMO_STATES_H
#define DEMO_STATES_H

/* Warning() — boot warning overlay (demo_00_attract_mode.c) */
typedef enum {
    WARN_INIT = 0,      /* set up, jump to fade-in                     */
    WARN_FADE_IN = 5,   /* FadeIn + Put_Warning                        */
    WARN_DISPLAY = 6,   /* hold on screen, check skip input            */
    WARN_SKIP_WAIT = 7, /* short pause before fade-out (interruptible) */
    WARN_FADE_OUT = 8,  /* FadeOut + Put_Warning                       */
    WARN_DONE = 9       /* TexRelease, signal Next_Demo                */
} WarningState;

/* CAPCOM_Logo() — logo animation sequence (demo_00_attract_mode.c) */
typedef enum {
    CAPLOGO_INIT = 0,      /* check ADX/sel files loaded          */
    CAPLOGO_LOAD_BGM = 1,  /* Standby_BGM, PPG init, queue tex   */
    CAPLOGO_WAIT_LOAD = 2, /* poll LDREQ clear                   */
    CAPLOGO_PRE_ANIM = 3,  /* countdown timer, then Go_BGM       */
    CAPLOGO_ANIMATE = 4,   /* palette cycle animation             */
    CAPLOGO_FADE_IN = 5,   /* FadeIn + queue next textures        */
    CAPLOGO_HOLD = 6,      /* hold on screen                      */
    CAPLOGO_FADE_OUT = 7   /* FadeOut                             */
} CapLogoState;

/* Title() — title screen opening demo (demo_01_instruction.c) */
typedef enum {
    TITLE_WAIT_LOAD = 0,      /* poll LDREQ, standby BGM         */
    TITLE_PLAY_OPENING = 1,   /* run opening_demo, wait for done */
    TITLE_PRE_TRANSITION = 2, /* opening_demo + Switch_Screen_Init */
    TITLE_TRANSITION = 3,     /* Switch_Screen fade              */
    TITLE_DONE = 4            /* final Switch_Screen tick         */
} TitleState;

/* Title_At_a_Dash() — quick title after attract (demo_01_instruction.c) */
typedef enum {
    TITLE_DASH_INIT = 0, /* TITLE_Init if needed, set timer */
    TITLE_DASH_SHOW = 1  /* countdown, TITLE_Move           */
} TitleDashState;

/* Demo00() — quick-start attract gameplay (demo_02_parry_tutorial.c) */
typedef enum {
    DEMO00_SETUP = 0,     /* texcash, Game_pause, random Weak_PL */
    DEMO00_COVER = 1,     /* Switch_Screen + Game_Fight, cover timer */
    DEMO00_REVEAL = 2,    /* Switch_Screen_Revival               */
    DEMO00_PLAY = 3,      /* main gameplay timer / Conclusion    */
    DEMO00_WIND_DOWN = 4, /* post-conclusion timer               */
    DEMO00_PAUSE = 5,     /* Game_pause, Disappear_LOGO          */
    DEMO00_FADE_OUT = 6   /* Switch_Screen + BGM_Stop            */
} Demo00State;

/* Demo01() — full attract: char select → gameplay (demo_02_parry_tutorial.c) */
typedef enum {
    DEMO01_SETUP = 0,   /* Before_Select_Sub, char/arts setup */
    DEMO01_SELECT = 1,  /* Game_CharSelect char select, Demo_Time_Stop */
    DEMO01_COVER = 2,   /* Switch_Screen + Game_Fight              */
    DEMO01_REVEAL = 3,  /* Switch_Screen_Revival              */
    DEMO01_PLAY = 4,    /* main gameplay timer                */
    DEMO01_PAUSE = 5,   /* countdown, SsBgmFadeOut            */
    DEMO01_FADE_OUT = 6 /* Switch_Screen + BGM_Stop           */
} Demo01State;

#endif /* DEMO_STATES_H */
