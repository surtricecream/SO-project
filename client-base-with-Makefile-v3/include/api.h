#ifndef API_H
#define API_H

typedef struct {
  int width;
  int height;
  int tempo;
  int victory;
  int game_over;
  int accumulated_points;
  char* data;
} Board;

int pacman_connect(char const *req_pipe_path, char const *notif_pipe_path, char const *server_pipe_path);

void pacman_play(char command);

/// @return 0 if the disconnection was successful, 1 otherwise.
int pacman_disconnect();

Board receive_board_update(void);

#endif