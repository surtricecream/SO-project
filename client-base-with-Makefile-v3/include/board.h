#ifndef BOARD_H
#define BOARD_H

#define MAX_MOVES 20
#define MAX_LEVELS 20
#define MAX_FILENAME 256
#define MAX_GHOSTS 25

#include <pthread.h>

typedef enum {
    REACHED_PORTAL = 1,
    VALID_MOVE = 0,
    INVALID_MOVE = -1,
    DEAD_PACMAN = -2,
} move_t;

typedef struct {
    char command;
    int turns;
    int turns_left;
} command_t;

typedef struct {
    int pos_x, pos_y; //current position
    int alive; // if is alive
    int points; // how many points have been collected
    int passo; // number of plays to wait before starting
    command_t moves[MAX_MOVES];
    int current_move;
    int n_moves;
    int waiting;
} pacman_t;

typedef struct {
    int pos_x, pos_y; //current position
    int passo; // number of plays to wait before starting
    command_t moves[MAX_MOVES];
    int n_moves;
    int current_move;
    int waiting;
    int charged;
} ghost_t;

typedef struct {
    char content; // stuff like 'P' for pacman 'M' for monster and 'W' for wall
    int has_dot; // whether there is a dot in this position or not
    int has_portal; // whether there is a portal in this position or not
    pthread_mutex_t lock;
} board_pos_t;

typedef struct {
    int width, height; //dimensions of the board
    board_pos_t* board; //actual board, most likely a row-major matrix
    int n_pacmans; //number of pacmans in the board
    pacman_t* pacmans; // array containing every pacman in the board to iterate through when processing
    int n_ghosts; //number of ghosts in the board
    ghost_t* ghosts; // array containing every ghost in the board to iterate through when processing
    char level_name[256]; //name for the level file to keep track of which will be the next
    char pacman_file[256]; // file with pacman movements
    char ghosts_files[MAX_GHOSTS][256]; // files with monster movements
    int tempo; // Duracao de cada jogada???
    pthread_rwlock_t state_lock;
} board_t;

/*Move pacman/monster in a certain direction on the board must check for boundaries, walls and other monsters
Maybe do 1 function for pacman and 1 for monsters if required
Maybe do 1 function for each direction
*/
int move_pacman(board_t* board, int pacman_index, command_t* command);
int move_ghost(board_t* board, int ghost_index, command_t* command);

/*Remove an object (Pacman)*/
void kill_pacman(board_t* board, int pacman_index);

/*Adds a pacman to the board from a file*/
int load_pacman(board_t* board);

/*Adds a ghost to the board from a file*/
int load_ghost(board_t* board);


/*
Fils the board with the information coming from the file
*/
int load_level(board_t* board, char* filename, char* dirname, int accumulated_points);
// Unloads levels loaded by load_level
void unload_level(board_t * board);

void print_board(board_t* board);

#endif
