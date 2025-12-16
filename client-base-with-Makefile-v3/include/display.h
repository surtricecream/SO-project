#ifndef DISPLAY_H
#define DISPLAY_H

#include "api.h"
#include <ncurses.h>
#include <board.h>

#define DRAW_GAME_OVER 0
#define DRAW_WIN 1
#define DRAW_MENU 2


/*
Potential Structures for ncurses
*/

/*Initialize everything ncurses requires*/
int terminal_init();

void draw_board_client(Board board);

char* get_board_displayed(board_t* board);

/*Draw the board on the screen*/
void draw_board(board_t* board, int mode);

/*Add a specific character with colour i into position (pos_x,pos_y) of the creen
Pre loaded colours:
1- Yellow
2- Red
3- Blue
4- White
5- Green
6- Magenta
7- Cyan
*/
void draw(char c, int colour_i, int pos_x, int pos_y);

/*Call ncurses refresh() to update the screen*/
void refresh_screen();

/*Ncurses will be reading the player's inputs*/
char get_input();

void terminal_cleanup();

void set_timeout(int tempo_ms);

#endif
