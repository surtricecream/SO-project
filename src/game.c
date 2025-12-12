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

static int g_can_save = 1;      
static int g_is_child = 0;     
static int g_request_save = 0;  

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

    while (board->game_running) {
        command_t cmd;

        if (pacman->n_moves == 0) {
            pthread_rwlock_wrlock(&board->board_lock);
            char key = board->input;
            board->input = '\0';
            pthread_rwlock_unlock(&board->board_lock);

            if (key == '\0') { 
                sleep_ms(10);
                continue;
            }

            if (key == 'Q') {
                pthread_rwlock_wrlock(&board->board_lock);
                board->game_running = 0;
                pthread_rwlock_unlock(&board->board_lock);
                break;
            }
 
            if (key == 'G') {
                if (!g_is_child && g_can_save) {
                    g_request_save = 1;
                    g_can_save = 0;
                    pthread_rwlock_wrlock(&board->board_lock);
                    board->game_running = 0;
                    pthread_rwlock_unlock(&board->board_lock);
                    break;
                }
                sleep_ms(10);
                continue;
            }

            cmd.command = key;
            cmd.turns = 1;
        } else {
            cmd = pacman->moves[pacman->current_move % pacman->n_moves];
            sleep_ms(board->tempo);
        }

        pthread_rwlock_wrlock(&board->board_lock);
        if (!board->game_running) { pthread_rwlock_unlock(&board->board_lock); break; }

        int result = move_pacman(board, 0, &cmd);

        if (result == REACHED_PORTAL) {
            board->level_finished = 1;
            board->game_running = 0;
        } else if (result == DEAD_PACMAN || !board->pacmans[0].alive) {
            board->game_running = 0;
        }

        pthread_rwlock_unlock(&board->board_lock);
        sleep_ms(10);
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

        char key = get_input();
        if (key) {
            pthread_rwlock_wrlock(&board->board_lock);
            if (board->input == '\0') {
                board->input = key;
            }
            pthread_rwlock_unlock(&board->board_lock);
        }

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

    pthread_rwlock_init(&game_board.board_lock, NULL);
    int resuming = 0;

    while (!end_game) {
        if (!resuming) {
            char lvlpath[MAX_FILENAME * 2];
            if(build_directory(game_board.base_dir, game_board.level_files[game_board.current_level], lvlpath, sizeof(lvlpath)) == -1){
                break;
            }
            if(load_level_from_file(&game_board, lvlpath, accumulated_points) < 0){
                break;
            }
            game_board.input = '\0';
            game_board.game_running = 1;
        } else {
            game_board.game_running = 1;
            resuming = 0;
            
            if (g_is_child) {
                pthread_rwlock_init(&game_board.board_lock, NULL);
            }
            resuming = 0;
        }

pthread_t t_pacman, t_render;
        pthread_t t_ghosts[MAX_GHOSTS];

        pthread_create(&t_pacman, NULL, pacman_thread, &game_board);

        for (int i = 0; i < game_board.n_ghosts; i++) {
            ghost_args_t* args = malloc(sizeof(ghost_args_t));
            args->board = &game_board;
            args->ghost_index = i;
            pthread_create(&t_ghosts[i], NULL, ghost_thread, args);
        }

        pthread_create(&t_render, NULL, render_thread, &game_board);

        pthread_join(t_pacman, NULL);

        game_board.game_running = 0;

        pthread_join(t_render, NULL);
        for (int i = 0; i < game_board.n_ghosts; i++) {
            pthread_join(t_ghosts[i], NULL);
        }


        if (g_request_save) {
            g_request_save = 0;
            
            pid_t pid = fork();
            
            if (pid == 0) {
                g_is_child = 1;
                resuming = 1;
                continue;
            } else {
                int status;
                waitpid(pid, &status, 0);
                
                if (WIFEXITED(status)) {
                    int code = WEXITSTATUS(status);
                    if (code == EXIT_RELOAD) {
                        resuming = 1;
                        continue;
                    } else {
                        exit(0);
                    }
                } else {
                    exit(1);
                }
            }
        }

        if (game_board.level_finished) {
            accumulated_points = game_board.pacmans[0].points;
            if(next_level(&game_board) == 0){
                screen_refresh(&game_board, DRAW_WIN);
                sleep_ms(1000);
                end_game = true;
            }
        } else {
            if (g_is_child) {
                if (!game_board.pacmans[0].alive) {
                    exit(EXIT_RELOAD);
                } 
                exit(EXIT_QUIT);
            }

            screen_refresh(&game_board, DRAW_GAME_OVER);
            sleep_ms(1000);
            end_game = true;
        }    

        if (!resuming) {
            unload_level(&game_board);
        }
    }

    terminal_cleanup();
    close_debug_file();
    return 0;
}
