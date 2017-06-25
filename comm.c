#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <python2.7/Python.h>

pthread_mutex_t msgmutex = PTHREAD_MUTEX_INITIALIZER;

struct PlayerState{
    int player_id; // same as client id
    int pos_x, pos_y; // position in the world
    int curr_hp; // current hp for the player
    int alive; // alive or not
};

struct Shot{
    int player_id; // which player fired the shot
    int active; // if the shot is active/currently on the screen
    int pos_x, pos_y; // shot curr pos in the screen
};

struct PlayerState state[2];

struct Shot shots[2];

int sfd_client; // socket file descriptors for client, used to get the game state from the server


pthread_t listenThreads;

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// decodes message and updates local game state
// BASICAMENTE ESCOVA STRING PRA PORRRAAA
void updateLocalState(char * msg, int length){
    char strPlayer[5];
    char strPosX[5];
    char strPosY[5];
    char strHp[5];
    char strAlive[5];

    int player;
    int new_posx;
    int new_posy;
    int curr_hp;
    int alive;

    char strShotPlayer;
    char strActive;
    char strShotPosX[5];
    char strShotPosY[5];

    int shotPlayer;
    int active;
    int shotPosX;
    int shotPosY;

    //printf("Msg:%s\n", msg);

    // getting player
    int j = 0;
    int i; // iterates through the message
    for(i = 1; msg[i] != ';'; i++){
        strPlayer[j] = msg[i];
        j++;
    }
    strPlayer[j] = '\0'; // inserts NUL terminator character
    player = atoi(strPlayer);

    // getting new X position
    j = 0;
    for(i = i + 1; msg[i] != ';'; i++, j++)
        strPosX[j] = msg[i];
    
    strPosX[j] = '\0';
    new_posx = atoi(strPosX);

    // getting new Y position
    j = 0;
    for(i = i + 1; msg[i] != ';'; i++, j++)
        strPosY[j] = msg[i];
    
    strPosY[j] = '\0';
    new_posy = atoi(strPosY);

    // getting current_hp
    j = 0;
    for(i = i + 1; msg[i] != ';'; i++, j++)
        strHp[j] = msg[i];
    strHp[j] = '\0';
    curr_hp = atoi(strHp);

    // getting alive status
    j = 0;
    for(i = i + 1; msg[i] != '-'; i++, j++)
        strAlive[j] = msg[i];
    strAlive[j] = '\0';
    alive = atoi(strAlive);
    //
    // change local structure for player 1
    state[player].pos_x = new_posx;
    state[player].pos_y = new_posy;
    state[player].curr_hp = curr_hp;
    state[player].alive = alive;

    // getting player 2 information
    j = 0;
    for(i = i+3; msg[i] != ';'; i++){
        strPlayer[j] = msg[i];
        j++;
    }
    strPlayer[j] = '\0'; // inserts NUL terminator character
    player = atoi(strPlayer);

    // getting new X position
    j = 0;
    for(i = i + 1; msg[i] != ';'; i++, j++)
        strPosX[j] = msg[i];
    
    strPosX[j] = '\0';
    new_posx = atoi(strPosX);

    // getting new Y position
    j = 0;
    for(i = i + 1; msg[i] != ';'; i++, j++)
        strPosY[j] = msg[i];
    
    strPosY[j] = '\0';
    new_posy = atoi(strPosY);

    // getting current_hp
    j = 0;
    for(i = i + 1; msg[i] != ';'; i++, j++)
        strHp[j] = msg[i];
    strHp[j] = '\0';
    curr_hp = atoi(strHp);

    // getting alive status
    j = 0;
    for(i = i + 1; msg[i] != '-'; i++, j++)
        strAlive[j] = msg[i];
    strAlive[j] = '\0';
    alive = atoi(strAlive);

    // change local structure for player 2
    state[player].pos_x = new_posx;
    state[player].pos_y = new_posy;
    state[player].curr_hp = curr_hp;
    state[player].alive = alive;

    i = i+3; // jump to playerPosition of the particle information

    // getting information of player who shot the shot (kek)
    strShotPlayer = msg[i];
    shotPlayer = strShotPlayer - '0'; 

    i = i + 2;
    strActive = msg[i];
    active = strActive - '0';
    
    j = 0;
    for(i = i + 2; msg[i] != ';'; i++, j++)
        strShotPosX[j] = msg[i];
    strShotPosX[j] = '\0';
    shotPosX = atoi(strShotPosX);
    
    j = 0;
    for(i = i + 1; msg[i] != '-'; i++, j++)
        strShotPosY[j] = msg[i];
    strShotPosY[j] = '\0';
    shotPosY = atoi(strShotPosY);

    // updating local struct
    shots[shotPlayer].active = active;
    shots[shotPlayer].pos_x = shotPosX;
    shots[shotPlayer].pos_y = shotPosY;

    // -0;160;200;3;1-*-1;100;500;3;1-#-0;0;0;0-*-1;0;0;0-
    // getting shot information of player 2

    i = i+3; // goes to next relevant position

    strShotPlayer = msg[i];
    shotPlayer = strShotPlayer - '0'; 

    i = i + 2;
    strActive = msg[i];
    active = strActive - '0';
    
    j = 0;
    for(i = i + 2; msg[i] != ';'; i++, j++)
        strShotPosX[j] = msg[i];
    strShotPosX[j] = '\0';
    shotPosX = atoi(strShotPosX);
    
    j = 0;
    for(i = i + 1; msg[i] != '-'; i++, j++)
        strShotPosY[j] = msg[i];
    strShotPosY[j] = '\0';
    shotPosY = atoi(strShotPosY);

    // updating local struct
    shots[shotPlayer].active = active;
    shots[shotPlayer].pos_x = shotPosX;
    shots[shotPlayer].pos_y = shotPosY;
}

// each client has its own thread running this function which
// continuously queries the server for the game state, updating
// the local struct
void * listenGameState(void * args){
    //int tid = (int)(long) args;

    int numbytes;
    char buffer[500];

    while(1){
        memset(buffer, 0, sizeof buffer);
        numbytes = recv(sfd_client, &buffer, sizeof buffer, 0); // waits for the server to give the game state
        //printf("numbytes: %d\n", numbytes);
        if (numbytes == -1){
            perror("recv");
            printf("Couldn't get game state from the server.\n");
        } else {
            buffer[numbytes] = '\0';
            if (numbytes > 60)
                continue;     

            //printf("[%d] buffer: %s\n", numbytes, buffer);
            updateLocalState(buffer, strlen(buffer));
            //printf("hmm\n");
            //printf("%d:%s\n", numbytes, buffer);
            
            // Shit protocol - each time the client queries for the player pos
            // the server responds with a string of the format:
            //          -id;pos_x;pos_y-*-id;pos_x;pos_y-
            // breaking string

            // retrieving player1 message from buffer
            //for(i = 0; buffer[i] != '*'; i++)
            //    msg_player1[i] = buffer[i];
            //updateLocalState(msg_player1, i); // updates player1

            //// rerieving player2 message from buffer            
            //for(j = 0, i = i + 1; buffer[i] != '*' && buffer[i] != '\0'; i++, j++)
            //    msg_player2[j] = buffer[i];
            //updateLocalState(msg_player2, j); // updates player2

            //printf("1:%s\n", msg_player1);
            //printf("2:%s\n", msg_player2);
        }
    }
}

// gets the current structure and builds to python values
static PyObject * comm_retrievePlayerState(PyObject * self, PyObject *args){
    int p_id; // determines which player to retrieve the structures

    if (!PyArg_ParseTuple(args, "i", &p_id)){
        printf("Failed to read arguments.\n");
        return NULL;
    }

    // returns a tuple of the format (pos_x, pos_y) for the queried player
    return Py_BuildValue("iiii", state[p_id].pos_x, state[p_id].pos_y, 
                                 state[p_id].curr_hp, state[p_id].alive);
}

static PyObject * comm_retrieveShotsState(PyObject * self){
    //printf("Shot0: %d:%d:%d\n", shots[0].active, shots[0].pos_x, shots[0].pos_y);
    //printf("Shot1: %d:%d:%d\n", shots[1].active, shots[1].pos_x, shots[1].pos_y);

    return Py_BuildValue("iiiiii", shots[0].active, shots[0].pos_x, shots[0].pos_y,
                                   shots[1].active, shots[1].pos_x, shots[1].pos_y);
}

// sends message from client passed from command line to the server
static PyObject * comm_sendMessage(PyObject *self, PyObject *args){
    const char *message;
    int length;

    if (!PyArg_ParseTuple(args, "s#", &message, &length)){
        printf("Invalid client ID!\n");
        return NULL;
    }
    
    //printf("Sending: %s", message);

    // sends message to server
    if (send(sfd_client, message, length, 0) == -1){
        printf("Client couldn't send message.\n");
    }

    Py_RETURN_NONE;
}

// creates a new socket object, connects to the given port and returns it
static PyObject * comm_clientConnect(PyObject *self, PyObject *args){
    char *port;
    if (!PyArg_ParseTuple(args, "s", &port)) // argument parsing
        return NULL;
    printf("Connecting to port %s\n", port);

    struct addrinfo hints, *servinfo;
    int rv; // return value
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // ipv4
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo("127.0.0.1", port, &hints, &servinfo)) != 0){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    // create socket
    if ((sfd_client = socket(servinfo->ai_family, servinfo->ai_socktype,
                         servinfo->ai_protocol)) == -1){
        perror("client: socket");
        exit(1);
    }

    // establish connection through socket
    if (connect(sfd_client, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
        close(sfd_client);
        perror("client: connect");
    }

    inet_ntop(servinfo->ai_family, get_in_addr((struct sockaddr *)servinfo->ai_addr), s, sizeof s); 
    printf("connecting to %s\n", s);

    // create a thread to listen the server
    pthread_create(&listenThreads, NULL, listenGameState, (void *)(long) 0);

    freeaddrinfo(servinfo);

    Py_RETURN_NONE;
}

static PyMethodDef comm_methods[] = {
    { "clientConnect", (PyCFunction) comm_clientConnect, METH_VARARGS, NULL },
    { "sendMessage", (PyCFunction) comm_sendMessage, METH_VARARGS, NULL },
    { "retrievePlayerState", (PyCFunction) comm_retrievePlayerState, METH_VARARGS, NULL },
    { "retrieveShotsState", (PyCFunction) comm_retrieveShotsState, METH_NOARGS, NULL },
    { NULL }
};

PyMODINIT_FUNC initcomm() {
    Py_InitModule3("comm", comm_methods, "meh");
}
