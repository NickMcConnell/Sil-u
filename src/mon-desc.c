/* File: mon-desc.c */

/* Purpose: describe monsters (using monster memory) */

/*
 * Copyright (c) 1989 James E. Wilson, Christopher J. Stuart
 *
 * This software may be copied and distributed for educational, research, and
 * not for profit purposes provided that this copyright and statement are
 * included in all such copies.
 */

#include "angband.h"


#undef CTRL
#define CTRL(C) ((C)&037)



/*
 * Pronoun arrays, by gender.
 */
static cptr wd_he[3] = { "it", "he", "she" };
static cptr wd_his[3] = { "its", "his", "her" };


/*
 * Pluralizer.  Args(count, singular, plural)
 */
#define plural(c,s,p) \
    (((c) == 1) ? (s) : (p))






/*
 * Max line size
 */
#define ROFF_WID 79



/*
 * Line buffer
 */
static char roff_buf[256];

/*
 * Current Pointer into roff_buf
 */
static char *roff_p = roff_buf;

/*
 * Last space saved into roff_buf
 */
static char *roff_s = NULL;

/*
 * Current row
 */
static int roff_row = 0;

/*
 * Last row erased
 */
static int roff_old = 0;





/*
 * Buffer text, dumping full lines via "Term_putstr()".
 *
 * Automatically wraps to the next line when necessary.
 * Also wraps to the next line on any "newline" in "str".
 * There are never more chars buffered than can be printed.
 *
 * If "str" is NULL, restart (?).
 */
static void roff(cptr str)
{
    cptr p, r;


    /* Scan the given string, character at a time */
    for (p = str; *p; p++) {

        int wrap = (*p == '\n');
        char ch = *p;

        /* Clean up the char */
        if (!isprint(ch)) ch = ' ';

        /* We may be "forced" to wrap */
        if (roff_p >= roff_buf + ROFF_WID) wrap = 1;

        /* Hack -- Try to avoid "hanging" single letter words */
        if ((ch == ' ') && (roff_p + 2 >= roff_buf + ROFF_WID)) wrap = 1;

        /* Handle line-wrap */
        if (wrap) {

            /* We must not dump past here */
            *roff_p = '\0';

            /* Assume nothing will be left over */
            r = roff_p;

            /* If we are in the middle of a word, try breaking on a space */
            if (roff_s && (ch != ' ')) {

                /* Nuke the space */
                *roff_s = '\0';

                /* Remember the char after the space */
                r = roff_s + 1;
            }

            /* Hack -- clear lines */
            while (roff_old <= roff_row) {
                Term_erase(0, roff_old, 80, 1);
                roff_old++;
            }

            /* Dump the line, advance the row */	
            Term_putstr(0, roff_row++, -1, TERM_WHITE, roff_buf);

            /* No spaces yet */
            roff_s = NULL;

            /* Restart the buffer scanner */
            roff_p = roff_buf;

            /* Restore any "wrapped" chars */
            while (*r) *roff_p++ = *r++;
        }

        /* Save the char.  Hack -- skip leading spaces (and newlines) */
        if ((roff_p > roff_buf) || (ch != ' ')) {
            if (ch == ' ') roff_s = roff_p;
            *roff_p++ = ch;
        }
    }
}



/*
 * Determine if the "armor" is known
 * The higher the level, the fewer kills needed.
 */
static bool know_armour(int r_idx)
{
    monster_race *r_ptr = &r_info[r_idx];

    s32b level = r_ptr->level;

    s32b kills = r_ptr->r_tkills;

    /* Normal monsters */
    if (kills > 304 / (4 + level)) return (TRUE);

    /* Skip non-uniques */
    if (!(r_ptr->flags1 & RF1_UNIQUE)) return (FALSE);

    /* Unique monsters */
    if (kills > 304 / (38 + (5*level) / 4)) return (TRUE);

    /* Assume false */
    return (FALSE);
}


/*
 * Determine if the "damage" of the given attack is known
 * the higher the level of the monster, the fewer the attacks you need,
 * the more damage an attack does, the more attacks you need
 */
static bool know_damage(int r_idx, int i)
{
    monster_race *r_ptr = &r_info[r_idx];

    s32b level = r_ptr->level;

    s32b a = r_ptr->r_blows[i];

    s32b d1 = r_ptr->blow[i].d_dice;
    s32b d2 = r_ptr->blow[i].d_side;

    s32b d = d1 * d2;

    /* Normal monsters */
    if ((4 + level) * a > 80 * d) return (TRUE);

    /* Skip non-uniques */
    if (!(r_ptr->flags1 & RF1_UNIQUE)) return (FALSE);

    /* Unique monsters */
    if ((4 + level) * (2 * a) > 80 * d) return (TRUE);

    /* Assume false */
    return (FALSE);
}


/*
 * Hack -- display monster information using "roff()"
 *
 * XXX XXX XXX We should seriously consider some method which would
 * allow the "monster description" fields to be read in only when
 * they were actually needed, which might save up to 60K of memory.
 */
static void roff_aux(int r_idx)
{
    monster_race	*r_ptr;

    bool		old = FALSE;
    bool		sin = FALSE;

    int			m, n, r;

    cptr		p, q;

    int			msex = 0;

    bool		breath = FALSE;
    bool		magic = FALSE;

    u32b		flags1 = 0L;
    u32b		flags2 = 0L;
    u32b		flags3 = 0L;
    u32b		flags4 = 0L;
    u32b		flags5 = 0L;
    u32b		flags6 = 0L;

    int			vn = 0;
    cptr		vp[64];

    monster_race        save_mem;



    /* Nothing erased */
    roff_old = 0;

    /* Reset the row */
    roff_row = 1;

    /* Reset the pointer */
    roff_p = roff_buf;

    /* No spaces yet */
    roff_s = NULL;


    /* Access the race and lore */
    r_ptr = &r_info[r_idx];


    /* Cheat -- Know everything */
    if (cheat_know) {

        /* XXX XXX XXX XXX XXX */

        /* Save the "old" memory */
        save_mem = *r_ptr;

        /* Hack -- Maximal kills */
        r_ptr->r_tkills = MAX_SHORT;

        /* Hack -- Maximal info */
        r_ptr->r_wake = r_ptr->r_ignore = MAX_UCHAR;

        /* Observe "maximal" attacks */
        for (m = 0; m < 4; m++) {

            /* Examine "actual" blows */
            if (r_ptr->blow[m].effect || r_ptr->blow[m].method) {

                /* Hack -- maximal observations */
                r_ptr->r_blows[m] = MAX_UCHAR;
            }
        }

        /* Hack -- maximal drops */
        r_ptr->r_drop_gold = r_ptr->r_drop_item =
            (((r_ptr->flags1 & RF1_DROP_4D2) ? 8 : 0) +
             ((r_ptr->flags1 & RF1_DROP_3D2) ? 6 : 0) +
             ((r_ptr->flags1 & RF1_DROP_2D2) ? 4 : 0) +
             ((r_ptr->flags1 & RF1_DROP_1D2) ? 2 : 0) +
             ((r_ptr->flags1 & RF1_DROP_90)  ? 1 : 0) +
             ((r_ptr->flags1 & RF1_DROP_60)  ? 1 : 0));

        /* Hack -- but only "valid" drops */
        if (r_ptr->flags1 & RF1_ONLY_GOLD) r_ptr->r_drop_item = 0;
        if (r_ptr->flags1 & RF1_ONLY_ITEM) r_ptr->r_drop_gold = 0;

        /* Hack -- observe many spells */
        r_ptr->r_cast_inate = MAX_UCHAR;
        r_ptr->r_cast_spell = MAX_UCHAR;

        /* Hack -- know all the flags */
        r_ptr->r_flags1 = r_ptr->flags1;
        r_ptr->r_flags2 = r_ptr->flags2;
        r_ptr->r_flags3 = r_ptr->flags3;
        r_ptr->r_flags4 = r_ptr->flags4;
        r_ptr->r_flags5 = r_ptr->flags5;
        r_ptr->r_flags6 = r_ptr->flags6;
    }


    /* Extract a gender (if applicable) */
    if (r_ptr->flags1 & RF1_FEMALE) msex = 2;
    else if (r_ptr->flags1 & RF1_MALE) msex = 1;


    /* Obtain a copy of the "known" flags */
    flags1 = (r_ptr->flags1 & r_ptr->r_flags1);
    flags2 = (r_ptr->flags2 & r_ptr->r_flags2);
    flags3 = (r_ptr->flags3 & r_ptr->r_flags3);
    flags4 = (r_ptr->flags4 & r_ptr->r_flags4);
    flags5 = (r_ptr->flags5 & r_ptr->r_flags5);
    flags6 = (r_ptr->flags6 & r_ptr->r_flags6);


    /* Assume some "obvious" flags */
    if (r_ptr->flags1 & RF1_UNIQUE) flags1 |= RF1_UNIQUE;
    if (r_ptr->flags1 & RF1_QUESTOR) flags1 |= RF1_QUESTOR;
    if (r_ptr->flags1 & RF1_MALE) flags1 |= RF1_MALE;
    if (r_ptr->flags1 & RF1_FEMALE) flags1 |= RF1_FEMALE;

    /* Assume some "creation" flags */
    if (r_ptr->flags1 & RF1_FRIEND) flags1 |= RF1_FRIEND;
    if (r_ptr->flags1 & RF1_FRIENDS) flags1 |= RF1_FRIENDS;
    if (r_ptr->flags1 & RF1_ESCORT) flags1 |= RF1_ESCORT;
    if (r_ptr->flags1 & RF1_ESCORTS) flags1 |= RF1_ESCORTS;

    /* Killing a monster reveals some properties */
    if (r_ptr->r_tkills) {

        /* Know "race" flags */
        if (r_ptr->flags3 & RF3_ORC) flags3 |= RF3_ORC;
        if (r_ptr->flags3 & RF3_TROLL) flags3 |= RF3_TROLL;
        if (r_ptr->flags3 & RF3_GIANT) flags3 |= RF3_GIANT;
        if (r_ptr->flags3 & RF3_DRAGON) flags3 |= RF3_DRAGON;
        if (r_ptr->flags3 & RF3_DEMON) flags3 |= RF3_DEMON;
        if (r_ptr->flags3 & RF3_UNDEAD) flags3 |= RF3_UNDEAD;
        if (r_ptr->flags3 & RF3_EVIL) flags3 |= RF3_EVIL;
        if (r_ptr->flags3 & RF3_ANIMAL) flags3 |= RF3_ANIMAL;

        /* Know "forced" flags */
        if (r_ptr->flags1 & RF1_FORCE_DEPTH) flags1 |= RF1_FORCE_DEPTH;
        if (r_ptr->flags1 & RF1_FORCE_MAXHP) flags1 |= RF1_FORCE_MAXHP;
    }


    /* Require a flag to show kills */
    if (!(recall_show_kill)) {

        /* nothing */
    }

    /* Treat uniques differently */
    else if (flags1 & RF1_UNIQUE) {

        /* Hack -- Determine if the unique is "dead" */
        bool dead = (r_ptr->max_num == 0);

        /* We've been killed... */
        if (r_ptr->r_deaths) {

            /* Killed ancestors */
            roff(format("%^s has slain %d of your ancestors",
                        wd_he[msex], r_ptr->r_deaths));

            /* But we've also killed it */
            if (dead) {
                roff(format(", but you have avenged %s!  ",
                            plural(r_ptr->r_deaths, "him", "them")));
            }

            /* Unavenged (ever) */
            else {
                roff(format(", who %s unavenged.  ",
                            plural(r_ptr->r_deaths, "remains", "remain")));
            }
        }

        /* Dead unique who never hurt us */
        else if (dead) {
            roff("You have slain this foe.  ");
        }
    }

    /* Not unique, but killed us */
    else if (r_ptr->r_deaths) {

        /* Dead ancestors */
        roff(format("%d of your ancestors %s been killed by this creature, ",
                    r_ptr->r_deaths, plural(r_ptr->r_deaths, "has", "have")));

        /* Some kills this life */
        if (r_ptr->r_pkills) {
            roff(format("and you have exterminated at least %d of the creatures.  ",
                        r_ptr->r_pkills));
        }

        /* Some kills past lives */
        else if (r_ptr->r_tkills) {
            roff(format("and %s have exterminated at least %d of the creatures.  ",
                        "your ancestors", r_ptr->r_tkills));
        }

        /* No kills */
        else {
            roff(format("and %s is not ever known to have been defeated.  ",
                        wd_he[msex]));
        }
    }

    /* Normal monsters */
    else {

        /* Killed some this life */
        if (r_ptr->r_pkills) {
            roff(format("You have killed at least %d of these creatures.  ",
                        r_ptr->r_pkills));
        }

        /* Killed some last life */
        else if (r_ptr->r_tkills) {
            roff(format("Your ancestors have killed at least %d of these creatures.  ",
                        r_ptr->r_tkills));
        }

        /* Killed none */
        else {
            roff("No battles to the death are recalled.  ");
        }
    }


    /* Descriptions */
    if (recall_show_desc) {

        /* Description */
        roff(r_text + r_ptr->text);
        roff("  ");
    }


    /* Nothing yet */
    old = FALSE;

    /* Describe location */
    if (r_ptr->level == 0) {
        roff(format("%^s lives in the town", wd_he[msex]));
        old = TRUE;
    }
    else if (r_ptr->r_tkills) {
        if (depth_in_feet) {
            roff(format("%^s is normally found at depths of %d feet",
                        wd_he[msex], r_ptr->level * 50));
        }
        else {
            roff(format("%^s is normally found on dungeon level %d",
                        wd_he[msex], r_ptr->level));
        }
        old = TRUE;
    }


    /* Describe movement */
    if (TRUE) {

        /* Introduction */
        if (old) {
            roff(", and ");
        }
        else {
            roff(format("%^s ", wd_he[msex]));
            old = TRUE;
        }
        roff("moves");

        /* Random-ness */
        if ((flags1 & RF1_RAND_50) || (flags1 & RF1_RAND_25)) {

            /* Adverb */
            if ((flags1 & RF1_RAND_50) && (flags1 & RF1_RAND_25)) {
                roff(" extremely");
            }
            else if (flags1 & RF1_RAND_50) {
                roff(" somewhat");
            }
            else if (flags1 & RF1_RAND_25) {
                roff(" a bit");
            }

            /* Adjective */
            roff(" erratically");

            /* Hack -- Occasional conjunction */
            if (r_ptr->speed != 110) roff(", and");
        }

        /* Speed */
        if (r_ptr->speed > 110) {
            if (r_ptr->speed > 130) roff(" incredibly");
            else if (r_ptr->speed > 120) roff(" very");
            roff(" quickly");
        }
        else if (r_ptr->speed < 110) {
            if (r_ptr->speed < 90) roff(" incredibly");
            else if (r_ptr->speed < 100) roff(" very");
            roff(" slowly");
        }
        else {
            roff(" at normal speed");
        }
    }

    /* The code above includes "attack speed" */
    if (flags1 & RF1_NEVER_MOVE) {

        /* Introduce */
        if (old) {
            roff(", but ");
        }
        else {
            roff(format("%^s ", wd_he[msex]));
            old = TRUE;
        }

        /* Describe */
        roff("does not deign to chase intruders");
    }

    /* End this sentence */
    if (old) {
        roff(".  ");
        old = FALSE;
    }


    /* Describe experience if known */
    if (r_ptr->r_tkills) {

        /* Introduction */
        if (flags1 & RF1_UNIQUE) {
            roff("Killing this");
        }
        else {
            roff("A kill of this");
        }

        /* Describe the "quality" */
        if (flags3 & RF3_ANIMAL) roff(" natural");
        if (flags3 & RF3_EVIL) roff(" evil");
        if (flags3 & RF3_UNDEAD) roff(" undead");

        /* Describe the "race" */
        if (flags3 & RF3_DRAGON) roff(" dragon");
        else if (flags3 & RF3_DEMON) roff(" demon");
        else if (flags3 & RF3_GIANT) roff(" giant");
        else if (flags3 & RF3_TROLL) roff(" troll");
        else if (flags3 & RF3_ORC) roff(" orc");
        else roff(" creature");

        /* Group some variables */
        if (TRUE) {

            long i, j;

            /* calculate the integer exp part */
            i = (long)r_ptr->mexp * r_ptr->level / p_ptr->lev;

            /* calculate the fractional exp part scaled by 100, */
            /* must use long arithmetic to avoid overflow  */
            j = ((((long)r_ptr->mexp * r_ptr->level % p_ptr->lev) * (long)1000 /
                 p_ptr->lev + 5) / 10);

            /* Mention the experience */
            roff(format(" is worth %ld.%02ld point%s",
                        (long)i, (long)j,
                        (((i == 1) && (j == 0)) ? "" : "s")));

            /* Take account of annoying English */
            p = "th";
            i = p_ptr->lev % 10;
            if ((p_ptr->lev / 10) == 1) ;
            else if (i == 1) p = "st";
            else if (i == 2) p = "nd";
            else if (i == 3) p = "rd";

            /* Take account of "leading vowels" in numbers */
            q = "";
            i = p_ptr->lev;
            if ((i == 8) || (i == 11) || (i == 18)) q = "n";

            /* Mention the dependance on the player's level */
            roff(format(" for a%s %lu%s level character.  ",
                        q, (long)i, p));
        }
    }


    /* Describe escorts */
    if ((flags1 & RF1_ESCORT) || (flags1 & RF1_ESCORTS)) {
        roff(format("%^s usually appears with escorts.  ",
                    wd_he[msex]));
    }

    /* Describe friends */
    else if ((flags1 & RF1_FRIEND) || (flags1 & RF1_FRIENDS)) {
        roff(format("%^s usually appears in groups.  ",
                    wd_he[msex]));
    }


    /* Collect inate attacks */
    vn = 0;
    if (flags4 & RF4_SHRIEK)		vp[vn++] = "shriek for help";
    if (flags4 & RF4_XXX2)		vp[vn++] = "do something";
    if (flags4 & RF4_XXX3)		vp[vn++] = "do something";
    if (flags4 & RF4_XXX4)		vp[vn++] = "do something";
    if (flags4 & RF4_ARROW_1)		vp[vn++] = "fire an arrow";
    if (flags4 & RF4_ARROW_2)		vp[vn++] = "fire arrows";
    if (flags4 & RF4_ARROW_3)		vp[vn++] = "fire a missile";
    if (flags4 & RF4_ARROW_4)		vp[vn++] = "fire missiles";

    /* Describe inate attacks */
    if (vn) {

        /* Intro */
        roff(format("%^s", wd_he[msex]));

        /* Scan */
        for (n = 0; n < vn; n++) {

            /* Intro */
            if (n == 0) roff(" may ");
            else if (n < vn-1) roff(", ");
            else roff(" or ");

            /* Dump */
            roff(vp[n]);
        }

        /* End */
        roff(".  ");
    }


    /* Collect breaths */
    vn = 0;
    if (flags4 & RF4_BR_ACID)		vp[vn++] = "acid";
    if (flags4 & RF4_BR_ELEC)		vp[vn++] = "lightning";
    if (flags4 & RF4_BR_FIRE)		vp[vn++] = "fire";
    if (flags4 & RF4_BR_COLD)		vp[vn++] = "frost";
    if (flags4 & RF4_BR_POIS)		vp[vn++] = "poison";
    if (flags4 & RF4_BR_NETH)		vp[vn++] = "nether";
    if (flags4 & RF4_BR_LITE)		vp[vn++] = "light";
    if (flags4 & RF4_BR_DARK)		vp[vn++] = "darkness";
    if (flags4 & RF4_BR_CONF)		vp[vn++] = "confusion";
    if (flags4 & RF4_BR_SOUN)		vp[vn++] = "sound";
    if (flags4 & RF4_BR_CHAO)		vp[vn++] = "chaos";
    if (flags4 & RF4_BR_DISE)		vp[vn++] = "disenchantment";
    if (flags4 & RF4_BR_NEXU)		vp[vn++] = "nexus";
    if (flags4 & RF4_BR_TIME)		vp[vn++] = "time";
    if (flags4 & RF4_BR_INER)		vp[vn++] = "inertia";
    if (flags4 & RF4_BR_GRAV)		vp[vn++] = "gravity";
    if (flags4 & RF4_BR_SHAR)		vp[vn++] = "shards";
    if (flags4 & RF4_BR_PLAS)		vp[vn++] = "plasma";
    if (flags4 & RF4_BR_WALL)		vp[vn++] = "force";
    if (flags4 & RF4_BR_MANA)		vp[vn++] = "mana";
    if (flags4 & RF4_XXX5)		vp[vn++] = "something";
    if (flags4 & RF4_XXX6)		vp[vn++] = "something";
    if (flags4 & RF4_XXX7)		vp[vn++] = "something";
    if (flags4 & RF4_XXX8)		vp[vn++] = "something";

    /* Describe breaths */
    if (vn) {

        /* Note breath */
        breath = TRUE;

        /* Intro */
        roff(format("%^s", wd_he[msex]));

        /* Scan */
        for (n = 0; n < vn; n++) {

            /* Intro */
            if (n == 0) roff(" may breathe ");
            else if (n < vn-1) roff(", ");
            else roff(" or ");

            /* Dump */
            roff(vp[n]);
        }
    }


    /* Collect spells */
    vn = 0;
    if (flags5 & RF5_BA_ACID)		vp[vn++] = "produce acid balls";
    if (flags5 & RF5_BA_ELEC)		vp[vn++] = "produce lightning balls";
    if (flags5 & RF5_BA_FIRE)		vp[vn++] = "produce fire balls";
    if (flags5 & RF5_BA_COLD)		vp[vn++] = "produce frost balls";
    if (flags5 & RF5_BA_POIS)		vp[vn++] = "produce poison balls";
    if (flags5 & RF5_BA_NETH)		vp[vn++] = "produce nether balls";
    if (flags5 & RF5_BA_WATE)		vp[vn++] = "produce water balls";
    if (flags5 & RF5_BA_MANA)		vp[vn++] = "invoke mana storms";
    if (flags5 & RF5_BA_DARK)		vp[vn++] = "invoke darkness storms";
    if (flags5 & RF5_DRAIN_MANA)	vp[vn++] = "drain mana";
    if (flags5 & RF5_MIND_BLAST)	vp[vn++] = "cause mind blasting";
    if (flags5 & RF5_BRAIN_SMASH)	vp[vn++] = "cause brain smashing";
    if (flags5 & RF5_CAUSE_1)		vp[vn++] = "cause light wounds";
    if (flags5 & RF5_CAUSE_2)		vp[vn++] = "cause serious wounds";
    if (flags5 & RF5_CAUSE_3)		vp[vn++] = "cause critical wounds";
    if (flags5 & RF5_CAUSE_4)		vp[vn++] = "cause mortal wounds";
    if (flags5 & RF5_BO_ACID)		vp[vn++] = "produce acid bolts";
    if (flags5 & RF5_BO_ELEC)		vp[vn++] = "produce lightning bolts";
    if (flags5 & RF5_BO_FIRE)		vp[vn++] = "produce fire bolts";
    if (flags5 & RF5_BO_COLD)		vp[vn++] = "produce frost bolts";
    if (flags5 & RF5_BO_POIS)		vp[vn++] = "produce poison bolts";
    if (flags5 & RF5_BO_NETH)		vp[vn++] = "produce nether bolts";
    if (flags5 & RF5_BO_WATE)		vp[vn++] = "produce water bolts";
    if (flags5 & RF5_BO_MANA)		vp[vn++] = "produce mana bolts";
    if (flags5 & RF5_BO_PLAS)		vp[vn++] = "produce plasma bolts";
    if (flags5 & RF5_BO_ICEE)		vp[vn++] = "produce ice bolts";
    if (flags5 & RF5_MISSILE)		vp[vn++] = "produce magic missiles";
    if (flags5 & RF5_SCARE)		vp[vn++] = "terrify";
    if (flags5 & RF5_BLIND)		vp[vn++] = "blind";
    if (flags5 & RF5_CONF)		vp[vn++] = "confuse";
    if (flags5 & RF5_SLOW)		vp[vn++] = "slow";
    if (flags5 & RF5_HOLD)		vp[vn++] = "paralyze";
    if (flags6 & RF6_HASTE)		vp[vn++] = "haste-self";
    if (flags6 & RF6_XXX1)		vp[vn++] = "do something";
    if (flags6 & RF6_HEAL)		vp[vn++] = "heal-self";
    if (flags6 & RF6_XXX2)		vp[vn++] = "do something";
    if (flags6 & RF6_BLINK)		vp[vn++] = "blink-self";
    if (flags6 & RF6_TPORT)		vp[vn++] = "teleport-self";
    if (flags6 & RF6_XXX3)		vp[vn++] = "do something";
    if (flags6 & RF6_XXX4)		vp[vn++] = "do something";
    if (flags6 & RF6_TELE_TO)		vp[vn++] = "teleport to";
    if (flags6 & RF6_TELE_AWAY)		vp[vn++] = "teleport away";
    if (flags6 & RF6_TELE_LEVEL)	vp[vn++] = "teleport level";
    if (flags6 & RF6_XXX5)		vp[vn++] = "do something";
    if (flags6 & RF6_DARKNESS)		vp[vn++] = "create darkness";
    if (flags6 & RF6_TRAPS)		vp[vn++] = "create traps";
    if (flags6 & RF6_FORGET)		vp[vn++] = "cause amnesia";
    if (flags6 & RF6_XXX6)		vp[vn++] = "do something";
    if (flags6 & RF6_XXX7)		vp[vn++] = "do something";
    if (flags6 & RF6_XXX8)		vp[vn++] = "do something";
    if (flags6 & RF6_S_MONSTER)		vp[vn++] = "summon a monster";
    if (flags6 & RF6_S_MONSTERS)	vp[vn++] = "summon monsters";
    if (flags6 & RF6_S_ANT)		vp[vn++] = "summon ants";
    if (flags6 & RF6_S_SPIDER)		vp[vn++] = "summon spiders";
    if (flags6 & RF6_S_HOUND)		vp[vn++] = "summon hounds";
    if (flags6 & RF6_S_REPTILE)		vp[vn++] = "summon reptiles";
    if (flags6 & RF6_S_ANGEL)		vp[vn++] = "summon an angel";
    if (flags6 & RF6_S_DEMON)		vp[vn++] = "summon a demon";
    if (flags6 & RF6_S_UNDEAD)		vp[vn++] = "summon an undead";
    if (flags6 & RF6_S_DRAGON)		vp[vn++] = "summon a dragon";
    if (flags6 & RF6_S_HI_UNDEAD)	vp[vn++] = "summon Greater Undead";
    if (flags6 & RF6_S_HI_DRAGON)	vp[vn++] = "summon Ancient Dragons";
    if (flags6 & RF6_S_WRAITH)		vp[vn++] = "summon Ring Wraiths";
    if (flags6 & RF6_S_UNIQUE)		vp[vn++] = "summon Unique Monsters";

    /* Describe spells */
    if (vn) {

        /* Note magic */
        magic = TRUE;

        /* Intro */
        if (breath) {
            roff(", and is also");
        }
        else {
            roff(format("%^s is", wd_he[msex]));
        }

        /* Verb Phrase */
        roff(" magical, casting spells");

        /* Adverb */
        if (flags2 & RF2_SMART) roff(" intelligently");

        /* Scan */
        for (n = 0; n < vn; n++) {

            /* Intro */
            if (n == 0) roff(" which ");
            else if (n < vn-1) roff(", ");
            else roff(" or ");

            /* Dump */
            roff(vp[n]);
        }
    }


    /* End the sentence about inate/other spells */
    if (breath || magic) {

        /* Total casting */
        m = r_ptr->r_cast_inate + r_ptr->r_cast_spell;

        /* Average frequency */
        n = (r_ptr->freq_inate + r_ptr->freq_spell) / 2;

        /* Describe the spell frequency */
        if (m > 100) {
            roff(format("; 1 time in %d", 100 / n));
        }

        /* Guess at the frequency */
        else if (m) {
            n = ((n + 9) / 10) * 10;
            roff(format("; about 1 time in %d", 100 / n));
        }

        /* End this sentence */
        roff(".  ");
    }


    /* Describe monster "toughness" */
    if (know_armour(r_idx)) {

        /* Armor */
        roff(format("%^s has an armor rating of %d",
                    wd_he[msex], r_ptr->ac));

        /* Maximized hitpoints */
        if (flags1 & RF1_FORCE_MAXHP) {
            roff(format(" and a life rating of %d.  ",
                        r_ptr->hdice * r_ptr->hside));
        }
        
        /* Variable hitpoints */
        else {
            roff(format(" and a life rating of %dd%d.  ",
                        r_ptr->hdice, r_ptr->hside));
        }
    }



    /* Collect special abilities. */
    vn = 0;
    if (flags2 & RF2_OPEN_DOOR) vp[vn++] = "open doors";
    if (flags2 & RF2_BASH_DOOR) vp[vn++] = "bash down doors";
    if (flags2 & RF2_PASS_WALL) vp[vn++] = "pass through walls";
    if (flags2 & RF2_KILL_WALL) vp[vn++] = "bore through walls";
    if (flags2 & RF2_MOVE_BODY) vp[vn++] = "push past weaker monsters";
    if (flags2 & RF2_KILL_BODY) vp[vn++] = "destroy weaker monsters";
    if (flags2 & RF2_TAKE_ITEM) vp[vn++] = "pick up objects";
    if (flags2 & RF2_KILL_ITEM) vp[vn++] = "destroy objects";

    /* Describe special abilities. */
    if (vn) {

        /* Intro */
        roff(format("%^s", wd_he[msex]));

        /* Scan */
        for (n = 0; n < vn; n++) {

            /* Intro */
            if (n == 0) roff(" can ");
            else if (n < vn-1) roff(", ");
            else roff(" and ");

            /* Dump */
            roff(vp[n]);
        }

        /* End */
        roff(".  ");
    }


    /* Describe special abilities. */
    if (flags2 & RF2_INVISIBLE) {
        roff(format("%^s is invisible.  ", wd_he[msex]));
    }
    if (flags2 & RF2_COLD_BLOOD) {
        roff(format("%^s is cold blooded.  ", wd_he[msex]));
    }
    if (flags2 & RF2_EMPTY_MIND) {
        roff(format("%^s is not detected by telepathy.  ", wd_he[msex]));
    }
    if (flags2 & RF2_WEIRD_MIND) {
        roff(format("%^s is rarely detected by telepathy.  ", wd_he[msex]));
    }
    if (flags2 & RF2_MULTIPLY) {
        roff(format("%^s breeds explosively.  ", wd_he[msex]));
    }
    if (flags2 & RF2_REGENERATE) {
        roff(format("%^s regenerates quickly.  ", wd_he[msex]));
    }


    /* Collect susceptibilities */
    vn = 0;
    if (flags3 & RF3_HURT_ROCK) vp[vn++] = "rock remover";
    if (flags3 & RF3_HURT_LITE) vp[vn++] = "bright light";
    if (flags3 & RF3_HURT_FIRE) vp[vn++] = "fire";
    if (flags3 & RF3_HURT_COLD) vp[vn++] = "cold";

    /* Describe susceptibilities */
    if (vn) {

        /* Intro */
        roff(format("%^s", wd_he[msex]));

        /* Scan */
        for (n = 0; n < vn; n++) {

            /* Intro */
            if (n == 0) roff(" is hurt by ");
            else if (n < vn-1) roff(", ");
            else roff(" and ");

            /* Dump */
            roff(vp[n]);
        }

        /* End */
        roff(".  ");
    }


    /* Collect immunities */
    vn = 0;
    if (flags3 & RF3_IM_ACID) vp[vn++] = "acid";
    if (flags3 & RF3_IM_ELEC) vp[vn++] = "lightning";
    if (flags3 & RF3_IM_FIRE) vp[vn++] = "fire";
    if (flags3 & RF3_IM_COLD) vp[vn++] = "cold";
    if (flags3 & RF3_IM_POIS) vp[vn++] = "poison";

    /* Describe immunities */
    if (vn) {

        /* Intro */
        roff(format("%^s", wd_he[msex]));

        /* Scan */
        for (n = 0; n < vn; n++) {

            /* Intro */
            if (n == 0) roff(" resists ");
            else if (n < vn-1) roff(", ");
            else roff(" and ");

            /* Dump */
            roff(vp[n]);
        }

        /* End */
        roff(".  ");
    }


    /* Collect resistances */
    vn = 0;
    if (flags3 & RF3_RES_NETH) vp[vn++] = "nether";
    if (flags3 & RF3_RES_WATE) vp[vn++] = "water";
    if (flags3 & RF3_RES_PLAS) vp[vn++] = "plasma";
    if (flags3 & RF3_RES_NEXU) vp[vn++] = "nexus";
    if (flags3 & RF3_RES_DISE) vp[vn++] = "disenchantment";

    /* Describe resistances */
    if (vn) {

        /* Intro */
        roff(format("%^s", wd_he[msex]));

        /* Scan */
        for (n = 0; n < vn; n++) {

            /* Intro */
            if (n == 0) roff(" resists ");
            else if (n < vn-1) roff(", ");
            else roff(" and ");

            /* Dump */
            roff(vp[n]);
        }

        /* End */
        roff(".  ");
    }


    /* Collect non-effects */
    vn = 0;
    if (flags3 & RF3_NO_STUN) vp[vn++] = "stunned";
    if (flags3 & RF3_NO_FEAR) vp[vn++] = "frightened";
    if (flags3 & RF3_NO_CONF) vp[vn++] = "confused";
    if (flags3 & RF3_NO_SLEEP) vp[vn++] = "slept";

    /* Describe non-effects */
    if (vn) {

        /* Intro */
        roff(format("%^s", wd_he[msex]));

        /* Scan */
        for (n = 0; n < vn; n++) {

            /* Intro */
            if (n == 0) roff(" cannot be ");
            else if (n < vn-1) roff(", ");
            else roff(" or ");

            /* Dump */
            roff(vp[n]);
        }

        /* End */
        roff(".  ");
    }


    /* Do we know how aware it is? */
    if ((((int)r_ptr->r_wake * (int)r_ptr->r_wake) > r_ptr->sleep) ||
        (r_ptr->r_ignore == MAX_UCHAR) ||
        ((r_ptr->sleep == 0) && (r_ptr->r_tkills >= 10))) {

        cptr act = NULL;

        if (r_ptr->sleep > 200) {
            act = "prefers to ignore";
        }
        else if (r_ptr->sleep > 95) {
            act = "pays very little attention to";
        }
        else if (r_ptr->sleep > 75) {
            act = "pays little attention to";
        }
        else if (r_ptr->sleep > 45) {
            act = "tends to overlook";
        }
        else if (r_ptr->sleep > 25) {
            act = "takes quite a while to see";
        }
        else if (r_ptr->sleep > 10) {
            act = "takes a while to see";
        }
        else if (r_ptr->sleep > 5) {
            act = "is fairly observant of";
        }
        else if (r_ptr->sleep > 3) {
            act = "is observant of";
        }
        else if (r_ptr->sleep > 1) {
            act = "is very observant of";
        }
        else if (r_ptr->sleep > 0) {
            act = "is vigilant for";
        }
        else {
            act = "is ever vigilant for";
        }

        roff(format("%^s %s intruders, which %s may notice from %d feet.  ",
             wd_he[msex], act, wd_he[msex], 10 * r_ptr->aaf));
    }


    /* Drops gold and/or items */
    if (r_ptr->r_drop_gold || r_ptr->r_drop_item) {

        /* No "n" needed */
        sin = FALSE;

        /* Intro */
        roff(format("%^s may carry", wd_he[msex]));

        /* Count maximum drop */
        n = MAX(r_ptr->r_drop_gold, r_ptr->r_drop_item);

        /* One drop (may need an "n") */
        if (n == 1) {
            roff(" a");
            sin = TRUE;
        }

        /* Two drops */
        else if (n == 2) {
            roff(" one or two");
        }

        /* Many drops */
        else {
            roff(format(" up to %d", n));
        }


        /* Great */
        if (flags1 & RF1_DROP_GREAT) {
            p = " exceptional";
        }

        /* Good (no "n" needed) */
        else if (flags1 & RF1_DROP_GOOD) {
            p = " good";
            sin = FALSE;
        }

        /* Okay */
        else {
            p = NULL;
        }


        /* Objects */
        if (r_ptr->r_drop_item) {

            /* Handle singular "an" */
            if (sin) roff("n");
            sin = FALSE;

            /* Dump "object(s)" */
            if (p) roff(p);
            roff(" object");
            if (n != 1) roff("s");

            /* Conjunction replaces variety, if needed for "gold" below */
            p = " or";
        }

        /* Treasures */
        if (r_ptr->r_drop_gold) {

            /* Cancel prefix */
            if (!p) sin = FALSE;

            /* Handle singular "an" */
            if (sin) roff("n");
            sin = FALSE;

            /* Dump "treasure(s)" */
            if (p) roff(p);
            roff(" treasure");
            if (n != 1) roff("s");
        }

        /* End this sentence */
        roff(".  ");
    }


    /* Count the number of "known" attacks */
    for (n = 0, m = 0; m < 4; m++) {

        /* Skip non-attacks */
        if (!r_ptr->blow[m].method) continue;

        /* Count known attacks */
        if (r_ptr->r_blows[m]) n++;
    }

    /* Examine (and count) the actual attacks */
    for (r = 0, m = 0; m < 4; m++) {

        int method, effect, d1, d2;

        /* Skip non-attacks */
        if (!r_ptr->blow[m].method) continue;

        /* Skip unknown attacks */
        if (!r_ptr->r_blows[m]) continue;


        /* Extract the attack info */
        method = r_ptr->blow[m].method;
        effect = r_ptr->blow[m].effect;
        d1 = r_ptr->blow[m].d_dice;
        d2 = r_ptr->blow[m].d_side;


        /* No method yet */
        p = NULL;

        /* Acquire the method */
        switch (method) {
            case RBM_HIT:	p = "hit"; break;
            case RBM_TOUCH:	p = "touch"; break;
            case RBM_PUNCH:	p = "punch"; break;
            case RBM_KICK:	p = "kick"; break;
            case RBM_CLAW:	p = "claw"; break;
            case RBM_BITE:	p = "bite"; break;
            case RBM_STING:	p = "sting"; break;
            case RBM_XXX1:	break;
            case RBM_BUTT:	p = "butt"; break;
            case RBM_CRUSH:	p = "crush"; break;
            case RBM_ENGULF:	p = "engulf"; break;
            case RBM_XXX2:	break;
            case RBM_CRAWL:	p = "crawl on you"; break;
            case RBM_DROOL:	p = "drool on you"; break;
            case RBM_SPIT:	p = "spit"; break;
            case RBM_XXX3:	break;
            case RBM_GAZE:	p = "gaze"; break;
            case RBM_WAIL:	p = "wail"; break;
            case RBM_SPORE:	p = "release spores"; break;
            case RBM_XXX4:	break;
            case RBM_BEG:	p = "beg"; break;
            case RBM_INSULT:	p = "insult"; break;
            case RBM_MOAN:	p = "moan"; break;
            case RBM_XXX5:	break;
        }


        /* Default effect */
        q = NULL;

        /* Acquire the effect */
        switch (effect) {
            case RBE_HURT:	q = "attack"; break;
            case RBE_POISON:	q = "poison"; break;
            case RBE_UN_BONUS:	q = "disenchant"; break;
            case RBE_UN_POWER:	q = "drain charges"; break;
            case RBE_EAT_GOLD:	q = "steal gold"; break;
            case RBE_EAT_ITEM:	q = "steal items"; break;
            case RBE_EAT_FOOD:	q = "eat your food"; break;
            case RBE_EAT_LITE:	q = "absorb light"; break;
            case RBE_ACID:	q = "shoot acid"; break;
            case RBE_ELEC:	q = "electrify"; break;
            case RBE_FIRE:	q = "burn"; break;
            case RBE_COLD:	q = "freeze"; break;
            case RBE_BLIND:	q = "blind"; break;
            case RBE_CONFUSE:	q = "confuse"; break;
            case RBE_TERRIFY:	q = "terrify"; break;
            case RBE_PARALYZE:	q = "paralyze"; break;
            case RBE_LOSE_STR:	q = "reduce strength"; break;
            case RBE_LOSE_INT:	q = "reduce intelligence"; break;
            case RBE_LOSE_WIS:	q = "reduce wisdom"; break;
            case RBE_LOSE_DEX:	q = "reduce dexterity"; break;
            case RBE_LOSE_CON:	q = "reduce constitution"; break;
            case RBE_LOSE_CHR:	q = "reduce charisma"; break;
            case RBE_LOSE_ALL:	q = "reduce all stats"; break;
            case RBE_SHATTER:	q = "shatter"; break;
            case RBE_EXP_10:	q = "lower experience (by 10d6+)"; break;
            case RBE_EXP_20:	q = "lower experience (by 20d6+)"; break;
            case RBE_EXP_40:	q = "lower experience (by 40d6+)"; break;
            case RBE_EXP_80:	q = "lower experience (by 80d6+)"; break;
        }


        /* Introduce the attack description */
        if (!r) {
            roff(format("%^s can ", wd_he[msex]));
        }
        else if (r < n-1) {
            roff(", ");
        }
        else {
            roff(", and ");
        }


        /* Hack -- force a method */
        if (!p) p = "do something weird";

        /* Describe the method */
        roff(p);


        /* Describe the effect (if any) */
        if (q) {

            /* Describe the attack type */
            roff(" to ");
            roff(q);

            /* Describe damage (if known) */
            if (d1 && d2 && know_damage(r_idx, m)) {

                /* Display the damage */
                roff(" with damage");
                roff(format(" %dd%d", d1, d2));
            }
        }


        /* Count the attacks as printed */
        r++;
    }

    /* Finish sentence above */
    if (r) {
        roff(".  ");
    }

    /* Notice lack of attacks */
    else if (flags1 & RF1_NEVER_BLOW) {
        roff(format("%^s has no physical attacks.  ", wd_he[msex]));
    }

    /* Or describe the lack of knowledge */
    else {
        roff(format("Nothing is known about %s attack.  ", wd_his[msex]));
    }


    /* Notice "Quest" monsters */
    if (flags1 & RF1_QUESTOR) {
        roff("You feel an intense desire to kill this monster...  ");
    }


    /* Go down a line */
    roff("\n");


    /* Hack -- Restore monster memory */
    if (cheat_know) {

        /* Restore memory */
        *r_ptr = save_mem;
    }
}





/*
 * Hack -- Display the "name" and "attr/chars" of a monster race
 */
static void roff_top(int r_idx)
{
    monster_race	*r_ptr = &r_info[r_idx];

    byte		a1, a2;
    char		c1, c2;


    /* Access the chars */
    c1 = r_ptr->r_char;
    c2 = r_ptr->l_char;

    /* Assume white */
    a1 = TERM_WHITE;
    a2 = TERM_WHITE;
    
#ifdef USE_COLOR

    /* Access the attrs */
    if (use_color) {
        a1 = r_ptr->r_attr;
        a2 = r_ptr->l_attr;
    }

#endif


    /* Clear the top line */
    Term_erase(0, 0, 80, 1);
    
    /* Reset the cursor */
    Term_gotoxy(0, 0);

    /* A title (use "The" for non-uniques) */
    if (!(r_ptr->flags1 & RF1_UNIQUE)) {
        Term_addstr(-1, TERM_WHITE, "The ");
    }

    /* Dump the name */
    Term_addstr(-1, TERM_WHITE, (r_name + r_ptr->name));

    /* Append the "standard" attr/char info */
    Term_addstr(-1, TERM_WHITE, " ('");
    Term_addch(a1, c1);
    Term_addstr(-1, TERM_WHITE, "')");

    /* Append the "optional" attr/char info */
    Term_addstr(-1, TERM_WHITE, "/('");
    Term_addch(a2, c2);
    Term_addstr(-1, TERM_WHITE, "'):");
}



/*
 * Hack -- describe the given monster race at the top of the screen
 */
void screen_roff(int r_idx)
{
    /* Flush messages */
    msg_print(NULL);

    /* Hack -- recall that monster */
    roff_aux(r_idx);

    /* Hack -- Erase one more line */
    Term_erase(0, roff_old, 80, 1);

    /* Hack -- describe the monster */
    roff_top(r_idx);
}




/*
 * Hack -- describe the given monster race in the current "term" window
 */
void display_roff(int r_idx)
{
    /* Erase the window */
    Term_erase(0, 0, 80, 24);

    /* Hack -- recall that monster */
    roff_aux(r_idx);

    /* Hack -- describe the monster */
    roff_top(r_idx);

    /* Flush the output */
    Term_fresh();
}




/*
 * The table of "symbol info" -- each entry is a string of the form
 * "X:desc" where "X" is the trigger, and "desc" is the "info".
 */
static cptr ident_info[] = {

    " :A dark grid",
    "!:A potion (or oil)",
    "\":An amulet (or necklace)",
    "#:A wall (or secret door)",
    "$:Treasure (gold or gems)",
    "%:A vein (magma or quartz)",
        /* "&:unused", */
    "':An open door",
    "(:Soft armor",
    "):A shield",
    "*:A vein with treasure",
    "+:A closed door",
    ",:Food (or mushroom patch)",
    "-:A wand (or rod)",
    ".:Floor",
    "/:A polearm (Axe/Pike/etc)",
        /* "0:unused", */
    "1:Entrance to General Store",
    "2:Entrance to Armory",
    "3:Entrance to Weaponsmith",
    "4:Entrance to Temple",
    "5:Entrance to Alchemy shop",
    "6:Entrance to Magic store",
    "7:Entrance to Black Market",
    "8:Entrance to your home",
        /* "9:unused", */
    "::Rubble",
    ";:A loose rock",
    "<:An up staircase",
    "=:A ring",
    ">:A down staircase",
    "?:A scroll",
    "@:You",
    "A:Angel",
    "B:Bird",
    "C:Canine",
    "D:Ancient Dragon (or Wyrm)",
    "E:Elemental",
    "F:Dragon Fly",
    "G:Ghost",
    "H:Hybrid",
    "I:Insect",
        /* "J:unused", */
    "K:Killer Beetle",
    "L:Lich",
    "M:Mummy",
        /* "N:unused", */
    "O:Ogre",
    "P:Giant humanoid",
    "Q:Quylthulg (Pulsing Flesh Mound)",
    "R:Reptile (or Amphibian)",
    "S:Spider (or Scorpion or Tick)",
    "T:Troll",
    "U:Major Demon",
    "V:Vampire",
    "W:Wight/Wraith",
    "X:Xorn/Xaren",
    "Y:Yeti",
    "Z:Zephyr hound (Elemental hound)",
    "[:Hard armor",
    "\\:A hafted weapon (mace/whip/etc)",
    "]:Misc. armor",
    "^:A trap",
    "_:A staff",
        /* "`:unused", */
    "a:Ant",
    "b:Bat",
    "c:Centipede",
    "d:Dragon",
    "e:Floating Eye",
    "f:Feline",
    "g:Golem",
    "h:Hobbit/Elf/Dwarf",
    "i:Icky Thing",
    "j:Jelly",
    "k:Kobold",
    "l:Louse",
    "m:Mold",
    "n:Naga",
    "o:Orc",
    "p:Person/Human",
    "q:Quadruped",
    "r:Rodent",
    "s:Skeleton",
    "t:Townsperson",
    "u:Minor demon",
    "v:Vortex",
    "w:Worm (or worm mass)",
        /* "x:unused", */
    "y:Yeek",
    "z:Zombie",
    "{:A missile (Arrow/bolt/bullet)",
    "|:An edged weapon (sword/dagger/etc)",
    "}:A launcher (Bow/crossbow/sling)",
    "~:A tool (or miscellaneous item)",
    NULL
};



/*
 * Identify a character, allow recall of monsters
 *
 * Several "special" responses recall "mulitple" monsters:
 *   ^A (all monsters)
 *   ^U (all unique monsters)
 *   ^N (all non-unique monsters)
 */
void do_cmd_query_symbol(void)
{
    int		i, j, n;
    char	sym, query;
    char	buf[128];

    bool	all = FALSE;
    bool	uniq = FALSE;
    bool	norm = FALSE;

    bool	kills = FALSE;
    bool	level = FALSE;

    u16b	who[MAX_R_IDX];


    /* Get a character, or abort */
    if (!get_com("Enter character to be identified: ", &sym)) return;

    /* Find that character info, and describe it */
    for (i = 0; ident_info[i]; ++i) {
        if (sym == ident_info[i][0]) break;
    }

    /* Describe */
    if (sym == CTRL('A')) {
        all = TRUE;
        strcpy(buf, "Full monster list.");
    }
    else if (sym == CTRL('U')) {
        all = uniq = TRUE;
        strcpy(buf, "Unique monster list.");
    }
    else if (sym == CTRL('N')) {
        all = norm = TRUE;
        strcpy(buf, "Non-unique monster list.");
    }
    else if (ident_info[i]) {
        sprintf(buf, "%c - %s.", sym, ident_info[i] + 2);
    }
    else {
        sprintf(buf, "%c - %s.", sym, "Unknown Symbol");
    }

    /* Display the result */
    prt(buf, 0, 0);


    /* Find the set of matching monsters */
    for (n = 0, i = MAX_R_IDX-1; i > 0; i--) {

        monster_race *r_ptr = &r_info[i];

        /* Nothing to recall */
        if (!cheat_know && !r_ptr->r_sights) continue;

        /* Require non-unique monsters if needed */
        if (norm && (r_ptr->flags1 & RF1_UNIQUE)) continue;

        /* Require unique monsters if needed */
        if (uniq && !(r_ptr->flags1 & RF1_UNIQUE)) continue;

        /* Collect "appropriate" monsters */
        if (all || (r_ptr->r_char == sym)) who[n++] = i;
    }

    /* Nothing to recall */
    if (!n) return;

    /* Ask permission or abort */
    put_str("Recall details? (k/p/y/n): ", 0, 40);
    query = inkey();
    prt("", 0, 40);

    /* Sort by kills (and level) */
    if (query == 'k') {
        level = TRUE;
        kills = TRUE;
        query = 'y';
    }

    /* Sort by level */
    if (query == 'p') {
        level = TRUE;
        query = 'y';
    }

    /* Catch "escape" */
    if (query != 'y') return;


    /* Hack -- bubble-sort by level (then experience) */
    if (level) {
        for (i = 0; i < n - 1; i++) {
            for (j = 0; j < n - 1; j++) {
                int i1 = j;
                int i2 = j + 1;
                int l1 = r_info[who[i1]].level;
                int l2 = r_info[who[i2]].level;
                int e1 = r_info[who[i1]].mexp;
                int e2 = r_info[who[i2]].mexp;
                if ((l1 < l2) || ((l1 == l2) && (e1 < e2))) {
                    int tmp = who[i1];
                    who[i1] = who[i2];
                    who[i2] = tmp;
                }
            }
        }
    }

    /* Hack -- bubble-sort by pkills (or kills) */
    if (kills) {
        for (i = 0; i < n - 1; i++) {
            for (j = 0; j < n - 1; j++) {
                int i1 = j;
                int i2 = j + 1;
                int x1 = r_info[who[i1]].r_pkills;
                int x2 = r_info[who[i2]].r_pkills;
                int k1 = r_info[who[i1]].r_tkills;
                int k2 = r_info[who[i2]].r_tkills;
                if ((x1 < x2) || (!x1 && !x2 && (k1 < k2))) {
                    int tmp = who[i1];
                    who[i1] = who[i2];
                    who[i2] = tmp;
                }
            }
        }
    }


    /* Scan the monster memory. */
    for (i = 0; i < n; i++) {

        int r_idx = who[i];

        /* Hack -- Auto-recall */
        recent_track(r_idx);
 
        /* Hack -- Handle stuff */
        handle_stuff();
        
        /* Hack -- Begin the prompt */
        roff_top(r_idx);

        /* Hack -- Complete the prompt */
        Term_addstr(-1, TERM_WHITE, " [(r)ecall, ESC]");

        /* Get a command */
        query = inkey();

        /* Recall as needed */
        while (query == 'r') {

            /* Recall */
            Term_save();
            screen_roff(who[i]);
            Term_addstr(-1, TERM_WHITE, "  --pause--");
            query = inkey();
            Term_load();
        }

        /* Stop scanning */
        if (query == ESCAPE) break;

        /* Back up one entry */
        if (query == '-') i = (i > 0) ? (i - 2) : (n - 2);
    }


    /* Re-display the identity */
    prt(buf, 0, 0);
}

