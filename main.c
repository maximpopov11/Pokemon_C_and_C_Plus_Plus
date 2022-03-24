#include <stdlib.h>
#include <time.h>
#include <limits.h>
#include <string.h>
#include <math.h>
#include <getopt.h>
#include <ncurses.h>
#include "heap.h"

#define SCREEN_HEIGHT 24
#define TILE_WIDTH_X 80
#define TILE_LENGTH_Y 21
#define WORLD_WIDTH_X 399
#define WORLD_LENGTH_Y 399
#define WORLD_CENTER_X 199
#define WORLD_CENTER_Y 199
#define COMMAND_MAX_SIZE 256
#define TERRAIN_BORDER_WEIGHT 1
#define MINIMUM_TURN 5
//77 = minimum number of paths in tile - 1 for PC so all trainers can be placed
#define MAX_NUM_TRAINERS 77

//Author Maxim Popov
enum character_type {
    PLAYER,
    RIVAL,
    HIKER,
    RANDOM_WALKER,
    PACER,
    WANDERER,
    STATIONARY
};

struct terrain {
    //id is for comparison
    int id;
    char printable_character;
    int path_weight;
    int pc_weight;
    int rival_weight;
    int hiker_weight;
    char color[10];
};

struct terrain none = {0,'_', 0, 0, 0, 0, "\033[0;30m"};
struct terrain edge = {1, '%', INT_MAX, INT_MAX, INT_MAX, INT_MAX, "\033[0;37m"};
struct terrain clearing =
        {2, '.', 5, 10, 10, 5, "\033[0;33m"};
struct terrain grass =
        {3, ',', 10, 15, 15, 5, "\033[0;32m"};
struct terrain forest = {4, '^', 100, INT_MAX, INT_MAX, 10, "\033[0;32m"};
struct terrain mountain = {5, '%', 150, INT_MAX, INT_MAX, 10, "\033[0;37m"};
struct terrain lake = {6, '~', 200, INT_MAX, INT_MAX, INT_MAX, "\033[0;34m"};
struct terrain path = {7,'#', 0, 5, 5, 5, "\033[0;30m"};
struct terrain center = {8, 'C', INT_MAX, 5, INT_MAX, INT_MAX, "\033[0;35m"};
struct terrain mart = {9, 'M', INT_MAX, 5, INT_MAX, INT_MAX, "\033[0;35m"};

struct character {
    int x;
    int y;
    enum character_type type_enum;
    char *type_string;
    char printable_character;
    char color[10];
    int turn;
    int direction_set;
    int x_direction;
    int y_direction;
    int in_building;
    int defeated;
};

struct point {
    int x;
    int y;
    struct terrain terrain;
    //for looped non-queue plant_seeds growth
    struct terrain grow_into;
    struct character *character;
    int distance;
    heap_node_t *heap_node;
};

struct tile {
    struct point tile[TILE_LENGTH_Y][TILE_WIDTH_X];
    int x;
    int y;
    int north_x;
    int south_x;
    int east_y;
    int west_y;
    struct character *player_character;
};

int rival_distance_tile[TILE_LENGTH_Y][TILE_WIDTH_X];
int hiker_distance_tile [TILE_LENGTH_Y][TILE_WIDTH_X];

static int32_t comparator_trainer_distance_tile(const void *key, const void *with) {
    return ((struct point *) key)->distance - ((struct point *) with)->distance;
}

static int32_t comparator_character_movement(const void *key, const void *with) {
    return ((struct character *) key)->turn - ((struct character *) with)->turn;
}

int print_usage();
int initialize_terminal();
int turn_based_movement(struct tile *tile, struct heap *turn_heap);
int player_turn(struct tile *tile, struct character *player_character, struct heap *turn_heap);
int move_character(struct tile *tile, int x, int y, int new_x, int new_y);
int combat(struct character *from_character, struct character *to_character);
int enter_center(struct character *player_character);
int enter_mart(struct character *player_character);
int interaction(struct heap *turn_heap);
int change_tile(int x, int y, struct heap *turn_heap);
struct tile create_tile(int x, int y, struct heap *turn_heap);
struct tile create_empty_tile();
int generate_terrain(struct tile *tile);
int plant_seeds(struct tile *tile, struct terrain terrain, int num_seeds);
int grow_seeds(struct tile *tile);
int place_edge(struct tile *tile);
int set_terrain_border_weights(struct tile *tile);
int generate_paths(struct tile *tile, int north_x, int south_x, int east_y, int west_y);
int generate_buildings(struct tile *tile, int x, int y);
int place_building(struct tile *tile, struct terrain terrain, double chance);
int place_player_character(struct tile *tile, struct heap *turn_heap);
int place_trainers(struct tile *tile, struct heap *turn_heap);
int place_trainer_type(struct tile *tile, struct heap *turn_heap, int num_trainer, enum character_type trainer_type,
        char character);
int dijkstra(struct tile *tile, enum character_type trainer_type);
int legal_overwrite(struct point point);
double distance(int x1, int y1, int x2, int y2);
int print_tile_terrain(struct tile *tile);
int reset_color();
int print_tile_trainer_distances(struct tile *tile);
int print_tile_trainer_distances_printer(struct tile *tile);

struct tile *world[WORLD_LENGTH_Y][WORLD_WIDTH_X] = {0};
int num_trainers;

int main(int argc, char *argv[]) {

    //get arguments
    int opt = 0;
    int numtrainers = 10;
    static struct option long_options[] = {
            {"numtrainers", required_argument,0,'t' },
            {0,0,0,0   }
    };
    int long_index =0;
    while ((opt = getopt_long(argc, argv,"t:", long_options, &long_index )) != -1) {
        switch (opt) {
            case 't' : numtrainers = atoi(optarg);
                break;
            default: print_usage();
                exit(EXIT_FAILURE);
        }
    }

    //check argument legality
    if (numtrainers < 0) {
        numtrainers = 0;
    }
    else if (numtrainers > MAX_NUM_TRAINERS) {
        numtrainers = MAX_NUM_TRAINERS;
    }
    num_trainers = numtrainers;

    //run program
    srand(time(NULL));
    initialize_terminal();
    struct heap turn_heap;
    heap_init(&turn_heap, comparator_character_movement, NULL);
    struct tile home_tile = create_tile(WORLD_CENTER_X, WORLD_CENTER_Y, &turn_heap);
    world[WORLD_CENTER_Y][WORLD_CENTER_X] = &home_tile;
    turn_based_movement(&home_tile, &turn_heap);
    endwin();
    return 0;

}

int print_usage() {

    //print expected inputs

    return 0;

}

int initialize_terminal() {

    initscr();
    raw();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);

    return 0;

}

int turn_based_movement(struct tile *tile, struct heap *turn_heap) {

    static struct character *character;
    while ((character = heap_remove_min(turn_heap))) {
        if (character->type_enum == PLAYER) {
            clear();
            addstr("It's your turn! Enter a command or press z for help!\n");
            print_tile_terrain(tile);
            refresh();
             if (player_turn(tile, character, turn_heap) == 1) {
                 return 1;
             }
        }
        else if (character->type_enum == RIVAL) {
            if (character->defeated == 1) {
                //no longer paths to PC
                character->turn += MINIMUM_TURN;
            } else {
                //find a legal point to change_tile to
                int new_x;
                int new_y;
                int new_distance = INT_MAX;
                for (int x = -1; x <= 1; x++) {
                    for (int y = -1; y <= 1; y++) {
                        int candidate_x = character->x + x;
                        int candidate_y = character->y + y;
                        if (candidate_x > 0 && candidate_x < TILE_WIDTH_X && candidate_y > 0 &&
                            candidate_y < TILE_LENGTH_Y
                            && rival_distance_tile[candidate_y][candidate_x] != INT_MAX
                            && (tile->tile[candidate_y][candidate_x].character == NULL
                                || (tile->tile[candidate_y][candidate_x].character->type_enum == PLAYER &&
                                    character->defeated == 0))) {
                            if (rival_distance_tile[candidate_y][candidate_x] < new_distance) {
                                new_x = candidate_x;
                                new_y = candidate_y;
                                new_distance = rival_distance_tile[candidate_y][candidate_x];
                            }
                        }
                    }
                }
                if (new_distance != INT_MAX) {
                    //if legal point to move to found, change_tile there
                    move_character(tile, character->x, character->y, new_x, new_y);
                    character->turn += tile->tile[new_y][new_x].terrain.rival_weight;
                } else {
                    //no legal point to change_tile to found
                    character->turn += MINIMUM_TURN;
                }
            }
        }
        else if (character->type_enum == HIKER) {
            if (character->defeated == 1) {
                //no longer paths to PC
                character->turn += MINIMUM_TURN;
            }
            else {
                int new_x;
                int new_y;
                int new_distance = INT_MAX;
                for (int x = -1; x <= 1; x++) {
                    for (int y = -1; y <= 1; y++) {
                        int candidate_x = character->x + x;
                        int candidate_y = character->y + y;
                        if (candidate_x > 0 && candidate_x < TILE_WIDTH_X && candidate_y > 0 &&
                            candidate_y < TILE_LENGTH_Y
                            && hiker_distance_tile[candidate_y][candidate_x] != INT_MAX
                            && (tile->tile[candidate_y][candidate_x].character == NULL
                                || (tile->tile[candidate_y][candidate_x].character->type_enum == PLAYER &&
                                    character->defeated == 0))) {
                            if (hiker_distance_tile[candidate_y][candidate_x] < new_distance) {
                                new_x = candidate_x;
                                new_y = candidate_y;
                                new_distance = hiker_distance_tile[candidate_y][candidate_x];
                            }
                        }
                    }
                }
                if (new_distance != INT_MAX) {
                    move_character(tile, character->x, character->y, new_x, new_y);
                    character->turn += tile->tile[new_y][new_x].terrain.hiker_weight;
                } else {
                    character->turn += MINIMUM_TURN;
                }
            }
        }
        else if (character->type_enum == RANDOM_WALKER) {
            int new_x = character->x + character->x_direction;
            int new_y = character->y + character->y_direction;
            //if we have a direction set and can continue in it
            if (character->direction_set == 1 && new_x > 0 && new_x < TILE_WIDTH_X && new_y > 0 && new_y < TILE_LENGTH_Y
            && tile->tile[new_y][new_x].terrain.rival_weight != INT_MAX
            && (tile->tile[new_y][new_x].character == NULL
            || (tile->tile[new_y][new_x].character->type_enum == PLAYER && character->defeated == 0))) {
                move_character(tile, character->x, character->y, new_x, new_y);
                character->turn += tile->tile[new_y][new_x].terrain.rival_weight;
            }
            //no direction set or can't continue in set direction
            else {
                //if at least 1 direction legal, choose randomly until legal direction is found
                int has_possible_direction = 0;
                for (int y = -1; y <= 1; y++) {
                    for (int x = -1; x <= 1; x++) {
                        if (x != 0 || y != 0) {
                            if (tile->tile[character->y + y][character->x + x].terrain.rival_weight != INT_MAX
                            && (tile->tile[character->y + y][character->x + x].character == NULL
                            || (tile->tile[character->y + y][character->x + x].character->type_enum == PLAYER
                            && character->defeated == 0))) {
                                has_possible_direction = 1;
                            }
                        }
                    }
                }
                if (has_possible_direction == 1) {
                    int found = 0;
                    int x;
                    int y;
                    while (found == 0) {
                        x = rand() % 3 - 1;
                        y = rand() % 3 - 1;
                        new_x = character->x + x;
                        new_y = character->y + y;
                        if ((x != 0 || y != 0) && new_x > 0 && new_x < TILE_WIDTH_X && new_y > 0 && new_y < TILE_LENGTH_Y
                        && tile->tile[new_y][new_x].terrain.rival_weight != INT_MAX
                        && (tile->tile[new_y][new_x].character == NULL
                        || (tile->tile[new_y][new_x].character->type_enum == PLAYER && character->defeated == 0))) {
                            found = 1;
                        }
                    }
                    character->x_direction = x;
                    character->y_direction = y;
                    character->direction_set = 1;
                    move_character(tile, character->x, character->y, new_x, new_y);
                    character->turn += tile->tile[new_y][new_x].terrain.rival_weight;
                }
                else {
                    character->turn += MINIMUM_TURN;
                }
            }
        }
        else if (character->type_enum == PACER) {
            int new_x = character->x + character->x_direction;
            int new_y = character->y + character->y_direction;
            //change_tile in direction
            if (character->direction_set == 1 && new_x > 0 && new_x < TILE_WIDTH_X && new_y > 0 && new_y < TILE_LENGTH_Y
                && tile->tile[new_y][new_x].terrain.rival_weight != INT_MAX
                && (tile->tile[new_y][new_x].character == NULL || tile->tile[new_y][new_x].character->type_enum == PLAYER)) {
                move_character(tile, character->x, character->y, new_x, new_y);
                character->turn += tile->tile[new_y][new_x].terrain.rival_weight;
            }
            //reverse direction
            else if (character->direction_set == 1) {
                character->x_direction *= -1;
                character->y_direction *= -1;
                int new_x = character->x + character->x_direction;
                int new_y = character->y + character->y_direction;
                move_character(tile, character->x, character->y, new_x, new_y);
            }
            //no direction yet
            else {
                //if at least 1 direction legal, choose randomly until legal direction is found
                int has_possible_direction = 0;
                for (int y = -1; y <= 1; y++) {
                    for (int x = -1; x <= 1; x++) {
                        if (x != 0 || y != 0) {
                            if (tile->tile[character->y + y][character->x + x].terrain.rival_weight != INT_MAX
                            && (tile->tile[character->y + y][character->x + x].character == NULL
                            || (tile->tile[character->y + y][character->x + x].character->type_enum == PLAYER
                            && character->defeated == 0))) {
                                has_possible_direction = 1;
                            }
                        }
                    }
                }
                if (has_possible_direction == 1) {
                    int found = 0;
                    int x;
                    int y;
                    while (found == 0) {
                        x = rand() % 3 - 1;
                        y = rand() % 3 - 1;
                        new_x = character->x + x;
                        new_y = character->y + y;
                        if ((x != 0 || y != 0) && new_x > 0 && new_x < TILE_WIDTH_X && new_y > 0 && new_y < TILE_LENGTH_Y
                            && tile->tile[new_y][new_x].terrain.rival_weight != INT_MAX
                            && (tile->tile[new_y][new_x].character == NULL
                            || (tile->tile[new_y][new_x].character == PLAYER && character->defeated == 0))) {
                            found = 1;
                        }
                    }
                    character->x_direction = x;
                    character->y_direction = y;
                    character->direction_set = 1;
                    move_character(tile, character->x, character->y, new_x, new_y);
                    character->turn += tile->tile[new_y][new_x].terrain.rival_weight;
                }
                else {
                    character->turn += MINIMUM_TURN;
                }
            }
        }
        else if (character->type_enum == WANDERER) {
            int new_x = character->x + character->x_direction;
            int new_y = character->y + character->y_direction;
            if (character->direction_set == 1 && new_x > 0 && new_x < TILE_WIDTH_X && new_y > 0 && new_y < TILE_LENGTH_Y
                && tile->tile[new_y][new_x].terrain.id == tile->tile[character->y][character->x].terrain.id
                && (tile->tile[new_y][new_x].character == NULL
                || (tile->tile[new_y][new_x].character->type_enum == PLAYER && character->defeated == 0))) {
                move_character(tile, character->x, character->y, new_x, new_y);
                character->turn += tile->tile[new_y][new_x].terrain.rival_weight;
            }
            else {
                //if at least 1 direction legal, choose randomly until legal direction is found
                int has_possible_direction = 0;
                for (int y = -1; y <= 1; y++) {
                    for (int x = -1; x <= 1; x++) {
                        if (x != 0 || y != 0) {
                            if ((tile->tile[character->y + y][character->x + x].terrain.id
                            == tile->tile[character->y][character->x].terrain.id)
                            && (tile->tile[character->y][character->x].character == NULL
                            || (tile->tile[character->y][character->x].character == PLAYER && character->defeated == 0))) {
                                has_possible_direction = 1;
                            }
                        }
                    }
                }
                if (has_possible_direction == 1) {
                    int found = 0;
                    int x;
                    int y;
                    while (found == 0) {
                        x = rand() % 3 - 1;
                        y = rand() % 3 - 1;
                        new_x = character->x + x;
                        new_y = character->y + y;
                        if ((x != 0 || y != 0) && new_x > 0 && new_x < TILE_WIDTH_X && new_y > 0 && new_y < TILE_LENGTH_Y
                            && tile->tile[new_y][new_x].terrain.id == tile->tile[character->y][character->x].terrain.id
                            && (tile->tile[new_y][new_x].character == NULL
                            || (tile->tile[new_y][new_x].character->type_enum == PLAYER && character->defeated == 0))) {
                            found = 1;
                        }
                    }
                    character->x_direction = x;
                    character->y_direction = y;
                    character->direction_set = 1;
                    move_character(tile, character->x, character->y, new_x, new_y);
                    character->turn += tile->tile[new_y][new_x].terrain.rival_weight;
                }
                else {
                    character->turn += MINIMUM_TURN;
                }
            }
        }
        else if (character->type_enum == STATIONARY) {
            character->turn += MINIMUM_TURN;
        }
        heap_insert(turn_heap, character);
    }
    heap_delete(turn_heap);

    return 0;

}

int player_turn(struct tile *tile, struct character *player_character, struct heap *turn_heap) {

    int turn_completed = 0;
    int in_help = 0;
    int x = player_character->x;
    int y = player_character->y;
    while (turn_completed == 0) {
        char input = getch();
        int moving = 0;
        int new_x = x;
        int new_y = y;
        //determine input
        if (input == '7' || input == 'y') {
            moving = 1;
            new_x--;
            new_y--;
        } else if (input == '8' || input == 'k') {
            moving = 1;
            new_y--;
        } else if (input == '9' || input == 'u') {
            moving = 1;
            new_x++;
            new_y--;
        } else if (input == '6' || input == 'l') {
            moving = 1;
            new_x++;
        } else if (input == '3' || input == 'n') {
            moving = 1;
            new_x++;
            new_y++;
        } else if (input == '2' || input == 'j') {
            moving = 1;
            new_y++;
        } else if (input == '1' || input == 'b') {
            moving = 1;
            new_x--;
            new_y++;
        } else if (input == '4' || input == 'h') {
            moving = 1;
            new_x--;
        } else if (input == '>') {
            if (tile->tile[y][x].terrain.id == center.id) {
                enter_center(player_character);
            } else if (tile->tile[y][x].terrain.id == mart.id) {
                enter_mart(player_character);
            } else {
                clear();
                addstr("There is no pokecenter or pokemart here so you can't enter one!\n");
                print_tile_terrain(tile);
            }
        } else if (input == '<') {
            if (player_character->in_building == 1) {
                clear();
                addstr("You have left the building!\n");
                print_tile_terrain(tile);
            } else {
                clear();
                addstr("You aren't in a building so you can't leave one!\n");
                print_tile_terrain(tile);
            }
        } else if (input == '5' || input == ' ' || input == '.') {
            player_character->turn += MINIMUM_TURN;
            turn_completed = 1;
        } else if (input == 't') {
            //todo: BUG: sometimes shows -North and -West (but also sometimes shows East correctly)
            struct character *trainers [num_trainers];
            int count = 0;
            for (int i = 1; i < TILE_LENGTH_Y - 1; i++) {
                for (int j = 1; j < TILE_WIDTH_X - 1; j++) {
                    struct character *character = tile->tile[i][j].character;
                    if (character != NULL && character->type_enum != PLAYER) {
                        trainers[count] = character;
                        count++;
                    }
                }
            }
            int position = 0;
            int screen_row = 1;
            int type_x = 0;
            int position_x = 19;
            int defeated_status_x = 40;
            clear();
            addstr("Trainer list: Press escape to return to the map\n");
            for (int i = position; i < position + SCREEN_HEIGHT - 1 && i < num_trainers; i++) {
                struct character *trainer = trainers[i];
                mvaddstr(screen_row, type_x, trainer->type_string);
                mvaddstr(screen_row, position_x, " ");
                if (trainer->y != player_character->y) {
                    char y_distance[3];
                    sprintf(y_distance, "%d ", trainer->y - player_character->y);
                    addstr(y_distance);
                    if (trainer->y < player_character->y) {
                        addstr("North ");
                    }
                    else {
                        addstr("South ");
                    }
                }
                if (trainer->x != player_character->x) {
                    char x_distance[3];
                    sprintf(x_distance, "%d ", trainer->x - player_character->x);
                    addstr(x_distance);
                    if (trainer->x < player_character->x) {
                        addstr("West");
                    }
                    else {
                        addstr("East");
                    }
                }
                if (trainer->defeated == 1) {
                    mvaddstr(screen_row, defeated_status_x, "Defeated");
                }
                addstr("\n");
                screen_row++;
            }
            refresh();
            char command = '?';
            while (command != 27 && command != ACS_UARROW && command != ACS_DARROW) {
                command = getch();
                if (command == 27) {
                    turn_completed = 1;
                }
                else if (command == ACS_UARROW) {
                    //todo: BUG TEST: test scroll up
                    if (position > 1) {
                        position -= SCREEN_HEIGHT - 1;
                        if (position < 0) {
                            position = 0;
                        }
                        clear();
                        addstr("Trainer list: Press escape to return to the map\n");
                        for (int i = position; i < position + SCREEN_HEIGHT - 1 && i < num_trainers; i++) {
                            struct character *trainer = trainers[i];
                            mvaddstr(screen_row, type_x, trainer->type_string);
                            mvaddstr(screen_row, position_x, " ");
                            if (trainer->y != player_character->y) {
                                char y_distance[3];
                                sprintf(y_distance, "%d ", trainer->y - player_character->y);
                                addstr(y_distance);
                                if (trainer->y < player_character->y) {
                                    addstr("North ");
                                }
                                else {
                                    addstr("South ");
                                }
                            }
                            if (trainer->x != player_character->x) {
                                char x_distance[3];
                                sprintf(x_distance, "%d ", trainer->x - player_character->x);
                                addstr(x_distance);
                                if (trainer->x < player_character->x) {
                                    addstr("West");
                                }
                                else {
                                    addstr("East");
                                }
                            }
                            if (trainer->defeated == 1) {
                                mvaddstr(screen_row, defeated_status_x, "Defeated");
                            }
                            addstr("\n");
                            screen_row++;
                        }
                        refresh();
                    }
                    else {
                        clear();
                        addstr("You are already at the top of the list so you cannot scroll up.\n");
                        for (int i = position; i < position + SCREEN_HEIGHT - 1 && i < num_trainers; i++) {
                            struct character *trainer = trainers[i];
                            mvaddstr(screen_row, type_x, trainer->type_string);
                            mvaddstr(screen_row, position_x, " ");
                            if (trainer->y != player_character->y) {
                                char y_distance[3];
                                sprintf(y_distance, "%d ", trainer->y - player_character->y);
                                addstr(y_distance);
                                if (trainer->y < player_character->y) {
                                    addstr("North ");
                                }
                                else {
                                    addstr("South ");
                                }
                            }
                            if (trainer->x != player_character->x) {
                                char x_distance[3];
                                sprintf(x_distance, "%d ", trainer->x - player_character->x);
                                addstr(x_distance);
                                if (trainer->x < player_character->x) {
                                    addstr("West");
                                }
                                else {
                                    addstr("East");
                                }
                            }
                            if (trainer->defeated == 1) {
                                mvaddstr(screen_row, defeated_status_x, "Defeated");
                            }
                            addstr("\n");
                            screen_row++;
                        }
                        refresh();
                    }
                }
                else if (command == ACS_DARROW) {
                    //todo: BUG TEST: test scroll down
                    if (position < num_trainers - SCREEN_HEIGHT + 1) {
                        position += SCREEN_HEIGHT - 1;
                        clear();
                        addstr("Trainer list: Press escape to return to the map\n");
                        for (int i = position; i < position + SCREEN_HEIGHT - 1 && i < num_trainers; i++) {
                            struct character *trainer = trainers[i];
                            mvaddstr(screen_row, type_x, trainer->type_string);
                            mvaddstr(screen_row, position_x, " ");
                            if (trainer->y != player_character->y) {
                                char y_distance[3];
                                sprintf(y_distance, "%d ", trainer->y - player_character->y);
                                addstr(y_distance);
                                if (trainer->y < player_character->y) {
                                    addstr("North ");
                                }
                                else {
                                    addstr("South ");
                                }
                            }
                            if (trainer->x != player_character->x) {
                                char x_distance[3];
                                sprintf(x_distance, "%d ", trainer->x - player_character->x);
                                addstr(x_distance);
                                if (trainer->x < player_character->x) {
                                    addstr("West");
                                }
                                else {
                                    addstr("East");
                                }
                            }
                            if (trainer->defeated == 1) {
                                mvaddstr(screen_row, defeated_status_x, "Defeated");
                            }
                            addstr("\n");
                            screen_row++;
                        }
                        refresh();
                    }
                    else {
                        clear();
                        addstr("You are already at the bottom of the list so you cannot scroll down.\n");
                        for (int i = position; i < position + SCREEN_HEIGHT - 1 && i < num_trainers; i++) {
                            struct character *trainer = trainers[i];
                            mvaddstr(screen_row, type_x, trainer->type_string);
                            mvaddstr(screen_row, position_x, " ");
                            if (trainer->y != player_character->y) {
                                char y_distance[3];
                                sprintf(y_distance, "%d ", trainer->y - player_character->y);
                                addstr(y_distance);
                                if (trainer->y < player_character->y) {
                                    addstr("North ");
                                }
                                else {
                                    addstr("South ");
                                }
                            }
                            if (trainer->x != player_character->x) {
                                char x_distance[3];
                                sprintf(x_distance, "%d ", trainer->x - player_character->x);
                                addstr(x_distance);
                                if (trainer->x < player_character->x) {
                                    addstr("West");
                                }
                                else {
                                    addstr("East");
                                }
                            }
                            if (trainer->defeated == 1) {
                                mvaddstr(screen_row, defeated_status_x, "Defeated");
                            }
                            addstr("\n");
                            screen_row++;
                        }
                        refresh();
                    }
                }
                else {
                    //command is invalid
                    clear();
                    addstr("That is not a valid command! Press escape to return to the map.\n");
                    for (int i = position; i < position + SCREEN_HEIGHT - 1 && i < num_trainers; i++) {
                        struct character *trainer = trainers[i];
                        mvaddstr(screen_row, type_x, trainer->type_string);
                        mvaddstr(screen_row, position_x, " ");
                        if (trainer->y != player_character->y) {
                            char y_distance[3];
                            sprintf(y_distance, "%d ", trainer->y - player_character->y);
                            addstr(y_distance);
                            if (trainer->y < player_character->y) {
                                addstr("North ");
                            }
                            else {
                                addstr("South ");
                            }
                        }
                        if (trainer->x != player_character->x) {
                            char x_distance[3];
                            sprintf(x_distance, "%d ", trainer->x - player_character->x);
                            addstr(x_distance);
                            if (trainer->x < player_character->x) {
                                addstr("West");
                            }
                            else {
                                addstr("East");
                            }
                        }
                        if (trainer->defeated == 1) {
                            mvaddstr(screen_row, defeated_status_x, "Defeated");
                        }
                        addstr("\n");
                        screen_row++;
                    }
                    refresh();
                }
            }
        } else if (input == 'Q') {
            clear();
            addstr("Are you sure you want to quit (y/n)? All progress will be lost.\n");
            refresh();
            char quit = '?';
            while (quit != 'y' || quit != 'n') {
                quit = getch();
                if (quit == 'y') {
                    return 1;
                } else if (quit == 'n') {
                    clear();
                    addstr("It's your turn! Enter a command or press z for help!\n");
                    print_tile_terrain(tile);
                } else {
                    clear();
                    addstr("Please enter (y/n) to quit. If you quit all progress will be lost.\n");
                    refresh();
                }
            }
        } else if (input == 'z') {
            if (in_help == 0) {
                //enter help
                clear();
                addstr("Enter z to enter/leave the help menu.\n");
                addstr("Enter 9 or u to move one cell to the upper right.\n");
                addstr("Enter 8 or k to move one cell up.\n");
                addstr("Enter 7 or y to move one cell to the upper left.\n");
                addstr("Enter 6 or l to move one cell to the right.\n");
                addstr("Enter 5 or space or . to rest for a turn.\n");
                addstr("Enter 4 or h to move one cell to the left.\n");
                addstr("Enter 3 or n to move one cell to the lower right.\n");
                addstr("Enter 2 or j to move one cell down.\n");
                addstr("Enter 1 or b to move one cell to the lower left.\n");
                addstr("Enter > to enter a pokemart or pokecenter.\n");
                addstr("Enter < to leave a pokemart or pokecenter.\n");
                addstr("Enter t to display a list of trainers.\n");
                addstr("Enter up arrow to scroll up on the trainer list.\n");
                addstr("Enter down arrow to scroll up on the trainer list.\n");
                addstr("Enter escape to leave the trainer list.\n");
                addstr("Enter Q to quit the game.\n");
                refresh();
            }
            else {
                //exit help
                clear();
                addstr("It's your turn! Enter a command or press z for help!\n");
                print_tile_terrain(tile);
            }
            in_help = 1 - in_help;
        } else {
            clear();
            addstr("That is not a valid command. Enter z for help!\n");
            print_tile_terrain(tile);
            refresh();
        }

        //call movement function if moving
        if (moving == 1) {
            //if terrain can be crossed
            if (tile->tile[new_y][new_x].terrain.pc_weight == INT_MAX) {
                clear();
                addstr("You can't cross that kind of terrain!\n");
                print_tile_terrain(tile);
            }
            //if there is an undefeated trainer there
            else if (tile->tile[new_y][new_x].character != NULL && tile->tile[new_y][new_x].character->defeated != 0) {
                clear();
                addstr("You have already defeated that trainer so they are too scared to battle you again!");
                print_tile_terrain(tile);
            }
            //if you are exiting the map
            else if (new_y == 0 || new_y == TILE_LENGTH_Y - 1 || new_x == 0 || new_x == TILE_WIDTH_X - 1) {
                //todo: BUG TEST: test moving onto new tile
                //todo: BUG TEST: test moving onto new tile with large game time for trainers time being updated correctly

                //call change map
                    //if successful
                        //update pc coords for pc
                        //update pc coords for tile
                        //refactor dijkstra distance tiles
                        //change current tile (for turn based movement) to this tile
                    //else (tried to walk out of world)
                        //don't update anything
                        //print message
                        //reprint map

                if (change_tile(tile->x + new_x - x, tile->y + new_y - y, turn_heap) == 0) {
                    //successfully changed tiles
                    //updates PC coordinates
                    if (new_x == 0) {
                        player_character->x = TILE_WIDTH_X - 2;
                    } else if (new_x == TILE_WIDTH_X) {
                        player_character->x = 1;
                    } else if (new_y == 0) {
                        player_character->y = TILE_LENGTH_Y - 2;
                    } else if (new_y == TILE_LENGTH_Y) {
                        player_character->y = 1;
                    }
                    //updates map with new PC coordinates
                    //todo: ASSIGNED: current tile defined globally by x and y (on world) defined globally and world already defined
                    //todo: ASSIGNED: update tile pc info
                    //todo: ASSIGNED: make new heap for this tile (attach heap to tile)
                    //todo: ASSIGNED: refactor dijkstra distance tiles
                }
                else {
                    //todo: BUG TEST: test trying to move off of edge of world
                    //cannot change tile because at edge of world
                    clear();
                    addstr("You can't go off of the edge of the world like that! It's your turn! Enter a command or press z for help!\n");
                    print_tile_terrain(tile);
                    refresh();
                }
            }
            else {
                move_character(tile, x, y, new_x, new_y);
                player_character->turn += tile->tile[new_y][new_x].terrain.pc_weight;
                //recreate distance tiles for new PC location
                dijkstra(tile, RIVAL);
                dijkstra(tile, HIKER);
                turn_completed = 1;
            }
        }
    }
    return 0;

}

int move_character(struct tile *tile, int x, int y, int new_x, int new_y) {

    struct character *from_character = tile->tile[y][x].character;
    struct character *to_character = tile->tile[new_y][new_x].character;
    //if moving onto PC
    if (to_character != NULL) {
        //pc-trainer combat instigated by either party
        if (from_character->type_enum == PLAYER || to_character->type_enum == PLAYER) {
            if (from_character->type_enum == PLAYER && to_character->defeated == 1) {
                //PC should not be allowed to move to a defeated trainer's location (as of assignment 1.05)
                return 2;
            }
            else {
                combat(from_character, to_character);
            }
        }
        //trainer -> trainer = no move
        else {
            //this should never happen because this is checked in turn_based_movement in trainer movement
            return 1;
        }
    }
    else {
        tile->tile[y][x].character->x = new_x;
        tile->tile[y][x].character->y = new_y;
        struct character *temp_character = tile->tile[y][x].character;
        tile->tile[y][x].character = NULL;
        tile->tile[new_y][new_x].character = temp_character;
    }
    return 0;

}

int combat(struct character *from_character, struct character *to_character) {

    if (from_character->type_enum == PLAYER) {
        //player attacks trainer
        to_character->defeated = 1;
        clear();
        addstr("Victory! You challenged a trainer to a duel and defeated them soundly! Press escape to leave.\n");
        refresh();
    }
    else {
        from_character->defeated = 1;
        //trainer attacks player
        clear();
        addstr("Victory! A trainer challenged you to a duel and you trounced them! Press escape to leave.\n");
        refresh();
    }
    char command = '?';
    while (command != 27) {
        command = getch();
        clear();
        addstr("Invalid command. Press press escape to stop your victory dance after defeating that trainer.\n");
        refresh();
    }
    return 0;

}

int enter_center(struct character *player_character) {

    player_character->in_building = 1;
    clear();
    addstr("You are in a pokecenter! Unfortunately this center is rather barren. Leave by entering \'<\'\n");
    refresh();

    return 0;

}

int enter_mart(struct character *player_character) {

    player_character->in_building = 1;
    clear();
    addstr("You are in a pokemart! Unfortunately this mart is rather barren. Leave by entering \'<\'\n");
    refresh();

    return 0;

}

int interaction(struct heap *turn_heap) {
    print_tile_terrain(world[WORLD_CENTER_Y][WORLD_CENTER_X]);
    int x = WORLD_CENTER_X;
    int y = WORLD_CENTER_Y;
    char *command = (malloc(COMMAND_MAX_SIZE));
    if (command == NULL) {
        return 1;
    }
    printf("Enter commands to take actions. Type \"help\" for help!\n");
    int playing = 1;
    while (playing == 1) {
        fgets(command, COMMAND_MAX_SIZE, stdin);
        if (strlen(command) > 0 && command[strlen(command) - 1] == '\n') {
            command[strlen(command) - 1] = '\0';
        }
        if (strcmp(command, "help") == 0) {
            printf("Commands:\n");
            printf("Get help: \"help\"\n");
            printf("Pan Screen: \"n\", \"s\", \"e\", \"w\"\n");
            printf("Fly: \"f x-coordinate y-coordinate\"\n");
            printf("Quit: \"q\"\n");
        } else if (strcmp(command, "n") == 0) {
            if (change_tile(x, y - 1, turn_heap) == 0) {
                y--;
                printf("Moved North to the tile at coordinates (%d, %d)!\n", x - WORLD_CENTER_X, y - WORLD_CENTER_Y);
            }
            else {
                print_tile_terrain(world[y][x]);
                printf("You are already at the Northernmost tile!\n");
            }
        } else if (strcmp(command, "s") == 0) {
            if (change_tile(x, y + 1, turn_heap) == 0) {
                y++;
                printf("Moved South to the tile at coordinates (%d, %d)!\n", x - WORLD_CENTER_X, y - WORLD_CENTER_Y);
            }
            else {
                print_tile_terrain(world[y][x]);
                printf("You are already at the Southernmost tile!\n");
            }
        } else if (strcmp(command, "e") == 0) {
            if (change_tile(x + 1, y, turn_heap) == 0) {
                x++;
                printf("Moved East to the tile at coordinates (%d, %d)!\n", x - WORLD_CENTER_X, y - WORLD_CENTER_Y);
            }
            else {
                print_tile_terrain(world[y][x]);
                printf("You are already at the Easternmost tile!\n");
            }
        } else if (strcmp(command, "w") == 0) {
            if (change_tile(x - 1, y, turn_heap) == 0) {
                x--;
                printf("Moved West to the tile at coordinates (%d, %d)!\n", x - WORLD_CENTER_X, y - WORLD_CENTER_Y);
            }
            else {
                print_tile_terrain(world[y][x]);
                printf("You are already at the Westernmost tile!\n");
            }
        } else if (strlen(command) > 0 && command[0] == 'f' && (strlen(command) == 1 || command[1] == ' ' )) {
            int failed = 0;
            int coordinates[2];
            int i = -1;
            char * split;
            split = strtok (command, " ");
            while (split != NULL) {
                if (i == 0 || i == 1) {
                    coordinates[i] = strtol(split, (char **) NULL, 10);
                }
                else if (i > 1) {
                    failed = 1;
                    printf("Command failed due to inputting more than one pair of coordinates.\n");
                    break;
                }
                i++;
                split = strtok (NULL, " ");
            }
            if (failed == 0 && i < 2) {
                printf("Command failed due to inputting less than one pair of coordinates.\n");
                failed = 1;
            }
            if (failed == 0) {
                int x_out_of_bounds = 0;
                int y_out_of_bounds = 0;
                if (coordinates[0] < -WORLD_CENTER_X || coordinates[0] > WORLD_CENTER_X) {
                    x_out_of_bounds = 1;
                }
                if (coordinates[1] < -WORLD_CENTER_Y || coordinates[1] > WORLD_CENTER_Y) {
                    y_out_of_bounds = 1;
                }
                if (x_out_of_bounds == 1 && y_out_of_bounds == 1) {
                    printf("Command failed due to the x and y coordinates being out of bounds: x = %d; y = %d.\n"
                            , coordinates[0], coordinates[1]);
                }
                else if (x_out_of_bounds == 1) {
                    printf("Command failed due to the x coordinate being out of bounds: x = %d;\n", coordinates[0]);
                }
                else if (y_out_of_bounds == 1) {
                    printf("Command failed due to the y coordinate being out of bounds: y = %d.\n", coordinates[1]);
                }
                else {
                    x = WORLD_CENTER_X + coordinates[0];
                    y = WORLD_CENTER_Y + coordinates[1];
                    change_tile(x, y, turn_heap);
                    printf("Flew to the tile at coordinates (%d, %d)!\n", x - WORLD_CENTER_X, y - WORLD_CENTER_Y);
                }
            }
        } else if (strcmp(command, "q") == 0) {
            printf("Are you sure you want to Quit (y/n)? All progress will be lose.\n");
            int processed = 0;
            while (processed == 0) {
                fgets(command, COMMAND_MAX_SIZE, stdin);
                if (strlen(command) > 0 && command[strlen(command) - 1] == '\n') {
                    command[strlen(command) - 1] = '\0';
                }
                if (strcmp(command, "y") == 0) {
                    printf("You have exited the game. All progress has been lost.\n");
                    playing = 0;
                    break;
                } else if (strcmp(command, "n") == 0) {
                    printf("You have chosen to continue playing!\n");
                    processed = 1;
                } else {
                    printf("Invalid command. Please input \"y\" to quit or \"n\" to keep playing.\n");
                }
            }
        } else {
            printf("%s is not a valid command. Type \"help\" for help!\n", command);
        }
    }
    free(command);

    return 0;

}

int change_tile(int x, int y, struct heap *turn_heap) {

    //todo: ASSIGNED: upon entering map set all trainers there to same heap time as PC
    if (x >= 0 && x < WORLD_WIDTH_X && y >= 0 && y < WORLD_LENGTH_Y) {
        if (world[y][x] == NULL) {
            struct tile *new_tile = (malloc(sizeof(struct tile)));
            *new_tile = create_tile(x, y, turn_heap);
            world[y][x] = new_tile;
        }
        clear();
        addstr("You entered a new tile! It's your turn! Enter a command or press z for help!\n");
        print_tile_terrain(world[y][x]);
        refresh();
        return 0;
    }
    else {
        return 1;
    }

}

struct tile create_tile(int x, int y, struct heap *turn_heap) {

    struct tile tile = create_empty_tile();
    tile.x = x;
    tile.y = y;
    generate_terrain(&tile);
    int north_x;
    if (y > 0 && world[y - 1][x] != NULL) {
        //north_x = world[y - 1][x]->south_x;
    }
    else {
        north_x = rand() % (TILE_WIDTH_X - 10) + 5;
    }
    int south_x;
    if (y < WORLD_LENGTH_Y - 1 && world[y + 1][x] != NULL) {
        //south_x = world[y + 1][x]->north_x;
    }
    else {
        south_x = rand() % (TILE_WIDTH_X - 10) + 5;
    }
    int east_y;
    if (x < WORLD_WIDTH_X - 1 && world[y][x + 1] != NULL) {
        //east_y = world[y][x + 1]->west_y;
    }
    else {
        east_y = rand() % (TILE_LENGTH_Y - 10) + 5;
    }
    int west_y;
    if (x > 0 && world[y][x - 1] != NULL) {
        //west_y = world[y][x - 1]->east_y;
    }
    else {
        west_y = rand() % (TILE_LENGTH_Y - 10) + 5;
    }
    generate_paths(&tile, north_x, south_x, east_y, west_y);
    generate_buildings(&tile, x, y);
    place_player_character(&tile, turn_heap);
    place_trainers(&tile, turn_heap);
    return tile;

}

struct tile create_empty_tile() {

    struct tile tile;
    struct point empty_point =
            {-1, -1,none, none, NULL, INT_MAX, NULL};
    for (int i = 0; i < TILE_LENGTH_Y; i++) {
        for (int j = 0; j < TILE_WIDTH_X; j++) {
            tile.tile[i][j] = empty_point;
            tile.tile[i][j].x = j;
            tile.tile[i][j].y = i;
        }
    }
    tile.north_x = -1;
    tile.south_x = -1;
    tile.east_y = -1;
    tile.west_y = -1;
    return tile;

}

int generate_terrain(struct tile *tile) {

    const int NUM_TALL_GRASS_SEEDS = rand() % 5 + 2;
    const int NUM_CLEARING_SEEDS = rand() % 5 + 2;
    const int NUM_FOREST_SEEDS = rand() % 5;
    const int NUM_MOUNTAIN_SEEDS = rand() % 4;
    const int NUM_LAKE_SEEDS = rand() % 3;
    plant_seeds(tile, grass, NUM_TALL_GRASS_SEEDS);
    plant_seeds(tile, clearing, NUM_CLEARING_SEEDS);
    plant_seeds(tile, forest, NUM_FOREST_SEEDS);
    plant_seeds(tile, mountain, NUM_MOUNTAIN_SEEDS);
    plant_seeds(tile, lake, NUM_LAKE_SEEDS);
    grow_seeds(tile);
    place_edge(tile);
    set_terrain_border_weights(tile);

    return 0;

}

int plant_seeds(struct tile *tile, struct terrain terrain, int num_seeds) {

    for (int i = 0; i < num_seeds; i++) {
        int placed = 0;
        while (placed == 0) {
            int x = rand() % (TILE_WIDTH_X - 2) + 1;
            int y = rand() % (TILE_LENGTH_Y - 2) + 1;
            if (tile->tile[y][x].terrain.id == none.id) {
                tile->tile[y][x].terrain = terrain;
                placed = 1;
            }
        }
    }

    return 0;

}

int grow_seeds(struct tile *tile) {

    //queue implementation:
    //add each (coordinate, terrain) tuple structure to queue
    //while queue not empty
    //pop
    //if does not have terrain
    //give terrain as specified in tuple
    //give space weight value for dijkstra (as defined earlier)
    //add all spaces within 3x and 1y to queue with same terrain

    //loop through non-edge to grow seeds
    int complete = 0;
    while (complete == 0) {
        //if no changes made in a loop then no more loops required
        complete = 1;
        //determine what must grow
        for (int i = 1; i < TILE_LENGTH_Y - 1; i++) {
            for (int j = 1; j < TILE_WIDTH_X - 1; j++) {
                if (tile->tile[i][j].terrain.id == none.id) {
                    //loop through nearby area to copy first terrain found
                    for (int k = -1; k <=1; k++) {
                        for (int l = -1; l <= 1; l++) {
                            int x = j+k;
                            int y = i+l;
                            if (x > 0 && x < TILE_WIDTH_X - 1 && y > 0 && y < TILE_LENGTH_Y - 1) {
                                struct terrain new_terrain = tile->tile[y][x].terrain;
                                if (new_terrain.id != none.id) {
                                    tile->tile[i][j].grow_into = new_terrain;
                                }
                            }
                        }
                    }
                    complete = 0;
                }
            }
        }
        //grow what must grow
        for (int i = 1; i < TILE_LENGTH_Y - 1; i++) {
            for (int j = 1; j < TILE_WIDTH_X - 1; j++) {
                struct terrain new_terrain = tile->tile[i][j].grow_into;
                if (new_terrain.id != none.id) {
                    tile->tile[i][j].terrain = new_terrain;
                }
            }
        }
    }

    return 0;

}

int place_edge(struct tile *tile) {

    //places edge (stones with different name and higher weight) on edges
    for (int i = 0; i < TILE_WIDTH_X; i ++) {
        tile->tile[0][i].terrain = edge;
        tile->tile[TILE_LENGTH_Y - 1][i].terrain = edge;
    }
    for (int i = 0; i < TILE_LENGTH_Y; i ++) {
        tile->tile[i][0].terrain = edge;
        tile->tile[i][TILE_WIDTH_X - 1].terrain = edge;
    }

    return 0;

}

int set_terrain_border_weights(struct tile *tile) {

    //Sets borders between non-edge terrain types to weight 0
    for (int i = 1; i < TILE_LENGTH_Y - 1; i++) {
        for (int j = 1; j < TILE_WIDTH_X - 1; j++) {
            struct terrain terrain = tile->tile[i][j].terrain;
            for (int k = -1; k <=1; k++) {
                for (int l = -1; l <= 1; l++) {
                    int x = j+k;
                    int y = i+l;
                    if (x > 0 && x < TILE_WIDTH_X - 1 && y > 0 && y < TILE_LENGTH_Y - 1) {
                        struct terrain other_terrain = tile->tile[y][x].terrain;
                        if (terrain.id != other_terrain.id && other_terrain.id != edge.id) {
                            tile->tile[i][j].terrain.path_weight = TERRAIN_BORDER_WEIGHT;
                        }
                    }
                }
            }
        }
    }

    return 0;

}

int generate_paths(struct tile *tile, int north_x, int south_x, int east_y, int west_y) {

    north_x = 39;
    south_x = 39;
    west_y = 10;
    east_y = 10;

    //used in both paths:
    int current_x;
    int current_y;
    //x = none; n = north; s = south; e = east; w = west
    char last_move;
    int moves_since_last_change;
    const int repetitive_limit = 3;

    //North/South path
    current_x = north_x;
    current_y = 0;
    last_move = 'x';
    moves_since_last_change = 0;
    tile->tile[current_y][current_x].terrain = path;
    while (current_y < TILE_LENGTH_Y - 2) {
        //determine weights
        int east_weight = INT_MAX;
        int west_weight = INT_MAX;
        int south_weight = INT_MAX;
        if (current_x < TILE_WIDTH_X - 4 && last_move != 'w'
            && !(moves_since_last_change > repetitive_limit && last_move == 'e')) {
            east_weight = tile->tile[current_y][current_x + 1].terrain.path_weight;
        }
        if (current_x > 2 && last_move != 'e' && !(moves_since_last_change > repetitive_limit && last_move == 'w')) {
            west_weight = tile->tile[current_y][current_x - 1].terrain.path_weight;
        }
        if (current_y < TILE_LENGTH_Y - 1 && !(moves_since_last_change > repetitive_limit && last_move == 's')) {
            south_weight = tile->tile[current_y + 1][current_x].terrain.path_weight;
        }
        //choose the lowest weight
        if (east_weight < west_weight && east_weight < south_weight) {
            current_x++;
            tile->tile[current_y][current_x].terrain = path;
            if (last_move == 'e') {
                moves_since_last_change++;
            }
            else {
                last_move = 'e';
                moves_since_last_change = 0;
            }
        }
        else if (west_weight < south_weight) {
            current_x--;
            tile->tile[current_y][current_x].terrain = path;
            if (last_move == 'w') {
                moves_since_last_change++;
            }
            else {
                last_move = 'w';
                moves_since_last_change = 0;
            }
        }
        else {
            current_y++;
            tile->tile[current_y][current_x].terrain = path;
            if (last_move == 's') {
                moves_since_last_change++;
            }
            else {
                last_move = 's';
                moves_since_last_change = 0;
            }
        }
    }
    if (current_x < south_x) {
        for (int i = current_x; i <= south_x; i++) {
            current_x = i;
            tile->tile[current_y][current_x].terrain = path;
        }
    }
    else if (current_x > south_x) {
        for (int i = current_x; i >= south_x; i--) {
            current_x = i;
            tile->tile[current_y][current_x].terrain = path;
        }
    }
    tile->tile[current_y + 1][current_x].terrain = path;

    //West/East path
    current_x = 0;
    current_y = west_y;
    last_move = 'x';
    moves_since_last_change = 0;
    tile->tile[current_y][current_x].terrain = path;
    while (current_x < TILE_WIDTH_X - 2) {
        //determine weights
        int north_weight = INT_MAX;
        int south_weight = INT_MAX;
        int east_weight = INT_MAX;
        if (current_y < TILE_LENGTH_Y - 3 && last_move != 'n'
            && !(moves_since_last_change > repetitive_limit && last_move == 's')) {
            south_weight = tile->tile[current_y + 1][current_x].terrain.path_weight;
        }
        if (current_y > 2 && last_move != 's' && !(moves_since_last_change > repetitive_limit && last_move == 'n')) {
            north_weight = tile->tile[current_y - 1][current_x].terrain.path_weight;
        }
        if (current_x < TILE_WIDTH_X - 2 && !(moves_since_last_change > repetitive_limit && last_move == 'e')) {
            east_weight = tile->tile[current_y][current_x + 1].terrain.path_weight;
        }
        //choose the lowest weight
        if (north_weight < south_weight && north_weight < east_weight) {
            current_y--;
            tile->tile[current_y][current_x].terrain = path;
            if (last_move == 'n') {
                moves_since_last_change++;
            }
            else {
                last_move = 'n';
                moves_since_last_change = 0;
            }
        }
        else if (south_weight < east_weight) {
            current_y++;
            tile->tile[current_y][current_x].terrain = path;
            if (last_move == 's') {
                moves_since_last_change++;
            }
            else {
                last_move = 's';
                moves_since_last_change = 0;
            }
        }
        else {
            current_x++;
            tile->tile[current_y][current_x].terrain = path;
            if (last_move == 'e') {
                moves_since_last_change++;
            }
            else {
                last_move = 'e';
                moves_since_last_change = 0;
            }
        }
    }
    if (current_y < east_y) {
        for (int i = current_y; i <= east_y; i++) {
            current_y = i;
            tile->tile[current_y][current_x].terrain = path;
        }
    }
    else if (current_y > east_y) {
        for (int i = current_y; i >= east_y; i--) {
            current_y = i;
            tile->tile[current_y][current_x].terrain = path;
        }
    }
    tile->tile[current_y][current_x + 1].terrain = path;

    tile->north_x = north_x;
    tile->south_x = south_x;
    tile->west_y = west_y;
    tile ->east_y = east_y;

    //uncomment below and modify it to match paths across tiles.
//    //used in both paths:
//    int current_x;
//    int current_y;
//    //x = none; n = north; s = south; e = east; w = west
//    char last_move;
//    int moves_since_last_change;
//    const int repetitive_limit = 3;
//
//    //North/South path
//    current_x = north_x;
//    current_y = 0;
//    last_move = 'x';
//    moves_since_last_change = 0;
//    tile->tile[current_y][current_x].terrain = terrain_path;
//    while (current_y < TILE_LENGTH_Y - 2) {
//        //determine weights
//        int east_weight = INT_MAX;
//        int west_weight = INT_MAX;
//        int south_weight = INT_MAX;
//        if (current_x < TILE_WIDTH_X - 4 && last_move != 'w'
//            && !(moves_since_last_change > repetitive_limit && last_move == 'e')) {
//            east_weight = tile->tile[current_y][current_x + 1].weight;
//        }
//        if (current_x > 2 && last_move != 'e' && !(moves_since_last_change > repetitive_limit && last_move == 'w')) {
//            west_weight = tile->tile[current_y][current_x - 1].weight;
//        }
//        if (current_y < TILE_LENGTH_Y - 1 && !(moves_since_last_change > repetitive_limit && last_move == 's')) {
//            south_weight = tile->tile[current_y + 1][current_x].weight;
//        }
//        //choose lowest weight
//        if (east_weight < west_weight && east_weight < south_weight) {
//            current_x++;
//            tile->tile[current_y][current_x].terrain = terrain_path;
//            if (last_move == 'e') {
//                moves_since_last_change++;
//            }
//            else {
//                last_move = 'e';
//                moves_since_last_change = 0;
//            }
//        }
//        else if (west_weight < south_weight) {
//            current_x--;
//            tile->tile[current_y][current_x].terrain = terrain_path;
//            if (last_move == 'w') {
//                moves_since_last_change++;
//            }
//            else {
//                last_move = 'w';
//                moves_since_last_change = 0;
//            }
//        }
//        else {
//            current_y++;
//            tile->tile[current_y][current_x].terrain = terrain_path;
//            if (last_move == 's') {
//                moves_since_last_change++;
//            }
//            else {
//                last_move = 's';
//                moves_since_last_change = 0;
//            }
//        }
//    }
//    tile->tile[current_y + 1][current_x].terrain = terrain_path;
//
//    //West/East path
//    current_x = 0;
//    current_y = west_y;
//    last_move = 'x';
//    moves_since_last_change = 0;
//    tile->tile[current_y][current_x].terrain = terrain_path;
//    while (current_x < TILE_WIDTH_X - 2) {
//        //determine weights
//        int north_weight = INT_MAX;
//        int south_weight = INT_MAX;
//        int east_weight = INT_MAX;
//        if (current_y < TILE_LENGTH_Y - 3 && last_move != 'n'
//            && !(moves_since_last_change > repetitive_limit && last_move == 's')) {
//            south_weight = tile->tile[current_y + 1][current_x].weight;
//        }
//        if (current_y > 2 && last_move != 's' && !(moves_since_last_change > repetitive_limit && last_move == 'n')) {
//            north_weight = tile->tile[current_y - 1][current_x].weight;
//        }
//        if (current_x < TILE_WIDTH_X - 2 && !(moves_since_last_change > repetitive_limit && last_move == 'e')) {
//            east_weight = tile->tile[current_y][current_x + 1].weight;
//        }
//        //choose lowest weight
//        if (north_weight < south_weight && north_weight < east_weight) {
//            current_y--;
//            tile->tile[current_y][current_x].terrain = terrain_path;
//            if (last_move == 'n') {
//                moves_since_last_change++;
//            }
//            else {
//                last_move = 'n';
//                moves_since_last_change = 0;
//            }
//        }
//        else if (south_weight < east_weight) {
//            current_y++;
//            tile->tile[current_y][current_x].terrain = terrain_path;
//            if (last_move == 's') {
//                moves_since_last_change++;
//            }
//            else {
//                last_move = 's';
//                moves_since_last_change = 0;
//            }
//        }
//        else {
//            current_x++;
//            tile->tile[current_y][current_x].terrain = terrain_path;
//            if (last_move == 'e') {
//                moves_since_last_change++;
//            }
//            else {
//                last_move = 'e';
//                moves_since_last_change = 0;
//            }
//        }
//    }
//    tile->tile[current_y][current_x + 1].terrain = terrain_path;
//
//    tile->north_x = north_x;
//    tile->south_x = south_x;
//    tile->west_y = west_y;
//    tile ->east_y = east_y;

    return 0;

}

int generate_buildings(struct tile *tile, int x, int y) {

    double chance;
    if (x == WORLD_CENTER_X && y == WORLD_CENTER_Y) {
        chance = 100;
    }
    else {
        chance = ((-45 * distance(x, y, WORLD_CENTER_X, WORLD_CENTER_Y) / 200) + 50);
        if (chance < 5) {
            chance = 5;
        }
    }
    place_building(tile, center, chance);
    place_building(tile, mart, chance);

    return 0;

}

int place_building(struct tile *tile, struct terrain terrain, double chance) {

    if (rand() % 100 < chance) {
        int x;
        int y;
        int valid = 1;
        while (valid == 1) {
            x = rand() % (TILE_WIDTH_X - 2) + 1;
            y = rand() % (TILE_LENGTH_Y - 2) + 1;
            struct point point = tile->tile[y][x];
            if (!legal_overwrite(point)) {
                if ((x > 0 && tile->tile[y][x - 1].terrain.id == path.id)
                    || (x < TILE_WIDTH_X - 1 && tile->tile[y][x + 1].terrain.id == path.id)
                    || (y > 0 && tile->tile[y - 1][x].terrain.id == path.id)
                    || (y < TILE_LENGTH_Y - 1 && tile->tile[y + 1][x].terrain.id == path.id)) {
                    valid = 0;
                }
            }
        }
        tile->tile[y][x].terrain = terrain;
    }

    return 0;

}

int place_player_character(struct tile *tile, struct heap *turn_heap) {

    int x;
    int y;
    int found = 0;
    while (found == 0) {
        x = rand() % 78 + 1;
        y = rand() % 19 + 1;
        if (tile->tile[y][x].terrain.id == path.id) {
            found = 1;
        }
    }

    struct character *player_character = malloc(sizeof(struct character));
    player_character->x = x;
    player_character->y = y;
    player_character->type_enum = PLAYER;
    player_character->type_string = "PLAYER";
    player_character->printable_character = '@';
    strcpy(player_character->color, "\033[0;36m");
    player_character->turn = 0;
    player_character->direction_set = 0;
    player_character->in_building = 0;
    player_character->defeated = 0;
    heap_insert(turn_heap, player_character);
    tile->player_character = player_character;
    tile->tile[y][x].character = player_character;
    //create distance tiles
    dijkstra(tile, RIVAL);
    dijkstra(tile, HIKER);

    return 0;

}

int place_trainers(struct tile *tile, struct heap *turn_heap) {

    int num_trainers_copy = num_trainers;
    int num_rivals = 0;
    int num_hikers = 0;
    int num_random_walkers = 0;
    int num_pacers = 0;
    int num_wanderers = 0;
    int num_stationaries = 0;
    while (num_trainers_copy > 0) {
        if (num_rivals == 0) {
            num_rivals++;
        }
        else if (num_hikers == 0) {
            num_hikers++;
        }
        else {
            int random = rand()%10;
            if (random >= 0 && random <= 2) {
                num_rivals++;
            }
            else if (random >= 3 && random <= 5) {
                num_hikers++;
            }
            else if (random == 6) {
                num_random_walkers++;
            }
            else if (random == 7) {
                num_pacers++;
            }
            else if (random == 8) {
                num_wanderers++;
            }
            else if (random == 9) {
                num_stationaries++;
            }
        }
        num_trainers_copy--;
    }

    place_trainer_type(tile, turn_heap, num_rivals, RIVAL, 'r');
    place_trainer_type(tile, turn_heap, num_hikers, HIKER, 'h');
    place_trainer_type(tile, turn_heap, num_random_walkers, RANDOM_WALKER, 'n');
    place_trainer_type(tile, turn_heap, num_pacers, PACER, 'p');
    place_trainer_type(tile, turn_heap, num_wanderers, WANDERER, 'w');
    place_trainer_type(tile, turn_heap, num_stationaries, STATIONARY, 's');

    return 0;

}

int place_trainer_type(struct tile *tile, struct heap *turn_heap, int num_trainer, enum character_type trainer_type,
        char character) {

    while (num_trainer > 0) {
        int x;
        int y;
        int found = 0;
        while (found == 0) {
            x = rand() % 78 + 1;
            y = rand() % 19 + 1;
            if (tile->tile[y][x].character == NULL) {
                if (trainer_type == HIKER) {
                    //spawns anywhere hiker can reach PC from
                    if (hiker_distance_tile[y][x] < INT_MAX) {
                        found = 1;
                    }
                }
                else {
                    //spawns anywhere rival can reach PC from
                    if (rival_distance_tile[y][x] < INT_MAX) {
                        found = 1;
                    }
                }
            }
        }
        struct character *trainer = malloc(sizeof(struct character));
        trainer->x = x;
        trainer->y = y;
        trainer->type_enum = trainer_type;
        //initialize type_string
        if (trainer->type_enum == RIVAL) {
            trainer->type_string = "RIVAL";
        }
        else if (trainer->type_enum == HIKER) {
            trainer->type_string = "HIKER";
        }
        else if (trainer->type_enum == RANDOM_WALKER) {
            trainer->type_string = "RANDOM WALKER";
        }
        else if (trainer->type_enum == PACER) {
            trainer->type_string = "PACER";
        }
        else if (trainer->type_enum == WANDERER) {
            trainer->type_string = "WANDERER";
        }
        else if (trainer->type_enum == STATIONARY) {
            trainer->type_string = "STATIONARY";
        }
        else {
            //trainer is not one of the trainer types
            return 1;
        }
        trainer->printable_character = character;
        strcpy(trainer->color, "\033[31m");
        trainer->turn = 0;
        trainer->direction_set = 0;
        trainer->in_building = 0;
        trainer->defeated = 0;
        heap_insert(turn_heap, trainer);
        tile->tile[y][x].character = trainer;
        num_trainer--;
    }

    return 0;

}

int dijkstra(struct tile *tile, enum character_type trainer_type) {

    int start_x = tile->player_character->x;
    int start_y =tile->player_character->y;

    for (int y = 0; y < TILE_LENGTH_Y; y++) {
        for (int x = 0; x < TILE_WIDTH_X; x++) {
            tile->tile[y][x].distance = INT_MAX;
        }
    }
    tile->tile[start_y][start_x].distance = 0;

    struct heap heap;
    static struct point *point;
    heap_init(&heap, comparator_trainer_distance_tile, NULL);
    for (int y = 0; y < TILE_LENGTH_Y; y++) {
        for (int x = 0; x < TILE_WIDTH_X; x++) {
            int weight;
            if (trainer_type == RIVAL) {
                weight = tile->tile[y][x].terrain.rival_weight;
            }
            else {
                //character_type type_enum == hiker
                weight = tile->tile[y][x].terrain.hiker_weight;
            }
            if (weight != INT_MAX) {
                tile->tile[y][x].heap_node = heap_insert(&heap, &tile->tile[y][x]);
            }
            else {
                tile->tile[y][x].heap_node = NULL;
            }
        }
    }
    while ((point = heap_remove_min(&heap))) {
        point->heap_node = NULL;
        for (int y = -1; y <= 1; y++) {
            for (int x = -1; x <= 1; x++) {
                if (point->y + y >= 0 && point->y + y < TILE_LENGTH_Y && point->x + x >= 0 && point->x + x < TILE_WIDTH_X)
                {
                    struct point *neighbor = &(tile->tile[point->y + y][point->x + x]);
                    int candidate_distance;
                    if (trainer_type == RIVAL) {
                        candidate_distance = point->distance + neighbor->terrain.rival_weight;
                    } else {
                        //character_type type_enum == hiker
                        candidate_distance = point->distance + neighbor->terrain.hiker_weight;
                    }
                    if (neighbor->heap_node != NULL && candidate_distance < neighbor->distance &&
                        candidate_distance > 0) {
                        neighbor->distance = candidate_distance;
                        heap_decrease_key_no_replace(&heap, neighbor->heap_node);
                    }
                }
            }
        }
    }
    heap_delete(&heap);

    //updates appropriate trainer distance tile for the data to endure through future dijkstra calls
    for (int i = 0; i < TILE_LENGTH_Y; i++) {
        for (int j = 0; j < TILE_WIDTH_X; j++) {
            if (trainer_type == RIVAL) {
                rival_distance_tile[i][j] = tile->tile[i][j].distance;
            }
            else {
                //printable_character type_enum = hiker
                hiker_distance_tile[i][j] = tile->tile[i][j].distance;
            }
        }
    }

    return 0;

}

int legal_overwrite(struct point point) {

    if (point.terrain.id == edge.id
        || point.terrain.id == path.id
        || point.terrain.id == center.id
        || point.terrain.id == mart.id) {
        return 1;
    }
    else {
        return 0;
    }

}

double distance(int x1, int y1, int x2, int y2) {
    double square_difference_x = (x2 - x1) * (x2 - x1);
    double square_difference_y = (y2 - y1) * (y2 - y1);
    double sum = square_difference_x + square_difference_y;
    double value = sqrt(sum);
    return value;
}

int print_tile_terrain(struct tile *tile) {

    for (int y = 0; y < TILE_LENGTH_Y; y++) {
        for (int x = 0; x < TILE_WIDTH_X; x++) {
            char printable_character = tile->tile[y][x].terrain.printable_character;
            if (tile->tile[y][x].character != NULL) {
                //todo: ASSIGNED: set color
                printable_character = tile->tile[y][x].character->printable_character;
            }
            else {
                //todo: ASSIGNED: set color
            }
            mvaddch(y + 1, x, printable_character);
        }
    }
    addch('\n');
    refresh();

    return 0;

}

int print_tile_trainer_distances(struct tile *tile) {

    dijkstra(tile, RIVAL);
    printf("Rival distance tile:\n");
    print_tile_trainer_distances_printer(tile);
    dijkstra(tile, HIKER);
    printf("Hiker distance tile:\n");
    print_tile_trainer_distances_printer(tile);

    return 0;

}

int print_tile_trainer_distances_printer(struct tile *tile) {

    for (int i = 0; i < TILE_LENGTH_Y; i++) {
        for (int j = 0; j < TILE_WIDTH_X; j++) {
            int distance = tile->tile[i][j].distance;
            if (distance == INT_MAX) {
                printf("  ");
            }
            else if (distance == 0) {
                printf("\033[31m");
                printf("00");
                printf("\033[0m");
            }
            else {
                printf("%02d", tile->tile[i][j].distance % 100);
            }
            printf(" ");
        }
        printf("\n");
    }

    return 0;

}