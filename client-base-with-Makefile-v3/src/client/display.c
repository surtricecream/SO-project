#include "display.h"
#include "board.h"
#include "api.h"
#include <stdlib.h>
#include <ctype.h>


int terminal_init() {
    // Initialize ncurses mode
    initscr();

    // Disable line buffering - get characters immediately
    cbreak();

    // Don't echo typed characters to the screen
    noecho();

    // Enable special keys (arrow keys, function keys, etc.)
    keypad(stdscr, TRUE);

    timeout(1000);

    // Make getch() non-blocking (return ERR if no input)
    // nodelay(stdscr, TRUE); // Uncomment if non-blocking input is desired

    // Hide the cursor
    curs_set(0);

    // Enable color if terminal supports it
    if (has_colors()) {
        start_color();

        // Define color pairs (foreground, background)
        init_pair(1, COLOR_YELLOW, COLOR_BLACK);  // Pacman
        init_pair(2, COLOR_RED, COLOR_BLACK);     // Ghosts
        init_pair(3, COLOR_BLUE, COLOR_BLACK);    // Walls
        init_pair(4, COLOR_WHITE, COLOR_BLACK);   // Points/dots
        init_pair(5, COLOR_GREEN, COLOR_BLACK);   // UI elements
        init_pair(6, COLOR_MAGENTA, COLOR_BLACK); // Extra
        init_pair(7, COLOR_CYAN, COLOR_BLACK);    // Extra
    }

    // Clear the screen
    clear();

    return 0;
}


void draw_board_client(Board board) {
    // Clear the screen before redrawing
    clear();

    // Draw the border/title
    attron(COLOR_PAIR(5));
    mvprintw(0, 0, "=== PACMAN GAME ===");
    if (board.game_over) {
        mvprintw(1, 0, " GAME OVER ");
    } else if (board.victory) {
        mvprintw(1, 0, " VICTORY ");
    } else {
        mvprintw(1, 0, " Use W/A/S/D to move | Q to quit");
    }

    // Starting row for the game board (leave space for UI)
    int start_row = 3;

    // Draw the board
    for (int y = 0; y < board.height; y++) {
        for (int x = 0; x < board.width; x++) {
            char ch = board.data[y * board.width + x];

            // Move cursor to position
            move(start_row + y, x);

            // Draw with appropriate color
            switch (ch) {
                case '#': // Wall
                    attron(COLOR_PAIR(3));
                    addch('#');
                    attroff(COLOR_PAIR(3));
                    break;

                case 'C': // Pacman
                    attron(COLOR_PAIR(1) | A_BOLD);
                    addch('C');
                    attroff(COLOR_PAIR(1) | A_BOLD);
                    break;

                case 'M': // Monster/Ghost
                    attron(COLOR_PAIR(2) | A_BOLD);
                    addch('M');
                    attroff(COLOR_PAIR(2) | A_BOLD);  
                    break;

                case 'G': // Charged Monster/Ghost
                    attron((COLOR_PAIR(2) | A_BOLD) | A_DIM);
                    addch('M');
                    attroff((COLOR_PAIR(2) | A_BOLD) | A_DIM);  
                    break;

                case '.': // Dot
                    attron(COLOR_PAIR(4));
                    addch('.');
                    attroff(COLOR_PAIR(4));
                    break;

                case '@': // Portal
                    attron(COLOR_PAIR(6));
                    addch('@');
                    attroff(COLOR_PAIR(6));
                    break;

                case ' ': // Empty space
                    addch(' ');
                    break;

                default:
                    addch(ch);
                    break;
            }
        }
    }

    // Draw score/status at the bottom
    attron(COLOR_PAIR(5));
    mvprintw(start_row + board.height + 1, 0, "Points: %d",
             board.accumulated_points);
    attroff(COLOR_PAIR(5));
}

// Does exaclty the same as draw board but stores the output in a string instead of printing it
char* get_board_displayed(board_t* board) {
    size_t buffer_size = (board->width  * board->height) + 1;
    char* output = malloc(buffer_size);
    size_t pos = 0;
    for (int y = 0; y < board->height; y++) {
        for (int x = 0; x < board->width; x++) {
            int index = y * board->width + x;
            char ch = board->board[index].content;
            int ghost_charged = 0;

            for (int g = 0; g < board->n_ghosts; g++) {
                ghost_t* ghost = &board->ghosts[g];
                if (ghost->pos_x == x && ghost->pos_y == y) {
                    if (ghost->charged)
                        ghost_charged = 1;
                    break;
                }
            }

            // Draw with appropriate character
            switch (ch) {
                case 'W': // Wall
                    output[pos++] = '#';
                    break;

                case 'P': // Pacman
                    output[pos++] = 'C';
                    break;

                case 'M': // Monster/Ghost
                    if (ghost_charged) {
                        output[pos++] = 'G'; 
                    } else {
                        output[pos++] = 'M';
                    }
                    break;

                case ' ': // Empty space
                    if (board->board[index].has_portal) {
                        output[pos++] = '@';
                    }
                    else if (board->board[index].has_dot) {
                        output[pos++] = '.';
                    }
                    else
                        output[pos++] = ' ';
                    break;

                default:
                    output[pos++] = ch;
                    break;
            }
        }
    }
    
    output[pos] = '\0';
    return output;
}

void draw_board(board_t* board, int mode) {
    // Clear the screen before redrawing
    clear();

    // Draw the border/title
    attron(COLOR_PAIR(5));
    mvprintw(0, 0, "=== PACMAN GAME ===");
    switch(mode) {
    case DRAW_GAME_OVER:
        mvprintw(1, 0, " GAME OVER ");
        break;

    case DRAW_WIN:
        mvprintw(1, 0, " VICTORY ");
        break;

    case DRAW_MENU:
        mvprintw(1, 0, "Level: %s | Use W/A/S/D to move | Q to quit | G to quicksave ", board->level_name);
        break;
    }


    // Starting row for the game board (leave space for UI)
    int start_row = 3;

    // Draw the board
    for (int y = 0; y < board->height; y++) {
        for (int x = 0; x < board->width; x++) {
            int index = y * board->width + x;
            char ch = board->board[index].content;
            int ghost_charged = 0;

            for (int g = 0; g < board->n_ghosts; g++) {
                ghost_t* ghost = &board->ghosts[g];
                if (ghost->pos_x == x && ghost->pos_y == y) {
                    if (ghost->charged)
                        ghost_charged = 1;
                    break;
                }
            }

            // Move cursor to position
            move(start_row + y, x);

            // Draw with appropriate color
            switch (ch) {
                case 'W': // Wall
                    attron(COLOR_PAIR(3));
                    addch('#');
                    attroff(COLOR_PAIR(3));
                    break;

                case 'P': // Pacman
                    attron(COLOR_PAIR(1) | A_BOLD);
                    addch('C');
                    attroff(COLOR_PAIR(1) | A_BOLD);
                    break;

                case 'M': // Monster/Ghost
                    attron((COLOR_PAIR(2) | A_BOLD) | ((ghost_charged) ? (A_DIM) : (0)));
                    addch('M');
                    attroff((COLOR_PAIR(2) | A_BOLD) | ((ghost_charged) ? (A_DIM) : (0)));
                    break;

                case ' ': // Empty space
                    if (board->board[index].has_portal) {
                        attron(COLOR_PAIR(6));
                        addch('@');
                        attroff(COLOR_PAIR(6));
                    }
                    else if (board->board[index].has_dot) {
                        attron(COLOR_PAIR(4));
                        addch('.');
                        attroff(COLOR_PAIR(4));
                    }
                    else
                        addch(' ');
                    break;

                default:
                    addch(ch);
                    break;
            }
        }
    }

    // Draw score/status at the bottom
    attron(COLOR_PAIR(5));
    mvprintw(start_row + board->height + 1, 0, "Points: %d",
             board->pacmans[0].points); // Assuming first pacman for now
    attroff(COLOR_PAIR(5));
}

void draw(char c, int colour_i, int pos_x, int pos_y) {
    move(pos_y, pos_x);
    attron(COLOR_PAIR(colour_i) | A_BOLD);
    addch(c);
    attroff(COLOR_PAIR(colour_i) | A_BOLD);
}

void refresh_screen() {
    // Update the physical screen with the virtual screen
    refresh();
}

char get_input() {
    // Get a character from the keyboard
    int ch = getch();

    // getch() returns ERR if no input is available
    if (ch == ERR) {
        return '\0'; // No input
    }

    ch = toupper((char)ch);

    switch ((char)ch) {
        case 'W':
        case 'S':
        case 'A':
        case 'D':
        case 'Q':
        case 'G':

            return (char)ch;
        
        default:
            return '\0';
    }
}

void terminal_cleanup() {
    // Restore terminal settings and clean up ncurses
    endwin();
}

void set_timeout(int timeout_ms) {
    timeout(timeout_ms);
}