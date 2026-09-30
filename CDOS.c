#include "fxlib.h"
#include <string.h>
#include <stdlib.h>

/*
 * ============================================================
 * C-DOS
 * DOS-style shell for Casio fx-9750GIII
 * Old Casio SH SDK
 * ============================================================
 *
 * NO ALPHA REQUIRED.
 *
 * The letters printed above the physical keys are used.
 *
 * Example:
 *
 * SIN = D
 * (   = I
 * 6   = R
 *
 * SIN -> ( -> 6 -> EXE
 *
 * gives:
 *
 * C:\>DIR
 *
 * Commands:
 *
 * DIR
 * CD
 * MD
 * MKDIR
 * RD
 * RMDIR
 * DEL
 * TYPE
 * CLS
 * ECHO
 * VER
 * MEM
 * HELP
 * GAME
 * EXIT
 *
 * C:\GAMES contains GAME.EXE
 *
 * ============================================================
 */


/* ============================================================
 * CONSTANTS
 * ============================================================
 */

#define MAX_DIRS       16
#define MAX_FILES      32

#define MAX_NAME       13
#define MAX_COMMAND    32
#define MAX_TEXT       96

/*
 * Seven lines of terminal history.
 * Line 8 is the live command line.
 */
#define TERM_LINES     7
#define TERM_WIDTH     21


/* ============================================================
 * DOS DIRECTORY
 * ============================================================
 */

typedef struct
{
    int used;
    char name[MAX_NAME];
    int parent;
} DOS_DIR;


/* ============================================================
 * DOS FILE
 * ============================================================
 */

typedef struct
{
    int used;
    char name[MAX_NAME];
    int parent;
    char data[MAX_TEXT];
} DOS_FILE;


/* ============================================================
 * GLOBAL FILESYSTEM
 * ============================================================
 */

static DOS_DIR dirs[MAX_DIRS];

static DOS_FILE files[MAX_FILES];

static int current_dir;


/* ============================================================
 * TERMINAL BUFFER
 * ============================================================
 */

static char terminal[TERM_LINES][TERM_WIDTH];


/* ============================================================
 * FORWARD DECLARATIONS
 *
 * Required by the old Hitachi compiler.
 * ============================================================
 */

static void upper(char *s);

static int same(char *a, char *b);

static void terminal_clear(void);

static void terminal_scroll(void);

static void terminal_print(char *text);

static void terminal_draw(void);

static void get_path(char *p);

static void get_prompt(char *p);

static char key_to_char(unsigned int key);

static void draw_command_line(char *command);

static int read_command(char *command);

static void wait_exe(void);

static void fs_init(void);

static int find_dir(char *name, int parent);

static int find_file(char *name, int parent);

static void cmd_dir(void);

static void cmd_cd(char *arg);

static void cmd_md(char *arg);

static void cmd_rd(char *arg);

static void cmd_del(char *arg);

static void cmd_type(char *arg);

static void cmd_cls(void);

static void cmd_ver(void);

static void cmd_help(void);

static void cmd_echo(char *arg);

static void cmd_mem(void);

static void cmd_game(void);

static void parse_command(
    char *input,
    char *cmd,
    char *arg1,
    char *arg2);

static int execute_command(char *input);

static void shell(void);


/* ============================================================
 * UPPERCASE
 * ============================================================
 */

static void upper(char *s)
{
    int i;

    i = 0;

    while (s[i] != 0)
    {
        if (s[i] >= 'a' && s[i] <= 'z')
        {
            s[i] =
                (char)(s[i] - 'a' + 'A');
        }

        i++;
    }
}


/* ============================================================
 * CASE-INSENSITIVE COMPARE
 * ============================================================
 */

static int same(char *a, char *b)
{
    char aa[MAX_NAME];
    char bb[MAX_NAME];

    strncpy(
        aa,
        a,
        MAX_NAME - 1);

    aa[MAX_NAME - 1] = 0;

    strncpy(
        bb,
        b,
        MAX_NAME - 1);

    bb[MAX_NAME - 1] = 0;

    upper(aa);
    upper(bb);

    if (strcmp(aa, bb) == 0)
    {
        return 1;
    }

    return 0;
}


/* ============================================================
 * CLEAR TERMINAL
 * ============================================================
 */

static void terminal_clear(void)
{
    int i;

    for (i = 0; i < TERM_LINES; i++)
    {
        terminal[i][0] = 0;
    }
}


/* ============================================================
 * SCROLL TERMINAL
 * ============================================================
 */

static void terminal_scroll(void)
{
    int i;

    for (i = 0; i < TERM_LINES - 1; i++)
    {
        strcpy(
            terminal[i],
            terminal[i + 1]);
    }

    terminal[TERM_LINES - 1][0] = 0;
}


/* ============================================================
 * PRINT TO TERMINAL
 * ============================================================
 */

static void terminal_print(char *text)
{
    char line[TERM_WIDTH];

    int i;
    int pos;

    pos = 0;

    line[0] = 0;

    i = 0;

    while (1)
    {
        /*
         * End of line or end of string.
         */

        if (text[i] == 0 ||
            text[i] == '\n')
        {
            line[pos] = 0;

            terminal_scroll();

            strcpy(
                terminal[TERM_LINES - 1],
                line);

            pos = 0;

            if (text[i] == 0)
            {
                break;
            }

            i++;

            continue;
        }


        /*
         * Current line full.
         */

        if (pos >= TERM_WIDTH - 1)
        {
            line[pos] = 0;

            terminal_scroll();

            strcpy(
                terminal[TERM_LINES - 1],
                line);

            pos = 0;
        }


        line[pos] = text[i];

        pos++;

        i++;
    }
}


/* ============================================================
 * DRAW TERMINAL
 * ============================================================
 */

static void terminal_draw(void)
{
    int i;

    Bdisp_AllClr_DDVRAM();

    for (i = 0; i < TERM_LINES; i++)
    {
        locate(
            1,
            i + 1);

        Print(
            (unsigned char *)terminal[i]);
    }
}


/* ============================================================
 * GET CURRENT PATH
 *
 * Root:
 *
 * C:\
 *
 * GAMES:
 *
 * C:\GAMES
 *
 * DOS:
 *
 * C:\DOS
 * ============================================================
 */

static void get_path(char *p)
{
    if (current_dir == 0)
    {
        strcpy(
            p,
            "C:\\");
    }
    else
    {
        strcpy(
            p,
            "C:\\");

        strcat(
            p,
            dirs[current_dir].name);
    }
}


/* ============================================================
 * GET DOS PROMPT
 *
 * C:\>
 *
 * C:\GAMES>
 * ============================================================
 */

static void get_prompt(char *p)
{
    get_path(p);

    strcat(
        p,
        ">");
}


/* ============================================================
 * PHYSICAL KEY -> LETTER
 *
 * This uses the printed letters on the calculator.
 *
 * NO ALPHA.
 * ============================================================
 */

static char key_to_char(unsigned int key)
{
    /*
     * Number keys.
     */

    if (key == KEY_CHAR_0)
        return 'Z';

    if (key == KEY_CHAR_1)
        return 'U';

    if (key == KEY_CHAR_2)
        return 'V';

    if (key == KEY_CHAR_3)
        return 'W';

    if (key == KEY_CHAR_4)
        return 'P';

    if (key == KEY_CHAR_5)
        return 'Q';

    if (key == KEY_CHAR_6)
        return 'R';

    if (key == KEY_CHAR_7)
        return 'M';

    if (key == KEY_CHAR_8)
        return 'N';

    if (key == KEY_CHAR_9)
        return 'O';


    /*
     * Arithmetic keys.
     */

    if (key == KEY_CHAR_PLUS)
        return 'X';

    if (key == KEY_CHAR_MINUS)
        return 'Y';

    if (key == KEY_CHAR_MULT)
        return 'S';

    if (key == KEY_CHAR_DIV)
        return 'T';


    /*
     * Decimal point acts as SPACE.
     */

    if (key == KEY_CHAR_DP)
        return ' ';


    /*
     * Alphabetic keys.
     */

    if (key == KEY_CHAR_FRAC)
        return 'G';

    if (key == KEY_CTRL_FD)
        return 'H';

    if (key == KEY_CHAR_LPAR)
        return 'I';

    if (key == KEY_CHAR_RPAR)
        return 'J';

    if (key == KEY_CHAR_COMMA)
        return 'K';

    if (key == KEY_CHAR_STORE)
        return 'L';

    if (key == KEY_CTRL_XTT)
        return 'A';

    if (key == KEY_CHAR_LOG)
        return 'B';

    if (key == KEY_CHAR_LN)
        return 'C';

    if (key == KEY_CHAR_SIN)
        return 'D';

    if (key == KEY_CHAR_COS)
        return 'E';

    if (key == KEY_CHAR_TAN)
        return 'F';


    return 0;
}


/* ============================================================
 * DRAW LIVE COMMAND LINE
 * ============================================================
 */

static void draw_command_line(char *command)
{
    char prompt[32];

    char line[TERM_WIDTH];


    get_prompt(prompt);


    /*
     * Draw terminal history.
     */

    terminal_draw();


    /*
     * Build prompt + command.
     */

    strcpy(
        line,
        prompt);

    strcat(
        line,
        command);


    /*
     * Last screen line.
     */

    locate(
        1,
        8);

    Print(
        (unsigned char *)line);
}


/* ============================================================
 * READ COMMAND
 * ============================================================
 */

static int read_command(char *command)
{
    unsigned int key;

    char c;

    int length;


    length = 0;

    command[0] = 0;


    while (1)
    {
        /*
         * Show current command.
         */

        draw_command_line(
            command);


        /*
         * Wait for key.
         */

        GetKey(&key);


        /*
         * EXIT key.
         */

        if (key == KEY_CTRL_EXIT)
        {
            return 0;
        }


        /*
         * EXE executes command.
         */

        if (key == KEY_CTRL_EXE)
        {
            command[length] = 0;

            return 1;
        }


        /*
         * DEL = backspace.
         */

        if (key == KEY_CTRL_DEL)
        {
            if (length > 0)
            {
                length--;

                command[length] = 0;
            }

            continue;
        }


        /*
         * Convert key.
         */

        c = key_to_char(key);


        if (c != 0)
        {
            if (length < MAX_COMMAND - 1)
            {
                command[length] = c;

                length++;

                command[length] = 0;
            }
        }
    }
}


/* ============================================================
 * WAIT FOR EXE
 * ============================================================
 */

static void wait_exe(void)
{
    unsigned int key;

    while (1)
    {
        GetKey(&key);

        if (key == KEY_CTRL_EXE)
        {
            return;
        }

        if (key == KEY_CTRL_EXIT)
        {
            return;
        }
    }
}


/* ============================================================
 * INITIALIZE FILESYSTEM
 *
 * ROOT:
 *
 * C:\
 *   GAMES\
 *   DOS\
 *   README.TXT
 *   AUTOEXEC.BAT
 *
 * GAMES:
 *
 * C:\GAMES
 *   GAME.EXE
 * ============================================================
 */

static void fs_init(void)
{
    int i;


    /*
     * Clear directories.
     */

    for (i = 0; i < MAX_DIRS; i++)
    {
        dirs[i].used = 0;

        dirs[i].name[0] = 0;

        dirs[i].parent = 0;
    }


    /*
     * Clear files.
     */

    for (i = 0; i < MAX_FILES; i++)
    {
        files[i].used = 0;

        files[i].name[0] = 0;

        files[i].parent = 0;

        files[i].data[0] = 0;
    }


    /*
     * ROOT
     *
     * C:\
     */

    dirs[0].used = 1;

    dirs[0].name[0] = 0;

    dirs[0].parent = 0;


    /*
     * GAMES
     *
     * C:\GAMES
     */

    dirs[1].used = 1;

    strcpy(
        dirs[1].name,
        "GAMES");

    dirs[1].parent = 0;


    /*
     * DOS
     *
     * C:\DOS
     */

    dirs[2].used = 1;

    strcpy(
        dirs[2].name,
        "DOS");

    dirs[2].parent = 0;


    /*
     * README.TXT
     *
     * C:\README.TXT
     */

    files[0].used = 1;

    strcpy(
        files[0].name,
        "README.TXT");

    files[0].parent = 0;

    strcpy(
        files[0].data,
        "C-DOS 1.0\n"
        "DOS SIMULATOR.");


    /*
     * AUTOEXEC.BAT
     *
     * C:\AUTOEXEC.BAT
     */

    files[1].used = 1;

    strcpy(
        files[1].name,
        "AUTOEXEC.BAT");

    files[1].parent = 0;

    strcpy(
        files[1].data,
        "ECHO C-DOS\n"
        "ECHO READY.");


    /*
     * GAME.EXE
     *
     * C:\GAMES\GAME.EXE
     */

    files[2].used = 1;

    strcpy(
        files[2].name,
        "GAME.EXE");

    files[2].parent = 1;

    strcpy(
        files[2].data,
        "C-DOS GAME.EXE\n"
        "NUMBER GAME.");
}


/* ============================================================
 * FIND DIRECTORY
 * ============================================================
 */

static int find_dir(char *name, int parent)
{
    int i;


    for (i = 1; i < MAX_DIRS; i++)
    {
        if (dirs[i].used)
        {
            if (dirs[i].parent == parent)
            {
                if (same(
                    dirs[i].name,
                    name))
                {
                    return i;
                }
            }
        }
    }


    return -1;
}


/* ============================================================
 * FIND FILE
 * ============================================================
 */

static int find_file(char *name, int parent)
{
    int i;


    for (i = 0; i < MAX_FILES; i++)
    {
        if (files[i].used)
        {
            if (files[i].parent == parent)
            {
                if (same(
                    files[i].name,
                    name))
                {
                    return i;
                }
            }
        }
    }


    return -1;
}


/* ============================================================
 * DIR
 * ============================================================
 */

static void cmd_dir(void)
{
    int i;

    char path[32];

    char line[TERM_WIDTH];


    /*
     * Current path.
     */

    get_path(path);


    terminal_print(
        "Volume in drive C is CASIO");


    terminal_print(
        "Directory of ");


    /*
     * The previous terminal_print would make a separate line,
     * so create the actual directory line here instead.
     */

    terminal_scroll();


    strcpy(
        line,
        "Directory of ");

    strcat(
        line,
        path);


    strcpy(
        terminal[TERM_LINES - 1],
        line);


    /*
     * Directories.
     */

    for (i = 1; i < MAX_DIRS; i++)
    {
        if (dirs[i].used)
        {
            if (dirs[i].parent == current_dir)
            {
                strcpy(
                    line,
                    "<DIR> ");

                strcat(
                    line,
                    dirs[i].name);

                terminal_print(line);
            }
        }
    }


    /*
     * Files.
     */

    for (i = 0; i < MAX_FILES; i++)
    {
        if (files[i].used)
        {
            if (files[i].parent == current_dir)
            {
                terminal_print(
                    files[i].name);
            }
        }
    }
}


/* ============================================================
 * CD
 * ============================================================
 */

static void cmd_cd(char *arg)
{
    int d;


    if (arg[0] == 0)
    {
        return;
    }


    /*
     * Root.
     */

    if (same(
        arg,
        "\\"))
    {
        current_dir = 0;

        return;
    }


    /*
     * Parent.
     */

    if (same(
        arg,
        ".."))
    {
        if (current_dir != 0)
        {
            current_dir =
                dirs[current_dir].parent;
        }

        return;
    }


    /*
     * Find directory.
     */

    d = find_dir(
        arg,
        current_dir);


    if (d < 0)
    {
        terminal_print(
            "The system cannot find the path specified.");

        return;
    }


    current_dir = d;
}


/* ============================================================
 * MD / MKDIR
 * ============================================================
 */

static void cmd_md(char *arg)
{
    int i;


    if (arg[0] == 0)
    {
        terminal_print(
            "The syntax of the command is incorrect.");

        return;
    }


    if (find_dir(
        arg,
        current_dir) >= 0)
    {
        terminal_print(
            "A subdirectory already exists.");

        return;
    }


    for (i = 1; i < MAX_DIRS; i++)
    {
        if (!dirs[i].used)
        {
            dirs[i].used = 1;

            strncpy(
                dirs[i].name,
                arg,
                MAX_NAME - 1);

            dirs[i].name[MAX_NAME - 1] = 0;

            upper(
                dirs[i].name);

            dirs[i].parent =
                current_dir;

            return;
        }
    }


    terminal_print(
        "Insufficient memory.");
}


/* ============================================================
 * RD / RMDIR
 * ============================================================
 */

static void cmd_rd(char *arg)
{
    int d;

    int i;


    d = find_dir(
        arg,
        current_dir);


    if (d < 0)
    {
        terminal_print(
            "The system cannot find the file specified.");

        return;
    }


    /*
     * Cannot remove non-empty directory.
     */

    for (i = 1; i < MAX_DIRS; i++)
    {
        if (dirs[i].used)
        {
            if (dirs[i].parent == d)
            {
                terminal_print(
                    "The directory is not empty.");

                return;
            }
        }
    }


    for (i = 0; i < MAX_FILES; i++)
    {
        if (files[i].used)
        {
            if (files[i].parent == d)
            {
                terminal_print(
                    "The directory is not empty.");

                return;
            }
        }
    }


    dirs[d].used = 0;
}


/* ============================================================
 * DEL
 * ============================================================
 */

static void cmd_del(char *arg)
{
    int f;


    f = find_file(
        arg,
        current_dir);


    if (f < 0)
    {
        terminal_print(
            "The system cannot find the file specified.");

        return;
    }


    files[f].used = 0;
}


/* ============================================================
 * TYPE
 * ============================================================
 */

static void cmd_type(char *arg)
{
    int f;

    int i;

    int pos;

    char line[TERM_WIDTH];


    f = find_file(
        arg,
        current_dir);


    if (f < 0)
    {
        terminal_print(
            "The system cannot find the file specified.");

        return;
    }


    pos = 0;


    for (i = 0;
         files[f].data[i] != 0;
         i++)
    {
        if (files[f].data[i] == '\n')
        {
            line[pos] = 0;

            terminal_print(line);

            pos = 0;
        }
        else
        {
            if (pos < TERM_WIDTH - 1)
            {
                line[pos] =
                    files[f].data[i];

                pos++;
            }
        }
    }


    if (pos > 0)
    {
        line[pos] = 0;

        terminal_print(line);
    }
}


/* ============================================================
 * CLS
 * ============================================================
 */

static void cmd_cls(void)
{
    terminal_clear();
}


/* ============================================================
 * VER
 * ============================================================
 */

static void cmd_ver(void)
{
    terminal_print(
        "C-DOS Version 1.0");

    terminal_print(
        "(C) CASIO C-DOS");
}


/* ============================================================
 * HELP
 * ============================================================
 */

static void cmd_help(void)
{
    terminal_print(
        "DIR CD MD RD DEL TYPE");

    terminal_print(
        "CLS ECHO VER MEM HELP");

    terminal_print(
        "GAME EXIT");
}


/* ============================================================
 * ECHO
 * ============================================================
 */

static void cmd_echo(char *arg)
{
    terminal_print(arg);
}


/* ============================================================
 * MEM
 * ============================================================
 */

static void cmd_mem(void)
{
    terminal_print(
        "C-DOS MEMORY");

    terminal_print(
        "RAM FILESYSTEM");

    terminal_print(
        "SIMULATED DISK");
}


/* ============================================================
 * GAME
 *
 * This is the program represented by GAME.EXE.
 *
 * Run:
 *
 * C:\GAMES>GAME
 *
 * ============================================================
 */

static void cmd_game(void)
{
    unsigned int key;

    int number;

    int secret;


    number = 0;

    secret = 42;


    terminal_print(
        "C-DOS GAME.EXE");

    terminal_print(
        "NUMBER GAME");

    terminal_print(
        "GUESS 1-100");

    terminal_print(
        "EXE TO GUESS");

    terminal_print(
        "EXIT TO QUIT");


    terminal_draw();


    while (1)
    {
        GetKey(&key);


        /*
         * Exit game.
         */

        if (key == KEY_CTRL_EXIT)
        {
            return;
        }


        /*
         * Number keys.
         */

        if (key == KEY_CHAR_0)
        {
            number =
                number * 10;
        }

        if (key == KEY_CHAR_1)
        {
            number =
                number * 10 + 1;
        }

        if (key == KEY_CHAR_2)
        {
            number =
                number * 10 + 2;
        }

        if (key == KEY_CHAR_3)
        {
            number =
                number * 10 + 3;
        }

        if (key == KEY_CHAR_4)
        {
            number =
                number * 10 + 4;
        }

        if (key == KEY_CHAR_5)
        {
            number =
                number * 10 + 5;
        }

        if (key == KEY_CHAR_6)
        {
            number =
                number * 10 + 6;
        }

        if (key == KEY_CHAR_7)
        {
            number =
                number * 10 + 7;
        }

        if (key == KEY_CHAR_8)
        {
            number =
                number * 10 + 8;
        }

        if (key == KEY_CHAR_9)
        {
            number =
                number * 10 + 9;
        }


        /*
         * DEL clears guess.
         */

        if (key == KEY_CTRL_DEL)
        {
            number = 0;
        }


        /*
         * EXE submits guess.
         */

        if (key == KEY_CTRL_EXE)
        {
            if (number == secret)
            {
                terminal_print(
                    "YOU WIN!");

                terminal_draw();

                wait_exe();

                return;
            }


            if (number < secret)
            {
                terminal_print(
                    "TOO LOW");
            }


            if (number > secret)
            {
                terminal_print(
                    "TOO HIGH");
            }


            number = 0;

            terminal_draw();
        }
    }
}


/* ============================================================
 * PARSE COMMAND
 * ============================================================
 */

static void parse_command(
    char *input,
    char *cmd,
    char *arg1,
    char *arg2)
{
    int i;

    int state;

    int pos;


    cmd[0] = 0;

    arg1[0] = 0;

    arg2[0] = 0;


    state = 0;

    pos = 0;


    for (i = 0;
         input[i] != 0;
         i++)
    {
        /*
         * Space.
         */

        if (input[i] == ' ')
        {
            if (pos > 0)
            {
                if (state == 0)
                {
                    cmd[pos] = 0;

                    state = 1;
                }
                else if (state == 1)
                {
                    arg1[pos] = 0;

                    state = 2;
                }

                pos = 0;
            }
        }
        else
        {
            if (state == 0)
            {
                if (pos < MAX_NAME - 1)
                {
                    cmd[pos++] =
                        input[i];
                }
            }
            else if (state == 1)
            {
                if (pos < MAX_NAME - 1)
                {
                    arg1[pos++] =
                        input[i];
                }
            }
            else
            {
                if (pos < MAX_NAME - 1)
                {
                    arg2[pos++] =
                        input[i];
                }
            }
        }
    }


    /*
     * Finish last token.
     */

    if (pos > 0)
    {
        if (state == 0)
        {
            cmd[pos] = 0;
        }
        else if (state == 1)
        {
            arg1[pos] = 0;
        }
        else
        {
            arg2[pos] = 0;
        }
    }


    upper(cmd);

    upper(arg1);

    upper(arg2);
}


/* ============================================================
 * EXECUTE COMMAND
 * ============================================================
 */

static int execute_command(char *input)
{
    char cmd[MAX_NAME];

    char arg1[MAX_NAME];

    char arg2[MAX_NAME];


    parse_command(
        input,
        cmd,
        arg1,
        arg2);


    /*
     * Empty command.
     */

    if (cmd[0] == 0)
    {
        return 1;
    }


    /*
     * DIR
     */

    if (same(cmd, "DIR"))
    {
        cmd_dir();
    }


    /*
     * CD
     */

    else if (same(cmd, "CD"))
    {
        cmd_cd(arg1);
    }


    /*
     * MD
     */

    else if (same(cmd, "MD"))
    {
        cmd_md(arg1);
    }


    /*
     * MKDIR
     */

    else if (same(cmd, "MKDIR"))
    {
        cmd_md(arg1);
    }


    /*
     * RD
     */

    else if (same(cmd, "RD"))
    {
        cmd_rd(arg1);
    }


    /*
     * RMDIR
     */

    else if (same(cmd, "RMDIR"))
    {
        cmd_rd(arg1);
    }


    /*
     * DEL
     */

    else if (same(cmd, "DEL"))
    {
        cmd_del(arg1);
    }


    /*
     * DELETE
     */

    else if (same(cmd, "DELETE"))
    {
        cmd_del(arg1);
    }


    /*
     * TYPE
     */

    else if (same(cmd, "TYPE"))
    {
        cmd_type(arg1);
    }


    /*
     * CLS
     */

    else if (same(cmd, "CLS"))
    {
        cmd_cls();
    }


    /*
     * ECHO
     */

    else if (same(cmd, "ECHO"))
    {
        cmd_echo(arg1);
    }


    /*
     * VER
     */

    else if (same(cmd, "VER"))
    {
        cmd_ver();
    }


    /*
     * MEM
     */

    else if (same(cmd, "MEM"))
    {
        cmd_mem();
    }


    /*
     * HELP
     */

    else if (same(cmd, "HELP"))
    {
        cmd_help();
    }


    /*
     * GAME
     */

    else if (same(cmd, "GAME"))
    {
        cmd_game();
    }


    /*
     * GAME.EXE
     *
     * This makes typing:
     *
     * GAME.EXE
     *
     * work too.
     */

    else if (same(cmd, "GAME.EXE"))
    {
        cmd_game();
    }


    /*
     * EXIT
     */

    else if (same(cmd, "EXIT"))
    {
        return 0;
    }


    /*
     * Unknown command.
     */

    else
    {
        terminal_print(
            "Bad command or file name.");
    }


    return 1;
}


/* ============================================================
 * DOS SHELL
 * ============================================================
 */

static void shell(void)
{
    char command[MAX_COMMAND];

    char prompt[32];

    char line[TERM_WIDTH];

    int running;


    running = 1;


    while (running)
    {
        /*
         * Read command.
         */

        if (!read_command(command))
        {
            return;
        }


        /*
         * Commit command to terminal.
         */

        get_prompt(prompt);


        strcpy(
            line,
            prompt);


        strcat(
            line,
            command);


        terminal_print(line);


        /*
         * Execute command.
         */

        running =
            execute_command(command);


        /*
         * Draw output.
         */

        terminal_draw();
    }
}


/* ============================================================
 * ADD-IN ENTRY POINT
 * ============================================================
 */

int AddIn_main(
    int isAppli,
    unsigned short OptionNum)
{
    /*
     * Avoid unused-variable warnings.
     */

    isAppli = isAppli;

    OptionNum = OptionNum;


    /*
     * Initialize filesystem.
     */

    fs_init();


    /*
     * Start at C:\.
     */

    current_dir = 0;


    /*
     * Clear terminal.
     */

    terminal_clear();


    /*
     * DOS boot text.
     */

    terminal_print(
        "C-DOS Version 1.0");

    terminal_print(
        "(C) CASIO");

    terminal_print(
        "");


    terminal_draw();


    /*
     * Enter DOS shell.
     */

    shell();


    /*
     * Exit screen.
     */

    Bdisp_AllClr_DDVRAM();

    locate(
        1,
        4);

    Print(
        (unsigned char *)"C-DOS EXIT");


    return 1;
}


/* ============================================================
 * OLD CASIO SDK STARTUP
 * ============================================================
 */

#pragma section _BR_Size

unsigned long BR_Size;

#pragma section


#pragma section _TOP

int InitializeSystem(
    int isAppli,
    unsigned short OptionNum)
{
    return INIT_ADDIN_APPLICATION(
        isAppli,
        OptionNum);
}

#pragma section
