#include "board.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>   
#include <unistd.h> 
#include <errno.h>

/*
This function is to build the rest od the path knowing the base directory
*/
int build_directory(const char* baseDir,const char* entity_name, char* entitypath, size_t entitysize){
    DIR *dirp=opendir(baseDir);
    if(dirp==NULL){
        return -1;
    }
    struct dirent *dp;
    errno=0;
    for(;;){
        dp=readdir(dirp);
        if (dp == NULL) {
            break;
        }
        if(errno!=0){
            closedir(dirp);
            return -1;
        }
        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)continue;
        
        if (strcmp(dp->d_name, entity_name)==0){
            snprintf(entitypath,entitysize,"%s/%s",baseDir,entity_name);
            closedir(dirp);
            return 1;
        }
    }
    closedir(dirp);
    return 0;
}
/*
This function is to check if it ends with .lvl, or .m or .p
*/
static int ends_with(const char *s, const char *suffix) {
    if (s == NULL || suffix == NULL) return 0;
    size_t s_len = strlen(s);
    size_t suf_len = strlen(suffix);

    if (suf_len == 0) return 1;          // sufixo vazio dá sempre certo
    if (suf_len > s_len) return 0;
    const char *start = s + (s_len - suf_len);
    return strncmp(start, suffix, suf_len) == 0; 
}


/* 
This functions will guard all the filepath of the levels and make a base dir
to allow to search for the rest of the entities monster and pacman
*/
int scan_directory_levels(const char* dir, board_t* board){
    DIR *dirp=opendir(dir);
    if(dirp==NULL){
        return -1;
    }
    struct dirent *dp;
    strncpy(board->base_dir, dir, MAX_FILENAME - 1);
    board->base_dir[MAX_FILENAME - 1] = '\0';
    board->level_count = 0;
    board->current_level = 0;
    for(;;){
        errno=0;
        dp=readdir(dirp);
        if (dp == NULL) {
            break;
        }
        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)continue;
        if (ends_with(dp->d_name,".lvl")==1){
            strncpy(board->level_files[board->level_count],dp->d_name,MAX_FILENAME-1);
            board->level_files[board->level_count][MAX_FILENAME-1]='\0';        
            board->level_count++;
        }
    }
    closedir(dirp);
    return 0;
}

int parse_level_file(board_t* board, const char *lvl_path){
    
    int fd= open(lvl_path,O_RDONLY);

    size_t used =0;
    size_t cap = 8192;

    if (fd < 0){
        return -1;
    }

    char *buf = (char*)malloc(cap);
    if (buf == NULL) { 
        close(fd); 
        return -1; 
    }

    for(;;){
        if (cap-used< 4096){
            size_t new_cap = cap *2;
            char *temp = (char*)realloc(buf, new_cap);
            if (temp==NULL) {
                free(buf);
                close(fd);
                return -1;
            }
            buf = temp; 
            cap = new_cap;
        }
        ssize_t readchars =read(fd,buf+used,cap-used);

        if (readchars > 0) {
            used += (size_t)readchars;
        }
        else if (readchars == 0){
            break;
        } 
        else{
            free(buf); 
            close(fd);
            return -1;   
        }

    }
    close(fd);
    if (used == cap) {
        char *temp = realloc(buf, cap + 1);
        if (!temp) {
            free(buf);
            return -1;
        }
        buf = temp;
    }
    buf[used]= '\0';

    board->tempo=0;
    board->n_ghosts=0;
    board->pacman_file[0]='\0';
    board->n_pacmans=1;
    char *save = NULL;
    char *line = strtok_r(buf, "\n", &save);
    char *game_line = NULL;
 

    while(line){
        int entered=0;
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\r') line[len-1] = '\0';

        if (line[0] == '#'){
            line = strtok_r(NULL,"\n",&save);
            entered=1;
        }
        else if (line[0] == 'D'){
            sscanf(line,"DIM %d %d",&board->width, &board->height);
        }
        else if (line[0] == 'T'){
            sscanf(line,"TEMPO %d", &board->tempo);
        }
        else if (line[0]=='P'){
            sscanf(line, "PAC %s", board->pacman_file);
        }
        else if(line[0]=='M'){
            char *p=line+3;
            int letters_used=0;
            char name[MAX_FILENAME+1];
            while(*p){
                if (*p == ' '){
                    p++;
                }
                if (sscanf(p,"%s%n", name,&letters_used)==1){
                    if(ends_with(name,".m") && board->n_ghosts<MAX_GHOSTS && letters_used<MAX_FILENAME){
                        strncpy(board->ghosts_files[board->n_ghosts],name, MAX_FILENAME-1);
                        board->ghosts_files[board->n_ghosts][MAX_FILENAME-1]='\0';
                        board->n_ghosts++;
                    }
                    p+=letters_used;
                }
                else{
                    break;
                }
            }
        }
        else{
            game_line=line;
            break;
        }
        if(line[0]!='X' && line[0]!='o' && line[0]!='@' && entered==0){
            line = strtok_r(NULL, "\n", &save);
        }

    }
    //Criar o espaço para a grelha e o numero de bixos e pacman
    if(board->height>0 && board->width>0){
        board->board = calloc(board->width * board->height, sizeof(board_pos_t));
        board->pacmans = calloc(board->n_pacmans, sizeof(pacman_t));
        board->ghosts = calloc(board->n_ghosts, sizeof(ghost_t));

        if(board->board==NULL || board->ghosts==NULL || board->pacmans==NULL){
            free(buf);
            free(board->board);
            free(board->pacmans);
            free(board->ghosts);
            return -1;
        }
                // inicializar toda a grelha
    }
    else{
        free(buf);
    }

    for(int y=0; y<board->height; y++){
        if(game_line==NULL){
            break;
        }
        
        for(int i=0;i<board->width;i++){
            if (game_line[i]=='X'){
                board->board[y*board->width+i].content= 'W';
            }
            else if (game_line[i]=='o'){
                board->board[y*board->width+i].content= ' ';
                board->board[y*board->width+i].has_dot=1;
            }
            else if (game_line[i]=='@'){
                board->board[y*board->width+i].content= ' ';
                board->board[y*board->width+i].has_portal=1;            
            }
        }
        game_line = strtok_r(NULL, "\n", &save);
    }


    free(buf);
    return 0;
}


int parse_entity_file(command_t* moves, int* n_moves, int* passo, int* pos_x, int* pos_y, const char* filepath){
    int fd= open(filepath,O_RDONLY);

    size_t used =0;
    size_t cap = 8192;

    if (fd < 0){
        return -1;
    }

    char *buf = (char*)malloc(cap);
    if (buf == NULL) { 
        close(fd); 
        return -1; 
    }

    for(;;){
        if (cap-used< 4096){
            size_t new_cap = cap *2;
            char *temp = (char*)realloc(buf, new_cap);
            if (temp==NULL) {
                free(buf);
                close(fd);
                return -1;
            }
            buf = temp; 
            cap = new_cap;
        }
        ssize_t readchars =read(fd,buf+used,cap-used);

        if (readchars > 0) {
            used += (size_t)readchars;
        }
        else if (readchars == 0){
            break;
        } 
        else{
            free(buf); 
            close(fd);
            return -1;   
        }
    }
    close(fd);
    if (used == cap) {
        char *temp = realloc(buf, cap + 1);
        if (!temp) {
            free(buf);
            return -1;
        }
        buf = temp;
    }
    buf[used]= '\0';

    *n_moves = 0;
    *passo = 0;
    *pos_x = 0;
    *pos_y = 0;

    char *save = NULL;
    char *line = strtok_r(buf, "\n", &save);
    char c;
    while(line){
        int entered=0;
        if (line[0] == '#'){
            line = strtok_r(NULL,"\n",&save);
            entered=1;
        }
        else if (line[0] == 'P' && line[1] == 'A'){ //PASSO
            sscanf(line,"PASSO %d", passo);
        }
        else if (line[0] == 'P' && line[1] == 'O'){
            sscanf(line,"POS %d %d", pos_y, pos_x);
        }
        else if (sscanf(line,"%c", &c)==1){
            if(*n_moves<MAX_MOVES){
                moves[*n_moves].command = c;
                moves[*n_moves].turns = 1;
                moves[*n_moves].turns_left = 1;
                (*n_moves)++;
            }
            
        }
        if(entered==0){
            line = strtok_r(NULL,"\n",&save);
        }

    }
    free(buf);
    return 0;
}
