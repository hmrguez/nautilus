
#define TRUE 1
#define FALSE !TRUE

// Shell pid, pgid, terminal modes
static pid_t NTL_PID;
static pid_t NTL_PGID;
static int NTL_IS_INTERACTIVE;
static struct termios NTL_TMODES;

static char* currentDirectory;
extern char** environ;

struct sigaction act_child;
struct sigaction act_int;

int no_reprint_prmpt;
int isPiping = 0;

pid_t pid;


// signal handler for SIGCHLD */
void signalHandler_child(int p);
// signal handler for SIGINT
void signalHandler_int(int p);


int changeDirectory(char * args[]);
