#include <algorithm>
#include <cstdlib> // For getenv
#include <fstream>
#include <locale.h>
#include <string>
#include <vector>
#include <codecvt>
#include <locale>
#include <luaapi.hpp>

std::ofstream debugFile;

#ifdef _WIN32
#    include <windows.h>

HANDLE hConOut = NULL;

HANDLE GetConsoleOutputHandle(void)
{
    SECURITY_ATTRIBUTES sa;

    if (!hConOut)
    {
        /* First call -- get the window handle one time and save it*/
        sa.nLength = sizeof(sa);
        sa.lpSecurityDescriptor = NULL;
        sa.bInheritHandle = TRUE;
        /* Using CreateFile we get the true console handle", avoiding
         * any redirection.*/
        hConOut =
            CreateFile(TEXT("CONOUT$"), GENERIC_READ | GENERIC_WRITE,
                       FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
                       OPEN_EXISTING, (DWORD)0, (HANDLE)0);
    }
    if (!hConOut)
    {
        printf("getConsoleOutputHandle(): failed to get Console "
               "Window Handle\n");
        return NULL;
    }
    return hConOut;
}

#endif

#define ctrl(x) (x & 0x01F)

enum Mode
{
    Mode_Normal,
    Mode_Insert,
    Mode_Command,
    Mode_Replace,
    Mode_ReplaceContinuous
};

int terminalRows;
int terminalCols;
int viewportTopRow;
int viewportLeftCol;
int command;
int indentLevel;

std::vector<Pane> panes;
int               activePane = 0;

std::vector<std::vector<int>> yankedBuffer = {};

std::string statusLine = ""; // Status text
std::string messageText =
    ""; // Message which will display on status line if message isn't
        // null, goes away once you enter insert mode
std::string countString =
    ""; // If you type in a number in normal mode, it will get
        // appended to this string. If you then move the cursor, it
        // will move it this amount and reset the string
std::string commandBuffer = ""; // Command text at bottom of screen

Mode currentMode;

WINDOW* statusWindow;

const int LINE_NUMBER_WIDTH = 5; // Width reserved for line numbers

bool isStartScreen = true;

#ifdef _WIN32
#    define ENTER_KEY 13

short old_screen_w = 0, old_screen_h = 0;

bool CheckResizeWindows(int* x, int* y)
{
    short                      current_screen_w, current_screen_h;
    CONSOLE_SCREEN_BUFFER_INFO csbi;

    if (!hConOut)
    {
        return FALSE;
    }

    if (!GetConsoleScreenBufferInfo(hConOut, &csbi))
    {
        return FALSE;
    }

    // Get the actual visible window size
    current_screen_w = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    current_screen_h = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;

    // Initialize on first call
    if (!old_screen_w && !old_screen_h)
    {
        old_screen_w = current_screen_w;
        old_screen_h = current_screen_h;
        return FALSE;
    }

    // Check if size changed
    if (current_screen_w != old_screen_w ||
        current_screen_h != old_screen_h)
    {
        old_screen_w = current_screen_w;
        old_screen_h = current_screen_h;
        *x = current_screen_w;
        *y = current_screen_h;

        return TRUE;
    }

    return FALSE;
}

#else
#    define ENTER_KEY '\n'
#endif

// If window is resized, this function will adjust the program
void HandleWindowsResize(int newCols, int newRows)
{
    // Let ncurses know about new width and height
    resize_term(newRows, newCols);

    // Update global terminal size
    terminalRows = newRows - 2; // Reserve space for status
    terminalCols = newCols;

    // Clear and refresh the main screen
    clear();
    refresh();

    // Resize and move status window
    wresize(statusWindow, 2, terminalCols);
    mvwin(statusWindow, terminalRows, 0);
    werase(statusWindow);
    wrefresh(statusWindow);

    // Recalculate pane sizes (simple case: single pane takes full
    // space)
    if (panes.size() == 1)
    {
        wresize(panes[0].window, terminalRows, terminalCols);
        mvwin(panes[0].window, 0, 0);
        panes[0].rows = terminalRows;
        panes[0].cols = terminalCols;
        panes[0].x = 0;
        panes[0].y = 0;
    }
    else
    {
        // This approach is hacky, and deletes all other panes
        // TODO: Fix this
        for (int i = 1; i < panes.size(); i++)
        {
            delwin(panes[i].window);
        }
        panes.resize(1);
        activePane = 0;

        wresize(panes[0].window, terminalRows, terminalCols);
        mvwin(panes[0].window, 0, 0);
        panes[0].rows = terminalRows;
        panes[0].cols = terminalCols;
        panes[0].x = 0;
        panes[0].y = 0;
    }

    // Ensure cursor position is still valid
    if (panes[activePane].currentRow >= terminalRows)
    {
        panes[activePane].viewportTopRow =
            panes[activePane].currentRow - terminalRows + 1;
    }
    if (panes[activePane].currentCol >=
        terminalCols - LINE_NUMBER_WIDTH)
    {
        panes[activePane].viewportLeftCol =
            panes[activePane].currentCol -
            (terminalCols - LINE_NUMBER_WIDTH) + 1;
    }

    // Refresh all windows
    for (auto& pane : panes)
    {
        werase(pane.window);
        wrefresh(pane.window);
    }

    // Force a complete redraw
    clearok(stdscr, TRUE);
    refresh();
}

// Expand tilde in filepath to the user directory on windows and home
// on UNIX
std::string ExpandTilde(const std::string& path)
{
    if (!path.empty() && path[0] == '~')
    {
        const char* home = nullptr;

        // Different environment variables based on platform
#ifdef _WIN32
        home = getenv("USERPROFILE");
        if (!home)
        {
            const char* homeDrive = getenv("HOMEDRIVE");
            const char* homePath = getenv("HOMEPATH");
            if (homeDrive && homePath)
            {
                static std::string windowsHome =
                    std::string(homeDrive) + std::string(homePath);
                home = windowsHome.c_str();
            }
        }
#else
        // It's so much simpler on UNIX
        home = getenv("HOME");
#endif

        if (home)
        {
            return std::string(home) +
                   path.substr(1); // Replace '~' with home directory
        }
    }
    return path;
}

// Read a file and return its content in a vector of strings
std::vector<std::string> ReadStartScreen(const std::string& fileName)
{
    std::vector<std::string> startScreenContents;
    std::string              expandedFileName = ExpandTilde(
        fileName); // Expand the tilde into the home directory
                                // on either operating systems

    // Open file and read its contents
    std::ifstream file(expandedFileName);
    if (file.is_open())
    {
        std::string line;
        while (std::getline(file, line))
        {
            startScreenContents.push_back(line);
        }
        file.close();
    }
    else
    {
        startScreenContents.push_back("    Openwell Slote v1.2    ");
        startScreenContents.push_back("     Made by Slugarius     ");
        startScreenContents.push_back("          -------          ");
        startScreenContents.push_back("        :q to quit         ");
        startScreenContents.push_back("        :h for help        ");
    }

    return startScreenContents;
}

std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;

void DisplayStartScreen(
    const std::vector<std::string>& startScreenContents)
{
    // Calculate window size based on content
    int height = startScreenContents.size();
    int width = 0;

    for (const auto& line : startScreenContents)
    {
        // Convert to wide string to properly count unicode characters
        std::wstring wideLine = converter.from_bytes(line);
        int          len = wideLine.length();
        if (len > width)
        {
            width = len;
        }
    }

    // Create window at center
    int     startY = (terminalRows - height) / 2;
    int     startX = (terminalCols - width) / 2;
    WINDOW* startWin = newwin(height, width, startY, startX);

    // Set background color
    wbkgd(startWin, COLOR_PAIR(2));

    // Print each line
    for (int i = 0; i < startScreenContents.size(); i++)
    {
        mvwprintw(startWin, i, 0, "%s",
                  startScreenContents[i].c_str());
    }

    wrefresh(startWin);
}

void MakeVerticalSplit()
{
    panes.push_back({});
    int newIndex = panes.size() - 1;

    // Calculate dimensions for vertical split
    int halfWidth = panes[activePane].cols / 2;

    // Resize current pane to left half
    wresize(panes[activePane].window, panes[activePane].rows,
            halfWidth);
    panes[activePane].cols = halfWidth;

    // Create new pane for right half
    panes[newIndex].x =
        panes[activePane].x + halfWidth;     // Position to the right
    panes[newIndex].y = panes[activePane].y; // Same vertical position
    panes[newIndex].rows = panes[activePane].rows; // Same height
    panes[newIndex].cols =
        panes[activePane].cols; // Same width as resized left pane

    // Create window with correct parameters: (height, width, start_y,
    // start_x)
    panes[newIndex].window =
        newwin(panes[newIndex].rows, panes[newIndex].cols,
               panes[newIndex].y, panes[newIndex].x);

    if (panes[newIndex].window == NULL)
    {
        // Handle error - remove the pane we just added
        panes.pop_back();
        return;
    }

    wbkgd(panes[newIndex].window, COLOR_PAIR(2));

    // Initialize buffer for new pane
    panes[newIndex].buffer.push_back({});

    // Switch to new pane
    activePane = newIndex;

    // Enable keypad for the new window
    keypad(panes[newIndex].window, TRUE);

    move(0, LINE_NUMBER_WIDTH);

    refresh();
}

void MakeHorizontalSplit()
{
    panes.push_back({});
    int newIndex = panes.size() - 1;

    // Calculate dimensions for horizontal split
    int halfHeight = panes[activePane].rows / 2;

    // Resize current pane to top half
    wresize(panes[activePane].window, halfHeight,
            panes[activePane].cols);
    panes[activePane].rows = halfHeight;

    // Create new pane for bottom half
    panes[newIndex].x =
        panes[activePane].x; // Same horizontal position
    panes[newIndex].y =
        panes[activePane].y + halfHeight; // Position below
    panes[newIndex].rows =
        panes[activePane].rows; // Same height as resized top pane
    panes[newIndex].cols = panes[activePane].cols; // Same width

    // Create window with correct parameters: (height, width, start_y,
    // start_x)
    panes[newIndex].window =
        newwin(panes[newIndex].rows, panes[newIndex].cols,
               panes[newIndex].y, panes[newIndex].x);

    if (panes[newIndex].window == NULL)
    {
        // Handle error - remove the pane we just added
        panes.pop_back();
        return;
    }

    wbkgd(panes[newIndex].window, COLOR_PAIR(2));

    // Initialize buffer for new pane
    panes[newIndex].buffer.push_back({});

    // Switch to new pane
    activePane = newIndex;

    // Enable keypad for the new window
    keypad(panes[newIndex].window, TRUE);

    refresh();
}

// Read in a file to current buffer
void ReadFile(const char* filename)
{
    // Clear the buffer to make room for new file
    panes[activePane].buffer.clear();
    std::vector<int> row {};

    // Open file
    std::ifstream ifs(filename);
    // Read in content
    std::string fileContent((std::istreambuf_iterator<char>(ifs)),
                            (std::istreambuf_iterator<char>()));
    for (int i = 0; i < fileContent.size(); i++)
    {
        if (fileContent[i] == '\n')
        {
            panes[activePane].buffer.push_back(row);
            row.clear();
        }
        else
        {
            row.push_back(fileContent[i]);
        }
    }

    // If file is invalid or has zero content, push a line onto the
    // buffer
    if (fileContent.size() == 0)
    {
        panes[activePane].buffer.push_back({});
    }

    if (row.size())
    {
        panes[activePane].buffer.push_back(row);
    }

    ifs.close();
}

// Execute a command
void ExecuteCommand(const std::string& cmd)
{
    // Close pane or quit program if less than 2 panes
    if (cmd == ":q")
    {
        if (panes.size() == 1)
        {
            endwin();
            panes[activePane].buffer.clear();
            yankedBuffer.clear();
            system("clear");
            exit(0);
        }
        else
        {
            // Delete current pane
            panes.erase(panes.begin() + activePane);
            delwin(panes[activePane].window);
            activePane = 0;
        }
    }
    // Save file
    else if (cmd == ":w")
    {
        std::ofstream ofs(panes[activePane].filename,
                          std::ofstream::out);
        std::string   fileContent = "";

        for (int row = 0; row < panes[activePane].buffer.size();
             row++)
        {
            for (int col = 0;
                 col < panes[activePane].buffer[row].size(); col++)
            {
                char character = panes[activePane].buffer[row][col];
                if (character)
                    fileContent += character;
            }
            fileContent += "\n";
        }

        ofs << fileContent;
        ofs.close();
        messageText =
            std::to_string(panes[activePane].buffer.size()) +
            " line(s) written to " + "\"" +
            panes[activePane].filename + "\"";
    }
    // Read file
    else if (cmd.find(":e") == 0)
    {
        std::string rest = cmd.substr(2); // length of ":e"
        rest.erase(0, 1);
        panes[activePane].filename = rest;
        ReadFile(panes[activePane].filename.c_str());
    }
    else if (cmd == ":vsp")
    {
        MakeVerticalSplit();
    }
    else if (cmd == ":sp")
    {
        MakeHorizontalSplit();
    }
    else
    {
        messageText = "Command " + cmd + " was not found";
    }
}

void InitColors()
{
    start_color();

    // Check if terminal supports color changes
    bool canChangeColors = can_change_color();

    // Use custom color indices above standard 8-color range
    const int MY_GREY1 = 100;
    const int MY_GREY2 = 101;
    const int MY_GREY3 = 102;

    if (can_change_color() && COLORS >= 256)
    {
        init_color(MY_GREY1, 800, 800, 800);
        init_color(MY_GREY2, 900, 900, 800);
        init_color(MY_GREY3, 200, 200, 200);

        // Use custom colors in pairs
        init_pair(1, MY_GREY3, MY_GREY2); // Normal text
        init_pair(2, MY_GREY1, MY_GREY3); // Status bar

        bkgd(COLOR_PAIR(2));
    }
    else
    {
        // Fallback for terminals that don't support color
        // redefinition Use standard colors that look good on most
        // terminals
        if (COLORS >= 8)
        {
            init_pair(1, COLOR_WHITE, COLOR_BLUE);  // Status bar
            init_pair(2, COLOR_WHITE, COLOR_BLACK); // Normal text
            init_pair(3, COLOR_BLACK,
                      COLOR_WHITE); // Alternative scheme

            // Use a neutral color scheme
            bkgd(COLOR_PAIR(2));
        }
        else
        {
            // Monochrome fallback
            init_pair(1, COLOR_WHITE, COLOR_BLACK);
            bkgd(COLOR_PAIR(1));
        }
    }
}

void DisplayPane()
{
    debugFile << "Man you takin my job";

    for (int i = 0; i < panes.size(); i++)
    {
        // References for easy access
        WINDOW* win = panes[i].window;
        Pane&   pane = panes[i];

        int currentRow = panes[activePane].currentRow;
        int currentCol = panes[activePane].currentCol;

        werase(win); // Clear window first

        // Go row by row on pane
        for (int row = 0; row < pane.rows;
             row++) // Use pane's own dimensions
        {
            // Get the index of the row we're on
            int bufferRowIndex = row + pane.viewportTopRow;

            // Line numbers
            wmove(win, row, 0);
            if (bufferRowIndex < pane.buffer.size())
            {
                wprintw(win, "%*d", LINE_NUMBER_WIDTH - 1,
                        bufferRowIndex + 1);
            }
            else
            {
                wprintw(win, "%*s", LINE_NUMBER_WIDTH - 1, "~");
            }

            // Text content
            int textWidth = pane.cols - LINE_NUMBER_WIDTH;
            for (int col = 0; col < textWidth; col++)
            {
                int bufferColIndex =
                    col + panes[activePane].viewportLeftCol;
                wmove(win, row, col + LINE_NUMBER_WIDTH);

                if (bufferRowIndex < pane.buffer.size() &&
                    bufferColIndex <
                        pane.buffer[bufferRowIndex].size())
                {
                    waddch(
                        win,
                        pane.buffer[bufferRowIndex][bufferColIndex]);
                }
                else
                {
                    waddch(win, ' '); // Clear with space
                }
            }
        }

        // Set cursor position for active pane
        if (i == activePane)
        {
            int cursorRow = currentRow - pane.viewportTopRow;
            int cursorCol = currentCol -
                            panes[activePane].viewportLeftCol +
                            LINE_NUMBER_WIDTH;
            if (cursorRow >= 0 && cursorRow < pane.rows &&
                cursorCol >= LINE_NUMBER_WIDTH &&
                cursorCol < pane.cols)
            {
                wmove(win, cursorRow, cursorCol);
            }
        }

        wrefresh(win);
    }
}

void DisplayStatus()
{
    int currentRow = panes[activePane].currentRow;
    int currentCol = panes[activePane].currentCol;

    // Turn current mode into a letter that we can display to the user
    std::string modeString;

    switch (currentMode)
    {
        case Mode_Normal:
            modeString = "n";
            break;
        case Mode_Insert:
            modeString = "i";
            break;
        case Mode_Command:
            modeString = "c";
            break;
        case Mode_Replace:
            modeString = "r";
            break;
        case Mode_ReplaceContinuous:
            modeString = "R";
            break;
    }

    statusLine = modeString + " \"" + panes[activePane].filename +
                 "\" " + std::to_string(currentRow + 1) + "/" +
                 std::to_string(panes[activePane].buffer.size());

    statusLine +=
        panes[activePane].buffer.size()
            ? " --" +
                  std::to_string(
                      (int)((currentRow + 1) * 100 /
                            panes[activePane].buffer.size())) +
                  "%-- "
            : "";

    statusLine += "col " + std::to_string(currentCol + 1) + " --x" +
                  (countString.length() ? countString : "0") + "--";

    // Clear the status window first
    werase(statusWindow);

    // Set color attribute
    wattron(statusWindow, COLOR_PAIR(1));

    std::string display_line;

    if (messageText.empty())
    {
        display_line = statusLine;
    }
    else
    {
        display_line = messageText;
    }

    // Ensure display_line fits the terminal width
    if (display_line.length() < terminalCols)
    {
        display_line +=
            std::string(terminalCols - display_line.length(), ' ');
    }
    else
    {
        display_line = display_line.substr(0, terminalCols);
    }

    // Display the status line at row 0 of the status window
    wmove(statusWindow, 0, 0);
    wprintw(statusWindow, "%s", display_line.c_str());

    wattroff(statusWindow, COLOR_PAIR(1));

    // Display command buffer on the second row if in command mode
    wmove(statusWindow, 1, 0);
    wprintw(statusWindow, "%s", commandBuffer.c_str());

    // Refresh the status window to make changes visible
    wrefresh(statusWindow);

    // Set cursor back to the main pane
    if (currentMode != Mode_Command)
    {
        wmove(panes[activePane].window,
              currentRow - panes[activePane].viewportTopRow,
              currentCol - panes[activePane].viewportLeftCol +
                  LINE_NUMBER_WIDTH);
        wrefresh(panes[activePane].window);
    }
}

void GetInput()
{
    int& currentRow = panes[activePane].currentRow;
    int& currentCol = panes[activePane].currentCol;

    int inputChar = -1;

    while (inputChar == -1)
    {
        inputChar = wgetch(panes[activePane].window);
    }

    isStartScreen = false;

    // Switch to normal mode if escape key is pressed
    if (inputChar == ('[' & 0x1f)) // ESCAPE key
    {
        if (currentCol)
        {
            currentCol--;
        }
        currentMode = Mode_Normal;
        curs_set(1);
        countString = "";
        return;
    }

#ifndef _WIN32
    if (inputChar == KEY_RESIZE)
    {
        int newCols, newRows;
        getmaxyx(stdscr, newRows, newCols);
        HandleWindowsResize(newCols, newRows);
    }
#endif

    int repeatCount = atoi(countString.c_str());
    if (repeatCount != 0)
        repeatCount--;

    if (currentMode == Mode_Normal)
    {
        // Switch to insert mode and move one column back
        if (inputChar == 'i')
        {
            currentMode = Mode_Insert;

            if (currentCol >=
                panes[activePane].buffer[currentRow].size())
                currentCol = 0;
            curs_set(3);
            // Reset message
            messageText = "";
            return;
        }
        // Switch to insert mode
        else if (inputChar == 'a')
        {
            currentMode = Mode_Insert;
            if (currentCol <
                panes[activePane]
                    .buffer[currentRow]
                    .size()) // Move one letter forward if not
                             // at the end of the line
                currentCol++;
            curs_set(3);
            // Reset message
            messageText = "";
            return;
        }
#ifdef _WIN32
        // Handle window resize
        else if (inputChar == ctrl('e'))
        {
            CONSOLE_SCREEN_BUFFER_INFO csbi;
            GetConsoleScreenBufferInfo(hConOut, &csbi);
            HandleWindowsResize(csbi.dwSize.X, csbi.dwSize.Y);
        }
#endif
        else if (inputChar == ctrl('c'))
        {
            messageText = "Type ':q' to exit Slote.";
        }
        // Switch pane
        else if (inputChar == ctrl('w'))
        {
            if (activePane < panes.size() - 1)
            {
                activePane++;
            }
            else
            {
                activePane = 0;
            }
        }
        // Go into command mode
        else if (inputChar == ':')
        {
            commandBuffer = ":";
            currentMode = Mode_Command;
        }
        // Switch to insert mode and make new line below current one
        else if (inputChar == 'o')
        {
            std::vector<int> row;
            panes[activePane].buffer.insert(
                panes[activePane].buffer.begin() + currentRow + 1,
                row);
            currentRow++;
            currentCol = 0;
            currentMode = Mode_Insert;
        }
        // Switch to insert mode and make new line above current one
        else if (inputChar == 'O')
        {
            std::vector<int> row;
            panes[activePane].buffer.insert(
                panes[activePane].buffer.begin() + currentRow, row);
            currentCol = 0;
            currentMode = Mode_Insert;
        }
        // Switch to insert mode and go to end of line
        else if (inputChar == 'A')
        {
            currentMode = Mode_Insert;
            currentCol = panes[activePane].buffer[currentRow].size();
        }
        // Switch to replace mode
        else if (inputChar == 'r')
        {
            currentMode = Mode_Replace;
        }
        // Switch to continuous replace mode
        else if (inputChar == 'R')
        {
            currentMode = Mode_ReplaceContinuous;
        }
        // Go to end of file
        else if (inputChar == 'G')
        {
            currentRow = (repeatCount - 1 <=
                                  panes[activePane].buffer.size() - 1
                              ? repeatCount - 1
                              : panes[activePane].buffer.size() - 1);
            countString = "";
        }
        else if (inputChar == 'p' and yankedBuffer.size())
        {
            for (int i = 0; i < yankedBuffer.size(); i++)
            {
                panes[activePane].buffer.insert(
                    panes[activePane].buffer.begin() + currentRow +
                        i + 1,
                    yankedBuffer[i]);
            }
            currentRow += yankedBuffer.size();
        }
        // Yank current line (Or yank current line and then delete it)
        else if (inputChar == 'y' || inputChar == 'd')
        {
            yankedBuffer.clear();
            for (int i = 0;
                 i < (countString.length() ? repeatCount : 1); i++)
            {
                if (currentRow + i < panes[activePane].buffer.size())
                {
                    std::vector<int> row;

                    for (int j = 0; j < panes[activePane]
                                            .buffer[currentRow + i]
                                            .size();
                         j++)
                    {
                        row.insert(row.begin() + j,
                                   panes[activePane]
                                       .buffer[currentRow + i][j]);
                    }
                    yankedBuffer.push_back(row);
                }
            }

            if (inputChar == 'd')
            {
                for (int i = 0;
                     i < (countString.length() ? repeatCount : 1);
                     i++)
                {
                    if (panes[activePane].buffer.size() > 1 &&
                        currentRow < panes[activePane].buffer.size())
                        panes[activePane].buffer.erase(
                            panes[activePane].buffer.begin() +
                            currentRow);
                    if (currentRow == panes[activePane].buffer.size())
                        currentRow--;
                }
            }
            countString = "";
            messageText = (inputChar == 'y' ? "Yank " : "Delete ") +
                          std::to_string(yankedBuffer.size()) +
                          " line(s)";
        }
        else if (inputChar == ' ' || inputChar == 127)
        {
            for (int i = 0;
                 i < (countString.length() ? repeatCount : 1); i++)
            {
                if (currentRow + i < panes[activePane].buffer.size())
                {
                    if (inputChar == ' ' &&
                        currentCol < panes[activePane]
                                         .buffer[currentRow + i]
                                         .size())
                        panes[activePane]
                            .buffer[currentRow + i]
                            .insert(panes[activePane]
                                            .buffer[currentRow + i]
                                            .begin() +
                                        currentCol,
                                    32);
                    else if (inputChar == 127 && currentCol &&
                             currentCol < panes[activePane]
                                              .buffer[currentRow + i]
                                              .size())
                        panes[activePane]
                            .buffer[currentRow + i]
                            .erase(panes[activePane]
                                       .buffer[currentRow + i]
                                       .begin() +
                                   currentCol - 1);
                }
            }

            if (inputChar == ' ')
                currentCol++;
            else if (currentCol)
                currentCol--;
        }
        else if (isdigit(inputChar))
        {
            countString.append(1, inputChar);
        }
        else
        {
            switch (inputChar)
            {
                case '#':
                    currentCol = 0;
                    break;
                case '$':
                    currentCol =
                        panes[activePane].buffer[currentRow].size();
                    break;
                case 'x':
                    if (panes[activePane].buffer[currentRow].size())
                    {
                        panes[activePane].buffer[currentRow].erase(
                            panes[activePane]
                                .buffer[currentRow]
                                .begin() +
                            currentCol);
                    }
                    break;
                case 'h':
                    currentCol ? currentCol -= (1 + repeatCount)
                               : currentCol;
                    countString = "";
                    break;
                case 'j':
                    // Make sure the repeat count doesn't get the
                    // cursor out of bounds
                    if (repeatCount >
                        (panes[activePane].buffer.size() -
                         currentRow) -
                            2)
                    {
                        repeatCount =
                            panes[activePane].buffer.size() -
                            currentRow - 2;
                    }

                    currentRow < panes[activePane].buffer.size() - 1
                        ? currentRow += (1 + repeatCount)
                        : currentRow;
                    countString = "";
                    break;
                case 'k':
                    // Make sure the repeat count doesn't get the
                    // cursor out of bounds
                    if (repeatCount > currentRow - 1)
                    {
                        repeatCount = currentRow - 1;
                    }

                    currentRow ? currentRow -= (1 + repeatCount)
                               : currentRow;
                    countString = "";
                    break;
                case 'l':
                    currentCol < panes[activePane]
                                         .buffer[currentRow]
                                         .size() -
                                     1
                        ? currentCol += (1 + repeatCount)
                        : currentCol;
                    countString = "";
                    break;
            }
            int currentLineLength =
                currentRow < panes[activePane].buffer.size()
                    ? panes[activePane].buffer[currentRow].size()
                    : 0;
            if (currentCol > currentLineLength - 1)
            {
                currentCol = currentLineLength ? currentLineLength - 1
                                               : currentLineLength;
            }
        }
        return;
    }
    else if (currentMode == Mode_Insert)
    {
        if (inputChar == ENTER_KEY)
        {
            std::vector<int> rightSide(
                panes[activePane].buffer[currentRow].size() -
                currentCol);
            std::vector<int> leftSide(currentCol);
            copy(panes[activePane].buffer[currentRow].begin() +
                     currentCol,
                 panes[activePane].buffer[currentRow].begin() +
                     panes[activePane].buffer[currentRow].size(),
                 rightSide.begin());
            copy(panes[activePane].buffer[currentRow].begin(),
                 panes[activePane].buffer[currentRow].begin() +
                     currentCol,
                 leftSide.begin());
            panes[activePane].buffer[currentRow].clear();

            panes[activePane].buffer[currentRow] = leftSide;
            currentRow++;
            currentCol = 0;

            panes[activePane].buffer.insert(
                panes[activePane].buffer.begin() + currentRow,
                rightSide);
            leftSide.clear();
            rightSide.clear();
        }
        else if (inputChar == KEY_BACKSPACE || inputChar == '\b' ||
                 inputChar == 127)
        {
            if (currentCol)
            {
                currentCol--;
                panes[activePane].buffer[currentRow].erase(
                    panes[activePane].buffer[currentRow].begin() +
                    currentCol);
            }
            else if (currentRow)
            {
                std::vector<int> rightSide(
                    panes[activePane].buffer[currentRow].size() -
                    currentCol);
                std::vector<int> leftSide(currentCol);
                copy(panes[activePane].buffer[currentRow].begin() +
                         currentCol,
                     panes[activePane].buffer[currentRow].begin() +
                         panes[activePane].buffer[currentRow].size(),
                     rightSide.begin());
                copy(panes[activePane].buffer[currentRow].begin(),
                     panes[activePane].buffer[currentRow].begin() +
                         currentCol,
                     leftSide.begin());
                panes[activePane].buffer.erase(
                    panes[activePane].buffer.begin() + currentRow);
                currentRow--;
                currentCol =
                    panes[activePane].buffer[currentRow].size();
                panes[activePane].buffer[currentRow].insert(
                    panes[activePane].buffer[currentRow].end(),
                    rightSide.begin(), rightSide.end());
                leftSide.clear();
                rightSide.clear();
            }
        }
        else if (inputChar == '\t')
        {
            panes[activePane].buffer[currentRow].insert(
                panes[activePane].buffer[currentRow].begin() +
                    currentCol,
                ' ');
            panes[activePane].buffer[currentRow].insert(
                panes[activePane].buffer[currentRow].begin() +
                    currentCol,
                ' ');
            panes[activePane].buffer[currentRow].insert(
                panes[activePane].buffer[currentRow].begin() +
                    currentCol,
                ' ');
            panes[activePane].buffer[currentRow].insert(
                panes[activePane].buffer[currentRow].begin() +
                    currentCol,
                ' ');
            currentCol = currentCol + 4;
        }
        else if (inputChar != (inputChar & 0x1f) && inputChar < 128)
        {
            panes[activePane].buffer[currentRow].insert(
                panes[activePane].buffer[currentRow].begin() +
                    currentCol,
                inputChar);
            currentCol++;
        }
    }
    else if (currentMode == Mode_Replace)
    {
        panes[activePane].buffer[currentRow][currentCol] = inputChar;
        currentMode = Mode_Normal;
    }
    else if (currentMode == Mode_ReplaceContinuous)
    {
        if (inputChar != (inputChar & 0x1f) && inputChar < 128 &&
            currentCol < panes[activePane].buffer[currentRow].size())
        {
            panes[activePane].buffer[currentRow][currentCol] =
                inputChar;
            currentCol++;
        }
        if (inputChar == KEY_RESIZE)
        {
            getmaxyx(stdscr, terminalRows, terminalCols);
            terminalRows--;
            currentRow = currentCol = 0;
            refresh();
        }
    }
    else if (currentMode == Mode_Command)
    {
        if (inputChar == KEY_BACKSPACE || inputChar == '\b' ||
            inputChar == 127)
        {
            commandBuffer.pop_back();
        }
        else if (inputChar != (inputChar & 0x1f) && inputChar < 128)
        {
            char newChar = inputChar;
            commandBuffer = commandBuffer + newChar;
        }
        else if (inputChar == ENTER_KEY)
        {
            ExecuteCommand(commandBuffer);
            commandBuffer.clear();
            currentMode = Mode_Normal;
        }
    }
}

void StartProgram()
{
    setlocale(LC_ALL,
              ""); // Set the locale to the default environment locale
    setlocale(LC_CTYPE, ""); // Set locale for UTF-8 support
    initscr();
    nodelay(stdscr, TRUE);
    noecho();
    raw();
}

int main(int argc, char** argv)
{
    debugFile.open("debug.txt");

#ifdef _WIN32
    hConOut = GetConsoleOutputHandle();
#endif

    StartProgram();

    InitLua();

    // Get terminal height and width
    getmaxyx(stdscr, terminalRows, terminalCols);
    // Make room for status bar
    terminalRows = terminalRows - 2;

    // File whose contents will be displayed on the start screen
    std::string              startFile = "~/slotestart.txt";
    std::vector<std::string> startScreenContents =
        ReadStartScreen(startFile);

    InitColors();

    // Make status window
    statusWindow = newwin(2, terminalCols, terminalRows, 0);
    refresh();
    wbkgd(statusWindow, COLOR_PAIR(2));

    // Make first pane
    panes.push_back({});
    panes[0].window = newwin(terminalRows, terminalCols, 0, 0);
    refresh();
    panes[0].x = 0;
    panes[0].y = 0;
    panes[0].rows = terminalRows;
    panes[0].cols = terminalCols;
    wbkgd(panes[0].window, COLOR_PAIR(2));

    // If argument is provided, assume it to be a file and read its
    // contents into the buffer
    if (argc == 2)
    {
        panes[activePane].filename = argv[1];
        isStartScreen = false;
        ReadFile(panes[activePane].filename.c_str());
    }
    // If we have an empty file, add a line to the buffer to not get
    // index errors and have something to work with
    else
    {
        panes[activePane].buffer.push_back({});
    }

    if (panes[activePane].filename != "noname.txt" &&
        panes[activePane].buffer.size() == 0)
    {
        panes[activePane].buffer.push_back({});
    }

    try
    {
        luaState.safe_script_file("D:/Repos/Slote/syntax.lua");
    }
    catch (const sol::error& e)
    {
        debugFile << e.what() << '\n';
    }

    luaState["DebugLog"] = [](const char* msg) { debugFile << msg; };
    luaState["DebugLogInt"] = [](int integer)
    { debugFile << integer; };
    luaState["get_pane"] = [](int index)
    { return LuaPane {.pane = &panes[index]}; };

    luaState["get_active_pane"] = []()
    { return activePane; };

    luaState["get_panes_size"] = []() { return panes.size(); };

    luaState["get_pane_buffer_size"] = [](int index)
    { return panes[index].buffer.size(); };

    luaState["get_pane_buffer_row_size"] =
        [](int index, int bufferRowIndex)
    { return (int)(panes[index].buffer[bufferRowIndex].size()); };

    luaState["get_pane_buffer_char"] =
        [](int index, int row, int col)
    { return panes[index].buffer[row][col]; };

    luaState["get_status_window"] = []()
    { return LuaWindow {.window = statusWindow, .owning = true}; };

    luaState["LINE_NUMBER_WIDTH"] = LINE_NUMBER_WIDTH;

    {
        sol::protected_function func = luaState["Start"];
        if (func)
        {
            auto res = func();
            if (!res.valid())
            {
                sol::error e = res;
                debugFile << e.what() << '\n';
            }
        }
    }

    sol::protected_function displayFunc = luaState["DisplayPane"];

    sol::protected_function func = luaState["Display"];

    // Main loop of program
    while (TRUE)
    {
        // Make sure cursor doesn't step out of bounds
        if (panes[activePane].currentRow <
            panes[activePane].viewportTopRow)
        {
            panes[activePane].viewportTopRow =
                panes[activePane].currentRow;
        }
        if (panes[activePane].currentRow >=
            panes[activePane].viewportTopRow + terminalRows)
        {
            panes[activePane].viewportTopRow =
                panes[activePane].currentRow - terminalRows + 1;
        }
        if (panes[activePane].currentCol <
            panes[activePane].viewportLeftCol)
        {
            panes[activePane].viewportLeftCol =
                panes[activePane].currentCol;
        }
        if (panes[activePane].currentCol >=
            panes[activePane].viewportLeftCol + terminalCols)
        {
            panes[activePane].viewportLeftCol =
                panes[activePane].currentCol - terminalCols + 1;
        }

        if (displayFunc)
        {
            auto res = displayFunc();
            if (!res.valid())
            {
                sol::error e = res;
                debugFile << e.what() << '\n';
            }
        }
        else
        {
            displayFunc();
        }

        DisplayStatus();

        if (isStartScreen)
        {
            DisplayStartScreen(startScreenContents);
        }

        if (func)
        {
            auto res = func();
            if (!res.valid())
            {
                sol::error e = res;
                debugFile << e.what();
            }
        }

        GetInput();

        continue;
    }

    // End of program
    endwin();
    panes[activePane].buffer.clear();
    yankedBuffer.clear();
    return 0;
}
