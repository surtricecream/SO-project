#include "board.h"
#include "display.h"
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>      
#include <errno.h>
#include <limits.h>     
#include <string.h>
#include <stdio.h> 

#define CONTINUE_PLAY 0
#define NEXT_LEVEL 1
#define QUIT_GAME 2
#define LOAD_BACKUP 3
#define CREATE_BACKUP 4

static int g_can_save = 1;

static void get_save_path(board_t *b, char *buffer, size_t size) {
    snprintf(buffer, size, "%s/.pacmanist.save", b->base_dir);
}

void screen_refresh(board_t * game_board, int mode) {
    debug("REFRESH\n");
    draw_board(game_board, mode);
    refresh_screen();
    if(game_board->tempo != 0)
        sleep_ms(game_board->tempo);       
}

int play_board(board_t * game_board) {
    pacman_t* pacman = &game_board->pacmans[0];
    command_t* play;
    if (pacman->n_moves == 0) { // if is user input
        command_t c; 
        c.command = get_input();

        if(c.command == '\0')
            return CONTINUE_PLAY;

        c.turns = 1;
        play = &c;
    }
    else { // else if the moves are pre-defined in the file
        // avoid buffer overflow wrapping around with modulo of n_moves
        // this ensures that we always access a valid move for the pacman
        play = &pacman->moves[pacman->current_move%pacman->n_moves];
    }

    debug("KEY %c\n", play->command);

    if (play->command == 'Q') {
        return QUIT_GAME;
    }

    if (play->command == 'G') {

        if (!g_can_save) {
            debug("Save denied: Save already used in this session.\n");
            return CONTINUE_PLAY;
        }

        char path[PATH_MAX];
        get_save_path(game_board, path, sizeof(path));

        // Only save if file does not exist
        if (access(path, F_OK) != 0) {
            pid_t pid = fork();
            if (pid < 0) {
                debug("Fork failed\n");
            } else if (pid == 0) {
                // Child process: Save and exit immediately
                // Using _exit to avoid flushing parent's stdio buffers
                save_game_state(game_board);
                _exit(0);
            } else {
                // Parent process: Continue game
                debug("Quicksave started in background (PID %d)\n", pid);
                // Optional: waitpid(pid, NULL, WNOHANG) to clean up zombies later
            }
        } else {
            debug("Save file already exists, ignoring G.\n");
        }
        return CONTINUE_PLAY;
    }

    int result = move_pacman(game_board, 0, play);
    if (result == REACHED_PORTAL) {
        // Next level
        return NEXT_LEVEL;
    }

    if (result == DEAD_PACMAN) {
        char path[PATH_MAX];
        get_save_path(game_board, path, sizeof(path));
        
        // If save exists, signal to load it
        if (access(path, F_OK) == 0) {
            return LOAD_BACKUP;
        }
        return QUIT_GAME;
    }
    
    for (int i = 0; i < game_board->n_ghosts; i++) {
        ghost_t* ghost = &game_board->ghosts[i];
        // avoid buffer overflow wrapping around with modulo of n_moves
        // this ensures that we always access a valid move for the ghost
        move_ghost(game_board, i, &ghost->moves[ghost->current_move%ghost->n_moves]);
    }

    if (!game_board->pacmans[0].alive) {
        char path[PATH_MAX];
        get_save_path(game_board, path, sizeof(path));
        if (access(path, F_OK) == 0) {
            return LOAD_BACKUP;
        }
        return QUIT_GAME;
    }      

    return CONTINUE_PLAY;  
}

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: %s <input_directory>\n", argv[0]);
        // TODO receive inputs
        return -1;
    }

    // Requirement: Cannot have a previous game save file
    char initial_path[PATH_MAX];
    snprintf(initial_path, sizeof(initial_path), "%s/.pacmanist.save", argv[1]);
    unlink(initial_path);

    // Random seed for any random movements
    srand((unsigned int)time(NULL));

    open_debug_file("debug.log");

    terminal_init();
    
    int accumulated_points = 0;
    bool end_game = false;
    board_t game_board=(board_t){0};
    //para ter os levels
    if(scan_directory_levels(argv[1], &game_board)<0){
        terminal_cleanup();

        close_debug_file();
        return 1;
    }

    while (!end_game) {
        char lvlpath[MAX_FILENAME *2];
        if(build_directory(game_board.base_dir, game_board.level_files[game_board.current_level],lvlpath,sizeof(lvlpath))==-1){
            break;
        }
        if(load_level_from_file(&game_board, lvlpath,accumulated_points)<0){
            break;
        }
        draw_board(&game_board, DRAW_MENU);
        refresh_screen();

        while(true) {
            int result = play_board(&game_board); 

            if(result == NEXT_LEVEL) {
                screen_refresh(&game_board, DRAW_WIN);
                sleep_ms(game_board.tempo);
                accumulated_points=game_board.pacmans->points;
                if(next_level(&game_board)==0){
                    end_game=true;
                }
                break;
            }

            if(result == LOAD_BACKUP) {
                if (load_game_state(&game_board) == 0) {
                    debug("Game reloaded from backup.\n");
                    
                    // Requirement: If I die and use the save file, I cannot save again that game
                    g_can_save = 0; 

                    draw_board(&game_board, DRAW_MENU);
                    refresh_screen();
                    continue; // Restart loop with loaded state
                } else {
                    debug("Failed to load backup.\n");
                    screen_refresh(&game_board, DRAW_GAME_OVER);
                    sleep_ms(game_board.tempo);
                    end_game = true;
                    break;
                }
            }

            if(result == QUIT_GAME) {
                screen_refresh(&game_board, DRAW_GAME_OVER); 
                sleep_ms(game_board.tempo);
                end_game = true;
                break;
            }
    
            screen_refresh(&game_board, DRAW_MENU); 

            accumulated_points = game_board.pacmans[0].points;      
        }
        print_board(&game_board);
        unload_level(&game_board);
    }    

    terminal_cleanup();

    close_debug_file();

    return 0;
}
