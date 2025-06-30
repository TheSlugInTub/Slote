#include <algorithm>
#include <cstdlib> // For getenv
#include <curses.h>
#include <fstream>
#include <locale.h>
#include <string>
#include <vector>
#include <codecvt>
#include <locale>

#define ctrl(x) (x & 0x01F)

enum Mode
{
    Mode_Normal,
    Mode_Insert,
    Mode_Command,
    Mode_Replace,
    Mode_ReplaceContinuous
};

struct Pane
{
    std::vector<std::vector<int>> buffer = {};
    int                           rows, cols;
    int                           x, y;
    WINDOW*                       window;

    int currentRow = 0;
    int currentCol = 0;
    int viewportTopRow = 0;
    int viewportLeftCol = 0;
};

int terminalRows, terminalCols, viewportTopRow, viewportLeftCol,
    command, indentLevel;

std::vector<Pane> panes;
int               activePane = 0;

std::vector<std::vector<int>> yankedBuffer = {};
std::string filename = "noname.txt", statusLine = "",
            messageText = "", countString = "", commandBuffer = "";

Mode currentMode;

WINDOW* statusWindow;

const int LINE_NUMBER_WIDTH = 5; // Width reserved for line numbers

std::ofstream debugFile;

#ifdef _WIN32
#    define ENTER_KEY 13
#else
#    define ENTER_KEY '\n'
#endif

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

std::vector<std::string> ReadStartScreen(const std::string& fileName)
{
    std::vector<std::string> startScreenContents;
    std::string              expandedFileName =
        ExpandTilde(fileName); // Expand the tilde

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
        startScreenContents.push_back("Openwell Slote v1.0");
    }

    return startScreenContents;
}

std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;

void DisplayStartScreen(
    const std::vector<std::string>& startScreenContents)
{
    clear();
    int startY = (terminalRows - startScreenContents.size()) / 2;
    int stringLen = 0; // Longest string in startScreenContents

    for (int i = 0; i < startScreenContents.size(); i++)
    {
        std::wstring wideLine =
            converter.from_bytes(startScreenContents[i]);
        int len = wideLine.length();

        if (len >= stringLen)
        {
            stringLen = len;
        }
    }

    int startX = (terminalCols / 2) - (stringLen / 2);

    for (int i = 0; i < startScreenContents.size(); i++)
    {
        mvprintw(startY + i, startX, "%s",
                 startScreenContents[i].c_str());
    }

    mvprintw(terminalRows - 1, (terminalCols - 26) / 2,
             "Press any key to continue...");
    refresh();
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

void ExecuteCommand(const std::string& cmd)
{
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
            panes.erase(panes.begin() + activePane);
            delwin(panes[activePane].window);
            activePane = 0;
        }
    }
    else if (cmd == ":w")
    {
        std::ofstream ofs(filename, std::ofstream::out);
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
            " line(s) written to " + "\"" + filename + "\"";
    }
    else if (cmd.find(":e") == 0)
    {
        std::string rest = cmd.substr(2); // length of ":e"
        rest.erase(0, 1);
        filename = rest;
        panes[activePane].buffer.clear();
        try
        {
            std::vector<int> row {};

            std::ifstream ifs(filename);
            std::string   fileContent(
                (std::istreambuf_iterator<char>(ifs)),
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
        catch (std::exception& e)
        {
        }
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
        init_color(MY_GREY1, 800, 800, 800); // Dark grey
        init_color(MY_GREY2, 900, 900, 800); // Medium grey
        init_color(MY_GREY3, 200, 200, 200); // Light grey

        // Use custom colors in pairs
        init_pair(1, MY_GREY3, MY_GREY2); // White on grey
        init_pair(2, MY_GREY1, MY_GREY3); // Grey on white

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
    for (int i = 0; i < panes.size(); i++)
    {
        WINDOW* win = panes[i].window;
        Pane&   pane = panes[i];

        int currentRow = panes[activePane].currentRow;
        int currentCol = panes[activePane].currentCol;

        werase(win); // Clear window first

        for (int row = 0; row < pane.rows;
             row++) // Use pane's own dimensions
        {
            int bufferRowIndex = row + viewportTopRow;

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
                int bufferColIndex = col + viewportLeftCol;
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
            int cursorRow = currentRow - viewportTopRow;
            int cursorCol =
                currentCol - viewportLeftCol + LINE_NUMBER_WIDTH;
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

    statusLine = modeString + " \"" + filename + "\" " +
                 std::to_string(currentRow + 1) + "/" +
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
        wmove(panes[activePane].window, currentRow - viewportTopRow,
              currentCol - viewportLeftCol + LINE_NUMBER_WIDTH);
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

    if (inputChar == ('[' & 0x1f))
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

    if (inputChar == KEY_RESIZE)
    {
        getmaxyx(stdscr, terminalRows, terminalCols);
        terminalRows -= 2;

        refresh();
    }

    int repeatCount = atoi(countString.c_str());

    if (currentMode == Mode_Normal)
    {
        if (inputChar == 'i')
        {
            currentMode = Mode_Insert;

            if (currentCol >=
                panes[activePane].buffer[currentRow].size())
                currentCol = 0;
            curs_set(3);
            return;
        }
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
            return;
        }
        else if (inputChar == ctrl('c'))
        {
            messageText = "Type ':q' to exit Slote.";
        }
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
        else if (inputChar == ':')
        {
            commandBuffer = ":";
            currentMode = Mode_Command;
        }
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
        else if (inputChar == 'O')
        {
            std::vector<int> row;
            panes[activePane].buffer.insert(
                panes[activePane].buffer.begin() + currentRow, row);
            currentCol = 0;
            currentMode = Mode_Insert;
        }
        else if (inputChar == 'A')
        {
            currentMode = Mode_Insert;
            currentCol = panes[activePane].buffer[currentRow].size();
        }
        else if (inputChar == 'r')
        {
            currentMode = Mode_Replace;
        }
        else if (inputChar == 'R')
        {
            currentMode = Mode_ReplaceContinuous;
        }
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
                    currentCol ? currentCol-- : currentCol;
                    break;
                case 'j':
                    currentRow < panes[activePane].buffer.size() - 1
                        ? currentRow++
                        : currentRow;
                    break;
                case 'k':
                    currentRow ? currentRow-- : currentRow;
                    break;
                case 'l':
                    currentCol < panes[activePane]
                                         .buffer[currentRow]
                                         .size() -
                                     1
                        ? currentCol++
                        : currentCol;

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
        indentLevel = countString.length() ? repeatCount : 0;
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

            for (int i = 0; i < indentLevel; i++)
            {
                panes[activePane].buffer[currentRow].insert(
                    panes[activePane].buffer[currentRow].begin() +
                        currentCol,
                    32);
                currentCol += 1;
            }
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

    StartProgram();

    getmaxyx(stdscr, terminalRows, terminalCols);
    terminalRows = terminalRows - 2;

    std::string              startFile = "~/slotestart.txt";
    std::vector<std::string> startScreenContents =
        ReadStartScreen(startFile);

    int startScreenChar = -1;

    InitColors();

    DisplayStartScreen(startScreenContents);

    while (startScreenChar == -1) { startScreenChar = getch(); }
    clear();

    statusWindow = newwin(2, terminalCols, terminalRows, 0);
    refresh();
    wbkgd(statusWindow, COLOR_PAIR(2));

    panes.push_back({});
    panes[0].window = newwin(terminalRows, terminalCols, 0, 0);
    refresh();
    panes[0].x = 0;
    panes[0].y = 0;
    panes[0].rows = terminalRows;
    panes[0].cols = terminalCols;
    wbkgd(panes[0].window, COLOR_PAIR(2));

    if (argc == 2)
    {
        filename = argv[1];
    }
    else
    {
        panes[activePane].buffer.push_back({});
    }

    if (filename != "noname.txt" &&
        panes[activePane].buffer.size() == 0)
    {
        panes[activePane].buffer.push_back({});
    }

    // Main loop of program
    while (TRUE)
    {
        // Make sure cursor doesn't step out of bounds
        if (panes[activePane].currentRow < viewportTopRow)
        {
            viewportTopRow = panes[activePane].currentRow;
        }
        if (panes[activePane].currentRow >=
            viewportTopRow + terminalRows)
        {
            viewportTopRow =
                panes[activePane].currentRow - terminalRows + 1;
        }
        if (panes[activePane].currentCol < viewportLeftCol)
        {
            viewportLeftCol = panes[activePane].currentCol;
        }
        if (panes[activePane].currentCol >=
            viewportLeftCol + terminalCols)
        {
            viewportLeftCol =
                panes[activePane].currentCol - terminalCols + 1;
        }

        DisplayPane();
        DisplayStatus();
        GetInput();

        continue;
    }

    endwin();
    panes[activePane].buffer.clear();
    yankedBuffer.clear();
    return 0;
}
