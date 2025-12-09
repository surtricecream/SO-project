#include "board.h"
#include "display.h"
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define CONTINUE_PLAY 0
#define NEXT_LEVEL 1
#define QUIT_GAME 2
#define LOAD_BACKUP 3
#define CREATE_BACKUP 4
#define EXIT_QUIT 0
#define EXIT_RELOAD 42
#define EXIT_WIN 100

typedef struct {
    board_t* board;
    int ghost_index;
} ghost_args_t;

void* ghost_thread(void* arg);
void* render_thread(void* arg);

void screen_refresh(board_t * game_board, int mode) {
    debug("REFRESH\n");
    draw_board(game_board, mode);
    refresh_screen();
    if(game_board->tempo != 0)
        sleep_ms(game_board->tempo);       
}

void* pacman_thread(void* arg) {
    board_t* board = (board_t*)arg;
    pacman_t* pacman = &board->pacmans[0];
    
    // State for child process management
    int is_child = 0;
    pthread_t child_t_render;
    pthread_t child_t_ghosts[MAX_GHOSTS];

    while (board->game_running) {
        command_t cmd;
        
        if (pacman->n_moves == 0) {
            cmd.command = get_input(); 
            if (cmd.command == '\0') {
                sleep_ms(10); 
                continue;
            }

            if (cmd.command == 'Q') {
                pthread_rwlock_wrlock(&board->board_lock);
                board->game_running = 0;
                pthread_rwlock_unlock(&board->board_lock);
                break;
            }

            // --- FORK / SAVE LOGIC ---
            if (cmd.command == 'G') {
                if (is_child) {
                    // Prevent nested saves for simplicity
                    continue; 
                }

                // 1. Acquire Write Lock (Pauses Parent Threads)
                pthread_rwlock_wrlock(&board->board_lock);
                
                pid_t pid = fork();
                
                if (pid < 0) {
                    // Error
                    pthread_rwlock_unlock(&board->board_lock);
                } 
                else if (pid == 0) {
                    // --- CHILD PROCESS ---
                    is_child = 1;
                    
                    // The inherited lock state is invalid because the owner TID changed.
                    // This resets it to a clean, unlocked state.
                    pthread_rwlock_init(&board->board_lock, NULL);
                    
                    // Recreate threads
                    pthread_create(&child_t_render, NULL, render_thread, board);
                    for(int i = 0; i < board->n_ghosts; i++) {
                        ghost_args_t* args = malloc(sizeof(ghost_args_t));
                        args->board = board;
                        args->ghost_index = i;
                        pthread_create(&child_t_ghosts[i], NULL, ghost_thread, args);
                    }
                    
                    // Continue the loop as the active game
                    continue;
                } 
                else {
                    // --- PARENT PROCESS (BACKUP) ---
                    // Wait for child. Lock is held, so other threads are paused.
                    int status;
                    waitpid(pid, &status, 0);
                    
                    if (WIFEXITED(status)) {
                        int code = WEXITSTATUS(status);
                        if (code == EXIT_RELOAD) {
                            // Child died, reload requested.
                            // Resume execution from here (state preserved)
                            pthread_rwlock_unlock(&board->board_lock);
                            // Force refresh to clear any child artifacts
                            draw_board(board, DRAW_MENU);
                            refresh_screen();
                            continue;
                        } else if (code == EXIT_WIN) {
                            // Child won.
                            board->level_finished = 1;
                            board->game_running = 0;
                            pthread_rwlock_unlock(&board->board_lock);
                            break;
                        } else {
                            // Quit (0) or others
                            board->game_running = 0;
                            pthread_rwlock_unlock(&board->board_lock);
                            break;
                        }
                    } else {
                        // Abnormal exit
                        board->game_running = 0;
                        pthread_rwlock_unlock(&board->board_lock);
                        break;
                    }
                }
            }
            
            cmd.turns = 1;
        } else {
            cmd = pacman->moves[pacman->current_move % pacman->n_moves];
            sleep_ms(board->tempo); 
        }

        pthread_rwlock_wrlock(&board->board_lock);
        if (!board->game_running) {
            pthread_rwlock_unlock(&board->board_lock);
            break;
        }

        int result = move_pacman(board, 0, &cmd);
        
        if (result == REACHED_PORTAL) {
            if (is_child) {
                pthread_rwlock_unlock(&board->board_lock);
                board->level_finished = 1;
                board->game_running = 0;
            } else {
                board->level_finished = 1;
                board->game_running = 0;
            }
        } else if (result == DEAD_PACMAN || !board->pacmans[0].alive) {
            if (is_child) {
                pthread_rwlock_unlock(&board->board_lock);
                board->game_running = 0; // Break loop to cleanup
            } else {
                board->game_running = 0; // Game Over (No save)
            }
        }
        
        pthread_rwlock_unlock(&board->board_lock);
        sleep_ms(10); 
    }

    // --- CLEANUP ---
    if (is_child) {
        // Stop threads
        board->game_running = 0; 
        pthread_join(child_t_render, NULL);
        for(int i = 0; i < board->n_ghosts; i++) {
            pthread_join(child_t_ghosts[i], NULL);
        }
        
        // Determine exit code
        if (board->level_finished) exit(EXIT_WIN);
        if (!board->pacmans[0].alive) exit(EXIT_RELOAD);
        exit(EXIT_QUIT);
    }

    return NULL;
}

void* ghost_thread(void* arg) {
    ghost_args_t* args = (ghost_args_t*)arg;
    board_t* board = args->board;
    int index = args->ghost_index;
    free(args); 

    while (board->game_running) {
        sleep_ms(board->tempo);

        pthread_rwlock_wrlock(&board->board_lock);
        if (!board->game_running) {
            pthread_rwlock_unlock(&board->board_lock);
            break;
        }

        ghost_t* ghost = &board->ghosts[index];
        command_t* cmd = &ghost->moves[ghost->current_move % ghost->n_moves];
        
        move_ghost(board, index, cmd);

        if (!board->pacmans[0].alive) {
            board->game_running = 0;
        }

        pthread_rwlock_unlock(&board->board_lock);
    }
    return NULL;
}

void* render_thread(void* arg) {
    board_t* board = (board_t*)arg;

    while (board->game_running) {
        pthread_rwlock_rdlock(&board->board_lock);
        
        draw_board(board, DRAW_MENU);
        refresh_screen();
        
        pthread_rwlock_unlock(&board->board_lock);

        sleep_ms(33);
    }
    return NULL;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: %s <input_directory>\n", argv[0]);
        return -1;
    }

    srand((unsigned int)time(NULL));
    open_debug_file("debug.log");
    terminal_init();
    
    int accumulated_points = 0;
    bool end_game = false;
    board_t game_board = (board_t){0};
    
    if(scan_directory_levels(argv[1], &game_board) < 0){
        terminal_cleanup();
        close_debug_file();
        return 1;
    }

    while (!end_game) {
        char lvlpath[MAX_FILENAME * 2];
        if(build_directory(game_board.base_dir, game_board.level_files[game_board.current_level], lvlpath, sizeof(lvlpath)) == -1){
            break;
        }
        if(load_level_from_file(&game_board, lvlpath, accumulated_points) < 0){
            break;
        }

        pthread_t t_pacman, t_render;
        pthread_t t_ghosts[MAX_GHOSTS];

        pthread_create(&t_pacman, NULL, pacman_thread, &game_board);

        for(int i = 0; i < game_board.n_ghosts; i++) {
            ghost_args_t* args = malloc(sizeof(ghost_args_t));
            args->board = &game_board;
            args->ghost_index = i;
            pthread_create(&t_ghosts[i], NULL, ghost_thread, args);
        }

        pthread_create(&t_render, NULL, render_thread, &game_board);

        pthread_join(t_pacman, NULL);

        game_board.game_running = 0;

        pthread_join(t_render, NULL);
        for(int i = 0; i < game_board.n_ghosts; i++) {
            pthread_join(t_ghosts[i], NULL);
        }

        if (game_board.level_finished) {
            screen_refresh(&game_board, DRAW_WIN);
            sleep_ms(1000);
            accumulated_points = game_board.pacmans[0].points;
            if(next_level(&game_board) == 0){
                end_game = true;
            }
        } else {
            screen_refresh(&game_board, DRAW_GAME_OVER);
            sleep_ms(1000);
            end_game = true;
        }

        unload_level(&game_board);
    }    

    terminal_cleanup();
    close_debug_file();
    return 0;
}