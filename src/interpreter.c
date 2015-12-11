/*
  SillyMUD Distribution V1.1b             (c) 1993 SillyMUD Developement
 
  See license.doc for distribution terms.   SillyMUD is based on DIKUMUD
*/
#include "config.h"

#include <stdlib.h>
#include <string.h>
#ifdef HAVE_CRYPT_H
#include <crypt.h>
#endif
#include <ctype.h>
#include <stdio.h>
#include <arpa/telnet.h>
#include <unistd.h>

#include "protos.h"
#include "parser.h"
#include "act.move.h"

#define NOT !
#define AND &&
#define OR ||

#define STATE(d) ((d)->connected)
#define MAX_CMD_LIST 400

extern struct RaceChoices RaceList[RACE_LIST_SIZE];
extern const struct title_type titles[MAX_CLASS][ABS_MAX_LVL];
extern char motd[MAX_STRING_LENGTH];
extern char wmotd[MAX_STRING_LENGTH];
extern struct char_data *character_list;
extern struct player_index_element *player_table;
extern int top_of_p_table;
extern struct index_data *mob_index;
extern struct index_data *obj_index;
#if HASH
extern struct hash_header room_db;
#else
extern struct room_data *room_db;
#endif



unsigned char echo_on[] = { IAC, WONT, TELOPT_ECHO, '\r', '\n', '\0' };
unsigned char echo_off[] = { IAC, WILL, TELOPT_ECHO, '\0' };

int WizLock;
int Silence = 0;
int plr_tick_count = 0;

void do_cset(struct char_data *ch, char *arg, int cmd);

#if PLAYER_AUTH
void do_auth(struct char_data *ch, char *arg, int cmd); /* jdb 3-1 */
#endif




char *fill[] = { "in",
  "from",
  "with",
  "the",
  "on",
  "at",
  "to",
  "\n"
};




int search_block(char *arg, char **list, bool exact) {
  register int i, l;

  /* Make into lower case, and get length of string */
  for (l = 0; *(arg + l); l++)
    *(arg + l) = LOWER(*(arg + l));

  if (exact) {
    for (i = 0; **(list + i) != '\n'; i++)
      if (!strcmp(arg, *(list + i)))
        return (i);
  }
  else {
    if (!l)
      l = 1;                    /* Avoid "" to match the first available string */
    for (i = 0; **(list + i) != '\n'; i++)
      if (!strncmp(arg, *(list + i), l))
        return (i);
  }

  return (-1);
}


int old_search_block(char *argument, int begin, int length, char **list,
                     int mode) {
  int guess, found, search;


  /* If the word contain 0 letters, then a match is already found */
  found = (length < 1);

  guess = 0;

  /* Search for a match */

  if (mode)
    while (NOT found AND * (list[guess]) != '\n') {
      found = ((size_t) length == strlen(list[guess]));
      for (search = 0; (search < length AND found); search++)
        found = (*(argument + begin + search) == *(list[guess] + search));
      guess++;
    }
  else {
    while (NOT found AND * (list[guess]) != '\n') {
      found = 1;
      for (search = 0; (search < length AND found); search++)
        found = (*(argument + begin + search) == *(list[guess] + search));
      guess++;
    }
  }

  return (found ? guess : -1);
}

void command_interpreter(struct char_data *ch, char *argument) {

  char buf[200];
  extern int no_specials;
  NODE *n;
  char buf1[255], buf2[255];

  REMOVE_BIT(ch->specials.affected_by, AFF_HIDE);

  if (MOUNTED(ch)) {
    if (ch->in_room != MOUNTED(ch)->in_room)
      dismount(ch, MOUNTED(ch), POSITION_STANDING);
  }

  /*
   *  a bug check.
   */
  if (!IS_NPC(ch)) {
    int found = FALSE;
    size_t i;
    if ((!ch->player.name[0]) || (ch->player.name[0] < ' ')) {
      log_msg("Error in character name.  Changed to 'Error'");
      free(ch->player.name);
      ch->player.name = (char *)malloc(6);
      strcpy(ch->player.name, "Error");
      return;
    }
    strcpy(buf, ch->player.name);
    for (i = 0; i < strlen(buf) && !found; i++) {
      if (buf[i] < 65) {
        found = TRUE;
      }
    }
    if (found) {
      log_msg("Error in character name.  Changed to 'Error'");
      free(ch->player.name);
      ch->player.name = (char *)malloc(6);
      strcpy(ch->player.name, "Error");
      return;
    }
  }
  if (!*argument || *argument == '\n') {
    return;
  }
  else if (!isalpha(*argument)) {
    buf1[0] = *argument;
    buf1[1] = '\0';
    if ((argument + 1))
      strcpy(buf2, (argument + 1));
    else
      buf2[0] = '\0';
  }
  else {
    register int i;
    half_chop(argument, buf1, buf2);
    i = 0;
    while (buf1[i] != '\0') {
      buf1[i] = LOWER(buf1[i]);
      i++;
    }
  }

/* New parser by DM */
  if (*buf1)
    n = find_valid_command(buf1);
  else
    n = NULL;
/*  
  cmd = old_search_block(argument,begin,look_at,command,0);
*/
  if (!n) {
    send_to_char("Pardon?\n\r", ch);
    return;
  }

  if (get_max_level(ch) < n->min_level) {
    send_to_char("Pardon?\n\r", ch);
    return;
  }

  if ((n->func != 0) || (n->func_dep)) {
    if ((!IS_AFFECTED(ch, AFF_PARALYSIS)) || (n->min_pos <= POSITION_STUNNED)) {
      if (GET_POS(ch) < n->min_pos) {
        switch (GET_POS(ch)) {
        case POSITION_DEAD:
          send_to_char("Lie still; you are DEAD!!! :-( \n\r", ch);
          break;
        case POSITION_INCAP:
        case POSITION_MORTALLYW:
          send_to_char
            ("You are in a pretty bad shape, unable to do anything!\n\r", ch);
          break;

        case POSITION_STUNNED:
          send_to_char
            ("All you can do right now, is think about the stars!\n\r", ch);
          break;
        case POSITION_SLEEPING:
          send_to_char("In your dreams, or what?\n\r", ch);
          break;
        case POSITION_RESTING:
          send_to_char("Nah... You feel too relaxed to do that..\n\r", ch);
          break;
        case POSITION_SITTING:
          send_to_char("Maybe you should get on your feet first?\n\r", ch);
          break;
        case POSITION_FIGHTING:
          send_to_char("No way! You are fighting for your life!\n\r", ch);
          break;
        case POSITION_STANDING:
          send_to_char("Fraid you can't do that\n\r", ch);
          break;
        }
      }
      else {

        if (!no_specials && special(ch, n->number, buf2))
          return;

        if (n->log) {
          SPRINTF(buf, "%s:%s", ch->player.name, argument);
          slog(buf);
        }
        if ((get_max_level(ch) >= LOW_IMMORTAL) && (get_max_level(ch) < 60)) {
          SPRINTF(buf, "%s:%s", ch->player.name, argument);
          slog(buf);
        }
        if (IS_AFFECTED2(ch, AFF2_LOG_ME)) {
          SPRINTF(buf, "%s:%s", ch->player.name, argument);
          slog(buf);
        }
        if (GET_GOLD(ch) > 2000000) {
          SPRINTF(buf, "%s:%s", fname(ch->player.name), argument);
          slog(buf);
        }

        if (n->func)
          ((*n->func)
           (ch, buf2, n->name));
        else
          ((*n->func_dep)
           (ch, buf2, n->number));
      }
      return;
    }
    else {
      send_to_char(" You are paralyzed, you can't do much!\n\r", ch);
      return;
    }
  }
  if (n && !n->func && !n->func_dep)
    send_to_char("Sorry, but that command has yet to be implemented...\n\r",
                 ch);
  else
    send_to_char("Pardon? \n\r", ch);
}

void argument_interpreter(char *argument, char *first_arg, char *second_arg) {
  int look_at, begin;

  begin = 0;

  do {
    /* Find first non blank */
    for (; *(argument + begin) == ' '; begin++);

    /* Find length of first word */
    for (look_at = 0; *(argument + begin + look_at) > ' '; look_at++)

      /* Make all letters lower case,
         AND copy them to first_arg */
      *(first_arg + look_at) = LOWER(*(argument + begin + look_at));

    *(first_arg + look_at) = '\0';
    begin += look_at;

  }
  while (fill_word(first_arg));

  do {
    /* Find first non blank */
    for (; *(argument + begin) == ' '; begin++);

    /* Find length of first word */
    for (look_at = 0; *(argument + begin + look_at) > ' '; look_at++)

      /* Make all letters lower case,
         AND copy them to second_arg */
      *(second_arg + look_at) = LOWER(*(argument + begin + look_at));

    *(second_arg + look_at) = '\0';
    begin += look_at;

  }
  while (fill_word(second_arg));
}

int is_number(char *str) {
/*   int look_at; */

  if (*str == '\0')
    return (0);
  else if (newstrlen(str) > 8)
    return (0);
  else if ((atoi(str) == 0) && (str[0] != '0'))
    return (0);
  else
    return (1);

/*  for(look_at=0;*(str+look_at) != '\0';look_at++)
    if((*(str+look_at)<'0')||(*(str+look_at)>'9'))
      return(0);
  return(1); */
}

/*  Quinn substituted a new one-arg for the old one.. I thought returning a 
    char pointer would be neat, and avoiding the func-calls would save a
    little time... If anyone feels pissed, I'm sorry.. Anyhow, the code is
    snatched from the old one, so it outta work..
    
    void one_argument(char *argument,char *first_arg )
    {
    static char dummy[MAX_STRING_LENGTH];
    
    argument_interpreter(argument,first_arg,dummy);
    }
    
    */


/* find the first sub-argument of a string, return pointer to first char in
   primary argument, following the sub-arg			            */
char *one_argument(char *argument, char *first_arg) {
  int begin, look_at;

  begin = 0;

  do {
    /* Find first non blank */
    for (; isspace(*(argument + begin)); begin++);

    /* Find length of first word */
    for (look_at = 0; *(argument + begin + look_at) > ' '; look_at++)

      /* Make all letters lower case,
         AND copy them to first_arg */
      *(first_arg + look_at) = LOWER(*(argument + begin + look_at));

    *(first_arg + look_at) = '\0';
    begin += look_at;
  }
  while (fill_word(first_arg));

  return (argument + begin);
}



void only_argument(char *argument, char *dest) {
  while (*argument && isspace(*argument))
    argument++;
  strcpy(dest, argument);
}




int fill_word(char *argument) {
  return (search_block(argument, fill, TRUE) >= 0);
}





/* determine if a given string is an abbreviation of another */
int is_abbrev(char *arg1, char *arg2) {
  if (!*arg1)
    return (0);

  for (; *arg1; arg1++, arg2++)
    if (LOWER(*arg1) != LOWER(*arg2))
      return (0);

  return (1);
}




/* return first 'word' plus trailing substring of input string */
void half_chop(char *string, char *arg1, char *arg2) {
  for (; isspace(*string); string++);

  for (; !isspace(*arg1 = *string) && *string; string++, arg1++);

  *arg1 = '\0';

  for (; isspace(*string); string++);

  for (; (*arg2 = *string) != '\0'; string++, arg2++);
}



int special(struct char_data *ch, int cmd, char *arg) {
  register struct obj_data *i;
  register struct char_data *k;
  int j;


  if (ch->in_room == NOWHERE) {
    char_to_room(ch, 3001);
    return (0);
  }

  /* special in room? */
  if (real_roomp(ch->in_room)->funct)
    if ((*real_roomp(ch->in_room)->funct) (ch, cmd, arg,
                                           real_roomp(ch->in_room),
                                           PULSE_COMMAND))
      return (1);

  /* special in equipment list? */
  for (j = 0; j <= (MAX_WEAR - 1); j++)
    if (ch->equipment[j] && ch->equipment[j]->item_number >= 0)
      if (obj_index[ch->equipment[j]->item_number].func)
        if ((*obj_index[ch->equipment[j]->item_number].func)
            (ch, cmd, arg, ch->equipment[j], PULSE_COMMAND))
          return (1);

  /* special in inventory? */
  for (i = ch->carrying; i; i = i->next_content)
    if (i->item_number >= 0)
      if (obj_index[i->item_number].func)
        if ((*obj_index[i->item_number].func) (ch, cmd, arg, i, PULSE_COMMAND))
          return (1);

  /* special in mobile present? */
  for (k = real_roomp(ch->in_room)->people; k; k = k->next_in_room)
    if (IS_MOB(k))
      if (mob_index[k->nr].func)
        if ((*mob_index[k->nr].func) (ch, cmd, arg, k, PULSE_COMMAND))
          return (1);

  /* special in object present? */
  for (i = real_roomp(ch->in_room)->contents; i; i = i->next_content)
    if (i->item_number >= 0)
      if (obj_index[i->item_number].func)
        if ((*obj_index[i->item_number].func) (ch, cmd, arg, i, PULSE_COMMAND))
          return (1);

  return (0);
}

void assign_command_pointers() {
  init_radix();
  add_command("north", do_move, POSITION_STANDING, 0);
  add_command("east", do_move, POSITION_STANDING, 0);
  add_command("south", do_move, POSITION_STANDING, 0);
  add_command("west", do_move, POSITION_STANDING, 0);
  add_command("up", do_move, POSITION_STANDING, 0);
  add_command("down", do_move, POSITION_STANDING, 0);
  add_command("enter", do_enter, POSITION_STANDING, 0);
  add_command_dep("exits", do_exits, 8, POSITION_RESTING, 0);
  add_command_dep("kiss", do_action, 9, POSITION_RESTING, 0);
  add_command_dep("get", do_get, 10, POSITION_RESTING, 1);
  add_command_dep("drink", do_drink, 11, POSITION_RESTING, 1);
  add_command_dep("eat", do_eat, 12, POSITION_RESTING, 1);
  add_command_dep("wear", do_wear, 13, POSITION_RESTING, 0);
  add_command_dep("wield", do_wield, 14, POSITION_RESTING, 1);
  add_command_dep("look", do_look, 15, POSITION_RESTING, 0);
  add_command_dep("score", do_score, 16, POSITION_DEAD, 0);
  add_command_dep("say", do_say, 17, POSITION_RESTING, 0);
  add_command_dep("shout", do_shout, 18, POSITION_RESTING, 2);
  add_command_dep("tell", do_tell, 19, POSITION_RESTING, 0);
  add_command_dep("inventory", do_inventory, 20, POSITION_DEAD, 0);
  add_command_dep("qui", do_qui, 21, POSITION_DEAD, 0);
  add_command_dep("bounce", do_action, 22, POSITION_STANDING, 0);
  add_command_dep("smile", do_action, 23, POSITION_RESTING, 0);
  add_command_dep("dance", do_action, 24, POSITION_STANDING, 0);
  add_command_dep("kill", do_kill, 25, POSITION_FIGHTING, 1);
  add_command_dep("cackle", do_action, 26, POSITION_RESTING, 0);
  add_command_dep("laugh", do_action, 27, POSITION_RESTING, 0);
  add_command_dep("giggle", do_action, 28, POSITION_RESTING, 0);
  add_command_dep("shake", do_action, 29, POSITION_RESTING, 0);
  add_command_dep("puke", do_action, 30, POSITION_RESTING, 0);
  add_command_dep("growl", do_action, 31, POSITION_RESTING, 0);
  add_command_dep("scream", do_action, 32, POSITION_RESTING, 0);
  add_command_dep("insult", do_insult, 33, POSITION_RESTING, 0);
  add_command_dep("comfort", do_action, 34, POSITION_RESTING, 0);
  add_command_dep("nod", do_action, 35, POSITION_RESTING, 0);
  add_command_dep("sigh", do_action, 36, POSITION_RESTING, 0);
  add_command_dep("sulk", do_action, 37, POSITION_RESTING, 0);
  add_command_dep("help", do_help, 38, POSITION_DEAD, 0);
  add_command_dep("who", do_who, 39, POSITION_DEAD, 0);
  add_command_dep("emote", do_emote, 40, POSITION_SLEEPING, 0);
  add_command_dep("echo", do_echo, 41, POSITION_SLEEPING, 1);
  add_command_dep("stand", do_stand, 42, POSITION_RESTING, 0);
  add_command_dep("sit", do_sit, 43, POSITION_RESTING, 0);
  add_command_dep("rest", do_rest, 44, POSITION_RESTING, 0);
  add_command_dep("sleep", do_sleep, 45, POSITION_SLEEPING, 0);
  add_command_dep("wake", do_wake, 46, POSITION_SLEEPING, 0);
  add_command_dep("force", do_force, 47, POSITION_SLEEPING, LESSER_GOD);
  add_command_dep("transfer", do_trans, 48, POSITION_SLEEPING, DEMIGOD);
  add_command_dep("hug", do_action, 49, POSITION_RESTING, 0);
  add_command_dep("snuggle", do_action, 50, POSITION_RESTING, 0);
  add_command_dep("cuddle", do_action, 51, POSITION_RESTING, 0);
  add_command_dep("nuzzle", do_action, 52, POSITION_RESTING, 0);
  add_command_dep("cry", do_action, 53, POSITION_RESTING, 0);
  add_command_dep("news", do_news, 54, POSITION_SLEEPING, 0);
  add_command_dep("equipment", do_equipment, 55, POSITION_SLEEPING, 0);
  add_command_dep("buy", do_not_here, 56, POSITION_STANDING, 0);
  add_command_dep("sell", do_not_here, 57, POSITION_STANDING, 0);
  add_command_dep("value", do_value, 58, POSITION_RESTING, 0);
  add_command_dep("list", do_not_here, 59, POSITION_STANDING, 0);
  add_command_dep("drop", do_drop, 60, POSITION_RESTING, 1);
  add_command_dep("goto", do_goto, 61, POSITION_SLEEPING, 0);
  add_command_dep("weather", do_weather, 62, POSITION_RESTING, 0);
  add_command_dep("read", do_read, 63, POSITION_RESTING, 0);
  add_command_dep("pour", do_pour, 64, POSITION_STANDING, 0);
  add_command_dep("grab", do_grab, 65, POSITION_RESTING, 0);
  add_command_dep("remove", do_remove, 66, POSITION_RESTING, 0);
  add_command_dep("put", do_put, 67, POSITION_RESTING, 0);
  add_command_dep("shutdow", do_shutdow, 68, POSITION_DEAD, SILLYLORD);
  add_command_dep("save", do_save, 69, POSITION_SLEEPING, 0);
  add_command_dep("hit", do_hit, 70, POSITION_FIGHTING, 1);
  add_command_dep("string", do_string, 71, POSITION_SLEEPING, SAINT);
  add_command_dep("give", do_give, 72, POSITION_RESTING, 1);
  add_command_dep("quit", do_quit, 73, POSITION_DEAD, 0);
  add_command_dep("stat", do_stat, 74, POSITION_DEAD, CREATOR);
  add_command_dep("guard", do_guard, 75, POSITION_STANDING, 1);
  add_command_dep("time", do_time, 76, POSITION_DEAD, 0);
  add_command_dep("load", do_load, 77, POSITION_DEAD, SAINT);
  add_command_dep("purge", do_purge, 78, POSITION_DEAD, LOW_IMMORTAL);
  add_command_dep("shutdown", do_shutdown, 79, POSITION_DEAD, SILLYLORD);
  add_command_dep("idea", do_idea, 80, POSITION_DEAD, 0);
  add_command_dep("typo", do_typo, 81, POSITION_DEAD, 0);
  add_command_dep("bug", do_bug, 82, POSITION_DEAD, 0);
  add_command_dep("whisper", do_whisper, 83, POSITION_RESTING, 0);
  add_command_dep("cast", do_cast, 84, POSITION_SITTING, 1);
  add_command_dep("at", do_at, 85, POSITION_DEAD, CREATOR);
  add_command_dep("ask", do_ask, 86, POSITION_RESTING, 0);
  add_command_dep("order", do_order, 87, POSITION_RESTING, 1);
  add_command_dep("sip", do_sip, 88, POSITION_RESTING, 0);
  add_command_dep("taste", do_taste, 89, POSITION_RESTING, 0);
  add_command_dep("snoop", do_snoop, 90, POSITION_DEAD, GOD);
  add_command_dep("follow", do_follow, 91, POSITION_RESTING, 0);
  add_command_dep("rent", do_not_here, 92, POSITION_STANDING, 1);
  add_command_dep("offer", do_not_here, 93, POSITION_STANDING, 1);
  add_command_dep("poke", do_action, 94, POSITION_RESTING, 0);
  add_command_dep("advance", do_advance, 95, POSITION_DEAD, IMPLEMENTOR);
  add_command_dep("accuse", do_action, 96, POSITION_SITTING, 0);
  add_command_dep("grin", do_action, 97, POSITION_RESTING, 0);
  add_command_dep("bow", do_action, 98, POSITION_STANDING, 0);
  add_command_dep("open", do_open, 99, POSITION_SITTING, 0);
  add_command_dep("close", do_close, 100, POSITION_SITTING, 0);
  add_command_dep("lock", do_lock, 101, POSITION_SITTING, 0);
  add_command_dep("unlock", do_unlock, 102, POSITION_SITTING, 0);
  add_command_dep("leave", do_leave, 103, POSITION_STANDING, 0);
  add_command_dep("applaud", do_action, 104, POSITION_RESTING, 0);
  add_command_dep("blush", do_action, 105, POSITION_RESTING, 0);
  add_command_dep("burp", do_action, 106, POSITION_RESTING, 0);
  add_command_dep("chuckle", do_action, 107, POSITION_RESTING, 0);
  add_command_dep("clap", do_action, 108, POSITION_RESTING, 0);
  add_command_dep("cough", do_action, 109, POSITION_RESTING, 0);
  add_command_dep("curtsey", do_action, 110, POSITION_STANDING, 0);
  add_command_dep("fart", do_action, 111, POSITION_RESTING, 0);
  add_command_dep("flip", do_action, 112, POSITION_STANDING, 0);
  add_command_dep("fondle", do_action, 113, POSITION_RESTING, 0);
  add_command_dep("frown", do_action, 114, POSITION_RESTING, 0);
  add_command_dep("gasp", do_action, 115, POSITION_RESTING, 0);
  add_command_dep("glare", do_action, 116, POSITION_RESTING, 0);
  add_command_dep("groan", do_action, 117, POSITION_RESTING, 0);
  add_command_dep("grope", do_action, 118, POSITION_RESTING, 0);
  add_command_dep("hiccup", do_action, 119, POSITION_RESTING, 0);
  add_command_dep("lick", do_action, 120, POSITION_RESTING, 0);
  add_command_dep("love", do_action, 121, POSITION_RESTING, 0);
  add_command_dep("moan", do_action, 122, POSITION_RESTING, 0);
  add_command_dep("nibble", do_action, 123, POSITION_RESTING, 0);
  add_command_dep("pout", do_action, 124, POSITION_RESTING, 0);
  add_command_dep("purr", do_action, 125, POSITION_RESTING, 0);
  add_command_dep("ruffle", do_action, 126, POSITION_STANDING, 0);
  add_command_dep("shiver", do_action, 127, POSITION_RESTING, 0);
  add_command_dep("shrug", do_action, 128, POSITION_RESTING, 0);
  add_command_dep("sing", do_action, 129, POSITION_RESTING, 0);
  add_command_dep("slap", do_action, 130, POSITION_RESTING, 0);
  add_command_dep("smirk", do_action, 131, POSITION_RESTING, 0);
  add_command_dep("snap", do_action, 132, POSITION_RESTING, 0);
  add_command_dep("sneeze", do_action, 133, POSITION_RESTING, 0);
  add_command_dep("snicker", do_action, 134, POSITION_RESTING, 0);
  add_command_dep("sniff", do_action, 135, POSITION_RESTING, 0);
  add_command_dep("snore", do_action, 136, POSITION_SLEEPING, 0);
  add_command_dep("spit", do_action, 137, POSITION_STANDING, 0);
  add_command_dep("squeeze", do_action, 138, POSITION_RESTING, 0);
  add_command_dep("stare", do_action, 139, POSITION_RESTING, 0);
  add_command_dep("strut", do_action, 140, POSITION_STANDING, 0);
  add_command_dep("thank", do_action, 141, POSITION_RESTING, 0);
  add_command_dep("twiddle", do_action, 142, POSITION_RESTING, 0);
  add_command_dep("wave", do_action, 143, POSITION_RESTING, 0);
  add_command_dep("whistle", do_action, 144, POSITION_RESTING, 0);
  add_command_dep("wiggle", do_action, 145, POSITION_STANDING, 0);
  add_command_dep("wink", do_action, 146, POSITION_RESTING, 0);
  add_command_dep("yawn", do_action, 147, POSITION_RESTING, 0);
  add_command_dep("snowball", do_action, 148, POSITION_STANDING, DEMIGOD);
  add_command_dep("write", do_write, 149, POSITION_STANDING, 1);
  add_command_dep("hold", do_grab, 150, POSITION_RESTING, 1);
  add_command_dep("flee", do_flee, 151, POSITION_SITTING, 1);
  add_command_dep("sneak", do_sneak, 152, POSITION_STANDING, 1);
  add_command_dep("hide", do_hide, 153, POSITION_RESTING, 1);
  add_command_dep("backstab", do_backstab, 154, POSITION_STANDING, 1);
  add_command_dep("pick", do_pick, 155, POSITION_STANDING, 1);
  add_command_dep("steal", do_steal, 156, POSITION_STANDING, 1);
  add_command_dep("bash", do_bash, 157, POSITION_FIGHTING, 1);
  add_command_dep("rescue", do_rescue, 158, POSITION_FIGHTING, 1);
  add_command_dep("kick", do_kick, 159, POSITION_FIGHTING, 1);
  add_command_dep("french", do_action, 160, POSITION_RESTING, 0);
  add_command_dep("comb", do_action, 161, POSITION_RESTING, 0);
  add_command_dep("massage", do_action, 162, POSITION_RESTING, 0);
  add_command_dep("tickle", do_action, 163, POSITION_RESTING, 0);
  add_command_dep("practice", do_practice, 164, POSITION_RESTING, 1);
  add_command_dep("pat", do_action, 165, POSITION_RESTING, 0);
  add_command_dep("examine", do_examine, 166, POSITION_SITTING, 0);
  add_command_dep("take", do_get, 167, POSITION_RESTING, 1);        /* TAKE */
  add_command_dep("info", do_info, 168, POSITION_SLEEPING, 0);
  add_command_dep("'", do_say, 169, POSITION_RESTING, 0);
  add_command_dep("practise", do_practice, 170, POSITION_RESTING, 1);
  add_command_dep("curse", do_action, 171, POSITION_RESTING, 0);
  add_command_dep("use", do_use, 172, POSITION_SITTING, 1);
  add_command_dep("where", do_where, 173, POSITION_DEAD, 1);
  add_command_dep("levels", do_levels, 174, POSITION_DEAD, 0);
  add_command_dep("reroll", do_reroll, 175, POSITION_DEAD, SILLYLORD);
  add_command_dep("pray", do_action, 176, POSITION_SITTING, 0);
  add_command_dep(",", do_emote, 177, POSITION_SLEEPING, 0);
  add_command_dep("beg", do_action, 178, POSITION_RESTING, 0);
  add_command_dep("bleed", do_not_here, 179, POSITION_RESTING, 0);
  add_command_dep("cringe", do_action, 180, POSITION_RESTING, 0);
  add_command_dep("daydream", do_action, 181, POSITION_SLEEPING, 0);
  add_command_dep("fume", do_action, 182, POSITION_RESTING, 0);
  add_command_dep("grovel", do_action, 183, POSITION_RESTING, 0);
  add_command_dep("hop", do_action, 184, POSITION_RESTING, 0);
  add_command_dep("nudge", do_action, 185, POSITION_RESTING, 0);
  add_command_dep("peer", do_action, 186, POSITION_RESTING, 0);
  add_command_dep("point", do_action, 187, POSITION_RESTING, 0);
  add_command_dep("ponder", do_action, 188, POSITION_RESTING, 0);
  add_command_dep("punch", do_action, 189, POSITION_RESTING, 0);
  add_command_dep("snarl", do_action, 190, POSITION_RESTING, 0);
  add_command_dep("spank", do_action, 191, POSITION_RESTING, 0);
  add_command_dep("steam", do_action, 192, POSITION_RESTING, 0);
  add_command_dep("tackle", do_action, 193, POSITION_RESTING, 0);
  add_command_dep("taunt", do_action, 194, POSITION_RESTING, 0);
  add_command_dep("think", do_commune, 195, POSITION_RESTING, LOW_IMMORTAL);
  add_command_dep("whine", do_action, 196, POSITION_RESTING, 0);
  add_command_dep("worship", do_action, 197, POSITION_RESTING, 0);
  add_command_dep("yodel", do_action, 198, POSITION_RESTING, 0);
  add_command_dep("brief", do_brief, 199, POSITION_DEAD, 0);
  add_command_dep("wizlist", do_wizlist, 200, POSITION_DEAD, 0);
  add_command_dep("consider", do_consider, 201, POSITION_RESTING, 0);
  add_command_dep("group", do_group, 202, POSITION_RESTING, 1);
  add_command_dep("restore", do_restore, 203, POSITION_DEAD, DEMIGOD);
  add_command_dep("return", do_return, 204, POSITION_DEAD, 0);
  add_command_dep("switch", do_switch, 205, POSITION_DEAD, 52);
  add_command_dep("quaff", do_quaff, 206, POSITION_RESTING, 0);
  add_command_dep("recite", do_recite, 207, POSITION_STANDING, 0);
  add_command_dep("users", do_users, 208, POSITION_DEAD, LOW_IMMORTAL);
  add_command_dep("pose", do_pose, 209, POSITION_STANDING, 0);
  add_command_dep("noshout", do_noshout, 210, POSITION_SLEEPING, LOW_IMMORTAL);
  add_command_dep("wizhelp", do_wizhelp, 211, POSITION_SLEEPING, LOW_IMMORTAL);
  add_command_dep("credits", do_credits, 212, POSITION_DEAD, 0);
  add_command_dep("compact", do_compact, 213, POSITION_DEAD, 0);
  add_command_dep(":", do_emote, 214, POSITION_SLEEPING, 0);
  add_command_dep("deafen", do_plr_noshout, 215, POSITION_SLEEPING, 1);
  add_command_dep("slay", do_kill, 216, POSITION_STANDING, SILLYLORD);
  add_command_dep("wimpy", do_wimp, 217, POSITION_DEAD, 0);
  add_command_dep("junk", do_junk, 218, POSITION_RESTING, 1);
  add_command_dep("deposit", do_not_here, 219, POSITION_RESTING, 1);
  add_command_dep("withdraw", do_not_here, 220, POSITION_RESTING, 1);
  add_command_dep("balance", do_not_here, 221, POSITION_RESTING, 1);
  add_command_dep("nohassle", do_nohassle, 222, POSITION_DEAD, LOW_IMMORTAL);
  add_command_dep("system", do_system, 223, POSITION_DEAD, SILLYLORD);
  add_command_dep("pull", do_not_here, 224, POSITION_STANDING, 1);
  add_command_dep("stealth", do_stealth, 225, POSITION_DEAD, LOW_IMMORTAL);
  add_command_dep("edit", do_edit, 226, POSITION_DEAD, CREATOR);
#ifdef TEST_SERVER
  add_command_dep("@", do_set, 227, POSITION_DEAD, CREATOR);
#else
  add_command_dep("@", do_set, 227, POSITION_DEAD, SILLYLORD);
#endif
  add_command_dep("rsave", do_rsave, 228, POSITION_DEAD, CREATOR);
  add_command_dep("rload", do_rload, 229, POSITION_DEAD, CREATOR);
  add_command_dep("track", do_track, 230, POSITION_DEAD, 1);
  add_command_dep("wizlock", do_wizlock, 231, POSITION_DEAD, DEMIGOD);
  add_command_dep("highfive", do_highfive, 232, POSITION_DEAD, 0);
  add_command_dep("title", do_title, 233, POSITION_DEAD, 43);
  add_command_dep("whozone", do_who, 234, POSITION_DEAD, 0);
  add_command_dep("assist", do_assist, 235, POSITION_FIGHTING, 1);
  add_command_dep("attribute", do_attribute, 236, POSITION_DEAD, 5);
  add_command_dep("world", do_world, 237, POSITION_DEAD, 0);
  add_command_dep("allspells", do_spells, 238, POSITION_DEAD, 0);
  add_command_dep("breath", do_breath, 239, POSITION_FIGHTING, 1);
  add_command_dep("show", do_show, 240, POSITION_DEAD, CREATOR);
  add_command_dep("debug", do_debug, 241, POSITION_DEAD, IMPLEMENTOR);
  add_command_dep("invisible", do_invis, 242, POSITION_DEAD, LOW_IMMORTAL);
  add_command_dep("gain", do_gain, 243, POSITION_DEAD, 1);
  add_command_dep("instazone", do_instazone, 244, POSITION_DEAD, CREATOR);
  add_command_dep("disarm", do_disarm, 245, POSITION_FIGHTING, 1);
  add_command_dep("bonk", do_action, 246, POSITION_SITTING, 1);
  add_command_dep("chpwd", do_passwd, 247, POSITION_SITTING, IMPLEMENTOR);
  add_command_dep("fill", do_not_here, 248, POSITION_SITTING, 0);
  add_command_dep("imptest", do_doorbash, 249, POSITION_SITTING, IMPLEMENTOR);
  add_command_dep("shoot", do_shoot, 250, POSITION_STANDING, 1);
  add_command_dep("silence", do_silence, 251, POSITION_STANDING, DEMIGOD);
  add_command_dep("teams", do_not_here, 252, POSITION_STANDING, LOKI);
  add_command_dep("player", do_not_here, 253, POSITION_STANDING, LOKI);
  add_command_dep("create", do_create, 254, POSITION_STANDING, GOD);
  add_command_dep("bamfin", do_bamfin, 255, POSITION_STANDING, LOW_IMMORTAL);
  add_command_dep("bamfout", do_bamfout, 256, POSITION_STANDING, LOW_IMMORTAL);
  add_command_dep("vis", do_invis, 257, POSITION_STANDING, 0);
  add_command_dep("doorbash", do_doorbash, 258, POSITION_STANDING, 1);
  add_command_dep("mosh", do_action, 259, POSITION_FIGHTING, 1);

/* alias commands */
  add_command_dep("alias", do_alias, 260, POSITION_SLEEPING, 1);
  add_command_dep("1", do_alias, 261, POSITION_DEAD, 1);
  add_command_dep("2", do_alias, 262, POSITION_DEAD, 1);
  add_command_dep("3", do_alias, 263, POSITION_DEAD, 1);
  add_command_dep("4", do_alias, 264, POSITION_DEAD, 1);
  add_command_dep("5", do_alias, 265, POSITION_DEAD, 1);
  add_command_dep("6", do_alias, 266, POSITION_DEAD, 1);
  add_command_dep("7", do_alias, 267, POSITION_DEAD, 1);
  add_command_dep("8", do_alias, 268, POSITION_DEAD, 1);
  add_command_dep("9", do_alias, 269, POSITION_DEAD, 1);
  add_command_dep("0", do_alias, 270, POSITION_DEAD, 1);
  add_command_dep("swim", do_swim, 271, POSITION_STANDING, 1);
  add_command_dep("spy", do_spy, 272, POSITION_STANDING, 1);
  add_command_dep("springleap", do_springleap, 273, POSITION_RESTING, 1);
  add_command_dep("quivering palm", do_quivering_palm, 274, POSITION_FIGHTING, 30);
  add_command_dep("feign death", do_feign_death, 275, POSITION_FIGHTING, 1);
  add_command_dep("mount", do_mount, 276, POSITION_STANDING, 1);
  add_command_dep("dismount", do_mount, 277, POSITION_MOUNTED, 1);
  add_command_dep("ride", do_mount, 278, POSITION_STANDING, 1);
  add_command_dep("sign", do_sign, 279, POSITION_RESTING, 1);
  add_command_dep("setsev", do_setsev, 280, POSITION_DEAD, IMMORTAL);
  add_command_dep("first aid", do_first_aid, 281, POSITION_RESTING, 1);
  add_command_dep("log", do_set_log, 282, POSITION_DEAD, 58);
  add_command_dep("recall", do_cast, 283, POSITION_DEAD, LOKI);
  add_command_dep("reload", reboot_text, 284, POSITION_DEAD, 57);
  add_command_dep("event", do_event, 285, POSITION_DEAD, 59);
  add_command_dep("disguise", do_disguise, 286, POSITION_STANDING, 1);
  add_command_dep("climb", do_climb, 287, POSITION_STANDING, 1);
  add_command_dep("beep", do_beep, 288, POSITION_DEAD, 51);
  add_command_dep("bite", do_bite, 289, POSITION_RESTING, 1);
  add_command_dep("redit", do_redit, 290, POSITION_SLEEPING, CREATOR);
  add_command_dep("display", do_display, 291, POSITION_SLEEPING, 1);
  add_command_dep("resize", do_resize, 292, POSITION_SLEEPING, 1);
  add_command_dep("\"", do_commune, 293, POSITION_SLEEPING, LOW_IMMORTAL);
  add_command_dep("#", do_cset, 294, POSITION_DEAD, 59);
  add_command_dep("inset", do_inset, 295, POSITION_RESTING, 1);
  add_command_dep("showexits", do_show_exits, 296, POSITION_DEAD, 1);
  add_command_dep("split", do_split, 297, POSITION_RESTING, 1);
  add_command_dep("report", do_report, 298, POSITION_RESTING, 1);
  add_command_dep("gname", do_gname, 299, POSITION_RESTING, 1);
#if STUPID
  /* this command is a little flawed.  Heavy usage generates obscenely 
     long linked lists in the "donation room" which cause the mud to 
     lag a horrible death. */
  add_command_dep("donate", do_donate, 300, POSITION_STANDING, 1);
#endif
  add_command_dep("auto", do_auto, 301, POSITION_RESTING, 1);
  add_command_dep("brew", do_makepotion, 302, POSITION_RESTING, 1);
  add_command_dep("changeform", do_changeform, 303, POSITION_STANDING, 1);
  add_command_dep("walk", do_walk, 301, POSITION_STANDING, 1);
  add_command_dep("fly", do_fly, 302, POSITION_STANDING, 1);
  add_command_dep("berserk", do_berserk, 303, POSITION_FIGHTING, 1);
  add_command_dep("palm", do_palm, 304, POSITION_STANDING, 1);
  add_command_dep("peek", do_peek, 305, POSITION_STANDING, 1);
  add_command_dep("prompt", do_prompt, 306, POSITION_RESTING, 1);
#if PLAYER_AUTH
  add_command_dep("auth", do_auth, 399, POSITION_SLEEPING, LOW_IMMORTAL);
#endif
}


/* *************************************************************************
 *  Stuff for controlling the non-playing sockets (get name, pwd etc)       *
 ************************************************************************* */




/* locate entry in p_table with entry->name == name. -1 mrks failed search */
int find_name(char *name) {
  int i;

  for (i = 0; i <= top_of_p_table; i++) {
    if (!str_cmp((player_table + i)->name, name))
      return (i);
  }

  return (-1);
}



int _parse_name(char *arg, char *name) {
  int i;

  /* skip whitespaces */
  for (; isspace(*arg); arg++);

  for (i = 0; (*name = *arg) != '\0'; arg++, i++, name++)
    if ((*arg < 0) || !isalpha(*arg) || i > 15)
      return (1);

  if (!i)
    return (1);

  return (0);
}





/* deal with newcomers and other non-playing sockets */
void nanny(struct descriptor_data *d, char *arg) {

  char buf[100], *help_thing;
  int player_i, count = 0, oops = FALSE, index = 0, number, choice;
  int i;
  int junk[6];                  /* generic counter */
  char tmp_name[20];
  bool koshername;
  struct char_file_u tmp_store;
  struct char_data *tmp_ch;
  struct descriptor_data *k;
  extern struct descriptor_data *descriptor_list;
  extern int WizLock;
  extern int plr_tick_count;
  extern int top_of_mobt;
  extern int RacialMax[][6];

  void do_look(struct char_data *ch, char *argument, int cmd);
  void load_char_objs(struct char_data *ch);
  int load_char(char *name, struct char_file_u *char_element);

  write(d->descriptor, echo_on, 6);

  switch (STATE(d)) {

  case CON_ALIGN:

    for (; isspace(*arg); arg++);

    if (!arg) {
      if (!(GET_ALIGNMENT(d->character)))
        GET_ALIGNMENT(d->character) = get_racial_alignment(d);
      if (!(GET_ALIGNMENT(d->character))) {
        SEND_TO_Q
          ("Shall you start with Good, Neutral, or Evil (G/N/E) tendencies? ",
           d);
        STATE(d) = CON_ALIGN;
        break;
      }
      else {                    /* We aren't neutral anyway, skip this part */
        if (GET_RACE(d->character) == RACE_VEGMAN) {
          SEND_TO_Q(VT_HOMECLR, d);
          SEND_TO_Q("\n\rVeggies have no gender.\n\r", d);
          d->character->player.sex = SEX_NEUTRAL;
          SEND_TO_Q(STATQ_MESSG, d);
          STATE(d) = CON_STAT_LIST;
          return;
        }
        SEND_TO_Q("What is your gender, male or female (M/F)?", d);
        STATE(d) = CON_QSEX;
        break;
      }
    }
    else if (!(GET_ALIGNMENT(d->character))) {
      if (!strcmp(arg, "G") || !strcmp(arg, "g")) {
        GET_ALIGNMENT(d->character) = 500;
        SEND_TO_Q("You will enter the realms a champion of goodness.", d);
        if (GET_RACE(d->character) == RACE_VEGMAN) {
          SEND_TO_Q(VT_HOMECLR, d);
          SEND_TO_Q("\n\rVeggies have no gender.\n\r", d);
          d->character->player.sex = SEX_NEUTRAL;
          SEND_TO_Q(STATQ_MESSG, d);
          STATE(d) = CON_STAT_LIST;
          return;
        }
        SEND_TO_Q("\n\r\n\rWhat is your gender, male or female (M/F)?", d);
        STATE(d) = CON_QSEX;
        break;
      }
      else if (!strcmp(arg, "N") || !strcmp(arg, "n")) {
        GET_ALIGNMENT(d->character) = 0;
        SEND_TO_Q("You will enter the realms unbiased.", d);
        if (GET_RACE(d->character) == RACE_VEGMAN) {
          SEND_TO_Q(VT_HOMECLR, d);
          SEND_TO_Q("\n\rVeggies have no gender.\n\r", d);
          d->character->player.sex = SEX_NEUTRAL;
          SEND_TO_Q(STATQ_MESSG, d);
          STATE(d) = CON_STAT_LIST;
          return;
        }
        SEND_TO_Q("\n\r\n\rWhat is your gender, male or female (M/F)?", d);
        STATE(d) = CON_QSEX;
        break;
      }
      else if (!strcmp(arg, "E") || !strcmp(arg, "e")) {
        GET_ALIGNMENT(d->character) = -500;
        SEND_TO_Q("You will enter the realms as a minion of evil.", d);
        if (GET_RACE(d->character) == RACE_VEGMAN) {
          SEND_TO_Q(VT_HOMECLR, d);
          SEND_TO_Q("\n\rVeggies have no gender.\n\r", d);
          d->character->player.sex = SEX_NEUTRAL;
          SEND_TO_Q(STATQ_MESSG, d);
          STATE(d) = CON_STAT_LIST;
          return;
        }
        SEND_TO_Q("\n\r\n\rWhat is your gender, male or female (M/F)?", d);
        STATE(d) = CON_QSEX;
        break;
      }
      else {
        SEND_TO_Q("Please enter (G/N/E) to describe your tendencies: ", d);
        STATE(d) = CON_ALIGN;
        break;
      }
    }
    else {                      /* railroaded into an alignment */
      if (GET_RACE(d->character) == RACE_VEGMAN) {
        SEND_TO_Q(VT_HOMECLR, d);
        SEND_TO_Q("\n\rVeggies have no gender.\n\r", d);
        d->character->player.sex = SEX_NEUTRAL;
        SEND_TO_Q(STATQ_MESSG, d);
        STATE(d) = CON_STAT_LIST;
        return;
      }
      SEND_TO_Q("What is your gender, male or female (M/F)?", d);
      STATE(d) = CON_QSEX;
      break;
    }
    break;
  case CON_QRACE:

    for (; isspace(*arg); arg++);
    if (!*arg) {
      SEND_TO_Q(VT_HOMECLR, d);
      SEND_TO_Q("Choose A Race:\n\r\n\r", d);
      display_races(d);
      STATE(d) = CON_QRACE;
    }
    else {
      if (is_number(arg)) {
        choice = atoi(arg);
      }
      else if (*arg == '?') {
        /*         SEND_TO_Q(RACEHELP, d); */
        arg++;                  /* increment past the ? */
        for (; isspace(*arg); arg++);   /* eat more spaces if any */
        if (is_number(arg)) {
          choice = atoi(arg);
          for (i = 1; RaceList[i].what[0] != '\n'; i++);
          if (choice < 1 || choice > i) {
            SEND_TO_Q("That is not a valid race number.\n\r", d);
            SEND_TO_Q("\n\r*** PRESS RETURN ***", d);
            STATE(d) = CON_QRACE;
            break;
          }
          else {
            extern struct help_index_element *help_index;

            if (!help_index) {
              SEND_TO_Q("Sorry, no help is currently available.\n\r", d);
              STATE(d) = CON_QRACE;
              break;
            }
            choice--;
            help_thing = RaceList[choice].what;
            GET_RACE(d->character) = RaceList[choice].race_num; /* for CVC() */
            if (*help_thing == '*')     /* infra types */
              ++help_thing;
            SEND_TO_Q(VT_HOMECLR, d);
            do_help(d->character, help_thing, 0);
            SEND_TO_Q("Level limits (NVC = Not a Valid Class):\n\r", d);
            if (check_valid_class(d, CLASS_MAGIC_USER)) {
              SPRINTF(buf, "Mage: %d  ",
                      RacialMax[RaceList[choice].race_num][0]);
              SEND_TO_Q(buf, d);
            }
            else
              SEND_TO_Q("Mage: NVC  ", d);

            if (check_valid_class(d, CLASS_CLERIC)) {
              SPRINTF(buf, "Cleric: %d  ",
                      RacialMax[RaceList[choice].race_num][1]);
              SEND_TO_Q(buf, d);
            }
            else
              SEND_TO_Q("Cleric: NVC  ", d);


            if (check_valid_class(d, CLASS_WARRIOR)) {
              SPRINTF(buf, "Warrior: %d  ",
                      RacialMax[RaceList[choice].race_num][2]);
              SEND_TO_Q(buf, d);
            }
            else
              SEND_TO_Q("Warrior: NVC  ", d);


            if (check_valid_class(d, CLASS_THIEF)) {
              SPRINTF(buf, "Thief: %d  ",
                      RacialMax[RaceList[choice].race_num][3]);
              SEND_TO_Q(buf, d);
            }
            else
              SEND_TO_Q("Thief: NVC  ", d);

            if (check_valid_class(d, CLASS_DRUID)) {
              SPRINTF(buf, "Druid: %d  ",
                      RacialMax[RaceList[choice].race_num][4]);
              SEND_TO_Q(buf, d);
            }
            else
              SEND_TO_Q("Druid: NVC  ", d);


            if (check_valid_class(d, CLASS_MONK)) {
              SPRINTF(buf, "Monk: %d  ",
                      RacialMax[RaceList[choice].race_num][0]);
              SEND_TO_Q(buf, d);
            }
            else
              SEND_TO_Q("Monk: NVC  ", d);


            SEND_TO_Q("\n\r", d);
            SEND_TO_Q("\n\r*** PRESS RETURN ***", d);
            STATE(d) = CON_QRACE;
            break;
          }
        }
        else {
          SEND_TO_Q("\n\rThat is not a valid choice!!", d);
          SEND_TO_Q("\n\r*** PRESS RETURN ***", d);
          STATE(d) = CON_QRACE;
          break;
        }
      }
      else {
        SEND_TO_Q("\n\rThat is not a valid choice!!", d);
        SEND_TO_Q("\n\r*** PRESS RETURN ***", d);
        STATE(d) = CON_QRACE;
        break;
      }
      /* assume a race number was chosen */
      for (i = 1; RaceList[i].what[0] != '\n'; i++);
      if (choice < 1 || choice > i) {
        SEND_TO_Q("That is not a valid race number.\n\r", d);
        SEND_TO_Q("\n\r*** PRESS RETURN ***", d);
        STATE(d) = CON_QRACE;
        break;
      }
      else {
        GET_RACE(d->character) = RaceList[choice - 1].race_num;
        if (check_valid_class(d, CLASS_DRUID))
          SEND_TO_Q("Reminder:  Druids must be neutral.\n\r", d);
        if (!get_racial_alignment(d)) { /* neutral chars can choose */
          SEND_TO_Q
            ("Shall you start with Good, Neutral, or Evil (G/N/E) tendencies? ",
             d);
          STATE(d) = CON_ALIGN;
          break;
        }
        else {
          GET_ALIGNMENT(d->character) = get_racial_alignment(d);
          if (GET_ALIGNMENT(d->character) < 0)
            SEND_TO_Q("You are now a minion of evil.\n\r", d);
          else
            SEND_TO_Q("You are now a champion of goodness.\n\r", d);
          if (GET_RACE(d->character) == RACE_VEGMAN) {
            SEND_TO_Q(VT_HOMECLR, d);
            SEND_TO_Q("\n\rVeggies have no gender.\n\r", d);
            d->character->player.sex = SEX_NEUTRAL;
            SEND_TO_Q(STATQ_MESSG, d);
            STATE(d) = CON_STAT_LIST;
            return;
          }
          SEND_TO_Q("What is your gender, male or female (M/F)?", d);
          STATE(d) = CON_QSEX;
          break;

        }
      }
    }

    break;

  case CON_NME:                /* wait for input of name       */
    if (!d->character) {
      CREATE(d->character, struct char_data, 1);
      clear_char(d->character);
      d->character->desc = d;
    }

    for (; isspace(*arg); arg++);
    if (!*arg)
      close_socket(d);
    else {

      if (_parse_name(arg, tmp_name)) {
        SEND_TO_Q("Illegal name, please try another.", d);
        SEND_TO_Q("Name: ", d);
        return;
      }
/*
  i would like to begin here, and explain the concept of the
  bicameral democratic system.


   NOT! :-)
*/

      if (!strncmp(d->host, "128.197.152.10", 14)) {
        if (!strcmp(tmp_name, "Kitten") || !strcmp(tmp_name, "SexKitten")
            || !strcmp(tmp_name, "Rugrat")) {
          SEND_TO_Q("You are a special exception.\n\r", d);
        }
        else {
          SEND_TO_Q("Sorry, this site is temporarily banned.\n\r", d);
          close_socket(d);
        }
      }
      /* Check if already playing */
      for (k = descriptor_list; k; k = k->next) {
        if ((k->character != d->character) && k->character) {
          if (k->original) {
            if (GET_NAME(k->original) &&
                (str_cmp(GET_NAME(k->original), tmp_name) == 0)) {
              SEND_TO_Q("Already playing, cannot connect\n\r", d);
              SEND_TO_Q("Name: ", d);
              return;
            }
          }
          else {                /* No switch has been made */
            if (GET_NAME(k->character) &&
                (str_cmp(GET_NAME(k->character), tmp_name) == 0)) {
              SEND_TO_Q("Already playing, cannot connect\n\r", d);
              SEND_TO_Q("Name: ", d);
              return;
            }
          }
        }
      }

      if ((player_i = load_char(tmp_name, &tmp_store)) > -1) {
        /*
         *  check for tmp_store.max_corpse;
         */
        /*
           if (tmp_store.max_corpse > 3) {
           SEND_TO_Q("Too many corpses in game, can not connect\n\r", d);
           SPRINTF(buf, "%s: too many corpses.",tmp_name);
           log_msg(buf);
           STATE(d) = CON_WIZLOCK;
           break;
           }
         */
        store_to_char(&tmp_store, d->character);
        strcpy(d->pwd, tmp_store.pwd);
        d->pos = player_table[player_i].nr;
        SEND_TO_Q("Password: ", d);
        write(d->descriptor, echo_off, 4);
        STATE(d) = CON_PWDNRM;
      }
      else {
        koshername = TRUE;

        for (number = 0; number <= top_of_mobt; number++) {
          if (isname(tmp_name, mob_index[number].name)) {
            koshername = FALSE;
            break;
          }
        }
        if (koshername == FALSE) {
          SEND_TO_Q("You have chosen a name in use by a monster.\n\r", d);
          SEND_TO_Q("For your own safety, choose another name: ", d);
          return;
        }

        /* player unknown gotta make a new */
        if (!WizLock) {
          CREATE(GET_NAME(d->character), char, strlen(tmp_name) + 1);
          strcpy(GET_NAME(d->character), CAP(tmp_name));
          SPRINTF(buf, "Did I get that right, %s (Y/N)? ", tmp_name);
          SEND_TO_Q(buf, d);
          STATE(d) = CON_NMECNF;
        }
        else {
          SPRINTF(buf, "Sorry, no new characters at this time\n\r");
          SEND_TO_Q(buf, d);
          STATE(d) = CON_WIZLOCK;
        }
      }
    }
    break;

  case CON_NMECNF:             /* wait for conf. of new name   */
    /* skip whitespaces */
    for (; isspace(*arg); arg++);

    if (*arg == 'y' || *arg == 'Y') {
      SEND_TO_Q("New character.\n\r", d);

      SPRINTF(buf, "Give me a password for %s: ", GET_NAME(d->character));

      SEND_TO_Q(buf, d);
      write(d->descriptor, echo_off, 4);
      STATE(d) = CON_PWDGET;
    }
    else {
      if (*arg == 'n' || *arg == 'N') {
        SEND_TO_Q("Ok, what IS it, then? ", d);
        free(GET_NAME(d->character));
        STATE(d) = CON_NME;
      }
      else {                    /* Please do Y or N */
        SEND_TO_Q("Please type Yes or No? ", d);
      }
    }
    break;

  case CON_PWDNRM:             /* get pwd for known player     */
    /* skip whitespaces */
    for (; isspace(*arg); arg++);
    if (!*arg)
      close_socket(d);
    else {
      if (strncmp(crypt(arg, d->character->player.name), d->pwd, 10)) {
        SEND_TO_Q("Wrong password.\n\r", d);
        close_socket(d);
        return;
      }
#if IMPL_SECURITY
      if (top_of_p_table > 0) {
        if (get_max_level(d->character) >= 58) {
          switch (sec_check(GET_NAME(d->character), d->host)) {
          case -1:
          case 0:
            SEND_TO_Q("Security check reveals invalid site\n\r", d);
            SEND_TO_Q("Speak to an implementor to fix problem\n\r", d);
            SEND_TO_Q("If you are an implementor, add yourself to the\n\r", d);
            SEND_TO_Q("Security directory (lib/security)\n\r", d);
            close_socket(d);
            break;
          }
        }
        else {
        }
      }
#endif

      for (tmp_ch = character_list; tmp_ch; tmp_ch = tmp_ch->next)
        if ((!str_cmp(GET_NAME(d->character), GET_NAME(tmp_ch)) &&
             !tmp_ch->desc && !IS_NPC(tmp_ch)) ||
            (IS_NPC(tmp_ch) && tmp_ch->orig &&
             !str_cmp(GET_NAME(d->character), GET_NAME(tmp_ch->orig)))) {

          write(d->descriptor, echo_on, 6);
          SEND_TO_Q("Reconnecting.\n\r", d);

          free_char(d->character);
          tmp_ch->desc = d;
          d->character = tmp_ch;
          tmp_ch->specials.timer = 0;
          if (!IS_IMMORTAL(tmp_ch)) {
            tmp_ch->invis_level = 0;

          }
          if (tmp_ch->orig) {
            tmp_ch->desc->original = tmp_ch->orig;
            tmp_ch->orig = 0;
          }
          d->character->persist = 0;
          STATE(d) = CON_PLYNG;

          act("$n has reconnected.", TRUE, tmp_ch, 0, 0, TO_ROOM);
          SPRINTF(buf, "%s[%s] has reconnected.",
                  GET_NAME(d->character), d->host);
          log_msg(buf);
          return;
        }


      SPRINTF(buf, "%s[%s] has connected.", GET_NAME(d->character), d->host);
      log_msg(buf);
      SEND_TO_Q(motd, d);
      SEND_TO_Q("\n\r\n*** PRESS RETURN: ", d);

      STATE(d) = CON_RMOTD;
    }
    break;

  case CON_PWDGET:             /* get pwd for new player       */
    /* skip whitespaces */
    for (; isspace(*arg); arg++);

    if (!*arg || strlen(arg) > 10) {

      write(d->descriptor, echo_on, 6);
      SEND_TO_Q("Illegal password.\n\r", d);
      SEND_TO_Q("Password: ", d);

      write(d->descriptor, echo_off, 4);
      return;
    }

    strncpy(d->pwd, crypt(arg, d->character->player.name), 10);
    *(d->pwd + 10) = '\0';
    write(d->descriptor, echo_on, 6);
    SEND_TO_Q("Please retype password: ", d);
    write(d->descriptor, echo_off, 4);
    STATE(d) = CON_PWDCNF;
    break;

  case CON_PWDCNF:             /* get confirmation of new pwd  */
    /* skip whitespaces */
    for (; isspace(*arg); arg++);

    if (strncmp(crypt(arg, d->character->player.name), d->pwd, 10)) {
      write(d->descriptor, echo_on, 6);

      SEND_TO_Q("Passwords do not match.\n\r", d);
      SEND_TO_Q("Retype password: ", d);
      STATE(d) = CON_PWDGET;
      write(d->descriptor, echo_off, 4);
      return;
    }
    else {
      write(d->descriptor, echo_on, 6);

      SEND_TO_Q("Choose A Race:\n\r\n\r", d);
      display_races(d);
      STATE(d) = CON_QRACE;
    }
    break;

  case CON_QSEX:               /* query sex of new user        */

    for (; isspace(*arg); arg++);
    switch (*arg) {
    case 'm':
    case 'M':
      /* sex MALE */
      d->character->player.sex = SEX_MALE;
      break;

    case 'f':
    case 'F':
      /* sex FEMALE */
      d->character->player.sex = SEX_FEMALE;
      break;

    default:
      SEND_TO_Q("That is not a valid gender type!\n\r", d);
      SEND_TO_Q("What IS your gender, male or female (M/F)?", d);
      return;
      break;
    }
    SEND_TO_Q(VT_HOMECLR, d);
    SEND_TO_Q(STATQ_MESSG, d);
    STATE(d) = CON_STAT_LIST;
    break;

  case CON_STAT_LIST:
    /* skip whitespaces */

    index = 0;
    for (i = 0; i < 6; i++)
      junk[i] = 0;

    while (*arg && index < MAX_STAT) {
      for (; isspace(*arg); arg++);
      if (*arg == 'S' || *arg == 's') {
        if (!junk[0])
          d->stat[index++] = 's';
        junk[0]++;
      }
      else if (*arg == 'I' || *arg == 'i') {
        if (!junk[1])
          d->stat[index++] = 'i';
        junk[1]++;
      }
      else if (*arg == 'W' || *arg == 'w') {
        if (!junk[2])
          d->stat[index++] = 'w';
        junk[2]++;
      }
      else if (*arg == 'D' || *arg == 'd') {
        if (!junk[3])
          d->stat[index++] = 'd';
        junk[3]++;
      }
      else if (*arg == 'C' || *arg == 'c') {
        arg++;
        if (*arg == 'O' || *arg == 'o') {
          if (!junk[4])
            d->stat[index++] = 'o';
          junk[4]++;
        }
        else if (*arg == 'H' || *arg == 'h') {
          if (!junk[5])
            d->stat[index++] = 'h';
          junk[5]++;
        }
        else {
          SEND_TO_Q(VT_HOMECLR, d);
          SEND_TO_Q("That was an invalid choice.\n\r", d);
          SEND_TO_Q(STATQ_MESSG, d);
          STATE(d) = CON_STAT_LIST;
          break;
        }
      }
      else if (*arg == '?') {
        SEND_TO_Q(VT_HOMECLR, d);
        SEND_TO_Q(STATHELP, d);
        SEND_TO_Q(STATQ_MESSG, d);
        return;
        break;
      }
      else {
        SEND_TO_Q(VT_HOMECLR, d);
        SPRINTF(buf, "Hey, what kinda statistic does an %c represent?\n\r",
                *arg);
        SEND_TO_Q(STATQ_MESSG, d);
        STATE(d) = CON_STAT_LIST;
        return;
        break;
      }
      arg++;
    }
    if (index < MAX_STAT) {
      SEND_TO_Q(VT_HOMECLR, d);
      SEND_TO_Q("You did not enter enough legal statistics.\n\r", d);
      SEND_TO_Q(STATQ_MESSG, d);
      STATE(d) = CON_STAT_LIST;
      break;
    }
    else {
      SEND_TO_Q("Ok.. all chosen.\n\r", d);
      SEND_TO_Q("\n\r", d);
      display_race_classes(d);
#if PLAYER_AUTH
      /* set the AUTH flags */
      /* (3 chances) */
      d->character->generic = NEWBIE_REQUEST + NEWBIE_CHANCES;
#endif
      STATE(d) = CON_QCLASS;
      break;
    }
    display_race_classes(d);

#if PLAYER_AUTH
    /* set the AUTH flags */
    /* (3 chances) */
    d->character->generic = NEWBIE_REQUEST + NEWBIE_CHANCES;
#endif
    STATE(d) = CON_QCLASS;
    break;

  case CON_QCLASS:{
      /* skip whitespaces */
      for (; isspace(*arg); arg++);
      d->character->player.class = 0;
      count = 0;
      oops = FALSE;
      for (; *arg && count < 3 && !oops; arg++) {
        if (count && GET_RACE(d->character) == RACE_HUMANTWO)
          break;
        switch (*arg) {
        case 'm':
        case 'M':{
            if (check_valid_class(d, CLASS_MAGIC_USER)) {
              if (!IS_SET(d->character->player.class, CLASS_MAGIC_USER))
                d->character->player.class += CLASS_MAGIC_USER;
              count++;
              STATE(d) = CON_RMOTD;
            }
            else {
              SEND_TO_Q(VT_HOMECLR, d);
              SEND_TO_Q("Your race may not be a magic user.\n\r", d);
              display_race_classes(d);
              STATE(d) = CON_QCLASS;
              return;
            }
            break;
          }
        case 'c':
        case 'C':{
            if (check_valid_class(d, CLASS_CLERIC)) {
              if (!IS_SET(d->character->player.class, CLASS_CLERIC))
                d->character->player.class += CLASS_CLERIC;
              count++;
              STATE(d) = CON_RMOTD;
            }
            else {
              SEND_TO_Q(VT_HOMECLR, d);
              SEND_TO_Q("Your race may not be a cleric.\n\r", d);
              display_race_classes(d);
              STATE(d) = CON_QCLASS;
              return;

            }
            break;
          }
        case 'k':
        case 'K':{
            if (check_valid_class(d, CLASS_MONK)) {
              if (!IS_SET(d->character->player.class, CLASS_MONK)) {
                d->character->player.class = CLASS_MONK;
                count = 4;
              }
              count++;
              STATE(d) = CON_RMOTD;
            }
            else {
              SEND_TO_Q(VT_HOMECLR, d);
              SEND_TO_Q("Only humans may be monks.\n\r", d);
              display_race_classes(d);
              STATE(d) = CON_QCLASS;
              return;
            }
            break;
          }
        case 'd':
        case 'D':{
            if (check_valid_class(d, CLASS_DRUID)) {
              if (!IS_SET(d->character->player.class, CLASS_DRUID))
                d->character->player.class += CLASS_DRUID;
              count++;
              STATE(d) = CON_RMOTD;
            }
            else {
              SEND_TO_Q(VT_HOMECLR, d);
              SEND_TO_Q("Your race may not be druids.\n\r", d);
              display_race_classes(d);
              STATE(d) = CON_QCLASS;
              return;
            }
            break;
          }
        case 'f':
        case 'F':
        case 'w':
        case 'W':{
            if (check_valid_class(d, CLASS_WARRIOR)) {
              if (!IS_SET(d->character->player.class, CLASS_WARRIOR))
                d->character->player.class += CLASS_WARRIOR;
              count++;
              STATE(d) = CON_RMOTD;
            }
            else {
              SEND_TO_Q(VT_HOMECLR, d);
              SEND_TO_Q("Your race may not be warriors.\n\r", d);
              display_race_classes(d);
              STATE(d) = CON_QCLASS;
              return;
            }
            break;
          }
        case 't':
        case 'T':{
            if (check_valid_class(d, CLASS_THIEF)) {
              if (!IS_SET(d->character->player.class, CLASS_THIEF))
                d->character->player.class += CLASS_THIEF;
              count++;
              STATE(d) = CON_RMOTD;
            }
            else {
              SEND_TO_Q(VT_HOMECLR, d);
              SEND_TO_Q("Your race may not be a thieves.\n\r", d);
              display_race_classes(d);
              STATE(d) = CON_QCLASS;
              return;
            }
            break;
          }
        case '\\':             /* ignore these */
        case '/':
          break;

        case '?':
          SEND_TO_Q("Cleric:       Good defense.  Healing spells\n\r", d);
          SEND_TO_Q
            ("Druid:        Real outdoors types.  Spells, not many items\n\r",
             d);
          SEND_TO_Q("Fighter:      Big, strong and stupid.  Nuff said.\n\r",
                    d);
          SEND_TO_Q
            ("Magic-users:  Weak, puny, smart and very powerful at high levels.\n\r",
             d);
          SEND_TO_Q
            ("Thieves:      Quick, agile, sneaky.  Nobody trusts them.\n\r",
             d);
          SEND_TO_Q
            ("Monks:        Masters of the martial arts.  They can only be single classed\n\r",
             d);
          SEND_TO_Q("\n\r", d);
          display_race_classes(d);
          STATE(d) = CON_QCLASS;
          return;
          break;

        default:
          SEND_TO_Q("I do not recognize that class.\n\r", d);
          STATE(d) = CON_QCLASS;
          oops = TRUE;
          break;
        }
      }
      if (count == 0) {
        SEND_TO_Q("You must choose at least one class!\n\r", d);
        SEND_TO_Q("\n\r", d);
        display_race_classes(d);
        STATE(d) = CON_QCLASS;
        break;
      }
      else {

#if PLAYER_AUTH
        STATE(d) = CON_AUTH;
        SEND_TO_Q("***PRESS ENTER**", d);
#else
        if (STATE(d) != CON_QCLASS) {
          SPRINTF(buf, "%s [%s] new player.", GET_NAME(d->character), d->host);
          log_msg(buf);
          /*
           ** now that classes are set, initialize
           */
          init_char(d->character);
          /* create an entry in the file */
          d->pos = create_entry(GET_NAME(d->character));
          save_char(d->character, AUTO_RENT);
          SEND_TO_Q(motd, d);
          SEND_TO_Q("\n\r\n*** PRESS RETURN: ", d);
          STATE(d) = CON_RMOTD;
        }
#endif
      }
    }
    break;


#if PLAYER_AUTH
  case CON_AUTH:{              /* notify gods */
      if (d->character->generic >= NEWBIE_START) {
        /*
         ** now that classes are set, initialize
         */
        init_char(d->character);
        /* create an entry in the file */
        d->pos = create_entry(GET_NAME(d->character));
        save_char(d->character, AUTO_RENT);
        SEND_TO_Q(motd, d);
        SEND_TO_Q("\n\r\n*** PRESS RETURN: ", d);
        STATE(d) = CON_RMOTD;
      }
      else if (d->character->generic >= NEWBIE_REQUEST) {
        SPRINTF(buf, "%s [%s] new player.", GET_NAME(d->character), d->host);
        log_sev(buf, 7);
        if (!strncmp(d->host, "128.197.152", 11))
          d->character->generic = 1;
        /* I decided to give them another chance.  -Steppenwolf  */
        /* They blew it. -DM */
        if (!strncmp(d->host, "oak.grove", 9)
            || !strncmp(d->host, "143.195.1.20", 12)) {
          d->character->generic = 1;
        }
        else {
          if (top_of_p_table > 0) {
            SPRINTF(buf, "type Auth[orize] %s to allow into game.",
                    GET_NAME(d->character));
            log_sev(buf, 6);
            log_sev("type 'Help Authorize' for other commands", 2);
          }
          else {
            log_msg("Initial character.  Authorized Automatically");
            d->character->generic = NEWBIE_START + 5;
          }
        }
        /*
         **  enough for gods.  now player is told to shut up.
         */
        d->character->generic--;        /* NEWBIE_START == 3 == 3 chances */
        SPRINTF(buf, "Please wait. You have %d requests remaining.\n\r",
                d->character->generic);
        SEND_TO_Q(buf, d);
        if (d->character->generic == 0) {
          SEND_TO_Q("Goodbye.", d);
          STATE(d) = CON_WIZLOCK;       /* axe them */
          break;
        }
        else {
          SEND_TO_Q("Please Wait.\n\r", d);
          STATE(d) = CON_AUTH;
        }
      }
      else {                    /* Axe them */
        STATE(d) = CON_WIZLOCK;
      }
    }
    break;
#endif

  case CON_RMOTD:              /* read CR after printing motd  */
    if (get_max_level(d->character) > 50) {
      SEND_TO_Q(wmotd, d);
      SEND_TO_Q("\n\r\n[PRESS RETURN]", d);
      STATE(d) = CON_WMOTD;
      break;
    }
    if (d->character->term != 0)
      screen_off(d->character);
    SEND_TO_Q(MENU, d);
    STATE(d) = CON_SLCT;
    if (WizLock) {
      if (get_max_level(d->character) < LOW_IMMORTAL) {
        SPRINTF(buf, "Sorry, the game is locked up for repair\n\r");
        SEND_TO_Q(buf, d);
        STATE(d) = CON_WIZLOCK;
      }
    }
    break;


  case CON_WMOTD:              /* read CR after printing motd  */

    SEND_TO_Q(MENU, d);
    STATE(d) = CON_SLCT;
    if (WizLock) {
      if (get_max_level(d->character) < LOW_IMMORTAL) {
        SPRINTF(buf, "Sorry, the game is locked up for repair\n\r");
        SEND_TO_Q(buf, d);
        STATE(d) = CON_WIZLOCK;
      }
    }
    break;

  case CON_WIZLOCK:
    close_socket(d);
    break;

  case CON_CITY_CHOICE:
    /* skip whitespaces */
    for (; isspace(*arg); arg++);
    if (d->character->in_room != NOWHERE) {
      SEND_TO_Q("This choice is only valid when you have been auto-saved\n\r",
                d);
      STATE(d) = CON_SLCT;
    }
    else {
      switch (*arg) {
      case '1':

        reset_char(d->character);
        SPRINTF(buf, "Loading %s's equipment", d->character->player.name);
        log_msg(buf);
        load_char_objs(d->character);
        save_char(d->character, AUTO_RENT);
        send_to_char(WELC_MESSG, d->character);
        d->character->next = character_list;
        character_list = d->character;

        char_to_room(d->character, 3001);
        d->character->player.hometown = 3001;


        d->character->specials.tick = plr_tick_count++;
        if (plr_tick_count == PLR_TICK_WRAP)
          plr_tick_count = 0;

        act("$n has entered the game.", TRUE, d->character, 0, 0, TO_ROOM);
        STATE(d) = CON_PLYNG;
        if (!get_max_level(d->character))
          do_start(d->character);
        do_look(d->character, "", 15);
        d->prompt_mode = 1;

        break;
      case '2':

        reset_char(d->character);
        SPRINTF(buf, "Loading %s's equipment", d->character->player.name);
        log_msg(buf);
        load_char_objs(d->character);
        save_char(d->character, AUTO_RENT);
        send_to_char(WELC_MESSG, d->character);
        d->character->next = character_list;
        character_list = d->character;

        char_to_room(d->character, 1103);
        d->character->player.hometown = 1103;

        d->character->specials.tick = plr_tick_count++;
        if (plr_tick_count == PLR_TICK_WRAP)
          plr_tick_count = 0;

        act("$n has entered the game.", TRUE, d->character, 0, 0, TO_ROOM);
        STATE(d) = CON_PLYNG;
        if (!get_max_level(d->character))
          do_start(d->character);
        do_look(d->character, "", 15);
        d->prompt_mode = 1;

        break;
      case '3':
        if (get_max_level(d->character) > 5) {

          reset_char(d->character);
          SPRINTF(buf, "Loading %s's equipment", d->character->player.name);
          log_msg(buf);
          load_char_objs(d->character);
          save_char(d->character, AUTO_RENT);
          send_to_char(WELC_MESSG, d->character);
          d->character->next = character_list;
          character_list = d->character;

          char_to_room(d->character, 18221);
          d->character->player.hometown = 18221;

          d->character->specials.tick = plr_tick_count++;
          if (plr_tick_count == PLR_TICK_WRAP)
            plr_tick_count = 0;

          act("$n has entered the game.", TRUE, d->character, 0, 0, TO_ROOM);
          STATE(d) = CON_PLYNG;
          if (!get_max_level(d->character))
            do_start(d->character);
          do_look(d->character, "", 15);
          d->prompt_mode = 1;
          break;

        }
        else {
          SEND_TO_Q("That was an illegal choice.\n\r", d);
          STATE(d) = CON_SLCT;
          break;
        }
      case '4':
        if (get_max_level(d->character) > 5) {

          reset_char(d->character);
          SPRINTF(buf, "Loading %s's equipment", d->character->player.name);
          log_msg(buf);
          load_char_objs(d->character);
          save_char(d->character, AUTO_RENT);
          send_to_char(WELC_MESSG, d->character);
          d->character->next = character_list;
          character_list = d->character;

          char_to_room(d->character, 3606);
          d->character->player.hometown = 3606;

          d->character->specials.tick = plr_tick_count++;
          if (plr_tick_count == PLR_TICK_WRAP)
            plr_tick_count = 0;

          act("$n has entered the game.", TRUE, d->character, 0, 0, TO_ROOM);
          STATE(d) = CON_PLYNG;
          if (!get_max_level(d->character))
            do_start(d->character);
          do_look(d->character, "", 15);
          d->prompt_mode = 1;
          break;

        }
        else {
          SEND_TO_Q("That was an illegal choice.\n\r", d);
          STATE(d) = CON_SLCT;
          break;
        }
      case '5':
        if (get_max_level(d->character) > 5) {

          reset_char(d->character);
          SPRINTF(buf, "Loading %s's equipment", d->character->player.name);
          log_msg(buf);
          load_char_objs(d->character);
          save_char(d->character, AUTO_RENT);
          send_to_char(WELC_MESSG, d->character);
          d->character->next = character_list;
          character_list = d->character;

          char_to_room(d->character, 16107);
          d->character->player.hometown = 16107;

          d->character->specials.tick = plr_tick_count++;
          if (plr_tick_count == PLR_TICK_WRAP)
            plr_tick_count = 0;

          act("$n has entered the game.", TRUE, d->character, 0, 0, TO_ROOM);
          STATE(d) = CON_PLYNG;
          if (!get_max_level(d->character))
            do_start(d->character);
          do_look(d->character, "", 15);
          d->prompt_mode = 1;
          break;

        }
        else {
          SEND_TO_Q("That was an illegal choice.\n\r", d);
          STATE(d) = CON_SLCT;
          break;
        }
      default:
        SEND_TO_Q("That was an illegal choice.\n\r", d);
        STATE(d) = CON_SLCT;
        break;
      }
    }
    break;

  case CON_SLCT:               /* get selection from main menu */
    /* skip whitespaces */
    for (; isspace(*arg); arg++);
    switch (*arg) {
    case '0':
      close_socket(d);
      break;

    case '1':
      reset_char(d->character);
      SPRINTF(buf, "Loading %s's equipment", d->character->player.name);
      log_msg(buf);
      load_char_objs(d->character);
      save_char(d->character, AUTO_RENT);
      send_to_char(WELC_MESSG, d->character);
      d->character->next = character_list;
      character_list = d->character;
      if (d->character->in_room == NOWHERE ||
          d->character->in_room == AUTO_RENT) {
        if (get_max_level(d->character) < LOW_IMMORTAL) {

          if (d->character->specials.start_room <= 0) {
            if (GET_RACE(d->character) == RACE_HALFLING) {
              char_to_room(d->character, 1103);
              d->character->player.hometown = 1103;
            }
            else {
              char_to_room(d->character, 3001);
              d->character->player.hometown = 3001;
            }
          }
          else {
            char_to_room(d->character, d->character->specials.start_room);
            d->character->player.hometown = d->character->specials.start_room;
          }
        }
        else {
          if (d->character->specials.start_room <= NOWHERE) {
            char_to_room(d->character, 1000);
            d->character->player.hometown = 1000;
          }
          else {
            if (real_roomp(d->character->specials.start_room)) {
              char_to_room(d->character, d->character->specials.start_room);
              d->character->player.hometown =
                d->character->specials.start_room;
            }
            else {
              char_to_room(d->character, 1000);
              d->character->player.hometown = 1000;
            }
          }
        }
      }
      else {
        if (real_roomp(d->character->in_room)) {
          char_to_room(d->character, d->character->in_room);
          d->character->player.hometown = d->character->in_room;
        }
        else {
          char_to_room(d->character, 3001);
          d->character->player.hometown = 3001;
        }
      }

      d->character->specials.tick = plr_tick_count++;
      if (plr_tick_count == PLR_TICK_WRAP)
        plr_tick_count = 0;

      act("$n has entered the game.", TRUE, d->character, 0, 0, TO_ROOM);
      STATE(d) = CON_PLYNG;
      if (!get_max_level(d->character))
        do_start(d->character);
      do_look(d->character, "", 15);
      d->prompt_mode = 1;
      break;

    case '2':
      SEND_TO_Q
        ("Enter a text you'd like others to see when they look at you.\n\r",
         d);
      SEND_TO_Q("Terminate with a '@'.\n\r", d);
      if (d->character->player.description) {
        SEND_TO_Q("Old description :\n\r", d);
        SEND_TO_Q(d->character->player.description, d);
        free(d->character->player.description);
        d->character->player.description = 0;
      }
      d->str = &d->character->player.description;
      d->max_str = 240;
      STATE(d) = CON_EXDSCR;
      break;

    case '3':
      SEND_TO_Q(STORY, d);
      STATE(d) = CON_RMOTD;
      break;
    case '4':
      SEND_TO_Q("Enter a new password: ", d);

      write(d->descriptor, echo_off, 4);

      STATE(d) = CON_PWDNEW;
      break;

    case '5':
      SEND_TO_Q("Where would you like to enter?\n\r", d);
      SEND_TO_Q("1.    Midgaard\n\r", d);
      SEND_TO_Q("2.    Shire\n\r", d);
      if (get_max_level(d->character) > 5)
        SEND_TO_Q("3.    Mordilnia\n\r", d);
      if (get_max_level(d->character) > 10)
        SEND_TO_Q("4.    New  Thalos\n\r", d);
      if (get_max_level(d->character) > 20)
        SEND_TO_Q("5.    The Gypsy Village\n\r", d);
      SEND_TO_Q("Your choice? ", d);
      STATE(d) = CON_CITY_CHOICE;
      break;

    case 'D':{
        int i;
        struct char_file_u ch_st;
        FILE *char_file;

        for (i = 0; i <= top_of_p_table; i++) {
          if (!str_cmp((player_table + i)->name, GET_NAME(d->character))) {
            free((player_table + i)->name);
            (player_table + i)->name = (char *)malloc(strlen("111111"));
            strcpy((player_table + i)->name, "111111");
            break;
          }
        }
        /* get the structure from player_table[i].nr */
        if (!(char_file = fopen(PLAYER_FILE, "r+"))) {
          perror("Opening player file for updating. (interpreter.c, nanny)");
          assert(0);
        }
        fseek(char_file, (long)(player_table[i].nr *
                                sizeof(struct char_file_u)), 0);

        /* read in the char, change the name, write back */
        fread(&ch_st, sizeof(struct char_file_u), 1, char_file);
        SPRINTF(ch_st.name, "111111");
        fseek(char_file, (long)(player_table[i].nr *
                                sizeof(struct char_file_u)), 0);
        fwrite(&ch_st, sizeof(struct char_file_u), 1, char_file);
        fclose(char_file);

        close_socket(d);
        break;
      }

    default:
      SEND_TO_Q("Wrong option.\n\r", d);
      SEND_TO_Q(MENU, d);
      break;
    }
    break;

  case CON_PWDNEW:
    /* skip whitespaces */
    for (; isspace(*arg); arg++);

    if (!*arg || strlen(arg) > 10) {
      write(d->descriptor, echo_on, 6);

      SEND_TO_Q("Illegal password.\n\r", d);
      SEND_TO_Q("Password: ", d);

      write(d->descriptor, echo_off, 4);


      return;
    }

    strncpy(d->pwd, crypt(arg, d->character->player.name), 10);
    *(d->pwd + 10) = '\0';
    write(d->descriptor, echo_on, 6);

    SEND_TO_Q("Please retype password: ", d);

    STATE(d) = CON_PWDNCNF;
    write(d->descriptor, echo_off, 4);


    break;
  case CON_PWDNCNF:
    /* skip whitespaces */
    for (; isspace(*arg); arg++);

    if (strncmp(crypt(arg, d->character->player.name), d->pwd, 10)) {
      write(d->descriptor, echo_on, 6);
      SEND_TO_Q("Passwords don't match.\n\r", d);
      SEND_TO_Q("Retype password: ", d);
      write(d->descriptor, echo_off, 4);

      STATE(d) = CON_PWDNEW;
      return;
    }
    write(d->descriptor, echo_on, 6);

    SEND_TO_Q("\n\rDone. You must enter the game to make the change final\n\r",
              d);
    SEND_TO_Q(MENU, d);
    STATE(d) = CON_SLCT;
    break;
  default:
    log_msg("Nanny: illegal state of con'ness");
    abort();
    break;
  }
}

char *class_selections[] = {
  "(M)age",
  "(C)leric",
  "(W)arrior",
  "(T)hief",
  "(D)ruid",
  "mon(K)",
  "\n"
};

int check_valid_class(struct descriptor_data *d, int class) {
  extern int RacialMax[][6];

  if (GET_RACE(d->character) == RACE_HUMANTWO)
    return (TRUE);

  if (class == CLASS_MONK)
    return (FALSE);

  if (class == CLASS_DRUID) {
    if (GET_RACE(d->character) == RACE_VEGMAN)  /* NOT VEGGIE! */
      return (TRUE);
    else
      return (FALSE);
  }

  if (class == CLASS_MAGIC_USER && RacialMax[GET_RACE(d->character)][0] > 10)
    return (TRUE);

  if (class == CLASS_CLERIC && RacialMax[GET_RACE(d->character)][1] > 10)
    return (TRUE);

  if (class == CLASS_WARRIOR && RacialMax[GET_RACE(d->character)][2] > 10)
    return (TRUE);

  if (class == CLASS_THIEF && RacialMax[GET_RACE(d->character)][3] > 10)
    return (TRUE);

  return (FALSE);
}

void display_race_classes(struct descriptor_data *d) {
  extern int RacialMax[][6];
  int i;
  bool bart = FALSE;
  char buf[40];

  int classes[MAX_CLASS];
  classes[0] = CLASS_MAGIC_USER;
  classes[1] = CLASS_CLERIC;
  classes[2] = CLASS_WARRIOR;
  classes[3] = CLASS_THIEF;
  classes[4] = CLASS_DRUID;
  classes[5] = CLASS_MONK;

  if (GET_RACE(d->character) == RACE_HUMAN) {
    SEND_TO_Q("Humans can only be single class characters.  They can choose",
              d);
    SEND_TO_Q("\n\rfrom any class though.", d);
  }

  SEND_TO_Q("\n\rYou can choose from the following classes:\n\r", d);
  SEND_TO_Q
    ("The number in brackets is the maximum level your race can attain.\n\r",
     d);
  for (i = bart = 0; i < MAX_CLASS; i++) {
    if (check_valid_class(d, classes[i])) {
      if (bart)
        SEND_TO_Q(", ", d);
      SPRINTF(buf, "%s [%d]", class_selections[i],
              RacialMax[GET_RACE(d->character)][i]);
      SEND_TO_Q(buf, d);
      bart = TRUE;
    }
  }
  SEND_TO_Q("\n\r", d);
  if (GET_RACE(d->character) != RACE_HUMAN)
    SEND_TO_Q("Enter M/W or M/C/T, T/M (etc) to be multi-classed.\n\r", d);
  SEND_TO_Q("Enter ? for help.\n\rYour choice: ", d);
}

void display_races(struct descriptor_data *d) {
  int i;
  char buf[80];

  SEND_TO_Q("     ", d);
  for (i = 1; RaceList[i - 1].what[0] != '\n'; i++) {
    SPRINTF(buf, "%2d - %-15s ", i, RaceList[i - 1].what);
    if (!(i % 3))
      strcat(buf, "\n\r     ");
    SEND_TO_Q(buf, d);
  }
  SEND_TO_Q("\n\rAn (*) signifies that this race has infravision.\n\r", d);
  SEND_TO_Q("\n\rFor more help on a race, enter ?#, where # is the number", d);
  SEND_TO_Q("\n\rof the race you want more information on.\n\rYour choice? ",
            d);
}


int get_racial_alignment(struct descriptor_data *d) {
  switch (GET_RACE(d->character)) {

    /* Good Aligned */
  case RACE_SMURF:
  case RACE_FAERIE:
    return (500);
    break;

  case RACE_DEMON:
  case RACE_DEVIL:
    return (-1000);
    break;
  case RACE_UNDEAD:
  case RACE_LYCANTH:
  case RACE_ORC:
  case RACE_GOBLIN:
  case RACE_TROLL:
  case RACE_DROW:
  case RACE_VAMPIRE:
  case RACE_OGRE:
  case RACE_SARTAN:
  case RACE_ENFAN:
  case RACE_MFLAYER:
  case RACE_ROO:
    return (-500);
    break;
  default:
    return (0);
  }
}
