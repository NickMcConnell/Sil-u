/* File: main-286.c */

/* Warning: This file is OUT OF DATE and probably not usable */


/*
 * Hack -- there are two versions in this file, I do not know
 * which is which, sorry, they are probably both out of date.
 */
 
#ifdef VERSION_OLD

/* File: main-286.c (use makefile.286 to compile)          */
/* which doesn't exist.  Yet.  Borland's prj2mak wants to put */
/* a bunch of upper ascii (>127) characters in the middle of the */
/* makefile for some reason.  RJR */
/* Purpose: Termcap support for "term.c", for Borland C/C++ */
/* Author:  akemi@netcom.com (David Boeren)                */
/* Modified for use by Borland C/C++ and the 286 version by */
/*          jay@map.com (Roland Jay Roberts)               */

/* Additional changes to Borland C conio text-mode console */
/* routines added 10/13/95.  The Watcom routines (basically */
/* straight memcpy to display memory) failed. */
/* jay@map.com (Roland Jay Roberts) */


#include "angband.h"

#ifdef USE_286

#include <bios.h>
#include <dos.h>
#include <string.h>
#include <conio.h>

/* Virtual Screen Size */
#define ROWS 25
#define COLS 80

/* Make sure the Borland ConIO functions do direct video */
/* writes. */

extern int directvideo = 1;

/*********************************************************************
*
 *
 *  defines for color video attributes in DOS
 *

*********************************************************************/

#define VID_BLACK       0
#define VID_BLUE        1
#define VID_GREEN       2
#define VID_CYAN        3
#define VID_RED         4
#define VID_MAGENTA     5
#define VID_YELLOW      6
#define VID_WHITE       7

#define VID_BLINK       8
#define VID_BRIGHT      8

/*
 * The Angband color table
 */
static const byte ibm_color[] = {
  (VID_BLACK)                   /* TERM_DARK */,
  (VID_WHITE)                   /* TERM_WHITE */,
  (VID_CYAN)                    /* TERM_SLATE */,
  (VID_YELLOW | VID_BRIGHT)     /* TERM_ORANGE */,
  (VID_RED)                     /* TERM_RED */,
  (VID_GREEN)                   /* TERM_GREEN */,
  (VID_BLUE)                    /* TERM_BLUE */,
  (VID_YELLOW)                  /* TERM_UMBER */,
  (VID_BLACK | VID_BRIGHT)      /* TERM_L_DARK */,
  (VID_CYAN | VID_BRIGHT)       /* TERM_L_WHITE */,
  (VID_MAGENTA)                 /* TERM_VIOLET */,
  (VID_YELLOW | VID_BRIGHT)     /* TERM_YELLOW */,
  (VID_RED | VID_BRIGHT)        /* TERM_L_RED */,
  (VID_GREEN | VID_BRIGHT)      /* TERM_L_GREEN */,
  (VID_BLUE | VID_BRIGHT)       /* TERM_L_BLUE */,
  (VID_YELLOW)                  /* TERM_L_UMBER */
};

/* The main screen */
static term term_screen_body;

/* Note Borland thinks the screen starts at 1,1, so we add 1 to */
/* positions. */

/* Move the cursor */
static errr Term_curs_286(int x, int y, int z)
{
    gotoxy(x+1,y+1);
    return (0);
}

/*
 * Place some text on the screen using an attribute
 */
/*
 * Changed 1995/08/28 - Roland Jay Roberts
 */
static errr Term_text_286(int x, int y, int n, byte a, cptr s)
{
    register int i;

    /* Set the attribute */
    textattr(ibm_color[a]);

    /* Place the cursor */
    gotoxy(x+1, y+1);

    for (i = 0; s[i]; i++) putch(s[i]);

    return (0);
}


/*
 * Erase a block of the screen
 */
static errr Term_wipe_286(int x, int y, int w, int h)
{
    /* Paranoia -- Verify the dimensions */
    if (ROWS < (h+y)) h = (ROWS-y);
    if (COLS < (w+x)) w = (COLS-x);

    /* Efficiency -- handle "full screen" case */
    if (!x && !y && (w >= COLS-1) && (h >= ROWS-1)) {
        /* Clear the screen */
        clrscr();

        /* Success */
        return (0);
    }

    /* Here we set the window to clear.  Why I don't need to add */
    /* 1 to the last two parameters in the first call is a mystery.  
*/
    /* But it works! */

    window(x+1,y+1,x+w,y+h);
    clrscr();
    window(1,1,COLS,ROWS);

    return (0);
}

/* Modifier flags (see below): */
#define K_RSHIFT       0   /* Right shift key down */
#define K_LSHIFT    1   /* Left shift key down  */
#define K_CTRL      2   /* Ctrl key down        */
#define K_ALT       3   /* Alt key down         */
#define K_SCROLL    4   /* Scroll lock on       */
#define K_NUM       5   /* Num lock on          */
#define K_CAPS      6   /* Caps lock on         */
#define K_INSERT    7   /* Insert on            */


/* Hack -- hold data about a keypress */
typedef struct {
    byte scan;
    byte value;
    bool modifier[8];
} Key;



/* Range of keypad scan codes */
#define KEYPADHI       83
#define KEYPADLOW      71

/* Determine if a "Key" is on the Keypad */
#define ISKEYPAD(X)    ((KEYPADLOW <= (X).scan) && ((X).scan <=
KEYPADHI))


/* Hack -- map the keypad keycodes into ascii keys */
static char keypad_char[KEYPADHI - KEYPADLOW + 1] = {
       '7',            /* 7 */
       '8',            /* 8 */
       '9',            /* 9 */
       '-',            /* - */
       '4',            /* 4 */
       '5',            /* 5  - move */
       '6',            /* 6 */
       '+',            /* + */
       '1',            /* 1 */
       '2',            /* 2 */
       '3',            /* 3 */
       '0',            /* 0/Ins */
       '.'             /* ./Del */
};

/*
 * Handle a "special request"
 *
 * Below, note that you can't get away with just sending the ASCII
code,
 * since certain ways of pressing the keypad return no ASCII (only
scan).
 * For instance, SHIFT while numlock is on.
 *
 * if a keypad key is pressed, we check whether it was numeric
 * (i.e. directional).  If so, we need to send some sort of
directional
 * command.  The possible directional commands are walk and dig. 
Also,
 * the shift key can cause any directional command to be repeated, so
 * that walk becomes run and dig becomes repeated digging.
 *
 *
 * This function was rewritten because nobody is allowed to reference
 * the "roguelike" flag except the inner circle.  -BEN-
 *
 * Hack -- This function automatically processes Control or Shift plus
a
 * keypad key, instead of using a nice clean macro.  However, we still
 * send a leading "control-caret" so it is still possible (though
ugly)
 * for the user to redefine control-keypad or shift-keypad.
 *
 * Note that we can send actual "numerical" directions, since all of
the
 * Angband code is designed to accept them.  We use the "reserved"
keys
 * for tunneling ('+') and running ('\\') plus a direction.  We cannot
 * use the '.' key to run, since that only works on original mode, and
 * we are not allowed to query the state of the "roguelike" flag.
 *
 * So, all keypad key first send the keypad signal (control-caret),
 * plus the run ("\\") or tunnel ("+") command if any is requested,
 * plus the actual ascii value of the key (usually numerical).
 * Note that this requires the "default macro" for "control-caret"
 * which maps to "nothing", but this is done automatically in
"main.c".
 *
 * Note that the user can access control+shift+keypad by watching for
 * the rather heinous key-sequence control-caret plus "\\" plus "+"
 * plus the numerical direction.  This is a total hack.  Sorry...
 *
 * Note that the "proper" way to handle this "mapping" thing is to
 * use the new "command macro" facility to make macros that map the
 * various control-underscore sequnces to the appropriate actions.
 *
 * Lastly, we may be able to speed up the I/O by providing another
 * "Term_xtra()" action which says that a certain line should be
 * flushed.  The question is which is faster, dumping a full 80
 * character line once, or dumping three or four much smaller
 * pieces of that line as they arrive.
 */
static errr Term_xtra_286(int n, int v)
{
    int i, j;
    Key key;

    /* Analyze the request */
    switch (n) {

       /* Make a noise */
       case TERM_XTRA_NOISE:

           (void)write(1, "\007", 1);
           return 0;

       /* Check for a keypress (no waiting) */
       case TERM_XTRA_CHECK:

           /* Check for a keypress */
           i = bioskey(1);

           /* Notice "no key ready" */
           if (!i) return (1);

           /* Fall through */

       /* Wait for a single event */
       case TERM_XTRA_EVENT:

           /* Wait for a keypress */
           i = bioskey(0);

           /* Analyze the keypress */
           key.value = i & 0xFF;
           key.scan = (i>>8) & 0xFF;

           /* Analyze the modifiers */
           i = bioskey(2);
           for (j = 0; j < 8; j++) {
           key.modifier[j] = (i & (1 << j)) ? TRUE : FALSE;
           }

           /* Mega-Hack -- Process "Keypad" keys */
           if (ISKEYPAD(key)) {

           /* Hack -- send the special "keypad" signal */
           Term_keypress(30);

           /* Hack -- convert shift key into "run" */
           if (key.modifier[K_RSHIFT] || key.modifier[K_LSHIFT]) {

               /* Hack -- send "run" command */
               Term_keypress('\\');
           }

           /* Hack -- convert control key into "tunnel" */
           if (key.modifier[K_CTRL]) {

               /* Hack -- send "tunnel" command */
               Term_keypress('+');
           }

           /* Hack -- Send the actual keypad symbol */
           Term_keypress(keypad_char[key.scan - KEYPADLOW]);
           }

           /* Handle "normal" keys */
           else if (key.value) {

           /* Must be a 'normal' key */
           Term_keypress(key.value);
           }

           /* Handle "special" keys */
           else {

           /* Hack -- a special "macro introducer" */
           Term_keypress(31);

           /* Send the modifiers */
           if (key.modifier[K_CTRL]) Term_keypress('C');
           if (key.modifier[K_LSHIFT]) Term_keypress('S');
           if (key.modifier[K_ALT]) Term_keypress('O');
           if (key.modifier[K_RSHIFT]) Term_keypress('X');

           /* The upper 8 bits are the "special key" */
           /* Hack -- encode the keypress (in decimal) */
           Term_keypress('0' + (key.scan % 1000) / 100);
           Term_keypress('0' + (key.scan % 100) / 10);
           Term_keypress('0' + (key.scan % 10));

           /* End the macro (with "return") */
           Term_keypress(15);
           }

           /* Success */
           return 0;
    }

    /* Unknown request */
    return (1);
}


/*
 * Initialize the Borland 286/IBM "visual module"
 */
errr init_286()
{
    int i;

    term *t = &term_screen_body;

    /* Erase the screen */
    clrscr();

    /* Initialize the term -- very large key buffer */
    /* do we need a large key buffer?  Why? (jay@map.com) */
    /* Buffer reduced to 256 bytes */

    term_init(t, COLS, ROWS - 1, 256);

    /* Connect the hooks */
    t->text_hook = Term_text_286;
    t->wipe_hook = Term_wipe_286;
    t->curs_hook = Term_curs_286;
    t->xtra_hook = Term_xtra_286;

    /* Save it */
    term_screen = t;

    /* Activate it */
    Term_activate(term_screen);

    /* Success */
    return 0;
}

#endif /* USE_286 */

#endif

#ifdef VERSION_OLDER

/* Purpose: Termcap support for "term.c", for Watcom C/C++ */
/* Author:  akemi@netcom.com (David Boeren)                */
/* Modified for use by Borland C/C++ and the 286 version by */
/*          jay@map.com (Roland Jay Roberts)               */

/* Additional changes to Borland C conio text-mode console */
/* routines added 10/13/95.  The Watcom routines (basically */
/* straight memcpy to display memory) failed. */
/* jay@map.com (Roland Jay Roberts) */


#include "angband.h"

#ifdef USE_286

#include <bios.h>
#include <dos.h>
#include <string.h>
#include <conio.h>

#undef CTRL
#define CTRL(X) ((X) & 31)

/* Virtual Screen Size */
static int rows, cols;

/* Make sure the Borland ConIO functions do direct video */
/* writes. */

extern int directvideo = 1;

/*********************************************************************
*
 *
 *  defines for color video attributes in DOS
 *

*********************************************************************/

#define VID_BLACK       0
#define VID_BLUE        1
#define VID_GREEN       2
#define VID_CYAN        3
#define VID_RED         4
#define VID_MAGENTA     5
#define VID_YELLOW      6
#define VID_WHITE       7

#define VID_BLINK       8
#define VID_BRIGHT      8

#define IBM_BLACK       (VID_BLACK)
#define IBM_WHITE       (VID_WHITE)
#define IBM_GRAY        (VID_WHITE)
#define IBM_ORANGE      (VID_YELLOW | VID_BRIGHT)
#define IBM_RED         (VID_RED)
#define IBM_GREEN       (VID_GREEN)
#define IBM_BLUE        (VID_BLUE)
#define IBM_UMBER       (VID_YELLOW)
#define IBM_D_GRAY      (VID_BLACK | VID_BRIGHT)
#define IBM_L_GRAY      (VID_CYAN | VID_BRIGHT)
#define IBM_VIOLET      (VID_MAGENTA)
#define IBM_YELLOW      (VID_YELLOW | VID_BRIGHT)
#define IBM_L_RED       (VID_RED | VID_BRIGHT)
#define IBM_L_GREEN     (VID_GREEN | VID_BRIGHT)
#define IBM_L_BLUE      (VID_BLUE | VID_BRIGHT)
#define IBM_L_UMBER     (VID_YELLOW)


/* The Angband color table */
static const byte ibm_color[] = {
    IBM_BLACK,  IBM_WHITE,   IBM_GRAY,   IBM_ORANGE,
    IBM_RED,    IBM_GREEN,   IBM_BLUE,   IBM_UMBER,
    IBM_D_GRAY, IBM_L_GRAY,  IBM_VIOLET, IBM_YELLOW,
    IBM_L_RED,  IBM_L_GREEN, IBM_L_BLUE, IBM_L_UMBER
   /* there may be 64 official Angband colors someday, so don't
    * fill these in with stuff you'll need.  Use the colors after 64.
    */
};

/* The main screen */
static term term_screen_body;

int getuid()
{
  return 0;
}

int ScreenRows(void)
{
    return 25;
}

int ScreenCols(void)
{
    return 80;
}

/* Note Borland thinks the screen starts at 1,1, so we add 1 to */
/* positions. */

void ScreenUpdateLine(byte *buf, int line)
{
    gotoxy(1,line+1);
    cputs(buf);
}

void ScreenClear(void)
{
   clrscr();
}

/* See note above about screen positioning. */

void ScreenSetCursor(int y, int x)
{
     gotoxy(x+1,y+1);
}

/* Move the cursor */
static errr Term_curs_286(int x, int y, int z)
{
    ScreenSetCursor(y,x);
    return (0);
}

/*
 * Place some text on the screen using an attribute
 */
/*
 * Changed 1995/08/28 - Roland Jay Roberts
 */
static errr Term_text_286(int x, int y, int n, byte a, cptr s)
{
    register int i;
    register byte attr;

    /* Allow negative length */
    if (n < 0) n = strlen(s);

    /* Paranoia */
    if (n > cols - x) n = cols - x;

    /* Convert the color */
    attr = ibm_color[a];

    for (i=0;(i < n) && s[i] ;i++ )
    {
       /* set the screen position where we want the text. */
       ScreenSetCursor(y,x+i);
       /* Set the attribute byte for the character */
       textattr(attr);
       /* write the character to the screen */
       putch(s[i]);
    } /* endfor */

    return (0);
}


/*
 * Erase a block of the screen
 */
static errr Term_wipe_286(int x, int y, int w, int h)
{
    /* Paranoia -- Verify the dimensions */
    if (rows < (h+y)) h = (rows-y);
    if (cols < (w+x)) w = (cols-x);

    /* Efficiency -- handle "full screen" case */
    if (!x && !y && (w >= cols-1) && (h >= rows-1)) {
        /* Clear the screen */
        ScreenClear();

        /* Success */
        return (0);
    }

    /* Here we set the window to clear.  Why I don't need to add */
    /* 1 to the last two parameters is a mystery.  But it works! */

    window(x+1,y+1,x+w,y+h);
    clrscr();
    window(1,1,cols,rows);

    return (0);
}



/* Modifier flags (see below): */
#define K_RSHIFT       0   /* Right shift key down */
#define K_LSHIFT    1   /* Left shift key down  */
#define K_CTRL      2   /* Ctrl key down        */
#define K_ALT       3   /* Alt key down         */
#define K_SCROLL    4   /* Scroll lock on       */
#define K_NUM       5   /* Num lock on          */
#define K_CAPS      6   /* Caps lock on         */
#define K_INSERT    7   /* Insert on            */


/* Hack -- hold data about a keypress */
typedef struct {
    byte scan;
    byte value;
    bool modifier[8];
} Key;



/* Range of keypad scan codes */
#define KEYPADHI       83
#define KEYPADLOW      71

/* Determine if a "Key" is on the Keypad */
#define ISKEYPAD(X)    ((KEYPADLOW <= (X).scan) && ((X).scan <=
KEYPADHI))


/* Hack -- map the keypad keycodes into ascii keys */
static char keypad_char[KEYPADHI - KEYPADLOW + 1] = {
       '7',            /* 7 */
       '8',            /* 8 */
       '9',            /* 9 */
       '-',            /* - */
       '4',            /* 4 */
       '5',            /* 5  - move */
       '6',            /* 6 */
       '+',            /* + */
       '1',            /* 1 */
       '2',            /* 2 */
       '3',            /* 3 */
       '0',            /* 0/Ins */
       '.'             /* ./Del */
};

/*
 * Handle a "special request"
 *
 * Below, note that you can't get away with just sending the ASCII
code,
 * since certain ways of pressing the keypad return no ASCII (only sca
n).
 * For instance, SHIFT while numlock is on.
 *
 * if a keypad key is pressed, we check whether it was numeric
 * (i.e. directional).  If so, we need to send some sort of
directional
 * command.  The possible directional commands are walk and dig. 
Also,
 * the shift key can cause any directional command to be repeated, so
 * that walk becomes run and dig becomes repeated digging.
 *
 *
 * This function was rewritten because nobody is allowed to reference
 * the "roguelike" flag except the inner circle.  -BEN-
 *
 * Hack -- This function automatically processes Control or Shift plus
a
 * keypad key, instead of using a nice clean macro.  However, we still
 * send a leading "control-caret" so it is still possible (though
ugly)
 * for the user to redefine control-keypad or shift-keypad.
 *
 * Note that we can send actual "numerical" directions, since all of
the
 * Angband code is designed to accept them.  We use the "reserved"
keys
 * for tunneling ('+') and running ('\\') plus a direction.  We cannot
 * use the '.' key to run, since that only works on original mode, and
 * we are not allowed to query the state of the "roguelike" flag.
 *
 * So, all keypad key first send the keypad signal (control-caret),
 * plus the run ("\\") or tunnel ("+") command if any is requested,
 * plus the actual ascii value of the key (usually numerical).
 * Note that this requires the "default macro" for "control-caret"
 * which maps to "nothing", but this is done automatically in
"main.c".
 *
 * Note that the user can access control+shift+keypad by watching for
 * the rather heinous key-sequence control-caret plus "\\" plus "+"
 * plus the numerical direction.  This is a total hack.  Sorry...
 *
 * Note that the "proper" way to handle this "mapping" thing is to
 * use the new "command macro" facility to make macros that map the
 * various control-underscore sequnces to the appropriate actions.
 *
 * Lastly, we may be able to speed up the I/O by providing another
 * "Term_xtra()" action which says that a certain line should be
 * flushed.  The question is which is faster, dumping a full 80
 * character line once, or dumping three or four much smaller
 * pieces of that line as they arrive.
 */
static errr Term_xtra_286(int n, int v)
{
    int i, j;
    Key key;

    /* Analyze the request */
    switch (n) {

       /* Make a noise */
       case TERM_XTRA_NOISE:

           (void)write(1, "\007", 1);
           return 0;

       /* Check for a keypress (no waiting) */
       case TERM_XTRA_CHECK:

           /* Check for a keypress */
           i = bioskey(1);

           /* Notice "no key ready" */
           if (!i) return (1);

           /* Fall through */

       /* Wait for a single event */
       case TERM_XTRA_EVENT:

           /* Wait for a keypress */
           i = bioskey(0);

           /* Analyze the keypress */
           key.value = i & 0xFF;
           key.scan = (i>>8) & 0xFF;

           /* Analyze the modifiers */
           i = bioskey(2);
           for (j = 0; j < 8; j++) {
           key.modifier[j] = (i & (1 << j)) ? TRUE : FALSE;
           }

           /* Mega-Hack -- Process "Keypad" keys */
           if (ISKEYPAD(key)) {

           /* Hack -- send the special "keypad" signal */
           Term_keypress(30);

           /* Hack -- convert shift key into "run" */
           if (key.modifier[K_RSHIFT] || key.modifier[K_LSHIFT]) {

               /* Hack -- send "run" command */
               Term_keypress('\\');
           }

           /* Hack -- convert control key into "tunnel" */
           if (key.modifier[K_CTRL]) {

               /* Hack -- send "tunnel" command */
               Term_keypress('+');
           }

           /* Hack -- Send the actual keypad symbol */
           Term_keypress(keypad_char[key.scan - KEYPADLOW]);
           }

           /* Handle "normal" keys */
           else if (key.value) {

           /* Must be a 'normal' key */
           Term_keypress(key.value);
           }

           /* Handle "special" keys */
           else {

           /* Hack -- a special "macro introducer" */
           Term_keypress(31);

           /* Send the modifiers */
           if (key.modifier[K_CTRL]) Term_keypress('C');
           if (key.modifier[K_LSHIFT]) Term_keypress('S');
           if (key.modifier[K_ALT]) Term_keypress('O');
           if (key.modifier[K_RSHIFT]) Term_keypress('X');

           /* The upper 8 bits are the "special key" */
           /* Hack -- encode the keypress (in decimal) */
           Term_keypress('0' + (key.scan % 1000) / 100);
           Term_keypress('0' + (key.scan % 100) / 10);
           Term_keypress('0' + (key.scan % 10));

           /* End the macro (with "return") */
           Term_keypress(15);
           }

           /* Success */
           return 0;
    }

    /* Unknown request */
    return (1);
}


/*
 * Initialize the Borland 286/IBM "visual module"
 */
errr init_286()
{
    int i;

    term *t = &term_screen_body;

    static int done = FALSE;

    /* Paranoia -- Already done */
    if (done) return (-1);

    /* Acquire the size of the screen */
    rows = ScreenRows();
    cols = ScreenCols();

    /* Erase the screen */
    ScreenClear();

    /* Initialize the term -- very large key buffer */
    /* do we need a large key buffer?  Why? (jay@map.com) */

    term_init(t, cols, rows - 1, 1024);

    /* Connect the hooks */
    t->text_hook = Term_text_286;
    t->wipe_hook = Term_wipe_286;
    t->curs_hook = Term_curs_286;
    t->xtra_hook = Term_xtra_286;

    /* Save it */
    term_screen = t;

    /* Activate it */
    Term_activate(term_screen);

    /* Done */
    done = TRUE;

    /* Success */
    return 0;
}

#endif /* USE_286 */

#endif
