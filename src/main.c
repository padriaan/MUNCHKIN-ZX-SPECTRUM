/*******************************************************************************************
munchkin_z80_colour.c 

Remake of Philips Videopac    38 Munchkin
          Magnavox Osyssey 2  38 KC Munchkin
          Originally released in 1981, programmed by Ed Averett.

Developed  with Z88DK in C for the Sinclair ZX Spectrum 48K.          
Created by Peter Adriaanse August 2025.

Version 0.7 (nr also in constant below)

Compile and link in Linux:
$ ./build.sh

Generates munchkin_z80_colour.tap (standalone for use in fuse or real heardware)

***********************************************************************************************/
#include <arch/zx.h>
#include <stdlib.h>
#include <string.h>
#include <arch/zx/sp1.h>
#include <alloc/balloc.h>
#include <input.h>
#include <intrinsic.h>

#include "gfx.h"
#include "int.h"
#include "playfx.h"
#include "sound.h"

// scratch ram at 0x5e24 before program
extern unsigned char TEMPMEM[1024];   // address placement defined in main.asm

#define VERSION "0.7"


// list of all beepfx sound effects
typedef struct effects_s
{
   void *effect;
   char *name;
} effects_t;
 
const effects_t beepfx[] = {
    {BEEPFX_PICK,              "BEEPFX_EAT_PILL"},
    {BEEPFX_HIT_2,             "BEEPFX_EAT_GHOST"},
    {BEEPFX_GULP,              "BEEPFX_GULP_GHOST"},
    {BEEPFX_ITEM_3,            "BEEPFX_EAT_POWERPILL"},
    {BEEPFX_POWER_OFF,         "BEEPFX_DYING"},
    {BEEPFX_FAT_BEEP_1,        "BEEPFX_DYING_SHORT"},
    {BEEPFX_JUMP_2,            "BEEPFX_MOVE"},
    {BEEPFX_ITEM_3,            "BEEPFX_MAZE_COMPLETE"}
};

// convenient globals (runs better in z88dk)
unsigned int key, i, j;
unsigned char *pt;

// colours for sprites
uint8_t ink_colour;

struct sp1_Rect cr = { 0, 0, 32, 24 };

//unsigned int no_frame_skip = 0;
//unsigned int one_frame_skip = 0;
//unsigned int two_frame_skips = 0;
//unsigned int three_frame_skips = 0;
//unsigned int four_or_more_frame_skips = 0;
//unsigned int saved_tick;


// unsigned char in stead of #define (runs/compiles better in z88dk)
unsigned char MAZE_OFFSET_X = 16;
unsigned char MAZE_OFFSET_Y = 32;
unsigned char NUM_PILLS = 12;            
unsigned char NUM_GHOSTS = 4;            

#define VERT_LINE_SIZE      18
#define HORI_LINE_SIZE      26
#define NUM_HORI_CELLS       9
#define NUM_VERT_CELLS       7
#define NUM_HORI_LINES_COL   8
#define NUM_VERT_LINES_ROW  10


// sp1 print string context
struct sp1_pss ps0;

// control keys
unsigned int keys[4] = { 'o', 'p', 'q', 'a' };   //default left, right, up, down

JOYFUNC joyfunc;
udk_t   joy_k;

char *redefine_texts[5] = {
   "\x14\x45" " LEFT:",
   "RIGHT:",
   "   UP:",
   " DOWN:"
};

// game variables
// (yes globals because of performance and z88dk!)
unsigned int  score, high_score;


// global variables for use in function rotate_maze_center()
// (needed this way, because compiler does not handle these as locals very well)

unsigned char udg_line_vert[8]            = {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18};
unsigned char udg_line_hori[8]            = {0x00,0x00,0x00,0xFF,0xFF,0x00,0x00,0x00};
unsigned char udg_top_left[8]             = {0x00,0x00,0x00,0x1F,0x1F,0x18,0x18,0x18};
unsigned char udg_top_right[8]            = {0x00,0x00,0x00,0xF8,0xF8,0x18,0x18,0x18};
unsigned char udg_bottom_left[8]          = {0x18,0x18,0x18,0x1F,0x1F,0x00,0x00,0x00};
unsigned char udg_bottom_right[8]         = {0x18,0x18,0x18,0xF8,0xF8,0x00,0x00,0x00};
unsigned char udg_line_hori_right[8]      = {0x00,0x00,0x00,0x1F,0x1F,0x00,0x00,0x00};
unsigned char udg_line_hori_left[8]       = {0x00,0x00,0x00,0xF8,0xF8,0x00,0x00,0x00};
unsigned char udg_line_vert_bottom[8]     = {0x18,0x18,0x18,0x18,0x18,0x00,0x00,0x00};
unsigned char udg_line_vert_top[8]        = {0x00,0x00,0x00,0x18,0x18,0x18,0x18,0x18};
unsigned char udg_line_hori_left_maze2[8] = {0x18,0x18,0x18,0xFF,0xFF,0x00,0x00,0x00};
unsigned char udg_line_vert_top_maze2[8]  = {0x18,0x18,0x18,0xF8,0xF8,0x18,0x18,0x18};
unsigned char udg_bottom_left_maze2[8]    = {0x18,0x18,0x18,0x1F,0x1F,0x18,0x18,0x18};
unsigned char udg_top_left_maze2[8]       = {0x18,0x18,0x18,0xFF,0xFF,0x18,0x18,0x18};

unsigned char udg_square_block[8]         = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

#define LEFT                 1
#define RIGHT                2
#define UP                   3
#define DOWN                 4

#define FALSE                0
#define TRUE                 1

unsigned char munchkin_x_factor1;
unsigned char munchkin_y_factor1;
unsigned char munchkin_auto_direction;        // if <> 0, then 1,2,3 or 4 for auto movement to cell
                                              //          1=left, 2=right, 3=up, 4-down
unsigned char munchkin_last_direction;        // (0=stopped, 1 left, 2 right, 3 up, 4 down)
unsigned char speed;                          // munchkin speed in pixels per frame
unsigned char munchkin_animation_frame;       // to display animations during movement
unsigned char munchkin_dying;                 // TRUE/FALSE
unsigned char munchkin_dying_animation;       // to display dying animations
unsigned char maze_completed;                 // TRUE/FALSE  1=completed
unsigned char maze_completed_animations;      // counter for end of level animations
unsigned char maze_center_open;               // 1=left, 2=right, 3=up, 4=down
unsigned char maze_color;                     // 3=magenta, 6=yellow
unsigned char maze_selected;                  // active maze: 1 or 2

unsigned char text_hori_line[NUM_HORI_CELLS + 1];        // + 1 for end of string
unsigned char text_vert_line[NUM_VERT_LINES_ROW + 1];    // + 1 for end of string

unsigned char last_pill_speed_increased;  // has the speed of the last pill already increased? TRUE/FALSE
unsigned char powerpill_active_timer;     // timer for how long ghosts are magenta (can be eaten)
unsigned char active_pills;               

int frame = 1;

typedef struct horizontal_line_type {     // contains 8 rows of 9 lines
  char line[NUM_HORI_CELLS + 1];
} horizontal_line_type;

horizontal_line_type horizontal_lines[NUM_HORI_LINES_COL];

typedef struct vertical_line_type {     // contains 8 rows of 9 lines
  char line[NUM_VERT_LINES_ROW + 1];
} vertical_line_type;

vertical_line_type vertical_lines[NUM_VERT_CELLS];

struct sp1_ss  *munchkin_sprite;

// structure for munchkin animations
struct {  unsigned char *graphic; }     // sprites in gfx.h
munchkin_sprite_graphic[] = {
  {munchkin},   
  {munchkin_left},
  {munchkin_right},
  {munchkin_center},
  {munchkin_up},
  {munchkin_down},
  {munchkin_closed},
  {munchkin_win},
  {munchkin_dying1},
  {munchkin_dying2},
  {munchkin_dying3},
  {munchkin_dying4},
  {munchkin_dying5}
};

// ghosts
typedef struct 
{
  struct sp1_ss *sprite;
  unsigned char x, y, status,          // status:  1 = normal, 2 = magenta (can be eaten)
                                       //          3 = eaten/dead, 4 = recharging in center
                colour,                //colour  1=yellow, 2=green, 3=red, 4=cyan (normal)
                                       //        5=magenta                  (can be eaten)
                                       //        7=white                    (dead)
                direction,             // LEFT,RIGHT, UP, DOWN
                speed,
                gulp_sound_delay;      // timer for gulp sound delay
  int           recharge_timer;        // timer for ghost being recharged
} ghost_sprite;

ghost_sprite ghosts[9];   // max 9

// structure for ghost animations
struct {  unsigned char *graphic; }     // sprites in gfx.h
ghost_sprite_graphic[] = {
  {ghost},   
  {ghost_dead}
};


// pills
typedef struct 
{
  struct sp1_ss *sprite;
  unsigned char x, y, 
                status,                // status: 0=off, 1=active, 2=powerpill
                colour_masked,         // TRUE/FALSE  ; if TRUE then colour is changed
                                       //               to prevent colour clash with ghost
                direction, speed;   
} pill_sprite;

pill_sprite pills[12];    // max 12

struct {  unsigned char *graphic; }     // sprites in gfx.h
pill_sprite_graphic[] = {
  {pill},   
  {pill_flash}
};

#define _________________________a
#define ___FORWARD_DECLARATIONS__b
#define _________________________c

void setup_maze(void);
void add_colour_to_sprite(unsigned int count, struct sp1_cs *c);
void handle_maze_completed(void);
void pad_numbers(unsigned char *s, unsigned int limit, long number);
void get_ink_colour(unsigned char a_colour);
void run_redefine_keys(void);
void draw_menu(void);
void run_intro(void);
//void update_horde(void);
void display_score(void);
//void update_player(void);
//void update_sprites(void);
//void update_script(void);
void handle_munchkin(unsigned char munchkin_direction, unsigned char munchkin_manual_move);
void get_user_input(void);
void run_play(void);
void draw_munchkin(void);
void setup_ghosts(void);
void draw_ghosts(void);
void handle_ghosts(void);
void choose_ghost_direction (unsigned char i);
void check_ghosts_hits_munchkin(void);
void setup_pills(void);
void draw_pills(void);
void check_pill_eaten(void);
void handle_pills(void);
void check_pills_mask(void);
void choose_pill_direction (unsigned char i);
void rotate_maze_center(void);
void setup(void);
void start_new_game(void);
void start_new_maze(void); 
void hide_sprites(void);
int main(void);

#define __________a
#define ___MAIN___b
#define __________c

// munchkin
void setup_maze(void)
{
  // global vairiables:
  //char text_hori_line[NUM_HORI_CELLS + 1];        // + 1 for end of string
  //char text_vert_line[NUM_VERT_LINES_ROW + 1];    // + 1 for end of string
  
  if (maze_selected == 1) {
  strcpy(text_hori_line, "xxxxxxxxx");  strcpy(horizontal_lines[0].line, text_hori_line);
  strcpy(text_vert_line, "|---|----|"); strcpy(vertical_lines[0].line,   text_vert_line);
  strcpy(text_hori_line, "-x---x-x-");  strcpy(horizontal_lines[1].line, text_hori_line);
  strcpy(text_vert_line, "|--|-----|"); strcpy(vertical_lines[1].line,   text_vert_line);
  strcpy(text_hori_line, "----xx---");  strcpy(horizontal_lines[2].line, text_hori_line);
  strcpy(text_vert_line, "|||--|-|||"); strcpy(vertical_lines[2].line,   text_vert_line);
  strcpy(text_hori_line, "--x---x--");  strcpy(horizontal_lines[3].line, text_hori_line);
  strcpy(text_vert_line, "|-|----|-|"); strcpy(vertical_lines[3].line,   text_vert_line);
  strcpy(text_hori_line, "x---x---x");  strcpy(horizontal_lines[4].line, text_hori_line);
  strcpy(text_vert_line, "---||||---"); strcpy(vertical_lines[4].line,   text_vert_line);
  strcpy(text_hori_line, "x-------x");  strcpy(horizontal_lines[5].line, text_hori_line);
  strcpy(text_vert_line, "|-|----|-|"); strcpy(vertical_lines[5].line,   text_vert_line);
  strcpy(text_hori_line, "-x--x-xx-");  strcpy(horizontal_lines[6].line, text_hori_line);
  strcpy(text_vert_line, "|--|-|---|"); strcpy(vertical_lines[6].line,   text_vert_line);
  strcpy(text_hori_line, "xxxxxxxxx");  strcpy(horizontal_lines[7].line, text_hori_line);
  } else {
  strcpy(text_hori_line, "xxxxxxxxx");  strcpy(horizontal_lines[0].line, text_hori_line);
  strcpy(text_vert_line, "||---|---|"); strcpy(vertical_lines[0].line,   text_vert_line);
  strcpy(text_hori_line, "-xxx--x-x");  strcpy(horizontal_lines[1].line, text_hori_line);
  strcpy(text_vert_line, "|----|-|-|"); strcpy(vertical_lines[1].line,   text_vert_line);
  strcpy(text_hori_line, "--x-x----");  strcpy(horizontal_lines[2].line, text_hori_line);
  strcpy(text_vert_line, "||-|-||-||"); strcpy(vertical_lines[2].line,   text_vert_line);
  strcpy(text_hori_line, "-x---x---");  strcpy(horizontal_lines[3].line, text_hori_line);
  strcpy(text_vert_line, "|---||-|-|"); strcpy(vertical_lines[3].line,   text_vert_line);
  strcpy(text_hori_line, "x-xxx-xxx");  strcpy(horizontal_lines[4].line, text_hori_line);
  strcpy(text_vert_line, "----|||---"); strcpy(vertical_lines[4].line,   text_vert_line);
  strcpy(text_hori_line, "xx---x--x");  strcpy(horizontal_lines[5].line, text_hori_line);
  strcpy(text_vert_line, "|--||--|-|"); strcpy(vertical_lines[5].line,   text_vert_line);
  strcpy(text_hori_line, "-x---x-x-");  strcpy(horizontal_lines[6].line, text_hori_line);
  strcpy(text_vert_line, "|---|----|"); strcpy(vertical_lines[6].line,   text_vert_line);
  strcpy(text_hori_line, "xxxxxxxxx");  strcpy(horizontal_lines[7].line, text_hori_line);
  }
} 

  
void add_colour_to_sprite(unsigned int count, struct sp1_cs *c)
{
    (void)count;    /* Suppress compiler warning about unused parameter */
    c->attr_mask = SP1_AMASK_INK;
    c->attr      = ink_colour;
}



void handle_maze_completed(void)
{
  // change maze color to yellow and magenta
  // smiling munchkin
  maze_completed_animations --;
  
  if (maze_completed_animations % 3 == 0) {
    if (maze_color == '3' ) {
         maze_color = '6';   
         ptiles[1] = 0x46; 
         ptiles_maze2[1] = 0x46; 
    } else {
         maze_color = '3';
         ptiles[1] = 0x43;  
         ptiles_maze2[1] = 0x43;  
    }  

    sp1_SetPrintPos(&ps0, 0, 0);

    if (maze_selected == 1) sp1_PrintString(&ps0, ptiles);
      else sp1_PrintString(&ps0, ptiles_maze2);

    display_score();

    if (maze_completed_animations % 3 == 0)
       bit_beepfx_di(beepfx[7].effect);
  }
}


void pad_numbers(unsigned char *s, unsigned int limit, long number)
{
   s += limit;
   *s = 0;

   // not a fast method since there are two 32-bit divisions in the loop
   // better would be ultoa or ldivu which would do one division or if
   // the library is so configured, they would do special case base 10 code.
   
   while (limit--)
   {
      *--s = (number % 10) + '0';
      number /= 10;
   }
}


void get_ink_colour(unsigned char a_colour)
{
   // ink_colour is a global variable
   switch (a_colour) {
   case 1:
          ink_colour = INK_YELLOW | PAPER_BLACK;      
          break;
   case 2:
          ink_colour = INK_GREEN | PAPER_BLACK;      
          break;
   case 3:
          ink_colour = INK_RED | PAPER_BLACK;      
          break;
   case 4:
          ink_colour = INK_CYAN | PAPER_BLACK;      
          break;
   case 5:
          ink_colour = INK_MAGENTA | PAPER_BLACK;      
          break;
   } 
}


void run_redefine_keys(void)
{
   struct sp1_Rect r = { 10, 2, 30, 10 };

   sp1_ClearRectInv(&r, INK_BLACK | PAPER_BLACK, 32, SP1_RFLAG_TILE | SP1_RFLAG_COLOUR);

   sp1_SetPrintPos(&ps0, 10, 10);
   sp1_PrintString(&ps0, "\x14\x47" "REDEFINE KEYS");

   for (i = 0; i < 4; ++i)
   {
      sp1_SetPrintPos(&ps0, 12 + i, 11);
      sp1_PrintString(&ps0, redefine_texts[i]);  
      sp1_UpdateNow();

      in_wait_key();
      keys[i] = in_inkey();
      in_wait_nokey();

      // nope!
      if (keys[i] < 32)
      {
         --i;
         continue;
      }

      // space is not visible, make it so
      if (keys[i] == 32)
      {
         sp1_SetPrintPos(&ps0, 12 + i, 18);
         sp1_PrintString(&ps0, "\x14\x46" "SPACE");
      }
      else
         sp1_PrintAtInv(12 + i, 18, INK_YELLOW, keys[i]);
      
      sp1_UpdateNow();
      playfx(FX_SELECT);
   }

   // some delay so the player can see last pressed key
   for (i = 0; i < 16; ++i)
      wait();
}

void draw_menu(void)
{
   unsigned char buffer[16];
   //struct sp1_Rect r = { 10, 2, 30, 10 };
   struct sp1_Rect r = { 9, 0, 32, 14 };

   // clear the screen first to avoid attribute artifacts
   sp1_ClearRectInv(&cr, INK_BLACK | PAPER_BLACK, 32, SP1_RFLAG_TILE | SP1_RFLAG_COLOUR);
   sp1_UpdateNow();

   // this will wait for vsync
   wait();
   //dzx7_standard(menu, (void *)0x4000);
   // sp1_Validate(&cr);  // not necessary since there was just an update

   sp1_TileEntry(':', udg_square_block);
   sp1_SetPrintPos(&ps0, 0, 0);
   sp1_PrintString(&ps0, "\x14\x02" "::::: : : ::: ::: : : :  : : :::"); 
   sp1_SetPrintPos(&ps0, 1, 0);
   sp1_PrintString(&ps0, "\x14\x02" ": : : : : : : :   : : : :  : : :"); 
   sp1_SetPrintPos(&ps0, 2, 0);
   sp1_PrintString(&ps0, "\x14\x02" ": : : : : : : :   ::: ::   : : :"); 
   sp1_SetPrintPos(&ps0, 3, 0);
   sp1_PrintString(&ps0, "\x14\x02" ": : : : : : : :   : : : :  : : :"); 
   sp1_SetPrintPos(&ps0, 4, 0);
   sp1_PrintString(&ps0, "\x14\x02" ": : : ::: : : ::: : : :  : : : :"); 

   sp1_SetPrintPos(&ps0, 9, 5);
   sp1_PrintString(&ps0, "\x14\x47" "BASED ON KC MUNCHKIN FOR");
   sp1_SetPrintPos(&ps0, 11, 8);
   sp1_PrintString(&ps0, "\x14\x46" "MAGNAVOX ODESSEY 2");
   sp1_SetPrintPos(&ps0, 12, 8 );
   sp1_PrintString(&ps0, "\x14\x46" "PHILIPS VIDEOPAC");


   sp1_SetPrintPos(&ps0, 14, 2);
   sp1_PrintString(&ps0, "\x14\x45" "Original by Ed Averett 1981");
   sp1_SetPrintPos(&ps0, 16, 1);
   sp1_PrintString(&ps0, "\x14\x43" "Remake by Peter Adriaanse 2025");


 
   sp1_SetPrintPos(&ps0, 20, 9);
   sp1_PrintString(&ps0, "\x14\x47" "PRESS ANY KEY!");

   sp1_SetPrintPos(&ps0, 23, 10);
   sp1_PrintString(&ps0, "\x14\x01" "version");
   sp1_SetPrintPos(&ps0, 23, 19);
   sp1_PrintString(&ps0, VERSION);
   sp1_UpdateNow();

   in_wait_key();
   // avoid the player pressing one of the options and triggering the associated code
   in_wait_nokey();

   sp1_ClearRectInv(&r, INK_BLACK | PAPER_BLACK, 32, SP1_RFLAG_TILE | SP1_RFLAG_COLOUR);

   sp1_SetPrintPos(&ps0, 11, 11);
   sp1_PrintString(&ps0, "\x14\x47" "1 KEYBOARD"
                 "\x0b\x0b\x06\x0b" "2 KEMPSTON"
                 "\x0b\x0b\x06\x0b" "3 SINCLAIR"
                 "\x0b\x0b\x06\x0b" "4 REDEFINE KEYS"
               );

   // the hiscore
   sp1_SetPrintPos(&ps0, 8, 11);
   sp1_PrintString(&ps0, "\x14\x47" "HIGH");

   buffer[0] = 0x14;
   buffer[1] = INK_YELLOW | PAPER_BLACK;
   pad_numbers(buffer + 2, 4, high_score);

   sp1_SetPrintPos(&ps0, 8, 17);
   sp1_PrintString(&ps0, buffer);

   sp1_SetPrintPos(&ps0, 9, 0);
   sp1_PrintString(&ps0, "\x14\x47" " ");

   sp1_SetPrintPos(&ps0, 14, 0);
   sp1_PrintString(&ps0, "\x14\x47" " ");

   // will update in the main loop
}


void run_intro(void)
{
   sp1_ClearRectInv(&cr, INK_BLACK | PAPER_BLACK, 32, SP1_RFLAG_TILE | SP1_RFLAG_COLOUR);
   sp1_UpdateNow();

   // the menu key may be is still pressed
   in_wait_nokey();

   // wait for vsync
   wait();

   // copy attributes to our temp memory area
   memcpy(TEMPMEM, (void *)0x5800, 768);

   sp1_ClearRectInv(&cr, INK_BLACK | PAPER_BLACK, 32, SP1_RFLAG_TILE | SP1_RFLAG_COLOUR);
   sp1_UpdateNow();


skip_intro:
   sp1_ClearRectInv(&cr, INK_BLACK | PAPER_BLACK, 32, SP1_RFLAG_TILE | SP1_RFLAG_COLOUR);
   sp1_UpdateNow();

   // in case a key was pressed to skip the intro
   in_wait_nokey();
}


void display_score(void)
{
   unsigned char buffer[16];

   buffer[0] = 0x14;
   buffer[1] = BRIGHT | INK_YELLOW | PAPER_BLACK;
   pad_numbers(buffer + 2, 4, high_score);

   sp1_SetPrintPos(&ps0, 20, 6);
   sp1_PrintString(&ps0, buffer);

   sp1_SetPrintPos(&ps0, 20, 10);
   sp1_PrintString(&ps0, "\x14\x47" "-> MUNCHKIN");

   buffer[0] = 0x14;
   buffer[1] = BRIGHT | INK_YELLOW | PAPER_BLACK;
   pad_numbers(buffer + 2, 4, score);

   sp1_SetPrintPos(&ps0, 20, 22);
   sp1_PrintString(&ps0, buffer);
}



void handle_munchkin(unsigned char munchkin_direction, unsigned char munchkin_manual_move)
{
  int cell_x, cell_y;           // integer, because cell_x == -1 when left of port

  cell_x = ( (munchkin_x_factor1) - (12 + MAZE_OFFSET_X) ) / (HORI_LINE_SIZE - 2);  
  cell_y = ( (munchkin_y_factor1) - ( 8 + MAZE_OFFSET_Y) ) / (VERT_LINE_SIZE - 2);  

  if (munchkin_manual_move == 1) {  // left
      switch (munchkin_direction) {

        case LEFT: 
          if (vertical_lines[cell_y].line[cell_x] == '|' && 
              ((munchkin_x_factor1 - speed) < ((MAZE_OFFSET_X + 12 + (cell_x) * (HORI_LINE_SIZE - 2))))
                                            // 7 munchkin offset in cel
              ) {
                   ; // continue left (not at center of cell yet)
           } else { 
              if ( (munchkin_last_direction == UP || munchkin_last_direction == DOWN) 
                   && 
                   // (MAZE_OFFSET_Y + MUNCHKIN_OFFSET_Y_) % (VERT_LINE_SIZE - 2) , so 32+8 % 18-2
                   ( (munchkin_y_factor1 - (40)) % (16) == 0)   ) {  // change direction only if on boundery
                   munchkin_x_factor1 = munchkin_x_factor1 - speed;
                   munchkin_auto_direction = LEFT;
                   munchkin_last_direction = LEFT;
               } else { 
                        if (munchkin_last_direction == LEFT || munchkin_last_direction == RIGHT
                            || munchkin_last_direction == 0  ) { //  if stationary and not on bounderary
                                                                 //  than movement allowed to the left 
                              munchkin_x_factor1 = munchkin_x_factor1 - speed;
                              munchkin_auto_direction = LEFT;
                              munchkin_last_direction = LEFT;
                          }
                      }   
           }
           // colour: left boundery:  16 - (26-2) + 12 = 16 - 24 + 12 = 4
           // colour: right boundery: 255 - (16 - (26-2) + 12 ) = 255 - 4 = 251
           if (munchkin_x_factor1 < 4) munchkin_x_factor1 = 252;  // wrap screen left     // -4 + 23 left: 196 + 23 right
        break;

        case RIGHT: 
          if (vertical_lines[cell_y].line[cell_x + 1] == '|' && 
              (munchkin_x_factor1 + speed) > ((MAZE_OFFSET_X + 12 + ((cell_x) * (HORI_LINE_SIZE - 2))))
                                         // 7 munchkin offset in cel
              ) { ;  // continue right (not at center of cell yet)
           } else {
              if ( (munchkin_last_direction == UP || munchkin_last_direction == DOWN) 
                   && 
                   ( (munchkin_y_factor1 - (40)) % (16) == 0)   ) {  // change direction only if on boundery
                   munchkin_x_factor1 = munchkin_x_factor1 + speed;
                   munchkin_auto_direction = RIGHT;
                   munchkin_last_direction = RIGHT;
               } else { ;
                        if (munchkin_last_direction == LEFT || munchkin_last_direction == RIGHT
                            || munchkin_last_direction == 0  ) { //  than movement allowed
                              munchkin_x_factor1 = munchkin_x_factor1 + speed;
                              munchkin_auto_direction = RIGHT;
                              munchkin_last_direction = RIGHT;
                          }
                      }   
           }
           if (munchkin_x_factor1 > 252) munchkin_x_factor1 = 4;  // wrap screen right    // -4 + 23 left: 196 + 23 right
        break;

        case UP:  
          if (horizontal_lines[cell_y].line[cell_x] == 'x' && 
              //(munchkin_y_factor1 - speed) < ((MAZE_OFFSET_Y + 4 + ((cell_y) * (VERT_LINE_SIZE - 2))))
              (munchkin_y_factor1 - speed) < ((MAZE_OFFSET_Y + 8 + ((cell_y) * (VERT_LINE_SIZE - 2))))
                                         // 8 munchkin offset in cel  
              ) {
                 ;
           } else { 
              //if ( (cell_x == -1 && cell_y == 4) || (cell_x == 9 && cell_y == 4) ) {
              if ( (munchkin_x_factor1 == 4 && cell_y == 4) || (cell_x == 9 && cell_y == 4) ) { // cell_x == -1 not for unsigned char
                 ;   // if munchkin outside maze (wrap via tunnel) do not allow UP
              } else {
                 munchkin_y_factor1 = munchkin_y_factor1 - speed;
                 munchkin_auto_direction = UP;
                 munchkin_last_direction = UP;
              }   

           }
        break;

        case DOWN: 
          if (horizontal_lines[cell_y + 1].line[cell_x] == 'x' && 
              (munchkin_y_factor1 + speed) > ((MAZE_OFFSET_Y + 8 + ((cell_y) * (VERT_LINE_SIZE - 2))))
                                         // 4 munchkin offset in cel
              ) {
                   ;  // continue down (not at center of cell yet)
           } else {
              
              //if ( (cell_x == -1 && cell_y == 4) || (cell_x == 9 && cell_y == 4) ) { // cell_x == -1 not for unsigned char
              if ( (munchkin_x_factor1 == 4 && cell_y == 4) || (cell_x == 9 && cell_y == 4) ) {
                 ;   // if munchkin outside maze (wrap via tunnel) do not allow DOWN
              } else {
                 munchkin_y_factor1 = munchkin_y_factor1 + speed;
                 munchkin_auto_direction = DOWN;
                 munchkin_last_direction = DOWN;
              } 
           }                  
        break;
      }   // switch
      }   // munchkin_manual_move == 1
 

  // auto direction
  // if munchkin at boundery of cell, stop auto movement
  //    only if automovement move munchkin
  if (munchkin_auto_direction != 0 && munchkin_manual_move == 0) {    // if no key pressed but auto move
    switch (munchkin_auto_direction) {
        case LEFT: 
         //if ( (munchkin_x_factor1 - (16)) % (20) == 0) munchkin_auto_direction = 0;
         //if ( (munchkin_x_factor1 - (39)) % (20) == 0) munchkin_auto_direction = 0;
             // now: (MAZE_OFFSET_X + MUNCHKIN_OFFSET_X_) % (HORI_LINE_SIZE - 2) , dus 16+12 % 26-2
             // 
         if ( (munchkin_x_factor1 - (28)) % (24) == 0) munchkin_auto_direction = 0;
          else { munchkin_x_factor1 = munchkin_x_factor1 - speed;
                 //if (munchkin_x_factor1 < -4) munchkin_x_factor1 = 196;    // wrap screen left
                 //if (munchkin_x_factor1 < 19) munchkin_x_factor1 = 219;    // wrap screen left
                 if (munchkin_x_factor1 < 4) munchkin_x_factor1 = 252;    // wrap screen left
                 munchkin_last_direction = LEFT;
               }  
        break;  
        case RIGHT: 
          //if ( (munchkin_x_factor1 - (80/5)) % (100/5) == 0) munchkin_auto_direction = 0;
          //if ( (munchkin_x_factor1 - (16)) % (20) == 0) munchkin_auto_direction = 0;
          if ( (munchkin_x_factor1 - (28)) % (24) == 0) munchkin_auto_direction = 0;
          else { munchkin_x_factor1 = munchkin_x_factor1 + speed;
                 //if (munchkin_x_factor1 > 980/5) munchkin_x_factor1 = -20/5;    // wrap screen right
                 if (munchkin_x_factor1 > 252) munchkin_x_factor1 = 4;    // wrap screen right
                 munchkin_last_direction = RIGHT;
               }  
        break;  
        case UP:
          //  vertical SDL 23, Z88DK 40 : so 17 more
          //if ( (munchkin_y_factor1 - (135/5)) % (70/5) == 0) munchkin_auto_direction = 0;
          //if ( (munchkin_y_factor1 - (27)) % (14) == 0) munchkin_auto_direction = 0;
          if ( (munchkin_y_factor1 - (40)) % (16) == 0) munchkin_auto_direction = 0;
          else { munchkin_y_factor1 = munchkin_y_factor1 - speed;
                 munchkin_last_direction = UP;
               }  
        break;  
        case DOWN: 
         //if ( (munchkin_y_factor1 - (135/5)) % (70/5) == 0) munchkin_auto_direction = 0;
         //if ( (munchkin_y_factor1 - (27)) % (14) == 0) munchkin_auto_direction = 0;
         if ( (munchkin_y_factor1 - (40)) % (16) == 0) munchkin_auto_direction = 0;
          else { munchkin_y_factor1 = munchkin_y_factor1 + speed;
                 munchkin_last_direction = DOWN;
               }  
        break;  
    }
  }   // if no key pressed but auto move
  
  // play "move" sound
  if (munchkin_manual_move != 0 || munchkin_auto_direction != 0) {
        if (active_pills != 1 && frame % 5 == 0) {
           playfx(FX_MOVE);
        } else { if (active_pills == 1 && frame % 3 == 0)   // increase "move" sound when last pill on screen
                 playfx(FX_MOVE);
               }
  }
}


void get_user_input(void)
{
    unsigned char munchkin_direction;   //1=left, 2=right, 3=up, 4=down
    unsigned char munchkin_manual_move; //0=no  1=yes

    munchkin_direction = 0;
    munchkin_manual_move = 0;

    key = (joyfunc)(&joy_k);

   /* Check continuous-response keys  */
   
   if (munchkin_dying == FALSE) {

       if (key & IN_STICK_LEFT && !(key & IN_STICK_RIGHT)) {
           if (munchkin_auto_direction == UP || munchkin_auto_direction == DOWN) {   
               ;  // // complete current auto move
           } else {
                    //if (munchkin_manual_move == 0) {   // is always 0 first if-statement
                        munchkin_direction = LEFT;
                        munchkin_manual_move = 1;
                     //}    
           }
       }
       if (key & IN_STICK_RIGHT && !(key & IN_STICK_LEFT)) { 
           if (munchkin_auto_direction == UP || munchkin_auto_direction == DOWN) {   
               ;  // // complete current auto move
           } else {
                    if (munchkin_manual_move == 0) {
                        munchkin_direction = RIGHT;
                        munchkin_manual_move = 1;
                     }   
           }    
       }
       if (key & IN_STICK_UP && !(key & IN_STICK_DOWN))  { 
           if (munchkin_auto_direction == LEFT || munchkin_auto_direction == RIGHT) {   
               ;  // // complete current auto move
           } else {
                   if (munchkin_manual_move == 0) {
                       munchkin_direction = UP;
                       munchkin_manual_move = 1;
                   }    
           }    
       }    
       if (key & IN_STICK_DOWN && !(key & IN_STICK_UP))  { 
           if (munchkin_auto_direction == LEFT || munchkin_auto_direction == RIGHT) {   
               ;  // // complete current auto move
           } else {
                   if (munchkin_manual_move == 0) {
                       munchkin_direction = DOWN;
                       munchkin_manual_move = 1;
                   }    
           }    
       }

       munchkin_last_direction = munchkin_direction;

       // when no key pressed and munchkin_auto_direction <> 0
       // move automatically in last direction

       if (maze_completed == FALSE) {   
              handle_munchkin(munchkin_direction, munchkin_manual_move);
              //if (munchkin_manual_move != 0 || munchkin_auto_direction != 0 ) play_sound(11, 1); 
              //      else play_sound(16, 6); 
       }

    } // not dying   
}


void run_play(void)
{
   //unsigned char buffer[16];

   sp1_ClearRectInv(&cr, INK_WHITE | PAPER_BLACK, 32, SP1_RFLAG_TILE | SP1_RFLAG_COLOUR);
   sp1_UpdateNow();

   start_new_game();

   display_score();
   draw_pills();  

   // set up munchkin sprite (keeps living in the game forever)
   munchkin_x_factor1 = (MAZE_OFFSET_X + 4 * (HORI_LINE_SIZE -2)) + 12;  // 112 + 12
   munchkin_y_factor1 = (MAZE_OFFSET_Y + 3 * (VERT_LINE_SIZE -2)) + 8;  // 80  +  8
   munchkin_sprite = sp1_CreateSpr(SP1_DRAW_MASK2LB, SP1_TYPE_2BYTE, 2, 0, 0);
   sp1_AddColSpr(munchkin_sprite, SP1_DRAW_MASK2RB, SP1_TYPE_2BYTE, 0, 0);
   
   ink_colour = INK_CYAN;
   sp1_IterateSprChar(munchkin_sprite, add_colour_to_sprite);
   sp1_MoveSprPix(munchkin_sprite, &cr, 0, munchkin_x_factor1, munchkin_y_factor1);


   // setup ghosts sprites
   
   for (i = 0; i < NUM_GHOSTS; i++)  {
       ghosts[i].sprite = sp1_CreateSpr(SP1_DRAW_MASK2LB, SP1_TYPE_2BYTE, 2, 0, 0);
       sp1_AddColSpr(ghosts[i].sprite, SP1_DRAW_MASK2RB, SP1_TYPE_2BYTE, 0, 0);
       get_ink_colour(ghosts[i].colour);
       sp1_IterateSprChar(ghosts[i].sprite, add_colour_to_sprite);
   } 
    

   /* ------------------
      - Main game loop -
      ------------------ */
   while(1)
   {
      //timer = tick;

      if (in_inkey() == 12) {   // backspace on PC keyboard
         hide_sprites();    // to clear all sprites
         break;             // exit current game
      }   
      
      /* restart_game after death */
      if (munchkin_dying == TRUE && munchkin_dying_animation == 15) {
         hide_sprites();    // to clear all sprites
         start_new_game();
      }  

      /* continue after completion maze */
      if (maze_completed == TRUE && maze_completed_animations == 0) {
              maze_completed = FALSE;   
              for (i = 0; i < 16; ++i) wait();   // pauze
              // hide all ghosts   
              for (i = 0; i < NUM_GHOSTS; i++) {
                 sp1_MoveSprAbs(ghosts[i].sprite, &cr, NULL, 0, 34, 0, 0);
                 get_ink_colour(ghosts[i].colour);  // set to original colour
                 sp1_IterateSprChar(ghosts[i].sprite, add_colour_to_sprite);
              } 
              sp1_UpdateNow();   
              // next maze
              if (maze_selected == 1) maze_selected = 2;
                 else maze_selected = 1;
              start_new_maze();
      }  

      get_user_input();  // also calls handle_munchkin();
      
      draw_munchkin();

      if (maze_completed == TRUE) handle_maze_completed();

      if (frame % 3 == 0 || active_pills == 1) check_pill_eaten();

      if (munchkin_dying == FALSE || munchkin_dying_animation == 0) handle_ghosts();

      /* draw ghosts if munchkin is not dead or munchkin is just dying */
      if (munchkin_dying == FALSE || munchkin_dying_animation <= 2) draw_ghosts();

      /* stop drawing ghost if munchkin is almost dead and dying animation == 3 */
      if (munchkin_dying == TRUE && munchkin_dying_animation == 3) {
           for (i = 0; i < NUM_GHOSTS; i++) 
              sp1_MoveSprAbs(ghosts[i].sprite, &cr, NULL, 0, 34, 0, 0); // hide ghosts
                                                                        // move to column 34
      }

      if (maze_completed == FALSE) check_ghosts_hits_munchkin();

      if (active_pills == 1) {   // last pill moves as fast as munchkin
         intrinsic_halt();  // to slow down frame rate a bit
         handle_pills();
         draw_pills();
      } else {   
         if (frame % 5 == 0) {  // handle pills every 5 frames
             handle_pills();
             draw_pills();
         }   
      }

      check_pills_mask();

      if (frame % 20 == 0 && maze_completed == FALSE) rotate_maze_center();    // rotate maze center

      frame++;

      intrinsic_halt();   // inline halt without impeding optimizer  
      sp1_UpdateNow();
   }  // main loop


   sp1_ClearRectInv(&cr, INK_BLACK | PAPER_BLACK, 32, SP1_RFLAG_TILE | SP1_RFLAG_COLOUR);
   sp1_UpdateNow();
}


void draw_munchkin(void)
{
  unsigned char image_num;  

 if (maze_completed == FALSE) {
      // determine which image to display
      switch (munchkin_last_direction) {
      case 0:
          image_num = 0;    // 2;     // stationary
          break;
      case LEFT:
          image_num = 1;    //3;     // left
          break;
      case RIGHT:
          image_num = 2;    //4;     // right
          break;
      case UP:
          image_num = 4;    //72;     // up
          break;
      case DOWN:
          image_num = 5;    //73;     // down
          break;
      }

      if (munchkin_last_direction != 0) {    // only if moving 
            if (munchkin_animation_frame == 0 || munchkin_animation_frame == 1 || munchkin_animation_frame == 2)
                 image_num = 3;  // 6;    // close image,  animation toggle 3 frames delay
            munchkin_animation_frame ++;
            if (munchkin_animation_frame == 6)  munchkin_animation_frame = 0;
      } 

   } else { // maze completed, animate munchkin   
            //printf("maze color %c\n", maze_color);
            ;
            if (maze_color == '3')
              image_num = 7;  // munchkin mouth open
            else 
              image_num = 6;  // munchkin mouth close
              
  }   // if maze_completed == FALSE
  


  if (munchkin_dying == TRUE) {
      // determine which image to display
      switch (munchkin_dying_animation) {
      case 2:
          image_num = 7; //76;     
          break;
      case 3:
          image_num = 6; //75;     
          break;
      case 4:
          image_num = 8; //143;     
          break;
      case 5:
          image_num = 9; //144;    
          break;
      case 6:
          image_num = 10; //145;  
          break;
      case 7:
          image_num = 11; //146;    
          break;
      case 8:
          image_num = 12; //147;    
          //printf("GAME OVER\n");
          //flash_high_score_timer = 55;  // +/- 5 seconds : flashing highscore name
          break;
      }
      if (munchkin_dying_animation > 8) {
         sp1_MoveSprAbs(munchkin_sprite, &cr, NULL, 0, 34, 0, 0); // hide 
      } else {
         sp1_MoveSprPix(munchkin_sprite, &cr, munchkin_sprite_graphic[image_num].graphic, munchkin_x_factor1, munchkin_y_factor1);
      }   
      
      if (frame % 10 == 0) {        // increase animation every x frames
        munchkin_dying_animation++;
        if (munchkin_dying_animation == 3)
            bit_beepfx_di(beepfx[5].effect); 
        if (munchkin_dying_animation == 6)
            bit_beepfx_di(beepfx[4].effect);  
      }
   }  // munchkin_dying

  if (munchkin_dying == FALSE)   
      sp1_MoveSprPix(munchkin_sprite, &cr, munchkin_sprite_graphic[image_num].graphic, munchkin_x_factor1, munchkin_y_factor1);
}


void setup_ghosts(void)
{
  /* start position in center cell (4,4)  offset like munchkin */

  // ghost_x = (89 + 7) * factor;  // 9 + 4*20 + 7 = (MAZE_OFFSET_X + 4 * (HORI_LINE_SIZE -2) + 12) * factor
  // ghost_y = (79 + 4) * factor;  //23 + 4*14 + 4 = (MAZE_OFFSET_Y + 4 * (VERT_LINE_SIZE -2) + 8) * factor

  for (i = 0; i < NUM_GHOSTS; i++)  {
       ghosts[i].colour = (i % 4) + 1;
       ghosts[i].status = 1;
       ghosts[i].recharge_timer = 0;
       ghosts[i].x = (MAZE_OFFSET_X + 4 * (HORI_LINE_SIZE -2) + 12);
       ghosts[i].y = (MAZE_OFFSET_Y + 4 * (VERT_LINE_SIZE -2) + 8);
       //ghosts[i].x = (MAZE_OFFSET_X + ((i) * (HORI_LINE_SIZE - 2 )) + 12);
       //ghosts[i].y = (MAZE_OFFSET_Y + ((i) * (VERT_LINE_SIZE - 2 )) + 8);

       ghosts[i].direction = DOWN;
       ghosts[i].speed = speed;  // same speed as munchkin
       ghosts[i].gulp_sound_delay = 0;

   }   
}


void draw_ghosts(void)
{
  unsigned char image_num;

  for (i = 0; i < NUM_GHOSTS; i++)  {
     switch (ghosts[i].status) {
     case  1:  // normal
        image_num = 0;
        break;
     case  2:  // can be eaten
        image_num = 0;
        break;
     case 3:   // dead ,invisible
        image_num = 2;
        break;
     case 5:   // recharging
        image_num = 2;
        break;
     }
     // flash ghost if powerpill almost not active anymore
     if (ghosts[i].status == 2 && powerpill_active_timer > 0 && powerpill_active_timer < 20) {
             if (powerpill_active_timer % 4 == 0) {
               image_num = 0;
             } else {  
               image_num = 1;
             } 
     } else {     
         if (ghosts[i].status == 3 || ghosts[i].status == 4 ) { // dead or recharging
            if (frame % 16 >= 0 && frame % 16 <=2) {  // 3 frames normal and 9 frames invisible
                image_num = 0;
            } else {
               image_num = 1;
            }    
         }   
     }
     sp1_MoveSprPix(ghosts[i].sprite, &cr, 
                    ghost_sprite_graphic[image_num].graphic,
                    ghosts[i].x, ghosts[i].y);
   }   
}


void handle_ghosts(void)
{
  if (powerpill_active_timer > 0) powerpill_active_timer --;

  if (powerpill_active_timer == 0) {   // timer completed, put ghosts to active
        for (i = 0; i < NUM_GHOSTS && maze_completed == FALSE; i++) {
             if (ghosts[i].status == 2) {   // can be eaten 
                // change to original colour

             get_ink_colour(ghosts[i].colour);
             sp1_IterateSprChar(ghosts[i].sprite, add_colour_to_sprite);
             ghosts[i].status = 1;
             }     
        }  // for loop
  }

  for (i = 0; i < NUM_GHOSTS && maze_completed == FALSE; i++) {

             if (ghosts[i].gulp_sound_delay > 0) {   // post gulp sound
               if (ghosts[i].gulp_sound_delay == 1) bit_beepfx_di(beepfx[2].effect);
               ghosts[i].gulp_sound_delay--;
             } else {

             choose_ghost_direction(i);

             switch (ghosts[i].direction) {
               case LEFT:   
                    ghosts[i].x = ghosts[i].x - ghosts[i].speed;
                    //if (ghosts[i].x < -2) ghosts[i].x = 194;  // wrap screen left
                    //if (ghosts[i].x < 21) ghosts[i].x = 217;  // wrap screen left                    
                    if (ghosts[i].x < 8) ghosts[i].x = 242;  // wrap screen left                    
                  break;
               case RIGHT:   
                    ghosts[i].x = ghosts[i].x + ghosts[i].speed;
                    //if (ghosts[i].x > 194) ghosts[i].x = -2;  // wrap screen right
                    //if (ghosts[i].x > 217) ghosts[i].x = 21;  // wrap screen right
                    if (ghosts[i].x > 242) ghosts[i].x = 8;  // wrap screen right
                  break;
               case UP:   
                    ghosts[i].y = ghosts[i].y - ghosts[i].speed;
                  break;
               case DOWN:    
                    ghosts[i].y = ghosts[i].y + ghosts[i].speed;
                   break;
              }   // end switch
           }      //ghosts[i].gulp_sound_delay > 0)
    }             // for loop
}


void choose_ghost_direction (unsigned char i)
{
  unsigned char cell_nr_x, cell_nr_y, cell_x_ghost, cell_y_ghost;
  unsigned char left_open, right_open, up_open, down_open;     //1=open, 0=closed
  unsigned char direction_to_center_set;
  unsigned char found;

  // cell_nr_x = ( (ghosts[i].x) - (12 + MAZE_OFFSET_X) ) / (HORI_LINE_SIZE - 2);  
  // cell_nr_y = ( (ghosts[i].y) - (8 + MAZE_OFFSET_Y) ) / (VERT_LINE_SIZE - 2);  
  // calculated before to speed up things
  cell_nr_x = ( (ghosts[i].x) - (28) ) / (24);  // truncated to nearest cell
  cell_nr_y = ( (ghosts[i].y) - (40) ) / (16);  // truncated to nearest cell


  //cell_x_ghost = (16 + 12 + cell_nr_x * 24);  // MAZE_OFFSET_X + 12 + cell_nr_x * (HORI_LINE_SIZE - 2)
  //cell_y_ghost = (32 +  8 + cell_nr_y * 16);  // MAZE_OFFSET_Y +  8 + cell_nr_y * (VERT_LINE_SIZE - 2)
  // calculated before to speed up things
  cell_x_ghost = (28 + cell_nr_x * 24);  // MAZE_OFFSET_X + 12 + cell_nr_x * (HORI_LINE_SIZE - 2)
  cell_y_ghost = (40 + cell_nr_y * 16);  // MAZE_OFFSET_Y +  8 + cell_nr_y * (VERT_LINE_SIZE - 2)


  direction_to_center_set = FALSE;     // for ghosts with status 3, going to center

  if (cell_x_ghost == ghosts[i].x && cell_y_ghost == ghosts[i].y) { // ghost exactly in middle of cell
     
     // determine available directions
     if (vertical_lines[cell_nr_y].line[cell_nr_x] == '|')       left_open  = 0; else left_open = 1;
     if (vertical_lines[cell_nr_y].line[cell_nr_x + 1] == '|')   right_open = 0; else right_open = 1;
     if (horizontal_lines[cell_nr_y].line[cell_nr_x] == 'x')     up_open = 0;    else up_open = 1;
     if (horizontal_lines[cell_nr_y + 1].line[cell_nr_x] == 'x') down_open = 0;  else down_open = 1;

     if (ghosts[i].status == 3) {  // eaten, looking for center
         if (cell_nr_x == 4 && cell_nr_y == 4) {
             //printf("Ghost %d reached center, going to recharge\n",i);
             ghosts[i].status = 4;
             ghosts[i].direction = 0;
             ghosts[i].recharge_timer = 150; 

           } else {  // try to move into center if nearby

             /* check movement to center when at cell (3,4) */
             if (cell_nr_x == 3 && cell_nr_y == 4 && right_open == 1) {
                ghosts[i].direction = RIGHT;
                direction_to_center_set = TRUE;
             }
             /* stay around cell (3,4) if center was not open */
             if (cell_nr_x == 3 && cell_nr_y == 4 && direction_to_center_set == FALSE) {
                if (ghosts[i].direction == UP && up_open == 1)     ghosts[i].direction = UP;    // continue up
                else if (ghosts[i].direction == UP && up_open == 0 && left_open == 1)   ghosts[i].direction = LEFT;   // NEW
                else if (ghosts[i].direction == UP && up_open == 0 && down_open == 1)   ghosts[i].direction = DOWN;   // NEW
                else if (ghosts[i].direction == DOWN && down_open == 1) ghosts[i].direction = DOWN;  // continue down
                else if (ghosts[i].direction == DOWN && down_open == 0 && left_open == 1) ghosts[i].direction = LEFT;  // NEW
                else if (ghosts[i].direction == DOWN && down_open == 0 && left_open == 0) ghosts[i].direction = UP;  // NEW
                else if (ghosts[i].direction == RIGHT && up_open == 1)  ghosts[i].direction = UP; 
                else if (ghosts[i].direction == RIGHT && up_open == 0 && down_open == 1) 
                      ghosts[i].direction = DOWN; 
                else if (ghosts[i].direction == RIGHT && up_open == 0 && down_open == 0) 
                      ghosts[i].direction = LEFT;  // go back

                direction_to_center_set = TRUE;
             }

             /* check movement to center when at cell (5,4) */
             /* (a bit ugly, sorry) */

             if (cell_nr_x == 5 && cell_nr_y == 4 && left_open == 1) {
              ghosts[i].direction = LEFT;
              direction_to_center_set = TRUE;
             }
             /* stay around cell (5,4) if center was not open */
             if (cell_nr_x == 5 && cell_nr_y == 4 && direction_to_center_set == FALSE) {
                if (ghosts[i].direction == UP && up_open == 1)     ghosts[i].direction = UP;    // continue up
                else if (ghosts[i].direction == UP && up_open == 0 && right_open == 1)     ghosts[i].direction = RIGHT;    // NEW
                else if (ghosts[i].direction == UP && up_open == 0 && right_open == 0)     ghosts[i].direction = DOWN;    // NEW
                else if (ghosts[i].direction == DOWN && down_open == 1) ghosts[i].direction = DOWN;  // continue down
                else if (ghosts[i].direction == DOWN && down_open == 0 && right_open == 1) ghosts[i].direction = RIGHT;  // NEW
                else if (ghosts[i].direction == DOWN && down_open == 0 && right_open == 0) ghosts[i].direction = UP;  // NEW
                else if (ghosts[i].direction == LEFT && up_open == 1)   ghosts[i].direction = UP; 
                else if (ghosts[i].direction == LEFT && up_open == 0 && down_open == 1) 
                      ghosts[i].direction = DOWN; 
                else if (ghosts[i].direction == LEFT && up_open == 0 && down_open == 0) 
                      ghosts[i].direction = RIGHT;  // go back

                direction_to_center_set = TRUE;
             }

             /* check movement to center when at cell (4,3) */
             if (cell_nr_x == 4 && cell_nr_y == 3 && down_open == 1) {
              ghosts[i].direction = DOWN;
              direction_to_center_set = TRUE;
             }
             /* stay around cell (4,3) if center was not open */
             if (cell_nr_x == 4 && cell_nr_y == 3 && direction_to_center_set == FALSE) {
                if (ghosts[i].direction == LEFT && left_open == 1)   ghosts[i].direction = LEFT;   // continue left
                else if (ghosts[i].direction == LEFT && left_open == 0 && up_open == 1)   ghosts[i].direction = UP;   // NEW
                else if (ghosts[i].direction == LEFT && left_open == 0 && up_open == 0)   ghosts[i].direction = RIGHT;   // NEW
                else if (ghosts[i].direction == RIGHT && right_open == 1) ghosts[i].direction = RIGHT;  // continue right
                else if (ghosts[i].direction == RIGHT && right_open == 0 && up_open == 1) ghosts[i].direction = UP;  // NEW
                else if (ghosts[i].direction == RIGHT && right_open == 0 && up_open == 0) ghosts[i].direction = LEFT;  // NEW
                else if (ghosts[i].direction == DOWN && left_open == 1)   ghosts[i].direction = LEFT; 
                else if (ghosts[i].direction == DOWN && left_open == 0 && right_open == 1) 
                      ghosts[i].direction = RIGHT; 
                else if (ghosts[i].direction == DOWN && left_open == 0 && right_open == 0) 
                      ghosts[i].direction = UP;  // go back

                direction_to_center_set = TRUE;
             }

             /* check movement to center when at cell (4,5) */
             if (cell_nr_x == 4 && cell_nr_y == 5 && up_open == 1) {
              ghosts[i].direction = UP;
              direction_to_center_set = TRUE;
             }
             /* stay around cell (4,5) if center was not open */
             if (cell_nr_x == 4 && cell_nr_y == 5 && direction_to_center_set == FALSE) {
                if (ghosts[i].direction == LEFT && left_open == 1)   ghosts[i].direction = LEFT;   // continue left
                else if (ghosts[i].direction == LEFT && left_open == 0 && down_open == 1)   ghosts[i].direction = DOWN;   // NEW
                else if (ghosts[i].direction == LEFT && left_open == 0 && down_open == 0)   ghosts[i].direction = RIGHT;   // NEW                
                else if (ghosts[i].direction == RIGHT && right_open == 1) ghosts[i].direction = RIGHT;  // continue right
                else if (ghosts[i].direction == RIGHT && right_open == 0 && down_open == 1) ghosts[i].direction = DOWN;  // NEW
                else if (ghosts[i].direction == RIGHT && right_open == 0 && down_open == 0) ghosts[i].direction = LEFT;  // NEW
                else if (ghosts[i].direction == UP && left_open == 1)     ghosts[i].direction = LEFT; 
                else if (ghosts[i].direction == UP && left_open == 0 && right_open == 1) 
                      ghosts[i].direction = RIGHT; 
                else if (ghosts[i].direction == UP && left_open == 0 && right_open == 0) 
                      ghosts[i].direction = DOWN;  // go back
                direction_to_center_set = TRUE;
             }
           }
     } 

     if (ghosts[i].status == 4) {  // recharging
             ghosts[i].recharge_timer --;
             if (ghosts[i].recharge_timer == -1) {
                 //printf("Ghost %d recharged, become normal\n",i);
                 get_ink_colour(ghosts[i].colour);
                 sp1_IterateSprChar(ghosts[i].sprite, add_colour_to_sprite);

                 ghosts[i].status = 1;
                 ghosts[i].recharge_timer = 0;
                 ghosts[i].direction = DOWN;    // but others directions are possible later on
             }
      }      

     if (direction_to_center_set == FALSE) {   // direction not already set for status = 3

          switch (ghosts[i].direction) {

          case LEFT:  
             // continue left (50% chance, else go up or down)
             //    if not, go right back (return)
             if (left_open == 1 && (up_open == 1 || down_open == 1) && ((rand()%10 >= 5) )) {
                ghosts[i].direction = LEFT;
             } else {
                      if (up_open == 1 || down_open == 1 ) {
                         found = 0;
                         while (found == 0) {  // go up or down, if possible
                             if (rand()%2 == 0 && up_open == 1) {
                                 found = 1;
                                 ghosts[i].direction = UP;
                             } else { 
                                      if (down_open == 1) {
                                          found = 1;      
                                          ghosts[i].direction = DOWN;
                                      }
                             }
                         }  // end while
                       } else { 
                               if (left_open == 1) {
                                   ghosts[i].direction = LEFT;
                               } else {    
                                   ghosts[i].direction = RIGHT;
                               } 
                       }
             }                 
           break;

         case RIGHT: 
             // continue right (50% chance, else go up or down)
             //    if not, go left back (return)
             if (right_open == 1 && (up_open == 1 || down_open == 1) && ((rand()%10 >= 5) )) {
                ghosts[i].direction = RIGHT;
             } else {
                      if (up_open == 1 || down_open == 1 ) {
                         found = 0;
                         while (found == 0) {  // go up or down, if possible
                             if (rand()%2 == 0 && up_open == 1) {
                                 found = 1;
                                 ghosts[i].direction = UP;
                             } else { 
                                      if (down_open == 1) {
                                          found = 1;      
                                          ghosts[i].direction = DOWN;
                                      }
                             }
                         }  // end while
                       } else { 
                               if (right_open == 1) {
                                   ghosts[i].direction = RIGHT;
                               } else {    
                                   ghosts[i].direction = LEFT;
                               } 
                       }
             }                 
           break;

         case UP:
             // continue up (50% chance, else go left or right)
             //    if not, go right down (return)
             if (up_open == 1 && (left_open == 1 || right_open == 1) && ((rand()%10 >= 5) )) {
                ghosts[i].direction = UP;
             } else {
                      if (left_open == 1 || right_open == 1 ) {
                         found = 0;
                         while (found == 0) {  // go left or right, if possible
                             if (rand()%2 == 0 && left_open == 1) {
                                 found = 1;
                                 ghosts[i].direction = LEFT;
                             } else { 
                                      if (right_open == 1) {
                                          found = 1;      
                                          ghosts[i].direction = RIGHT;
                                      }
                             }
                         }  // end while
                       } else { 
                               if (up_open == 1) {
                                   ghosts[i].direction = UP;
                               } else {    
                                   ghosts[i].direction = DOWN;
                               } 
                       }
             }                 
           break;

         case DOWN: 
             // continue down (50% chance, else go left or right)
             //    if not, go right up (return)
             if (down_open == 1 && (left_open == 1 || right_open == 1) && ((rand()%10 >= 5) )) {
                ghosts[i].direction = DOWN;
             } else {
                      if (left_open == 1 || right_open == 1 ) {
                         found = 0;
                         while (found == 0) {  // go left or right, if possible
                             if (rand()%2 == 0 && left_open == 1) {
                                 found = 1;
                                 ghosts[i].direction = LEFT;
                             } else { 
                                      if (right_open == 1) {
                                          found = 1;      
                                          ghosts[i].direction = RIGHT;
                                      }
                             }
                         }  // end while
                       } else { 
                               if (down_open == 1) {
                                   ghosts[i].direction = DOWN;
                               } else {    
                                   ghosts[i].direction = UP;
                               } 
                       }
             }                 
           break;
         }   // end switch( direction )
     }       // end directiion_to_center_set     
  }          // if middle of cell
}


void check_ghosts_hits_munchkin(void)
{
  unsigned char a_x, a_y, a_xr, a_yb;   // top-left and bottom-right coordinates of ghost
  
  if (munchkin_dying == FALSE) {
     /* check if munchkin collides with a ghost while ghosts is active or can be eaten */
     
     /* loop active ghosts */
     for (i = 0; i < NUM_GHOSTS; i++)
     {
       if (ghosts[i].status == 1 || ghosts[i].status == 2) {
          a_xr = (ghosts[i].x + 8) ;   // width  factor pixel
          a_yb = (ghosts[i].y + 8) ;   // height factor pixel
          a_x  = ghosts[i].x;
          a_y  = ghosts[i].y;

          if ( munchkin_x_factor1 + 6          > a_x   &&
               munchkin_x_factor1 + 2          < a_xr  &&
               munchkin_y_factor1 + 6          > a_y   &&
               munchkin_y_factor1 + 2          < a_yb) {

               if (ghosts[i].status == 1) {
                     //printf("%d - DEADLY COLLISION!\n", frame);
                     munchkin_dying = TRUE;
                     munchkin_dying_animation = 1;
               } else {   // ghost has status 2 and can be eaten
                      // change color to white
                     ink_colour = INK_WHITE | PAPER_BLACK;      
                     sp1_IterateSprChar(ghosts[i].sprite, add_colour_to_sprite);

                     ghosts[i].status = 3;
                     score = score + 10;
                     ghosts[i].gulp_sound_delay = 5;  // postpone sound a little
                     if (score > high_score) {
                         high_score = score;
                     }
                     display_score();    
               }  
          }
     } // end status = 1 or 2
    }  // end loop active ghosts
  }    // munchkin_dying = FALSE
   ;
}


void setup_pills(void)
{
  // delete any inactive pill-sprites (e.g. when level not completed)
  // (pill status initialized in setup()
  for (i = 0; i < NUM_PILLS; i++) {
    if (pills[i].status != 0) {
          sp1_MoveSprAbs(pills[i].sprite, &cr, NULL, 0, 34, 0, 0);  // remove from screen
                                                                    // print at column 34
          sp1_DeleteSpr(pills[i].sprite);
          pills[i].status = 0;   
     }      
  }

  // top-left
  if (NUM_PILLS >= 1) {
     pills[0].x = (MAZE_OFFSET_X  + 12       );   // 12 + 0*24
     pills[0].y = (MAZE_OFFSET_Y + 8       );     //  8 + 0*16
     pills[0].direction = 4;                
     pills[0].status = 2;        // powerpill
  }   
  if (NUM_PILLS >= 2) {
     pills[1].x = (MAZE_OFFSET_X  + 36       );   // 12 + 1*24
     pills[1].y = (MAZE_OFFSET_Y + 8       );     //   8 + 0*16
     pills[1].direction = 1;                
     pills[1].status = 1;     
  }   
  if (NUM_PILLS >= 3) {
     pills[2].x = (MAZE_OFFSET_X  + 12       );   // etc
     pills[2].y = (MAZE_OFFSET_Y + 24      );
     pills[2].direction = 2;                
     pills[2].status = 1;     
  }   
  // top-right
  if (NUM_PILLS >= 4) {
     pills[3].x = (MAZE_OFFSET_X  + 180      );
     pills[3].y = (MAZE_OFFSET_Y + 8       );
     pills[3].direction = 1;                
     pills[3].status = 1;     
  }   
  if (NUM_PILLS >= 5) {
     pills[4].x = (MAZE_OFFSET_X  + 204      );
     pills[4].y = (MAZE_OFFSET_Y + 8       );
     pills[4].direction = 4;                
     pills[4].status = 2;        // powerpill
  }   
  if (NUM_PILLS >= 6) {
     pills[5].x = (MAZE_OFFSET_X  + 204  );
     pills[5].y = (MAZE_OFFSET_Y + 24      );
     pills[5].direction = 1;                
     pills[5].status = 1;     
  }   
  // bottom-left
  if (NUM_PILLS >= 7) {
     pills[6].x = (MAZE_OFFSET_X  + 12       );
     pills[6].y = (MAZE_OFFSET_Y + 88      );
     pills[6].direction = 2;                
     pills[6].status = 1;     
  }   
  if (NUM_PILLS >= 8) {
     pills[7].x = (MAZE_OFFSET_X  + 12       );
     pills[7].y = (MAZE_OFFSET_Y + 104    );
     pills[7].direction = 3;                
     pills[7].status = 2;       // powerpill
  }   
  if (NUM_PILLS >= 9) {
     pills[8].x = (MAZE_OFFSET_X  + 36       );
     pills[8].y = (MAZE_OFFSET_Y + 104     );
     pills[8].direction = 2;                
     pills[8].status = 1;     
  }   
  // bottom-right
  if (NUM_PILLS >= 10) {
     pills[9].x = (MAZE_OFFSET_X  + 204      );
     pills[9].y = (MAZE_OFFSET_Y + 88      );
     pills[9].direction = 1;                
     pills[9].status = 1;     
  }   
  if (NUM_PILLS >= 11) {
     pills[10].x = (MAZE_OFFSET_X  + 180      );
     pills[10].y = (MAZE_OFFSET_Y + 104     );
     pills[10].direction = 1;                
     pills[10].status = 1;     
  }   
  if (NUM_PILLS >= 12) {
     pills[11].x = (MAZE_OFFSET_X  + 204      );
     pills[11].y = (MAZE_OFFSET_Y + 104    );
     pills[11].direction = 3;                
     pills[11].status = 2;       // powerpill
  }   

  if (NUM_PILLS >= 13) {  // spread the rest of the pills random across to entire maze 
    for (i = 12; i < NUM_PILLS; i++) {
      pills[i].x = (MAZE_OFFSET_X  + 7 + ( rand()%8 ) *24);  //random cell x between 0 and 8
      pills[i].y = (MAZE_OFFSET_Y + 4 + ( rand()%6 ) *16);  //random cell x between 0 and 8
      pills[i].status = 1; 
      pills[i].direction = 2;    // must have value for choose_pill_direction
      choose_pill_direction(i);
    }
  }
  
  for (i = 0; i < NUM_PILLS; i++) {
      pills[i].speed = 1;   // initial speed
      pills[i].sprite = sp1_CreateSpr(SP1_DRAW_MASK2LB, SP1_TYPE_2BYTE, 2, 0, i+10);
                                                                            // ^ on lower plane
      sp1_AddColSpr(pills[i].sprite, SP1_DRAW_MASK2RB, SP1_TYPE_2BYTE, 0, 0);
      
      ink_colour = INK_WHITE | PAPER_BLACK;
      sp1_IterateSprChar(pills[i].sprite, add_colour_to_sprite);
      pills[i].colour_masked = FALSE;   // normal white
  }
}


void draw_pills(void)
{
  for (i = 0; i < NUM_PILLS; i++)
  {
    if (pills[i].status != 0) {
   
      if (pills[i].status == 1) {
                sp1_MoveSprPix(pills[i].sprite, &cr, pill_sprite_graphic[0].graphic, pills[i].x, pills[i].y);
      } else {
           if (tick % 4 == 0 ) {   // flash pill (tick in stead of frame)
              sp1_MoveSprPix(pills[i].sprite, &cr, pill_sprite_graphic[1].graphic, pills[i].x, pills[i].y);
           } else {
           sp1_MoveSprPix(pills[i].sprite, &cr, pill_sprite_graphic[0].graphic, pills[i].x, pills[i].y);
           }
      }
    }   // if pill alive
   }     // for loop

}


void check_pill_eaten(void)
{
  unsigned char a_x, a_y, a_xr, a_yb;       // top-left and bottom-right pill
  unsigned char b_x, b_y, b_xr, b_yb;       // top-left and bottom-right munchkin
  unsigned char active_pills;

  // make munchkin dection area smaller to give the impression that 
  // the pill is really eaten (ie pill detecten in center of munchkin)
  b_xr = (munchkin_x_factor1) + 4; // width factor pixel
  b_yb = (munchkin_y_factor1) + 4; // height factor pixel
  b_x  = (munchkin_x_factor1) + 2;
  b_y  = (munchkin_y_factor1) + 2;

  for (i = 0; i < NUM_PILLS && maze_completed == FALSE && munchkin_dying == FALSE; i++) {
 
     if (pills[i].status != 0) {   // active

        a_xr = (pills[i].x + 7);   // width  factor pixel
        a_yb = (pills[i].y + 7);   // height factor pixel
        a_x  = pills[i].x + 1;  // pill sprite is bigger than SDL version
        a_y  = pills[i].y + 2;
 
        /* check overlap munchkin with pill  */
        if (b_xr  > a_x   &&
            b_x   < a_xr  &&
            b_yb  > a_y   &&
            b_y   < a_yb) {
              //printf("Pill %d eaten !\n", i);
              /*
               if (munchkin_dying == FALSE) {
                 if (pills[i].status == 1) play_sound(12,2);
                     else play_sound(14,4);
               } 
               */   

               /* increase score and change ghost status if powerpill */
               if (pills[i].status == 1) { 
                    score++;
                    display_score();
                    bit_beepfx_di(beepfx[0].effect);
               }
               if (pills[i].status == 2) {  // powerpill
                    score = score + 3;   
                    display_score();
                    bit_beepfx_di(beepfx[3].effect);
                    for (j = 0; j < NUM_GHOSTS; j++) {  // loop active ghosts
                       if (ghosts[j].status == 1 || ghosts[j].status == 2) {   // can still be 2
                            // change to magenta
                            if (ghosts[j].status == 1) {
                              // ghosts[j].colour = 5;  // do not change, keep original colour
                              ink_colour = INK_MAGENTA | PAPER_BLACK;      
                              sp1_IterateSprChar(ghosts[j].sprite, add_colour_to_sprite);
                            }  
                            ghosts[j].status = 2;       // ghost can be eaten now
                            powerpill_active_timer = 90;
                       }
                    }
                }     

               if (score > high_score) {
                    high_score = score;
                    display_score();
               } 

               // de-activate pill    
               pills[i].status = 0;   
               sp1_MoveSprAbs(pills[i].sprite, &cr, NULL, 0, 34, 0, 0);  // remove from screen
                                                                       // print at column 34
               sp1_DeleteSpr(pills[i].sprite);

          }       // check overlap
      }           // pills[i].status != 0  active
   }              // for loop

  if (maze_completed == FALSE) {
    active_pills = 0;
    for (i = 0; i < NUM_PILLS; i++) {
           if (pills[i].status != 0) {
               active_pills++;
               //printf("Active pills: %d\n", active_pills);
           }  
     }      
     if (active_pills == 0) {
       //printf("Maze completed\n");
       maze_completed = TRUE;
       maze_completed_animations = 20;  // +/-  2 seconds
       bit_beepfx_di(beepfx[7].effect);
       //play_sound(13, 3);
     }
   }    // maze_completed == FALSE
}


void handle_pills(void)
{
  unsigned char last_active_pill;    // nr of the last found active pill
  int cell_nr_x, cell_nr_y, cell_x_pill, cell_y_pill;
  
  for (i = 0; i < NUM_PILLS && maze_completed == FALSE; i++) {

     if (pills[i].status != 0) {   // active

             choose_pill_direction(i);

             switch (pills[i].direction) {
               case LEFT:    
                    pills[i].x = pills[i].x - pills[i].speed;
                    //if ( ((pills[i].x - screen_offset_x) / factor) < (MAZE_OFFSET_X - 7)) pills[i].x = (187 + MAZE_OFFSET_X) * factor + screen_offset_x ;  // wrap screen left
                    if (pills[i].x < 8) pills[i].x = 242;  // wrap screen left                    
                  break;
               case RIGHT:    
                    pills[i].x = pills[i].x + pills[i].speed;
                    //if ( ((pills[i].x  - screen_offset_x) / factor ) > (187 + MAZE_OFFSET_X) ) pills[i].x = (MAZE_OFFSET_X - 7) * factor + screen_offset_x;  // wrap screen right
                    if (pills[i].x > 242) pills[i].x = 8;  // wrap screen right
                  break;
               case UP:   
                    pills[i].y = pills[i].y - pills[i].speed;
                  break;
               case DOWN:   
                    pills[i].y = pills[i].y + pills[i].speed;
                   break;
              }   // end switch


      }           // pills[i].status != 0  active
  }               // for loop

  active_pills = 0;
  for (i = 0; i < NUM_PILLS; i++) {
         if (pills[i].status != 0) {
             active_pills++;
             last_active_pill = i;
             //printf("Active pills: %d\n", active_pills);
          }   
  }        
      
  // increase speed of last pill to speed of munchkin 
  if (active_pills == 1 && last_pill_speed_increased == FALSE) {  // increase speed only once

     cell_nr_x = ( (pills[last_active_pill].x) - (12 + MAZE_OFFSET_X) ) / (HORI_LINE_SIZE - 2);  
     cell_nr_y = ( (pills[last_active_pill].y) - (8 + MAZE_OFFSET_Y) ) / (VERT_LINE_SIZE - 2);  
  
     cell_x_pill = (16 + 12 + cell_nr_x * 24);
     cell_y_pill = (32 +  8 + cell_nr_y * 16);
   
     if (cell_x_pill == pills[last_active_pill].x 
         && 
         cell_y_pill == pills[last_active_pill].y) { // last pill exactly in middle cell

           pills[last_active_pill].speed = 2;
           last_pill_speed_increased = TRUE;
     }     
  }
}


void check_pills_mask(void)
{
  unsigned char match_found;
  match_found = FALSE;
  
  /* To minimize colour clash between pills and ghosts, a pill is temporarly hidden
     when it overlaps with a ghost */

  for (i = 0; i < NUM_PILLS && maze_completed == FALSE; i++) {

     if (pills[i].status != 0) {  // check only active pills

       match_found = FALSE;
       for (j = 0; j < NUM_GHOSTS && match_found == FALSE ; j++) {

          if (ghosts[j].status == 1 || ghosts[j].status == 2) {

            // check overlap ghost and pill
            if ( pills[i].x + 6          > ghosts[j].x - 2       &&
                 pills[i].x             < (ghosts[j].x + 6)      &&
                 pills[i].y + 8          > ghosts[j].y - 2       &&
                 pills[i].y - 2          < (ghosts[j].y + 8) ) {

              match_found = TRUE;

              if (pills[i].colour_masked == FALSE) {  // change colour of pill to colour of ghost to minimize colour clash
                                                      // (if already changed, skip colour change)
                if (ghosts[j].status == 1 ) get_ink_colour(ghosts[j].colour);
                   else ink_colour = INK_MAGENTA | PAPER_BLACK;
                sp1_IterateSprChar(pills[i].sprite, add_colour_to_sprite);
                pills[i].colour_masked = TRUE;
              }
            }  
       } // ghost.status 1 || 2
     } // loop ghosts

     if (match_found == FALSE && pills[i].colour_masked == TRUE) {     // back to normal white colour (if not white)
           ink_colour = INK_WHITE | PAPER_BLACK;
           sp1_IterateSprChar(pills[i].sprite, add_colour_to_sprite);
           pills[i].colour_masked = FALSE;
     }

    }  // pill.status != 0 
  }    // loop pills   

}


void choose_pill_direction (unsigned char i)
{
  int cell_nr_x, cell_nr_y, cell_x_pill, cell_y_pill;
  unsigned char left_open, right_open, up_open, down_open;     //1=open, 0=closed
  unsigned char found;

  
  //cell_nr_x = ( ((pills[i].x - screen_offset_x) / factor) - (7 + MAZE_OFFSET_X) ) / (HORI_LINE_SIZE - 2);  
  //cell_nr_y = ( ((pills[i].y - screen_offset_y) / factor) - (4 + MAZE_OFFSET_Y) ) / (VERT_LINE_SIZE - 2);  
  cell_nr_x = ( ((pills[i].x)) - (12 + MAZE_OFFSET_X) ) / (HORI_LINE_SIZE - 2);  
  cell_nr_y = ( ((pills[i].y)) - ( 8 + MAZE_OFFSET_Y) ) / (VERT_LINE_SIZE - 2);    

  //cell_x_pill = (MAZE_OFFSET_X  + 7 + cell_nr_x * 20) * factor + screen_offset_x;
  //cell_y_pill = (MAZE_OFFSET_Y  + 4 + cell_nr_y * 14) * factor + screen_offset_y;
  cell_x_pill = (MAZE_OFFSET_X  + 12 + cell_nr_x * 24);
  cell_y_pill = (MAZE_OFFSET_Y  +  8 + cell_nr_y * 16);

 
  if (cell_x_pill == pills[i].x && cell_y_pill == pills[i].y) { // pill exactly in middle of cell
 
     // determine available directions
     if (vertical_lines[cell_nr_y].line[cell_nr_x] == '|')       left_open  = 0; else left_open = 1;
     if (vertical_lines[cell_nr_y].line[cell_nr_x + 1] == '|')   right_open = 0; else right_open = 1;
     if (horizontal_lines[cell_nr_y].line[cell_nr_x] == 'x')     up_open = 0;    else up_open = 1;
     if (horizontal_lines[cell_nr_y + 1].line[cell_nr_x] == 'x') down_open = 0;  else down_open = 1;

     // do not choose center cell
     if (cell_nr_y == 4 && cell_nr_x == 3)     right_open  = 0;
     if (cell_nr_y == 4 && cell_nr_x == 5)     left_open  = 0;
     if (cell_nr_y == 5 && cell_nr_x == 4)     up_open  = 0;
     if (cell_nr_y == 3 && cell_nr_x == 4)     down_open  = 0;

     switch (pills[i].direction) {
       case LEFT:  
          // continue left (70% chance, else go up or down)
          //    if not, go right back (return)
          if (left_open == 1 && (up_open == 1 || down_open == 1) && ((rand()%10 >= 3) )) {
             pills[i].direction = LEFT;
          } else {
                   if (up_open == 1 || down_open == 1 ) {
                      found = 0;
                      while (found == 0) {  // go up or down, if possible
                          if (rand()%2 == 0 && up_open == 1) {
                              found = 1;
                              pills[i].direction = UP;
                          } else { 
                                   if (down_open == 1) {
                                       found = 1;      
                                       pills[i].direction = DOWN;
                                   }
                          }
                      }  // end while
                    } else { 
                            if (left_open == 1) {
                                pills[i].direction = LEFT;
                            } else {    
                                pills[i].direction = RIGHT;
                            } 
                    }
          }                 
        break;
      case RIGHT: 
          // continue right (70% chance, else go up or down)
          //    if not, go left back (return)
          if (right_open == 1 && (up_open == 1 || down_open == 1) && ((rand()%10 >= 3) )) {
             pills[i].direction = RIGHT;
          } else {
                   if (up_open == 1 || down_open == 1 ) {
                      found = 0;
                      while (found == 0) {  // go up or down, if possible
                          if (rand()%2 == 0 && up_open == 1) {
                              found = 1;
                              pills[i].direction = UP;
                          } else { 
                                   if (down_open == 1) {
                                       found = 1;      
                                       pills[i].direction = DOWN;
                                   }
                          }
                      }  // end while
                    } else { 
                            if (right_open == 1) {
                                pills[i].direction = RIGHT;
                            } else {    
                                pills[i].direction = LEFT;
                            } 
                    }
          }                 
        break;
      case UP: 
          // continue up (70% chance, else go left or right)
          //    if not, go right down (return)
          if (up_open == 1 && (left_open == 1 || right_open == 1) && ((rand()%10 >= 3) )) {
             pills[i].direction = UP;
          } else {
                   if (left_open == 1 || right_open == 1 ) {
                      found = 0;
                      while (found == 0) {  // go left or right, if possible
                          if (rand()%2 == 0 && left_open == 1) {
                              found = 1;
                              pills[i].direction = LEFT;
                          } else { 
                                   if (right_open == 1) {
                                       found = 1;      
                                       pills[i].direction = RIGHT;
                                   }
                          }
                      }  // end while
                    } else { 
                            if (up_open == 1) {
                                pills[i].direction = UP;
                            } else {    
                                pills[i].direction = DOWN;
                            } 
                    }
          }                 
        break;
      case DOWN: 
          // continue down (70% chance, else go left or right)
          //    if not, go right up (return)
          if (down_open == 1 && (left_open == 1 || right_open == 1) && ((rand()%10 >= 3) )) {
             pills[i].direction = DOWN;
          } else {
                   if (left_open == 1 || right_open == 1 ) {
                      found = 0;
                      while (found == 0) {  // go left or right, if possible
                          if (rand()%2 == 0 && left_open == 1) {
                              found = 1;
                              pills[i].direction = LEFT;
                          } else { 
                                   if (right_open == 1) {
                                       found = 1;      
                                       pills[i].direction = RIGHT;
                                   }
                          }
                      }  // end while
                    } else { 
                            if (down_open == 1) {
                                pills[i].direction = DOWN;
                            } else {    
                                pills[i].direction = UP;
                            } 
                    }
          }                 
        break;
     }
  }  // if middle of cell
}


void rotate_maze_center(void)
{
   /* set next rotate action (clockwise) */
   if (maze_center_open == DOWN) maze_center_open = LEFT;
   else if (maze_center_open == LEFT) maze_center_open = UP;
     else if (maze_center_open == UP) maze_center_open = RIGHT;
        else if (maze_center_open == RIGHT) maze_center_open = DOWN;
   
   /* change maze structure */
   switch (maze_center_open) {
   case LEFT:    
     horizontal_lines[4].line[4] = 'x';
     vertical_lines[4].line[4] = '-'; vertical_lines[4].line[5] = '|';
     horizontal_lines[5].line[4] = 'x';
     break;
   case RIGHT:    
     horizontal_lines[4].line[4] = 'x';
     vertical_lines[4].line[4] = '|'; vertical_lines[4].line[5] = '-';
     horizontal_lines[5].line[4] = 'x';
     break;
   case UP:    
     horizontal_lines[4].line[4] = '-';
     vertical_lines[4].line[4] = '|'; vertical_lines[4].line[5] = '|';
     horizontal_lines[5].line[4] = 'x';
     break;
   case DOWN:    
     horizontal_lines[4].line[4] = 'x';
     vertical_lines[4].line[4] = '|'; vertical_lines[4].line[5] = '|';
     horizontal_lines[5].line[4] = '-';
     break;
   }

     /* Change maze display (close previous, open new
        Using global variables ! UDG's set in setup()*/
     switch (maze_center_open) {
      case RIGHT:    
       // close top
       if (maze_selected == 1) sp1_TileEntry('#', udg_top_left); 
         else sp1_TileEntry('#', udg_top_left_maze2); 
       //sp1_TileEntry('@', udg_line_hori);  // already defnied

       sp1_PrintAtInv(12, 14, BRIGHT | INK_MAGENTA | PAPER_BLACK, '#');
       sp1_PrintAtInv(12, 15, BRIGHT | INK_MAGENTA | PAPER_BLACK, '@');
       sp1_PrintAtInv(12, 16, BRIGHT | INK_MAGENTA | PAPER_BLACK, '@');

       // open right
       if (maze_selected == 1) sp1_TileEntry('(', udg_line_hori_left); 
         else sp1_TileEntry('(', udg_bottom_right); 
       if (maze_selected == 1) sp1_TileEntry('{', udg_line_hori_left);   
         else sp1_TileEntry('{',  udg_line_hori); 

       sp1_PrintAtInv(12, 17, BRIGHT | INK_MAGENTA | PAPER_BLACK, '(');
       sp1_PrintAtInv(13, 17, BRIGHT | INK_MAGENTA | PAPER_BLACK, ' ');
       sp1_PrintAtInv(14, 17, BRIGHT | INK_MAGENTA | PAPER_BLACK, '{');
       break;
     case LEFT:    
       sp1_TileEntry('@', udg_line_hori);
       if (maze_selected == 1)  sp1_TileEntry(']', udg_line_hori_right);
         else sp1_TileEntry(']', udg_line_hori_left_maze2);
       if (maze_selected == 1) sp1_TileEntry('[', udg_bottom_right); 
          else  sp1_TileEntry('[', udg_line_hori_left_maze2);
       if (maze_selected == 1) sp1_TileEntry(']', udg_line_hori_right);
          else sp1_TileEntry('*', udg_top_left);

       // open left
       sp1_PrintAtInv(12, 14, BRIGHT | INK_MAGENTA | PAPER_BLACK, ']');
       sp1_PrintAtInv(13, 14, BRIGHT | INK_MAGENTA | PAPER_BLACK, ' ');
       if (maze_selected == 1) sp1_PrintAtInv(14, 14, BRIGHT | INK_MAGENTA | PAPER_BLACK, ']');
          else sp1_PrintAtInv(14, 14, BRIGHT | INK_MAGENTA | PAPER_BLACK, '*');
       
       // close bottom
       sp1_PrintAtInv(14, 15, BRIGHT | INK_MAGENTA | PAPER_BLACK, '@');
       sp1_PrintAtInv(14, 16, BRIGHT | INK_MAGENTA | PAPER_BLACK, '@');
       sp1_PrintAtInv(14, 17, BRIGHT | INK_MAGENTA | PAPER_BLACK, '[');
       break;
     case UP:    
       // close left
       if (maze_selected == 1)  sp1_TileEntry('%', udg_line_vert_top); 
          else sp1_TileEntry('%', udg_line_vert_top_maze2); 
       sp1_TileEntry('^', udg_line_vert); 
       if (maze_selected == 1) sp1_TileEntry('&', udg_bottom_left); 
          else sp1_TileEntry('&', udg_bottom_left_maze2); 
       sp1_PrintAtInv(12, 14, BRIGHT | INK_MAGENTA | PAPER_BLACK, '%');
       sp1_PrintAtInv(13, 14, BRIGHT | INK_MAGENTA | PAPER_BLACK, '^');
       sp1_PrintAtInv(14, 14, BRIGHT | INK_MAGENTA | PAPER_BLACK, '&');
       
       // open top
       if (maze_selected == 1) sp1_TileEntry(')', udg_line_vert_top);
          else sp1_TileEntry(')', udg_line_vert); 

       sp1_PrintAtInv(12, 15, BRIGHT | INK_MAGENTA | PAPER_BLACK, ' ');
       sp1_PrintAtInv(12, 16, BRIGHT | INK_MAGENTA | PAPER_BLACK, ' ');
       sp1_PrintAtInv(12, 17, BRIGHT | INK_MAGENTA | PAPER_BLACK, ')');
       break;
     case DOWN:    
       // close right
       if (maze_selected == 1) sp1_TileEntry('+', udg_top_right); 
         else sp1_TileEntry('+',  udg_line_vert_top_maze2); 
       //sp1_TileEntry('^', udg_line_vert);   // already defined
       if (maze_selected == 1) sp1_TileEntry('?', udg_line_vert_bottom); 
         else sp1_TileEntry('?', udg_bottom_left); 
       if (maze_selected == 1) sp1_TileEntry(';', udg_line_vert_bottom); 
         else sp1_TileEntry(';', udg_line_vert);   

       sp1_PrintAtInv(12, 17, BRIGHT | INK_MAGENTA | PAPER_BLACK, '+');
       sp1_PrintAtInv(13, 17, BRIGHT | INK_MAGENTA | PAPER_BLACK, '^');
       sp1_PrintAtInv(14, 17, BRIGHT | INK_MAGENTA | PAPER_BLACK, '?');

       // open down
       //sp1_TileEntry('?', udg_line_vert_bottom); // already defined

       sp1_PrintAtInv(14, 14, BRIGHT | INK_MAGENTA | PAPER_BLACK, ';');
       sp1_PrintAtInv(14, 15, BRIGHT | INK_MAGENTA | PAPER_BLACK, ' ');
       sp1_PrintAtInv(14, 16, BRIGHT | INK_MAGENTA | PAPER_BLACK, ' ');
       break;
     }     
     wait();
     sp1_UpdateNow();

}


unsigned char block_of_ram[5000];   // ??


void setup(void)
{
   // the crt has disabled interrupts before main is called
   zx_border(INK_BLACK);

   // set up the block memory allocator with one queue
   // max size requested by sp1 will be 24 bytes or block size of 25 (+1 for overhead)
   balloc_reset(0);                                              // make queue 0 empty
   balloc_addmem(0, sizeof(block_of_ram)/25, 24, block_of_ram);  // add free memory from bss section
   balloc_addmem(0, 8, 24, (void *)0xd101);                      // another eight from an unused area

   // interrupt mode 2
   setup_int();

   sp1_Initialize(SP1_IFLAG_MAKE_ROTTBL | SP1_IFLAG_OVERWRITE_TILES | SP1_IFLAG_OVERWRITE_DFILE, INK_BLACK | PAPER_BLACK, ' ');
   
   ps0.bounds = &cr;
   ps0.flags = SP1_PSSFLAG_INVALIDATE;
   ps0.visit = 0;

   intrinsic_ei();

   // setup the backgroundd tiles
   pt = tiles;
   for (i = 0; i < TILES_LEN; ++i, pt += 8)
      sp1_TileEntry(TILES_BASE + i, pt);

   // initialize pills.status (needed for cleaning of sprites later)
   for (i = 0; i < NUM_PILLS; i++) (pills[i].status = 0);
}


void start_new_game(void) 
{
  munchkin_dying = FALSE;
  score = 0;
  
  speed = 2;             // values 1 or 2 
  maze_selected = 1;
  start_new_maze();
}


void start_new_maze(void) 
{
  munchkin_x_factor1 = (MAZE_OFFSET_X + 4 * (HORI_LINE_SIZE -2)) + 12;  // 112 + 12
  munchkin_y_factor1 = (MAZE_OFFSET_Y + 3 * (VERT_LINE_SIZE -2)) + 8;  // 80  +  8


  munchkin_auto_direction  = 0;    // stationary 
  munchkin_last_direction  = 0;    // stationary 
  munchkin_animation_frame = 0;
  munchkin_dying_animation = 0;
  maze_center_open = DOWN;         // 4=down open at startup
  maze_completed = FALSE;             
  maze_color = 3;
  last_pill_speed_increased = FALSE;
  powerpill_active_timer = 0;

  setup_maze();

  // setup the background tiles (16 UDG's)
  if (maze_selected == 1)  pt = tiles;
      else pt = tiles_maze2;

  // set up tiles (UDG's) for the maze
  for (i = 0; i < TILES_LEN; ++i, pt += 8)
      sp1_TileEntry(TILES_BASE + i, pt);

  setup_ghosts();
  setup_pills();

  // display maze color: magenta
  ptiles[1] = 0x43;
  ptiles_maze2[1] = 0x43;
  sp1_SetPrintPos(&ps0, 0, 0);

  if (maze_selected == 1) sp1_PrintString(&ps0, ptiles);
     else  sp1_PrintString(&ps0, ptiles_maze2);

  display_score();
}


void hide_sprites(void)
{
  // delete any inactive pill-sprites (e.g. when level not completed)
  // (pill status initialized in setup()
  for (i = 0; i < NUM_PILLS; i++) {
    if (pills[i].status != 0) {
          sp1_MoveSprAbs(pills[i].sprite, &cr, NULL, 0, 34, 0, 0);  // remove from screen
                                                                    // print at column 34
          sp1_DeleteSpr(pills[i].sprite);
          pills[i].status = 0;   
     }      
  }

  for (i = 0; i < NUM_GHOSTS; i++)  {
    sp1_MoveSprAbs(ghosts[i].sprite, &cr, NULL, 0, 34, 0, 0);  // remove from screen
                                                               // print at column 34
    get_ink_colour(ghosts[i].colour);  // set to original colour
    sp1_IterateSprChar(ghosts[i].sprite, add_colour_to_sprite);
  }  

  sp1_MoveSprAbs(munchkin_sprite, &cr, NULL, 0, 34, 0, 0);  // remove from screen
}


int main(void)
{
   unsigned char idle = 0;

   setup();

   zx_border(INK_BLACK);

   // set up the block memory allocator with one queue
   // max size requested by sp1 will be 24 bytes or block size of 25 (+1 for overhead)
   balloc_reset(0);                                              // make queue 0 empty
   balloc_addmem(0, sizeof(block_of_ram)/25, 24, block_of_ram);  // add free memory from bss section
   balloc_addmem(0, 8, 24, (void *)0xd101);                      // another eight from an unused area

   // interrupt mode 2
   setup_int();

   sp1_Initialize(SP1_IFLAG_MAKE_ROTTBL | SP1_IFLAG_OVERWRITE_TILES | SP1_IFLAG_OVERWRITE_DFILE, INK_BLACK | PAPER_BLACK, ' ');
   
   ps0.bounds = &cr;
   ps0.flags = SP1_PSSFLAG_INVALIDATE;
   ps0.visit = 0;

   intrinsic_ei();

   draw_menu();

   srand(tick);  // 256 different games are possible
   high_score = 0,
   active_pills = NUM_PILLS;

   while(1)
   {
      key = in_inkey();
      if (key)
      {
         if (key == '4')
         {
            playfx(FX_SELECT);

            in_wait_nokey();
            run_redefine_keys();
            idle = 0;
         }


         if (key == '1' || key == '2' || key == '3')
         {
            playfx(FX_SELECT);

            joy_k.left  = in_key_scancode(keys[0]);
            joy_k.right = in_key_scancode(keys[1]);
            joy_k.down  = in_key_scancode(keys[3]);
            joy_k.up    = in_key_scancode(keys[2]);

            if (key == '1')
               joyfunc = (JOYFUNC)in_stick_keyboard;
            if (key == '2')
               joyfunc = (JOYFUNC)in_stick_kempston;
            if (key == '3')
               joyfunc = (JOYFUNC)in_stick_sinclair1;

            run_intro();

            run_play();
            idle = 0;
            draw_menu();
         }
      }

      if (idle++ == 255)
      {
         // go back to the welcome screen after a while
         // if the player doesn't do anything
         idle = 0;
         draw_menu();
      }

      wait();
      sp1_UpdateNow();
   }
}
