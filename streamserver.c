#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

#define UPDATE_INTERVAL 17 // in milisseconds

#define MAX_CLIENTS 2 // max number of connections to be queued up

#define RELOAD_TIME 3 // in seconds
#define NO_DAMAGE_TIME 150 // in miliseconds

#define PLAYER_SIDE_SIZE 23 // only need one side since sprites are squared

#define SHOTS_SPEED 12

#define DEF_SPEED 5
#define MAX_WIDTH 800
#define MAX_HEIGHT 600

// stores both players information
struct PlayerState{
    int player_id; // same as client id
    int pos_x, pos_y; // position in the world
    int curr_hp; // current hp for the player
    int alive; // alive or not
};

struct Shot{
    int player_id; // which player fired the shot
    int active; // if the shot is active/currently drawn on the screen
    int damage; // if 0, doesn't cause damage. This is used when spawning a new shot so it doesn't hit the player firing
    int reloaded; // if 1 ready to be shot again
    float pos_x, pos_y; // shot curr pos in the screen
    float vel_x, vel_y; // velocity of the shot in x and y
};

// filling out structs with default values
struct PlayerState player[2] = { 
{ .player_id = 0, .pos_x = 100, .pos_y = 300, .curr_hp = 3, .alive = 1 },
{ .player_id = 1, .pos_x = 700, .pos_y = 300, .curr_hp = 3, .alive = 1 } };

struct Shot shots[2] = {
{ .player_id = 0, .active = 0, .reloaded = 1, .pos_x = 0, .pos_y = 0, .vel_x = 0, .vel_y = 0},
{ .player_id = 1, .active = 0, .reloaded = 1, .pos_x = 0, .pos_y = 0, .vel_x = 0, .vel_y = 0},
}; // shots on the screen

int client_sockets[MAX_CLIENTS]; // list of sockets to be used to communicate with clients
pthread_mutex_t accept_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t send_mutex = PTHREAD_MUTEX_INITIALIZER;
//pthread_cond_t waitplayers = PTHREAD_COND_INITIALIZER; // used to wait both players before starting game thread

char port[MAX_CLIENTS];

typedef struct {
    long thread_id; // thread identifier
    int *socketfd; // pointer to a socket file descriptor
} thread_args;

// get sockaddr, IPv4 or IPv6
void *get_in_addr(struct sockaddr *sa){
    if (sa->sa_family == AF_INET)
        return &(((struct sockaddr_in*)sa)->sin_addr);

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// receives a pointer to a socket and configures all the shit needed
// for it to work
void setupServer(int *socketfd){
    struct addrinfo hints, *res;
    int status;
    char hoststr[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP/Stream server
    hints.ai_flags = AI_PASSIVE; // gets my IP

    // getting this hosts info
    if ((status = getaddrinfo(NULL, port, &hints, &res)) != 0){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        exit(1);
    }
    // res now holds info needed to create the host socket
    struct sockaddr_in *ipv4 = (struct sockaddr_in *)res->ai_addr;
    void *addr = &(ipv4->sin_addr);
    inet_ntop(res->ai_family, addr, hoststr, sizeof hoststr);
    printf("Server created at %s\n", hoststr);

    // create the socket with host info
    *socketfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (*socketfd == -1){
        fprintf(stderr, "socket: %s\n", gai_strerror(*socketfd));
        exit(2);
    }

    // binding to the port passed in getaddrinfo
    if(bind(*socketfd, res->ai_addr, res->ai_addrlen) == -1){
        fprintf(stderr, "Failed to bind socket!\n");
        exit(3);
    }

    // starts listening on the created socket
    if (listen(*socketfd, MAX_CLIENTS) == -1){
        fprintf(stderr, "Failed to start listening!\n");
        exit(4);
    }

    freeaddrinfo(res);
}

void updatePlayerState(int player_id, char direction){
    // update pos in the game state struct
    
    if (direction == 'a')
        player[player_id].pos_x -= DEF_SPEED;
    else if (direction == 'd')
        player[player_id].pos_x += DEF_SPEED;
    else if (direction == 's')
        player[player_id].pos_y += DEF_SPEED;
    else if (direction == 'w')
        player[player_id].pos_y -= DEF_SPEED;

    // diagonal movements
    else if (direction == 'e'){
        player[player_id].pos_x += DEF_SPEED;
        player[player_id].pos_y -= DEF_SPEED;
    }
    else if (direction == 'q'){
        player[player_id].pos_x -= DEF_SPEED;
        player[player_id].pos_y -= DEF_SPEED;
    }
    else if (direction == 'z'){
        player[player_id].pos_x -= DEF_SPEED;
        player[player_id].pos_y += DEF_SPEED;
    }
    else if (direction == 'c'){
        player[player_id].pos_x += DEF_SPEED;
        player[player_id].pos_y += DEF_SPEED;
    }

    // checks player collision against walls
    if (player[player_id].pos_x < PLAYER_SIDE_SIZE)
        player[player_id].pos_x = PLAYER_SIDE_SIZE;
    else if (player[player_id].pos_x > MAX_WIDTH - PLAYER_SIDE_SIZE)
        player[player_id].pos_x = MAX_WIDTH - PLAYER_SIDE_SIZE;
    
    if (player[player_id].pos_y < PLAYER_SIDE_SIZE)
        player[player_id].pos_y = PLAYER_SIDE_SIZE;
    else if (player[player_id].pos_y > MAX_HEIGHT - PLAYER_SIDE_SIZE) 
        player[player_id].pos_y = MAX_HEIGHT - PLAYER_SIDE_SIZE;
}

// send the current game state to the client
void sendGameState(int client_id){

    // How this shitty protocol works:
    // The server sends a single message to the client
    // with all the information needed to represent a whole game state,
    // including each player positions, (2 players for now, MAYBE make this
    // parametrizable if everything's working fine and dandy later), current_hp
    // and alive status. Also sends the same information for every enemy and
    // shots (or shots if you will) information.

    char state_msg[200];
    memset(state_msg, 0, sizeof state_msg); // makes sure state_msg is "clean"

    sprintf(state_msg, "-%d;%d;%d;%d;%d-*-%d;%d;%d;%d;%d-#-%d;%d;%d;%d-*-%d;%d;%d;%d-", 
    0, player[0].pos_x, player[0].pos_y, player[0].curr_hp, player[0].alive,
    1, player[1].pos_x, player[1].pos_y, player[1].curr_hp, player[1].alive,
    0, shots[0].active, (int)shots[0].pos_x, (int)shots[0].pos_y,
    1, shots[1].active, (int)shots[1].pos_x, (int)shots[1].pos_y);

    //printf("%s\n", state_msg);
    send(client_sockets[client_id], state_msg, strlen(state_msg), 0);
}

// calculates distance between given points
float calcDist(int p0_x, int p0_y, int p1_x, int p1_y){
    float dist;

    dist = sqrt(pow((float)p1_x - (float)p0_x, 2) + pow((float)p1_y - (float)p0_y, 2));

    return dist;
}

void resetGame(){
    printf("Game over, sleeping for 3 seconds\n");
    sleep(3);

    player[0].pos_x = 100;
    player[0].pos_y = 300;
    player[0].curr_hp = 3;
    player[0].alive = 1;

    player[1].pos_x = 700;
    player[1].pos_y = 300;
    player[1].curr_hp = 3;
    player[1].alive = 1;
}

// called when a shot has hit a player
void hitPlayer(int p_id){
    pthread_mutex_lock(&send_mutex);
    player[p_id].curr_hp -= 1;
    if (player[p_id].curr_hp == 0)
        player[p_id].alive = 0;
    pthread_mutex_unlock(&send_mutex);
}

// thread that will contain game rules and take care of continuosly updating 
// the server game state
void * updateGameState(void * args){
    int i;
    int j;

    while (1){
        // update shots
        for(i = 0; i < 2; i++){
            if (shots[i].active == 1){ // if shot is active then update its position
                shots[i].pos_x += shots[i].vel_x * SHOTS_SPEED;
                shots[i].pos_y += shots[i].vel_y * SHOTS_SPEED;

                // check if a shot hit a player
                for(j = 0; j < 2; j++){ // iterates through players checking collision
                    if (calcDist(shots[i].pos_x, shots[i].pos_y, player[j].pos_x, player[j].pos_y) < PLAYER_SIDE_SIZE && 
                        shots[i].damage == 1){
                        shots[i].active = 0; // "destroys" the shot
                        hitPlayer(j);
                    }
                }
            }

            // makes the shot bounce off the wall
            if (shots[i].pos_x >= MAX_WIDTH){
                shots[i].pos_x = MAX_WIDTH;
                shots[i].vel_x = - shots[i].vel_x;
            } else if (shots[i].pos_x <= 0){
                shots[i].pos_x = 0;
                shots[i].vel_x = - shots[i].vel_x;
            }

            if (shots[i].pos_y >= MAX_HEIGHT){
                shots[i].pos_y = MAX_HEIGHT;
                shots[i].vel_y = - shots[i].vel_y;
            } else if(shots[i].pos_y <= 0){
                shots[i].pos_y = 0;
                shots[i].vel_y = - shots[i].vel_y;
            }

        }

        // sends gameState to both clients
        pthread_mutex_lock(&send_mutex);
        sendGameState(0);
        sendGameState(1);
        pthread_mutex_unlock(&send_mutex);

        if (player[0].alive == 0 || player[1].alive == 0)
            resetGame();

        usleep(1000 * UPDATE_INTERVAL); // rest a little bit
    }
}

// This thread counts some time before making a shot active again - reloading it
void * reloadTimer(void * arg){
    int p_id = (int)(long) arg;
    shots[p_id].reloaded = 0;

    int elapsed_time = 0, trigger = RELOAD_TIME * 1000; // 3 seconds
    int flag = 0;
    
    clock_t start = clock();
    do{ // counts to defined time
        clock_t diff = clock() - start;
        elapsed_time = diff * 1000 / CLOCKS_PER_SEC;

        if (elapsed_time >= NO_DAMAGE_TIME && flag == 0){ // turns on damage
            shots[p_id].damage = 1;
            flag = 1;
        }

    }while (elapsed_time < trigger);

    //printf("Time taken: %d seconds, %d milliseconds\n", elapsed_time/1000, elapsed_time%1000);

    // locks with mutex so server doesn't try to send state while it is being changed
    shots[p_id].active = 0; // makes shot "ready" to be activated/fired again
    shots[p_id].reloaded = 1;

    pthread_exit(NULL); // thread finished its work, rest in peace sweet boy
}

void createNewShot(int p_id, char * msg){
    int i, j;
    char strTargetX[4];
    char strTargetY[4];
    
    // point where client has clicked on the screen
    int targetX;
    int targetY; 

    // getting player position to determine where the particle starts
    float ox = player[p_id].pos_x;
    float oy = player[p_id].pos_y;
    float magnitude;
    float v_x;
    float v_y;
    int vectorX;
    int vectorY;

    // first calculate the vector which points to the direction of the shot
    for(i = 3, j = 0; msg[i] != ':'; i++, j++)
        strTargetX[j] = msg[i];
    strTargetX[j] = '\0';
    targetX = atoi(strTargetX);

    for(i = i + 1, j = 0; msg[i] != '-'; i++, j++)
        strTargetY[j] = msg[i];
    strTargetY[j] = '\0';
    targetY = atoi(strTargetY);

    // we've gotten the information needed to calculate vel_x and vel_y

    vectorX = targetX - ox;
    vectorY = targetY - oy;

    magnitude = sqrt(pow(vectorX, 2) + pow(vectorY, 2));
    v_x = vectorX / magnitude;
    v_y = vectorY / magnitude;

    shots[p_id].pos_x = ox;
    shots[p_id].pos_y = oy;
    shots[p_id].vel_x = v_x;
    shots[p_id].vel_y = v_y;
    shots[p_id].damage = 0; // initially doesn't cause damage, reloadTimer thread will change this to 1 after some time
    shots[p_id].active = 1; // shot is now active
    
    // starts timer thread to "reload" the shot
    pthread_t reloadThread;
    pthread_create(&reloadThread, NULL, reloadTimer, (void *)(long) p_id);
}

void * handleConnection(void * args){
    thread_args *targs = args;
    long tid = targs->thread_id;
    int sockfd = *targs->socketfd;
    int numbytes; // number of bytes received
    
    char buffer[100]; // buffer used to store coming messages
    char msg[150];

    while(1){
        memset(buffer, 0, sizeof buffer); // cleans buffer before next message
        memset(msg, 0, sizeof msg); // cleans msg
        numbytes = recv(sockfd, &buffer, sizeof(buffer), 0); // retrieve message from socket into buffer
        if (numbytes == -1){
            perror("recv");
            break;
        } else if (numbytes == 0){
            perror("recv");
            break;
        }

        buffer[numbytes] = '\0'; // inserts EOF character at the end of the message

        if (buffer[0] == 'c'){
            //printf("From Client[%d] msg: %s\n", (int)tid, buffer);
            if (buffer[1] == 'f'){ // 'fire' command
                if (shots[(int) tid].reloaded == 1){
                    pthread_mutex_lock(&send_mutex);
                    createNewShot((int)tid, buffer);
                    pthread_mutex_unlock(&send_mutex);
                }
            }
            else{ // moving command
                // locks with mutex here so the updateGameState doesn't try
                // to send the information while it is
                // begin changed here
                pthread_mutex_lock(&send_mutex);
                updatePlayerState((int)tid, buffer[1]);
                pthread_mutex_unlock(&send_mutex);
            }
        } 
    }
    pthread_exit(NULL);
}

int main(int argc, char *argv[]){
    if (argc < 2){
        printf("Usage: serverstream <port number>\n");
        exit(-1);
    }

    strcpy(port, argv[1]); // gets port from command line argument

    // creates thread that will take care of updating the local gamestate
    pthread_t gameThread;

    pthread_t thread[MAX_CLIENTS];
    long n_thread = 0; // thread identifier
    thread_args targs;

    struct sockaddr_storage coming_addr;
    socklen_t addr_size;
    int sockfd;
    char ipstr[INET6_ADDRSTRLEN];

    setupServer(&sockfd); // setups socket file descriptor

    while(n_thread < MAX_CLIENTS){ // main accept() loop
        // accept an incoming connection:
        addr_size = sizeof coming_addr;

        // mutex lock
        pthread_mutex_lock(&accept_mutex);
        client_sockets[n_thread] = accept(sockfd, (struct sockaddr *)&coming_addr, &addr_size);
        if (client_sockets[n_thread] == -1){
            fprintf(stderr, "Failed to accept connection!\n");
            return 5;
        }

        // argument packing
        targs.thread_id = n_thread;
        targs.socketfd = &client_sockets[n_thread];
        
        // Create a thread for the new_fd to establish communication with sever
        // and still accept more connections
        pthread_create(&thread[n_thread], NULL, handleConnection, (void *) &targs);
        n_thread++;

        inet_ntop(coming_addr.ss_family,
                get_in_addr((struct sockaddr *)&coming_addr),
                ipstr, sizeof ipstr);

        printf("server: got connection from %s\n", ipstr);
        // mutex unlock
        pthread_mutex_unlock(&accept_mutex);
    }
    
    // game thread starts after both players have connected
    pthread_create(&gameThread, NULL, updateGameState, NULL);

    pthread_exit(NULL);

    return 0;
}
