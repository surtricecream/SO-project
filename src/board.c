#include "board.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>      
#include <sys/stat.h>
#include <limits.h>     
#include <errno.h>
#include <string.h>

FILE * debugfile;

// Helper private function to find and kill pacman at specific position
static int find_and_kill_pacman(board_t* board, int new_x, int new_y) {
    for (int p = 0; p < board->n_pacmans; p++) {
        pacman_t* pac = &board->pacmans[p];
        if (pac->pos_x == new_x && pac->pos_y == new_y && pac->alive) {
            pac->alive = 0;
            kill_pacman(board, p);
            return DEAD_PACMAN;
        }
    }
    return VALID_MOVE;
}

// Helper private function for getting board position index
static inline int get_board_index(board_t* board, int x, int y) {
    return y * board->width + x;
}

// Helper private function for checking valid position
static inline int is_valid_position(board_t* board, int x, int y) {
    return (x >= 0 && x < board->width) && (y >= 0 && y < board->height); // Inside of the board boundaries
}

void sleep_ms(int milliseconds) {
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

int move_pacman(board_t* board, int pacman_index, command_t* command) {
    if (pacman_index < 0 || !board->pacmans[pacman_index].alive) {
        return DEAD_PACMAN; // Invalid or dead pacman
    }

    pacman_t* pac = &board->pacmans[pacman_index];
    int new_x = pac->pos_x;
    int new_y = pac->pos_y;

    // check passo
    if (pac->waiting > 0) {
        pac->waiting -= 1;
        return VALID_MOVE;        
    }
    pac->waiting = pac->passo;

    char direction = command->command;

    if (direction == 'R') {
        char directions[] = {'W', 'S', 'A', 'D'};
        direction = directions[rand() % 4];
    }

    // Calculate new position based on direction
    switch (direction) {
        case 'W': // Up
            new_y--;
            break;
        case 'S': // Down
            new_y++;
            break;
        case 'A': // Left
            new_x--;
            break;
        case 'D': // Right
            new_x++;
            break;
        case 'T': // Wait
            if (command->turns_left == 1) {
                pac->current_move += 1; // move on
                command->turns_left = command->turns;
            }
            else command->turns_left -= 1;
            return VALID_MOVE;
        default:
            return INVALID_MOVE; // Invalid direction
    }

    // Logic for the WASD movement
    pac->current_move+=1;

    // Check boundaries
    if (!is_valid_position(board, new_x, new_y)) {
        return INVALID_MOVE;
    }

    int new_index = get_board_index(board, new_x, new_y);
    int old_index = get_board_index(board, pac->pos_x, pac->pos_y);
    char target_content = board->board[new_index].content;

    if (board->board[new_index].has_portal) {
        board->board[old_index].content = ' ';
        board->board[new_index].content = 'P';
        return REACHED_PORTAL;
    }

    // Check for walls
    if (target_content == 'W') {
        return INVALID_MOVE;
    }

    // Check for ghosts
    if (target_content == 'M') {
        kill_pacman(board, pacman_index);
        return DEAD_PACMAN;
    }

    // Collect points
    if (board->board[new_index].has_dot) {
        pac->points++;
        board->board[new_index].has_dot = 0;
    }

    board->board[old_index].content = ' ';
    pac->pos_x = new_x;
    pac->pos_y = new_y;
    board->board[new_index].content = 'P';

    return VALID_MOVE;
}

// Helper private function for charged ghost movement in one direction
static int move_ghost_charged_direction(board_t* board, ghost_t* ghost, char direction, int* new_x, int* new_y) {
    int x = ghost->pos_x;
    int y = ghost->pos_y;
    *new_x = x;
    *new_y = y;
    
    switch (direction) {
        case 'W': // Up
            if (y == 0) return INVALID_MOVE;
            *new_y = 0; // In case there is no colision
            for (int i = y - 1; i >= 0; i--) {
                char target_content = board->board[get_board_index(board, x, i)].content;
                if (target_content == 'W' || target_content == 'M') {
                    *new_y = i + 1; // stop before colision
                    return VALID_MOVE;
                }
                else if (target_content == 'P') {
                    *new_y = i;
                    return find_and_kill_pacman(board, *new_x, *new_y);
                }
            }
            break;

        case 'S': // Down
            if (y == board->height - 1) return INVALID_MOVE;
            *new_y = board->height - 1; // In case there is no colision
            for (int i = y + 1; i < board->height; i++) {
                char target_content = board->board[get_board_index(board, x, i)].content;
                if (target_content == 'W' || target_content == 'M') {
                    *new_y = i - 1; // stop before colision
                    return VALID_MOVE;
                }
                if (target_content == 'P') {
                    *new_y = i;
                    return find_and_kill_pacman(board, *new_x, *new_y);
                }
            }
            break;

        case 'A': // Left
            if (x == 0) return INVALID_MOVE;
            *new_x = 0; // In case there is no colision
            for (int j = x - 1; j >= 0; j--) {
                char target_content = board->board[get_board_index(board, j, y)].content;
                if (target_content == 'W' || target_content == 'M') {
                    *new_x = j + 1; // stop before colision
                    return VALID_MOVE;
                }
                if (target_content == 'P') {
                    *new_x = j;
                    return find_and_kill_pacman(board, *new_x, *new_y);
                }
            }
            break;

        case 'D': // Right
            if (x == board->width - 1) return INVALID_MOVE;
            *new_x = board->width - 1; // In case there is no colision
            for (int j = x + 1; j < board->width; j++) {
                char target_content = board->board[get_board_index(board, j, y)].content;
                if (target_content == 'W' || target_content == 'M') {
                    *new_x = j - 1; // stop before colision
                    return VALID_MOVE;
                }
                if (target_content == 'P') {
                    *new_x = j;
                    return find_and_kill_pacman(board, *new_x, *new_y);
                }
            }
            break;
        default:
            debug("DEFAULT CHARGED MOVE - direction = %c\n", direction);
            return INVALID_MOVE;
    }
    return VALID_MOVE;
}   

int move_ghost_charged(board_t* board, int ghost_index, char direction) {
    ghost_t* ghost = &board->ghosts[ghost_index];
    int x = ghost->pos_x;
    int y = ghost->pos_y;
    int new_x = x;
    int new_y = y;

    ghost->charged = 0; //uncharge
    int result = move_ghost_charged_direction(board, ghost, direction, &new_x, &new_y);
    if (result == INVALID_MOVE) {
        debug("DEFAULT CHARGED MOVE - direction = %c\n", direction);
        return INVALID_MOVE;
    }

    // Get board indices
    int old_index = get_board_index(board, ghost->pos_x, ghost->pos_y);
    int new_index = get_board_index(board, new_x, new_y);

    // Update board - clear old position (restore what was there)
    board->board[old_index].content = ' '; // Or restore the dot if ghost was on one
    // Update ghost position
    ghost->pos_x = new_x;
    ghost->pos_y = new_y;
    // Update board - set new position
    board->board[new_index].content = 'M';
    return result;
}

int move_ghost(board_t* board, int ghost_index, command_t* command) {
    ghost_t* ghost = &board->ghosts[ghost_index];
    int new_x = ghost->pos_x;
    int new_y = ghost->pos_y;

    // check passo
    if (ghost->waiting > 0) {
        ghost->waiting -= 1;
        return VALID_MOVE;
    }
    ghost->waiting = ghost->passo;

    char direction = command->command;
    
    if (direction == 'R') {
        char directions[] = {'W', 'S', 'A', 'D'};
        direction = directions[rand() % 4];
    }

    // Calculate new position based on direction
    switch (direction) {
        case 'W': // Up
            new_y--;
            break;
        case 'S': // Down
            new_y++;
            break;
        case 'A': // Left
            new_x--;
            break;
        case 'D': // Right
            new_x++;
            break;
        case 'C': // Charge
            ghost->current_move += 1;
            ghost->charged = 1;
            return VALID_MOVE;
        case 'T': // Wait
            if (command->turns_left == 1) {
                ghost->current_move += 1; // move on
                command->turns_left = command->turns;
            }
            else command->turns_left -= 1;
            return VALID_MOVE;
        default:
            return INVALID_MOVE; // Invalid direction
    }

    // Logic for the WASD movement
    ghost->current_move++;
    if (ghost->charged)
        return move_ghost_charged(board, ghost_index, direction);

    // Check boundaries
    if (!is_valid_position(board, new_x, new_y)) {
        return INVALID_MOVE;
    }

    // Check board position
    int new_index = get_board_index(board, new_x, new_y);
    int old_index = get_board_index(board, ghost->pos_x, ghost->pos_y);
    char target_content = board->board[new_index].content;

    // Check for walls and ghosts
    if (target_content == 'W' || target_content == 'M') {
        return INVALID_MOVE;
    }

    int result = VALID_MOVE;
    // Check for pacman
    if (target_content == 'P') {
        result = find_and_kill_pacman(board, new_x, new_y);
    }

    // Update board - clear old position (restore what was there)
    board->board[old_index].content = ' '; // Or restore the dot if ghost was on one

    // Update ghost position
    ghost->pos_x = new_x;
    ghost->pos_y = new_y;

    // Update board - set new position
    board->board[new_index].content = 'M';
    return result;
}

void kill_pacman(board_t* board, int pacman_index) {
    debug("Killing %d pacman\n\n", pacman_index);
    pacman_t* pac = &board->pacmans[pacman_index];
    int index = pac->pos_y * board->width + pac->pos_x;

    // Remove pacman from the board
    board->board[index].content = ' ';

    // Mark pacman as dead
    pac->alive = 0;
}

// Static Loading
int load_pacman(board_t* board, int points) {
    board->board[1 * board->width + 1].content = 'P'; // Pacman
    board->pacmans[0].pos_x = 1;
    board->pacmans[0].pos_y = 1;
    board->pacmans[0].alive = 1;
    board->pacmans[0].points = points;
    return 0;
}

// Static Loading
int load_ghost(board_t* board) {
    // Ghost 0
    board->board[3 * board->width + 1].content = 'M'; // Monster
    board->ghosts[0].pos_x = 1;
    board->ghosts[0].pos_y = 3;
    board->ghosts[0].passo = 0;
    board->ghosts[0].waiting = 0;
    board->ghosts[0].current_move = 0;
    board->ghosts[0].n_moves = 16;
    for (int i = 0; i < 8; i++) {
        board->ghosts[0].moves[i].command = 'D';
        board->ghosts[0].moves[i].turns = 1; 
    }
    for (int i = 8; i < 16; i++) {
        board->ghosts[0].moves[i].command = 'A';
        board->ghosts[0].moves[i].turns = 1; 
    }

    // Ghost 1
    board->board[2 * board->width + 4].content = 'M'; // Monster
    board->ghosts[1].pos_x = 4;
    board->ghosts[1].pos_y = 2;
    board->ghosts[1].passo = 1;
    board->ghosts[1].waiting = 1;
    board->ghosts[1].current_move = 0;
    board->ghosts[1].n_moves = 1;
    board->ghosts[1].moves[0].command = 'R'; // Random
    board->ghosts[1].moves[0].turns = 1; 
    
    return 0;
}




// Non-Static Loading
int load_ghost_nonstatic(board_t* board, int ghostIndex, const char* monster_name) {
    // Ghost 0
    if (ghostIndex < 0 || ghostIndex >= board->n_ghosts) return -1;
    ghost_t* fantasmaGaspar=&board->ghosts[ghostIndex];
    
    char filepath[MAX_FILENAME* 2];

    if(build_directory(board->base_dir, monster_name, filepath, sizeof(filepath))==-1){
        return -1;
    }
    //tem de se tornar o m1.m no diretorio
  
    int passo=0,gx=0,gy=0,n_moves=0;
    if(parse_entity_file(fantasmaGaspar->moves,&n_moves, &passo, &gx, &gy, filepath)==-1){
        return -1;
    }
    if (gx < 0 || gx >= board->width || gy < 0 || gy >= board->height) {
        return -1;
    }
    if (board->board[gy * board->width + gx].content == 'W'){
        return -1;
    }
    fantasmaGaspar->n_moves=n_moves;
    fantasmaGaspar->passo=passo;
    fantasmaGaspar->pos_x=gx;
    fantasmaGaspar->pos_y=gy;
    fantasmaGaspar->waiting = passo;
    fantasmaGaspar->current_move = 0;
    fantasmaGaspar->charged=0;
 
    board->board[gy * board->width + gx].content = 'M';
    
    return 0;
}
// Non-Static Loading
int load_pacman_nonstatic(board_t* board,int points) {

    pacman_t* pikachu=&board->pacmans[0];
    char filepath[MAX_FILENAME* 2];
    pikachu->alive=1;
    pikachu->points=points;

    if(board->pacman_file[0]=='\0'){
        pikachu->pos_x = 1; 
        pikachu->pos_y = 1;
        pikachu->passo = 0;
        pikachu->waiting = 0;
        pikachu->n_moves = 0;
        pikachu->current_move=0;
    }else{
        if(build_directory(board->base_dir, board->pacman_file, filepath, sizeof(filepath))==-1){
            return -1;
        }
        int passo=0,gx=0,gy=0,n_moves=0;
        if(parse_entity_file(pikachu->moves,&n_moves, &passo, &gx, &gy, filepath)==-1){
            return -1;
        }
        if (gx < 0 || gx >= board->width || gy < 0 || gy >= board->height) {
            return -1;
        }
        if (board->board[gy * board->width + gx].content == 'W'){
            return -1;
        }
        pikachu->n_moves=n_moves;
        pikachu->passo=passo;
        pikachu->pos_x=gx;
        pikachu->pos_y=gy;
        pikachu->waiting = passo;
        pikachu->current_move = 0;
    }
 
    board->board[pikachu->pos_y * board->width + pikachu->pos_x].content = 'P';
    
    return 0;
}

int load_level(board_t *board, int points) {
    board->height = 5;
    board->width = 10;
    board->tempo = 10;

    board->n_ghosts = 2;
    board->n_pacmans = 1;

    board->board = calloc(board->width * board->height, sizeof(board_pos_t));
    board->pacmans = calloc(board->n_pacmans, sizeof(pacman_t));
    board->ghosts = calloc(board->n_ghosts, sizeof(ghost_t));

    sprintf(board->level_name, "Static Level");

    for (int i = 0; i < board->height; i++) {
        for (int j = 0; j < board->width; j++) {
            if (i == 0 || j == 0 || j == (board->width - 1)) {
                board->board[i * board->width + j].content = 'W';
            }
            else if (i == 4 && j == 8) {
                board->board[i * board->width + j].content = ' ';
                board->board[i * board->width + j].has_portal = 1;
            }
            else {
                board->board[i * board->width + j].content = ' ';
                board->board[i * board->width + j].has_dot = 1;
            }
        }
    }

    load_ghost(board);
    load_pacman(board, points);
    return 0;
}


int load_level_from_file(board_t *board, const char *filepath, int accumulated_points) {
    if(parse_level_file(board, filepath)!=0){
        return -1;
    }
    sprintf(board->level_name, "Level %d",board->current_level+1);
    //pacman
    if(load_pacman_nonstatic(board,accumulated_points)==-1){
        return -1;
    }
    //fantasma
    for(int i=0;i<board->n_ghosts;i++){
        if(load_ghost_nonstatic(board,i, board->ghosts_files[i])){
            return -1;
        }
    }
//
//    for (int i = 0; i < board->height; i++) {
//        for (int j = 0; j < board->width; j++) {
//            if (i == 0 || j == 0 || j == (board->width - 1)) {
//                board->board[i * board->width + j].content = 'W';
//            }
//            else if (i == 4 && j == 8) {
//                board->board[i * board->width + j].content = ' ';
//                board->board[i * board->width + j].has_portal = 1;
//            }
//            else {
//                board->board[i * board->width + j].content = ' ';
//                board->board[i * board->width + j].has_dot = 1;
//            }
//        }
//    }
//
//    load_ghost(board);
//    load_pacman(board, points);
//
    return 0;
//    
}
int next_level(board_t* board){
    if(board!=NULL){
        if(board->current_level<=board->level_count){
            board->current_level++;
            return 1;
        }
    }
    return 0;
}

void unload_level(board_t * board) {
    free(board->board);
    free(board->pacmans);
    free(board->ghosts);
    board->board = NULL;
    board->pacmans = NULL;
    board->ghosts = NULL;
    board->n_ghosts = 0;
    board->n_pacmans = 0;
    board->width = 0;
    board->height = 0;
    board->pacman_file[0] = '\0';
}

void open_debug_file(char *filename) {
    debugfile = fopen(filename, "w");
}

void close_debug_file() {
    fclose(debugfile);
}

void debug(const char * format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(debugfile, format, args);
    va_end(args);

    fflush(debugfile);
}

void print_board(board_t *board) {
    if (!board || !board->board) {
        debug("[%d] Board is empty or not initialized.\n", getpid());
        return;
    }

    // Large buffer to accumulate the whole output
    char buffer[8192];
    size_t offset = 0;

    offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                       "=== [%d] LEVEL INFO ===\n"
                       "Dimensions: %d x %d\n"
                       "Tempo: %d\n"
                       "Pacman file: %s\n",
                       getpid(), board->height, board->width, board->tempo, board->pacman_file);

    offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                       "Monster files (%d):\n", board->n_ghosts);

    for (int i = 0; i < board->n_ghosts; i++) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                           "  - %s\n", board->ghosts_files[i]);
    }

    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "\n=== BOARD ===\n");

    for (int y = 0; y < board->height; y++) {
        for (int x = 0; x < board->width; x++) {
            int idx = y * board->width + x;
            if (offset < sizeof(buffer) - 2) {
                buffer[offset++] = board->board[idx].content;
            }
        }
        if (offset < sizeof(buffer) - 2) {
            buffer[offset++] = '\n';
        }
    }

    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "==================\n");

    buffer[offset] = '\0';

    debug("%s", buffer);
}

static void get_save_path_board(board_t *b, char *buffer, size_t size) {
    snprintf(buffer, size, "%s/.pacmanist.save", b->base_dir);
}

int save_game_state(board_t *board) {
    char path[PATH_MAX];
    get_save_path_board(board, path, sizeof(path));

    // Open with POSIX flags: Write Only, Create if missing, Truncate if exists
    // Mode 0644 (rw-r--r--)
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        debug("Error opening file for save: %s\n", strerror(errno));
        return -1;
    }

    // Write scalar values
    write(fd, &board->width, sizeof(int));
    write(fd, &board->height, sizeof(int));
    write(fd, &board->tempo, sizeof(int));
    write(fd, &board->level_count, sizeof(int));
    write(fd, &board->current_level, sizeof(int));
    write(fd, &board->n_pacmans, sizeof(int));
    write(fd, &board->n_ghosts, sizeof(int));

    // Write fixed-size strings
    write(fd, board->base_dir, MAX_FILENAME);
    write(fd, board->pacman_file, MAX_FILENAME);
    write(fd, board->level_name, 256);

    // Write arrays of strings
    write(fd, board->level_files, sizeof(char) * MAX_LEVELS * MAX_FILENAME);
    write(fd, board->ghosts_files, sizeof(char) * MAX_GHOSTS * 256);

    // Write dynamic arrays content
    size_t board_size = board->width * board->height * sizeof(board_pos_t);
    write(fd, board->board, board_size);

    size_t pac_size = board->n_pacmans * sizeof(pacman_t);
    write(fd, board->pacmans, pac_size);

    size_t ghost_size = board->n_ghosts * sizeof(ghost_t);
    write(fd, board->ghosts, ghost_size);

    close(fd);
    return 0;
}

int load_game_state(board_t *board) {
    char path[PATH_MAX];
    get_save_path_board(board, path, sizeof(path));

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        debug("Error opening file for load: %s\n", strerror(errno));
        return -1;
    }

    // Clean up current memory
    unload_level(board);

    // Read scalars
    read(fd, &board->width, sizeof(int));
    read(fd, &board->height, sizeof(int));
    read(fd, &board->tempo, sizeof(int));
    read(fd, &board->level_count, sizeof(int));
    read(fd, &board->current_level, sizeof(int));
    read(fd, &board->n_pacmans, sizeof(int));
    read(fd, &board->n_ghosts, sizeof(int));

    // Read strings
    read(fd, board->base_dir, MAX_FILENAME);
    read(fd, board->pacman_file, MAX_FILENAME);
    read(fd, board->level_name, 256);

    // Read arrays of strings
    read(fd, board->level_files, sizeof(char) * MAX_LEVELS * MAX_FILENAME);
    read(fd, board->ghosts_files, sizeof(char) * MAX_GHOSTS * 256);

    // Allocate memory
    board->board = calloc(board->width * board->height, sizeof(board_pos_t));
    board->pacmans = calloc(board->n_pacmans, sizeof(pacman_t));
    board->ghosts = calloc(board->n_ghosts, sizeof(ghost_t));

    if (!board->board || !board->pacmans || !board->ghosts) {
        debug("Allocation failed during load\n");
        close(fd);
        return -1;
    }

    // Read dynamic content
    size_t board_size = board->width * board->height * sizeof(board_pos_t);
    read(fd, board->board, board_size);

    size_t pac_size = board->n_pacmans * sizeof(pacman_t);
    read(fd, board->pacmans, pac_size);

    size_t ghost_size = board->n_ghosts * sizeof(ghost_t);
    read(fd, board->ghosts, ghost_size);

    close(fd);

    // Delete save file after loading
    unlink(path);

    return 0;
}