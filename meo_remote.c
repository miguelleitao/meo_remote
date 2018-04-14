/*
 * meoRemote
 *
 * jml, 2018
 *
 */

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <SDL.h>
#include <SDL_gfxPrimitives.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>

#define SCREEN_WIDTH  121
#define SCREEN_HEIGHT 640
#define BPP 32

#define TITLE_X 30
#define TITLE_Y 10

#define FIELD_PADDING 1
#define BORDER_WIDTH 1
#define POINT_RADIUS 10

#define POINT_SPEED 3

#define COLOR_BORDER 0x0000FFFF
#define COLOR_FIELD  0x00FF00FF
#define COLOR_POINT  0xFF0000FF

static int field_x, field_y; // top-left corner
static int field_width, field_height;
static int point_x, point_y;

char *destAddr = "192.168.1.65";
char *destMask = NULL;
int  destPort  = 8082;
int  destSock  = 0;

short int draw_buttons = 0;
short int draw_mark = 0;

typedef struct {
  short int x, y;
  char *tag;
  char *desc;
  short int code;
} tButton;

#define MAX_BUTTONS (44)
tButton Button[MAX_BUTTONS];
int nButtons = 0;
int selectedButton = 0;

SDL_Surface *gHelloWorld = NULL;

/* 
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(0);
}

static void checkSDLResult(int result) {
    if (!result) {
        fprintf(stderr, "Error: %s\n", SDL_GetError());
        exit(EXIT_FAILURE);
    }
}

static SDL_Surface *createSurface() {
    SDL_Surface *screen;

    checkSDLResult(SDL_Init(SDL_INIT_VIDEO) == 0);
    atexit(SDL_Quit);

    screen = SDL_SetVideoMode(SCREEN_WIDTH, SCREEN_HEIGHT, BPP, SDL_HWSURFACE);
    checkSDLResult(screen != 0);

    checkSDLResult(SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL) == 0);

    return screen;
}

void markButton(SDL_Surface *screen, int b) {
    aacircleColor(screen, Button[b].x, Button[b].y, POINT_RADIUS, COLOR_POINT);
}

void drawButtons(SDL_Surface *screen) {
    for( int i=0 ; i<nButtons ; i++ ) {
	aacircleColor(screen, Button[i].x, Button[i].y, POINT_RADIUS, COLOR_FIELD);
    }
}

static void screenDraw(SDL_Surface *screen) {
    // fill all screen with black color
    SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 0, 0, 0));

    //Apply the image
    SDL_BlitSurface( gHelloWorld, NULL, screen, NULL );

    if ( draw_buttons ) drawButtons(screen);

}

static void initialDraw(SDL_Surface *screen) {
    // fill all screen with black color
    SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 0, 0, 0));

    stringColor(screen, TITLE_X, TITLE_Y, "Use arrows or mouse to move point, use Esc to exit.", 0x000000ff);

    // draw field
    boxColor(screen, field_x + BORDER_WIDTH, field_y + BORDER_WIDTH,
            field_x + field_width - BORDER_WIDTH - 1, field_y + field_height - BORDER_WIDTH - 1,
            COLOR_FIELD);

    point_x = field_x + field_width/2;
    point_y = field_y + field_height/2;

    //aacircleColor(screen, point_x, point_y, POINT_RADIUS, COLOR_POINT);
}

static int limitValue(int x, int min, int max) {
    return (x < min) ? min : ((x > max) ? max : x);
}

static void movePoint(SDL_Surface *screen, int new_x, int new_y) {
    new_x = limitValue(new_x,
                       field_x + BORDER_WIDTH + POINT_RADIUS,
                       field_x + field_width - BORDER_WIDTH - POINT_RADIUS - 1);

    new_y = limitValue(new_y,
                       field_y + BORDER_WIDTH + POINT_RADIUS,
                       field_y + field_height - BORDER_WIDTH - POINT_RADIUS - 1);

    if (new_x != point_x || new_y != point_y) {
        //filledCircleColor(screen, point_x, point_y, POINT_RADIUS, COLOR_FIELD);
	//aacircleColor(screen, point_x, point_y, POINT_RADIUS, COLOR_FIELD);
        point_x = new_x;
        point_y = new_y;
        aacircleColor(screen, point_x, point_y, POINT_RADIUS, COLOR_POINT);
    }
}

int findNextButton(int dx, int dy) {
    int best = selectedButton;
    int minDist = 1e8;
    for( int i=0 ; i<nButtons ; i++ ) {
	if ( i==selectedButton ) continue;
	int bdx = Button[i].x - Button[selectedButton].x; 
	int bdy = Button[i].y - Button[selectedButton].y;
	if ( bdx*dx<0 || bdy*dy<0 ) continue;
	int d = abs(bdx)/(30*dx*dx+1)+abs(bdy)/(30*dy*dy+1);
	if ( d<minDist ) {
	    minDist = d;
	    best = i;
	}
    }
    return best;
}

/* Returns 1 if quit. */
static int processKey(SDL_Surface *screen, SDLKey key) {
    // position change
    int dx = 0;
    int dy = 0;

    switch (key) {
        case SDLK_ESCAPE:
            return 1;
        case SDLK_UP:
            dy = -1;
            break;
        case SDLK_DOWN:
            dy = 1;
            break;
        case SDLK_LEFT:
            dx = -1;
            break;
        case SDLK_RIGHT:
            dx = 1;
            break;
        default:
            break;
    }

    if (dx != 0 || dy != 0) {
        //movePoint(screen, point_x + dx * POINT_SPEED, point_y + dy * POINT_SPEED);
	selectedButton = findNextButton(dx,dy);
	screenDraw(screen);
	markButton(screen, selectedButton);
    }
    return 0;
}

void sendCommand(int cmd) {
	if ( destSock<0 ) return;
	char msg[20];
	sprintf(msg, "key=%d\n", cmd);
    	/* send the message line to the server */
    	int n = write(destSock, msg, strlen(msg));
    	if (n < 0) error("ERROR writing to socket");
}

void processMouseDown(SDL_Surface *screen, Uint8 button, Uint16 x, Uint16 y) {
    if (button == 1) {
        movePoint(screen, x, y);
    }
    int sel=-1;
    const int rad2 = POINT_RADIUS*POINT_RADIUS;
printf("x y = %d %d\n", x,y);
    for( int i=0 ; i<nButtons ; i++ ) {
	int dx = x - Button[i].x;
	int dy = y - Button[i].y;
	int dist = dx*dx + dy*dy;

	//printf("  dist = %d %d, %d %d\n", i, dist, Button[i].x, Button[i].y);
	if ( dist<=rad2 ) {
	    sel = i;
	    break;
	}
    }
    if ( sel>=0 ) {
	selectedButton = Button[sel].code;
        sendCommand(selectedButton);
	printf("enviou = %d %d\n", sel, selectedButton);
    }
}


void addButton(int x, int y, int cod, char *tag) {
    if ( nButtons>=MAX_BUTTONS ) {
	return;
    } 
    //aacircleColor(screen, x, y, POINT_RADIUS, COLOR_FIELD);
    Button[nButtons].x = x;
    Button[nButtons].y = y;
    Button[nButtons].code = cod;
    Button[nButtons].tag = strdup(tag);
    nButtons++;
}

void addNumButton(int x, int y, int num) {
    char numTag[8];
    sprintf(numTag,"%d",num);
    addButton(x,y,num+48,numTag);
}

int defineButtons() {

    const int DX3 = 38;
    const int CENTER = 61;
    const int LEFT3  = CENTER-DX3;
    const int RIGHT3 = CENTER+DX3;

    const int BASE = 30;
    const int BASE5 = BASE+13;
    const int LINE = 30;
    const int BASE12 = BASE+13*LINE;

    const int QUAD1 = LEFT3-3;
    const int QUAD2 = CENTER-13;
    const int QUAD3 = CENTER+13;
    const int QUAD4 = RIGHT3+3;

    addButton(RIGHT3, BASE, 233, "Power");

    for( int y=1 ; y<4 ; y++ ) {
        for( int x=0 ; x<3 ; x++ ) {
	    addNumButton( x*38+23, y*LINE+BASE, nButtons );
 	}
    }
    addNumButton( CENTER, 4*LINE+BASE, 0 );

    addButton( LEFT3,  4*LINE+BASE, nButtons+42, "Back");   // code not set
    addButton( RIGHT3, 4*LINE+BASE, 43, "Enter");

    // vol    
    addButton( LEFT3,  5*LINE+BASE5, 175, "Vol+");
    addButton( LEFT3,  7*LINE+BASE5+17, 174, "Vol-");

    // p+/p-
    addButton( RIGHT3, 5*LINE+BASE5, 33, "Prog+");
    addButton( RIGHT3, 7*LINE+BASE5+17, 34, "Prog-");

    // +
    addButton( CENTER, 5*LINE+BASE5,    38, "Up");
    addButton( CENTER, 7*LINE+BASE5+17, 40, "Down");
    addButton( LEFT3,  6*LINE+BASE5+8, 37, "Left");
    addButton( RIGHT3, 6*LINE+BASE5+8, 39, "Right");

    addButton( CENTER, 6*LINE+BASE5+8, 34, "Ok");     // code not set

    // moves
    addButton( QUAD1, BASE12, 177, "Prev");
    addButton( QUAD2, BASE12, 40, "Rewind");
    addButton( QUAD3, BASE12, 37, "Forward");
    addButton( QUAD4, BASE12, 178, "Next");

    // Colors
    addButton( QUAD1, BASE12+LINE-1, 38, "Red");
    addButton( QUAD2, BASE12+LINE-1, 40, "Green");
    addButton( QUAD3, BASE12+LINE-1, 37, "Yellow");
    addButton( QUAD4, BASE12+LINE-1, 39, "Blue");

    // bottom
    addButton( QUAD1, BASE12+2*LINE-1, 173, "Mute");
    addButton( QUAD2, BASE12+2*LINE-1, 40,  "Sound");
    addButton( QUAD3, BASE12+2*LINE-1, 37,  "Options");
    addButton( QUAD4, BASE12+2*LINE-1, 39,  "TV/STB");


/*
    for( int y=3 ; y<4 ; y++ ) {
        for( int x=0 ; x<3 ; x++ ) {
	    addButton( x*DX3+LEFT, y*30+60, nButtons+42, "Num");
 	}
    }

    for( int y=0 ; y<3 ; y++ ) {
        for( int x=0 ; x<3 ; x++ ) {
            addButton( x*39+22, y*39+194, nButtons+49, "");
 	}
    }
        for( int x=0 ; x<4 ; x++ ) {
            addButton( x*28+19, 319, nButtons+49, "");
 	}
    for( int y=0 ; y<2 ; y++ ) {
        for( int x=0 ; x<3 ; x++ ) {
            addButton( x*39+22, y*30+354, nButtons+49, "");
 	}
    }
    for( int y=0 ; y<3 ; y++ ) {
        for( int x=0 ; x<4 ; x++ ) {
            addButton( x*28+18, y*29+420, nButtons+102, "");
 	}
    }
*/

// 33 prog+
// 34 prog-
// 36 menu
// 37 <
// 38 /\ .
// 39 >
// 40 v
// 43 enter
// 188 audio descrição
// 190 legendagem digital
// 233 power
// 173 mute
// 174 vol-
// 175 vol+
// 177 <-7seg
// 178 ->
// 179 pause

  return nButtons;
}

int main(int argc, char **argv) {
    int quit;
    while( argc>1 ) {
	if ( argv[1][0]=='-' ) {
	    switch (argv[1][1]) {
		case 'b':
		    draw_buttons = 1;
		    break;
	 	case 'c':
		    draw_mark = 1;
		    break;
		case 'a':
		    if ( argc>2 ) {
		    	destAddr = argv[2];
			destMask = NULL;
		    	argc--;
		    	argv++;
		    }
		    break;
		case 'm':
		    if ( argc>2 ) {
		    	destMask = argv[2];
		    	argc--;
		    	argv++;
		    }
		    break;
		case 'p':
		    if ( argc>2 ) {
		    	destPort = atoi(argv[2]);
		    	argc--;
		    	argv++;
		    }
		    break;
	    }
	}
	else {
	    sendCommand(atoi(argv[2]));
	}
	argc--;
	argv++;
    } // while
		    
    SDL_Surface *screen = createSurface();

    // calculate size of field and other initializations
    field_x = FIELD_PADDING;
    field_y = FIELD_PADDING + TITLE_Y + 10; // 10 is approximation of title height
    field_width = SCREEN_WIDTH - field_x - FIELD_PADDING;
    field_height = SCREEN_HEIGHT - field_y - FIELD_PADDING;

    initialDraw(screen);
    defineButtons();

    gHelloWorld = SDL_LoadBMP( "meo_remote.bmp" );
    if( gHelloWorld == NULL )     {
        printf( "Unable to load image %s! SDL Error: %s\n", "meo_remote.bmp", SDL_GetError() );
    }
    else //Apply the image
            SDL_BlitSurface( gHelloWorld, NULL, screen, NULL );

    if ( draw_buttons ) drawButtons(screen);

    /* socket: create the socket */
    destSock = socket(AF_INET, SOCK_STREAM, 0);
    if (destSock < 0) 
        error("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
    struct hostent *server = gethostbyname(destAddr);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", destAddr);
        exit(0);
    }

    /* build the server's Internet address */
    struct sockaddr_in serveraddr;
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
	  (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(destPort);

    /* connect: create a connection with the server */
    if (connect(destSock, &serveraddr, sizeof(serveraddr)) < 0) 
      perror("ERROR connecting");

    quit = 0;
    while (!quit) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    quit = 1;
                    break;

                case SDL_KEYDOWN:
                    if (processKey(screen, event.key.keysym.sym)) {
                        quit = 1;
                    }
                    break;

                case SDL_MOUSEBUTTONDOWN: {
                    SDL_MouseButtonEvent be = event.button;
                    processMouseDown(screen, be.button, be.x, be.y);
/*
	char *msg = "key=51\n";
    	int n = write(destSock, msg, strlen(msg));
    	if (n < 0) error("ERROR writing to socket");*/

                    break;
                }
            }
        }

        SDL_Flip(screen);
        SDL_Delay(33); // ~ 60 fps
        // screenDraw(screen);
    }

    close(destSock);
    return 0;
}

