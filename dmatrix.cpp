#include <algorithm>
#include <iostream>
#include <sys/ioctl.h>
#include <unistd.h>
#include <vector>
#include <thread>
#include <chrono>
#include <termios.h>
#include <fcntl.h>
#include <signal.h>

using namespace std;

const string ESC = "\x1b[";

const vector<string> kata_glyphs = {
    "ｱ", "ｲ", "ｳ", "ｴ", "ｵ",
    "ｶ", "ｷ", "ｸ", "ｹ", "ｺ",
    "ｻ", "ｼ", "ｽ", "ｾ", "ｿ",
    "ﾀ", "ﾁ", "ﾂ", "ﾃ", "ﾄ",
    "ﾅ", "ﾆ", "ﾇ", "ﾈ", "ﾉ",
    "ﾊ", "ﾋ", "ﾌ", "ﾍ", "ﾎ",
    "ﾏ", "ﾐ", "ﾑ", "ﾒ", "ﾓ",
    "ﾔ", "ﾕ", "ﾖ",
    "ﾗ", "ﾘ", "ﾙ", "ﾚ", "ﾛ",
    "ﾜ", "ﾝ"
};

int delayMap[10] = {
    0, 100, 50, 25, 20, 15, 10, 7, 5, 3
};

enum Color {
    RED = 31,
	GREEN = 32,
	YELLOW = 33,
	BLUE = 34,
	WHITE = 37,
};

enum Style {
    NORMAL = 0,
    BOLD = 1,
    DIM = 2,
};

enum Direction {
    UP = -1,
    DOWN = 1
};

struct Strand {
    bool active = false;
    string symbol = " ";
    int head = -1;
    int column = 0;
    int length = 0;
    int speed = 0;
    int tick = 0;

    Strand () = default;
    Strand(int hd, int col, int len, int spd) :
        active(true), head(hd), column(col), length(len), speed(spd)
    {}
};

struct WindowSize {
    int rows = 10;
    int cols = 10;
};

struct Terminal {
    // for saving/restoring terminal settings
    termios oldTerminalSettings;

    void initialize() {
        // save old terminal settings
        tcgetattr(STDIN_FILENO, &oldTerminalSettings);

        // set new settings for raw mode - start with old settings
        termios raw = oldTerminalSettings;

        // disable line buffering, echo, and canonical mode
        raw.c_lflag &= ~(ECHO | ICANON);
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);

        // make stdin non-blocking
        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    }

    void restore() {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldTerminalSettings);
    }
};

static Terminal *terminalPtr = nullptr;

static void updateWindowSize(WindowSize &ws) {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    ws.rows = w.ws_row;
    ws.cols = w.ws_col / 2;
}

static void changeColor(string &frame, Color c) {
    frame += ESC;
    frame += to_string(c);
    frame += "m";
}

static void changeStyle(string &frame, Style s) {
    frame += ESC;
    frame += to_string(s);
    frame += "m";
}

static void moveCursor(string &frame, int row, int col) {
    frame += ESC;
    frame += to_string(row + 1); // ansi sequences are 1-indexed :(
    frame += ";";
    frame += to_string(col * 2 + 1);
    frame += "H";
}

static void appendChar(string &frame, int row, int col, string c) {
    // move cursor, change color, then append
    moveCursor(frame, row, col);
    frame += c;
}

static void appendCharWithColorChange(string &frame, int row, int col, string c, Color color) {
    moveCursor(frame, row, col);
    changeColor(frame, color);
    // append char with 2 spaces to give it extra room
    frame += c;
}

static void moveHome(string &frame) {
    frame += ESC;
    frame += "H";
}

static void resetColor(string &frame) {
    frame += ESC;
    frame += "0m";
}

static void clearScreen() {
    // hide cursor, clear screen, move cursor to home position
    cout << ESC << "?25l" << ESC << "H" << ESC << "J";
    cout.flush();
}

static bool flipCoin() {
    return random() % 2 == 0;
}

static inline string getRandomChar() {
    // returns a random printable ASCII character (33-126)
    if (flipCoin())
        return kata_glyphs[random() % kata_glyphs.size()];
    else
        return string(1,33 + random() % (126 - 33 + 1));
}

static inline int getRandomDelay(int min = 2, int max = 8) {
    // Delay between 3 - 8 ticks
    return random() % (max - min + 1) + min;
}

static inline int getRandomLength(int max) {
    return random() % (max - 2) + 2; // length between 2 - num_rows-1 (inclusive)
}

static bool getInput(char *cmd) {
    return read(STDIN_FILENO, cmd, 1) > 0;
}

static void handleInput(char cmd, Color &c, int &delay) {
    // RGBY -> Color change
    if (cmd == 'r' || cmd == 'g' || cmd == 'b' || cmd == 'y') {
        switch (cmd) {
            case 'r': c = RED; break;
            case 'g': c = GREEN; break;
            case 'b': c = BLUE; break;
            case 'y': c = YELLOW; break;
        }

    // 1-9 -> Delay change
    } else if (cmd >= '1' && cmd <= '9') {
        delay = delayMap[cmd - '0'];
    }
}

static void initNewStrands(vector<bool> &occupied, string &frame, int rows, int cols, vector<Strand> &strands, Direction dir) {
    // loop over each column and check if its free to create a new strand
    for (int i = 0; i < cols; ++i) {

        // if space free && coin flip == true -> create strand at [0,i]
        if (!occupied[i] && flipCoin()) {
            bool didntFindOldStrand = true;
            int headPosition = dir == UP ? rows : -1;

            // find/re-use innactive Strand if possible
            for (Strand &s : strands) {
                if (!s.active) {
                    s = Strand(
                        headPosition,
                        i,
                        getRandomLength(rows),
                        getRandomDelay()
                    );
                    didntFindOldStrand = false;
                    break;
                }
            }

            if (didntFindOldStrand) { // didnt find an inactive strand, create new one
                strands.emplace_back(
                    headPosition,
                    i,
                    getRandomLength(rows),
                    getRandomDelay()
                );
            }

            // mark col as occupied
            occupied[i] = true;
        }
    }
}


static void updateStrands(vector<Strand> &strands, vector<bool> &occupied, string &frame, int rows, int cols, Color c, Direction dir) {

    // in range lambda to check if strand head/tail in view of screen
    auto inRange = [rows](int pos) {
        return pos >= 0 && pos < rows;
    };

    // loop through each active strand and update its position
    for (Strand &s : strands) {
        if (!s.active) continue; // skip inactive strands

        // length of 0 -> means strand is out of view -> deactivate it and continue
        if (s.length == 0) {
            s.active = false;
            continue;
        }

        // if timer is up -> move strand down
        if (s.tick >= s.speed) {
            // reset tick -> 0
            s.tick = 0;

            // move the strand down
            // delete tail (if in view)
            int tail = s.head + ((s.length - 1) * (-1 * dir));
            if (inRange(tail)) {
                appendChar(frame, tail, s.column, " ");
                // if strand is half way down the screen, mark col as free (prevents collisions)
                if (tail == rows / 2) occupied[s.column] = false;
            }

            // paint old head color c (if in range)
            if (inRange(s.head)) {
                // paint old head color and move it up/down one cell
                appendCharWithColorChange(frame, s.head, s.column, s.symbol, c);
                s.head += dir;

            // if strand is at starting position, just move it down one cell
            } else if ((s.head == -1 && dir == DOWN) || (s.head == rows && dir == UP)) {
                s.head += dir;
            // if head is out of view, just decrement length
            } else s.length--;

            // if head still in view after moving, print new symbol in WHITE
            if (inRange(s.head)) {
                s.symbol = getRandomChar();
                appendCharWithColorChange(frame, s.head, s.column, s.symbol, WHITE);
            }

        // increment timer
        } else s.tick++;
    }
}

static void resetSimulation(vector<bool> &occupied, WindowSize &ws, int &rows, int &cols, vector<Strand> &strands) {
    // update rows/cols to new window size
    rows = ws.rows;
    cols = ws.cols;

    // reset occupied array and innactivate all strands
    occupied.clear();
    occupied.resize(cols, false);
    // set all strands to inactive
    for (Strand &s : strands)
        s.active = false;

    clearScreen();
}

static void cleanUP(Terminal *terminal) {
    // restore old terminal settings
    if (terminalPtr) {
        terminalPtr->restore();
    }

    cout << ESC << "0m"; // reset color
    cout << ESC << "?25h"; // show cursor
    cout << ESC << "H"; // move cursor home
    cout << ESC << "J"; // clear screen
    cout.flush();
}

static void onSignal(int sig) {
    // cleanup and exit on SIGINT or SIGTERM
    if (sig == SIGINT || sig == SIGTERM) {
        cleanUP(terminalPtr);
        exit(0);
    }
}



int main(int argc, char *argv[]) {
    // init random seed
    srandom(time(NULL));

    // get terminal window size to fill screen with matrix
    WindowSize ws;
    updateWindowSize(ws);
    int rows = ws.rows;
    int cols = ws.cols;

    // create a Frame buffer to hold the entire screen output
    string frame;
    frame.reserve(rows * cols * 32); // reserve enough space for entire screen output

    // create an array of strands to hold the active strands
    vector<Strand> strands(rows * cols);
    // occupied columns array to track which cols are occupied
    vector<bool> occupied(cols, false);

    // switch to raw mode and  non-blocking input
    Terminal terminal;
    terminal.initialize();
    terminalPtr = &terminal;

    // register signal handlers for SIGINT & SIGTERM to cleanup/restore terminal settings on CTL+C or CTL+Z
    signal(SIGINT, onSignal);
    signal(SIGTERM, onSignal);

    // command for capturing user input ('q' to quit, '1', '2', '3' to change speed, '!', '@', '#' to change color)
    int delay = 20;
    Color c = GREEN;
    Direction dir = DOWN; // 1 = down, -1 = up
    Style s = NORMAL;
    char cmd = 0;

    // use case -> ./dmatrix <direction or style> or ./dmatrix <dir> <style>
    if (argc > 1) {
        // -B -> bold / -b -> dim
        string arg1 = argv[1];
        if (arg1 == "-u")
            dir = UP;
        else if (arg1 == "-B")
            s = BOLD;
        else if (arg1 == "-b")
            s = DIM;

        // 2 args
        if (argc > 2) {
            arg1 = argv[2];
            if (arg1 == "-B")
                s = BOLD;
            else if (arg1 == "-b")
                s = DIM;
        }
    }

    // clear the screen once to hide any output + hide cursor
    clearScreen();

    // main loop to run sim
    while (cmd != 'q') {

        // reset frame buffer for next round of ansi commands
        frame.clear();

        // check for user input
        if (getInput(&cmd))
            handleInput(cmd, c, delay);

        // update window size in case user resized terminal
        updateWindowSize(ws);
        if (ws.rows != rows || ws.cols != cols)
            // if it changed, re-configure/reset the simulation to fit new window size
            resetSimulation(occupied, ws, rows, cols, strands);

        // initialize new strands, then update all active strands (move them down)
        changeStyle(frame, s);
        initNewStrands(occupied, frame, rows, cols, strands, dir);
        updateStrands(strands, occupied, frame, rows, cols, c, dir);

        // print entire frame buffer
        resetColor(frame);
        cout << frame;
        cout.flush();

        // sleep and repeat
        std::this_thread::sleep_for(std::chrono::milliseconds(delay)); // sleep
    }

    // cleanup and exit
    cleanUP(terminalPtr);
    return 0;
}
