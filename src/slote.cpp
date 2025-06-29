#define PDC_WIDE
#include <algorithm>
#include <cstdlib> // For getenv
#include <curses.h>
#include <fstream>
#include <locale.h>
#include <string>
#include <vector>

#define ctrl(x) (x & 0x01F)

enum Mode
{
    Mode_Normal,
    Mode_Insert,
    Mode_Command,
    Mode_Replace,
    Mode_ReplaceContinuous
};

int terminalRows, terminalCols, currentRow, currentCol,
    viewportTopRow, viewportLeftCol, command, indentLevel;

std::string filename = "noname.txt", statusLine = "",
            messageText = "", countString = "", commandBuffer = "";
std::vector<std::vector<int>> buffer = {}, yankedBuffer = {};

Mode currentMode;

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

        // Try different environment variables based on platform
#ifdef _WIN32
        // Windows: try USERPROFILE first, then HOMEDRIVE+HOMEPATH
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
        // Unix/Linux: use HOME
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
        while (getline(file, line))
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

void DisplayStartScreen(
    const std::vector<std::string>& startScreenContents)
{
    clear();
    int startY = (terminalRows - startScreenContents.size()) / 2;
    int startX;
    for (int i = 0; i < startScreenContents.size(); i++)
    {
        startX = (terminalCols - startScreenContents[i].length()) / 2;
        mvprintw(startY + i, startX, "%s",
                 startScreenContents[i].c_str());
    }

    mvprintw(terminalRows - 1, (terminalCols - 26) / 2,
             "Press any key to continue...");
    refresh();
}

void ExecuteCommand(const std::string& cmd)
{
    if (cmd == ":q")
    {
        endwin();
        buffer.clear();
        yankedBuffer.clear();
        system("clear");
        exit(0);
    }
    else if (cmd == ":w")
    {
        std::ofstream ofs(filename, std::ofstream::out);
        std::string   fileContent = "";

        for (int row = 0; row < buffer.size(); row++)
        {
            for (int col = 0; col < buffer[row].size(); col++)
            {
                char character = buffer[row][col];
                if (character)
                    fileContent += character;
            }
            fileContent += "\n";
        }

        ofs << fileContent;
        ofs.close();
        messageText = std::to_string(buffer.size()) +
                      " line(s) written to " + "\"" + filename + "\"";
    }
    else if (cmd.find(":e") == 0)
    {
        std::string rest = cmd.substr(2); // length of ":e"
        rest.erase(0, 1);
        filename = rest;
        buffer.clear();
        try
        {
            std::vector<int> row;

            std::ifstream ifs(filename);
            std::string   fileContent(
                (std::istreambuf_iterator<char>(ifs)),
                (std::istreambuf_iterator<char>()));
            for (int i = 0; i < fileContent.size(); i++)
            {
                if (fileContent[i] == '\n')
                {
                    buffer.push_back(row);
                    row.clear();
                }
                else
                {
                    row.push_back(fileContent[i]);
                }
            }

            if (row.size())
            {
                buffer.push_back(row);
            }

            ifs.close();
        }
        catch (std::exception& e)
        {
        }
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
        init_color(MY_GREY1, 400, 400, 400); // Dark grey
        init_color(MY_GREY2, 300, 300, 300); // Medium grey
        init_color(MY_GREY3, 900, 900, 900); // Light grey

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

int main(int argc, char** argv)
{
    debugFile.open("debug.txt");

    setlocale(LC_ALL,
              ""); // Set the locale to the default environment locale
    setlocale(LC_CTYPE, ""); // Set locale for UTF-8 support
    initscr();
    nodelay(stdscr, TRUE);
    noecho();
    raw();

    getmaxyx(stdscr, terminalRows, terminalCols);
    terminalRows = terminalRows - 2;
    std::vector<int> row;

    std::string              startFile = "~/slotestart.txt";
    std::vector<std::string> startScreenContents =
        ReadStartScreen(startFile);

    int startScreenChar = -1;

    InitColors();
    clear();

    DisplayStartScreen(startScreenContents);

    while (startScreenChar == -1) { startScreenChar = getch(); }

    clear();

    if (argc == 2)
    {
        filename = argv[1];
    }
    else
    {
        buffer.push_back(row);
    }

    try
    {
        std::vector<int> row;

        std::ifstream ifs(filename);
        std::string fileContent((std::istreambuf_iterator<char>(ifs)),
                                (std::istreambuf_iterator<char>()));
        for (int i = 0; i < fileContent.size(); i++)
        {
            if (fileContent[i] == '\n')
            {
                buffer.push_back(row);
                row.clear();
            }
            else
            {
                row.push_back(fileContent[i]);
            }
        }

        if (row.size())
        {
            buffer.push_back(row);
        }

        ifs.close();
    }
    catch (std::exception& e)
    {
    }

    if (filename != "noname.txt" && buffer.size() == 0)
    {
        buffer.push_back(row);
    }

    // Main loop of program
    while (TRUE)
    {
        // Make sure cursor doesn't step out of bounds
        if (currentRow < viewportTopRow)
        {
            viewportTopRow = currentRow;
        }
        if (currentRow >= viewportTopRow + terminalRows)
        {
            viewportTopRow = currentRow - terminalRows + 1;
        }
        if (currentCol < viewportLeftCol)
        {
            viewportLeftCol = currentCol;
        }
        if (currentCol >= viewportLeftCol + terminalCols)
        {
            viewportLeftCol = currentCol - terminalCols + 1;
        }

        move(0, 0);

        for (int row = 0; row < terminalRows; row++)
        {
            int bufferRowIndex = row + viewportTopRow;

            // Print line numbers
            if (bufferRowIndex < buffer.size())
            {
                mvprintw(row, 0, "%*d", LINE_NUMBER_WIDTH - 1,
                         bufferRowIndex + 1);
            }
            else
            {
                mvprintw(row, 0, "%*s", LINE_NUMBER_WIDTH - 1, "~");
            }

            for (int col = 0; col < terminalCols; col++)
            {
                int bufferColIndex = col + viewportLeftCol;
                if (bufferRowIndex < buffer.size() &&
                    bufferColIndex < buffer[bufferRowIndex].size())
                {
                    mvaddch(row, col + LINE_NUMBER_WIDTH,
                            buffer[bufferRowIndex][bufferColIndex]);
                }
            }

            clrtoeol();
            addstr(bufferRowIndex < buffer.size() - 1 ? "\n" : "\n~");
        }

        std::string modeString;

        switch (currentMode)
        {
            case Mode_Normal:
            {
                modeString = "n";
                break;
            }
            case Mode_Insert:
            {
                modeString = "i";
                break;
            }
            case Mode_Command:
            {
                modeString = "c";
                break;
            }
            case Mode_Replace:
            {
                modeString = "r";
                break;
            }
            case Mode_ReplaceContinuous:
            {
                modeString = "R";
                break;
            }
        }

        statusLine = modeString + " \"" + filename + "\" " +
                     std::to_string(currentRow + 1) + "/" +
                     std::to_string(buffer.size());

        statusLine +=
            buffer.size()
                ? " --" +
                      std::to_string((int)((currentRow + 1) * 100 /
                                           buffer.size())) +
                      "%-- "
                : "";

        statusLine +=
            "col " + std::to_string(currentCol + 1) + " --x" +
            (countString.length() ? countString : "0") + "--";

        move(terminalRows, 0);

        attron(COLOR_PAIR(1));

        std::string display_line;

        if (messageText.empty())
        {
            display_line = statusLine;
        }
        else
        {
            display_line = messageText;
        }

        // Ensure display_line is the same length as the width of the
        // window or the desired line length
        if (display_line.length() < terminalCols)
        {
            display_line += std::string(
                terminalCols - display_line.length(), ' ');
        }
        else
        {
            display_line = display_line.substr(
                0, terminalCols); // Truncate if it's too long
        }

        // Display the line
        for (int i = 0; i < display_line.length(); i++)
        {
            addch(display_line[i]);
            messageText.clear();
        }

        move(terminalRows + 1, 0);
        for (int i = 0; i < commandBuffer.length(); i++)
            addch(commandBuffer[i]);

        attroff(COLOR_PAIR(1));

        clrtoeol();
        move(currentRow - viewportTopRow,
             currentCol - viewportLeftCol + LINE_NUMBER_WIDTH);
        refresh();

        int inputChar = -1;

        while (inputChar == -1) { inputChar = getch(); }

        if (inputChar == ('[' & 0x1f))
        {
            if (currentCol)
            {
                currentCol--;
            }
            currentMode = Mode_Normal;
            curs_set(1);
            countString = "";
            continue;
        }

        int repeatCount = atoi(countString.c_str());

        if (currentMode == Mode_Normal)
        {
            if (inputChar == 'i')
            {
                currentMode = Mode_Insert;

                if (currentCol >= buffer[currentRow].size())
                    currentCol = 0;
                curs_set(3);
                continue;
            }
            else if (inputChar == 'a')
            {
                currentMode = Mode_Insert;
                if (currentCol <
                    buffer[currentRow]
                        .size()) // Move one letter forward if not
                                 // at the end of the line
                    currentCol++;
                curs_set(3);
                continue;
            }
            else if (inputChar == ctrl('c'))
            {
                messageText = "Type ':q' to exit Slote.";
            }
            else if (inputChar == ':')
            {
                commandBuffer = ":";
                currentMode = Mode_Command;
            }
            else if (inputChar == 'o')
            {
                std::vector<int> row;
                buffer.insert(buffer.begin() + currentRow + 1, row);
                currentRow++;
                currentCol = 0;
                currentMode = Mode_Insert;
            }
            else if (inputChar == 'O')
            {
                std::vector<int> row;
                buffer.insert(buffer.begin() + currentRow, row);
                currentCol = 0;
                currentMode = Mode_Insert;
            }
            else if (inputChar == 'A')
            {
                currentMode = Mode_Insert;
                currentCol = buffer[currentRow].size();
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
                currentRow = (repeatCount - 1 <= buffer.size() - 1
                                  ? repeatCount - 1
                                  : buffer.size() - 1);
                countString = "";
            }
            else if (inputChar == 'p' and yankedBuffer.size())
            {
                for (int i = 0; i < yankedBuffer.size(); i++)
                {
                    buffer.insert(buffer.begin() + currentRow + i + 1,
                                  yankedBuffer[i]);
                }
                currentRow += yankedBuffer.size();
            }
            else if (inputChar == 'y' || inputChar == 'd')
            {
                yankedBuffer.clear();
                for (int i = 0;
                     i < (countString.length() ? repeatCount : 1);
                     i++)
                {
                    if (currentRow + i < buffer.size())
                    {
                        std::vector<int> row;

                        for (int j = 0;
                             j < buffer[currentRow + i].size(); j++)
                        {
                            row.insert(row.begin() + j,
                                       buffer[currentRow + i][j]);
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
                        if (buffer.size() > 1 &&
                            currentRow < buffer.size())
                            buffer.erase(buffer.begin() + currentRow);
                        if (currentRow == buffer.size())
                            currentRow--;
                    }
                }
                countString = "";
                messageText =
                    (inputChar == 'y' ? "Yank " : "Delete ") +
                    std::to_string(yankedBuffer.size()) + " line(s)";
            }
            else if (inputChar == ' ' || inputChar == 127)
            {
                for (int i = 0;
                     i < (countString.length() ? repeatCount : 1);
                     i++)
                {
                    if (currentRow + i < buffer.size())
                    {
                        if (inputChar == ' ' &&
                            currentCol <
                                buffer[currentRow + i].size())
                            buffer[currentRow + i].insert(
                                buffer[currentRow + i].begin() +
                                    currentCol,
                                32);
                        else if (inputChar == 127 && currentCol &&
                                 currentCol <
                                     buffer[currentRow + i].size())
                            buffer[currentRow + i].erase(
                                buffer[currentRow + i].begin() +
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
                        currentCol = buffer[currentRow].size();
                        break;
                    case 'x':
                        if (buffer[currentRow].size())
                        {
                            buffer[currentRow].erase(
                                buffer[currentRow].begin() +
                                currentCol);
                        }
                        break;
                    case 'h':
                        currentCol ? currentCol-- : currentCol;
                        break;
                    case 'j':
                        currentRow < buffer.size() - 1 ? currentRow++
                                                       : currentRow;
                        break;
                    case 'k':
                        currentRow ? currentRow-- : currentRow;
                        break;
                    case 'l':
                        currentCol < buffer[currentRow].size() - 1
                            ? currentCol++
                            : currentCol;
                        break;
                }
                int currentLineLength =
                    currentRow < buffer.size()
                        ? buffer[currentRow].size()
                        : 0;
                if (currentCol > currentLineLength - 1)
                {
                    currentCol = currentLineLength
                                     ? currentLineLength - 1
                                     : currentLineLength;
                }
            }
            continue;
        }
        else if (currentMode == Mode_Insert)
        {
            indentLevel = countString.length() ? repeatCount : 0;
            if (inputChar == ENTER_KEY)
            {
                std::vector<int> rightSide(buffer[currentRow].size() -
                                           currentCol);
                std::vector<int> leftSide(currentCol);
                copy(buffer[currentRow].begin() + currentCol,
                     buffer[currentRow].begin() +
                         buffer[currentRow].size(),
                     rightSide.begin());
                copy(buffer[currentRow].begin(),
                     buffer[currentRow].begin() + currentCol,
                     leftSide.begin());
                buffer[currentRow].clear();

                buffer[currentRow] = leftSide;
                currentRow++;
                currentCol = 0;

                buffer.insert(buffer.begin() + currentRow, rightSide);
                leftSide.clear();
                rightSide.clear();

                for (int i = 0; i < indentLevel; i++)
                {
                    buffer[currentRow].insert(
                        buffer[currentRow].begin() + currentCol, 32);
                    currentCol += 1;
                }
            }
            else if (inputChar == KEY_BACKSPACE ||
                     inputChar == '\b' || inputChar == 127)
            {
                if (currentCol)
                {
                    currentCol--;
                    buffer[currentRow].erase(
                        buffer[currentRow].begin() + currentCol);
                }
                else if (currentRow)
                {
                    std::vector<int> rightSide(
                        buffer[currentRow].size() - currentCol);
                    std::vector<int> leftSide(currentCol);
                    copy(buffer[currentRow].begin() + currentCol,
                         buffer[currentRow].begin() +
                             buffer[currentRow].size(),
                         rightSide.begin());
                    copy(buffer[currentRow].begin(),
                         buffer[currentRow].begin() + currentCol,
                         leftSide.begin());
                    buffer.erase(buffer.begin() + currentRow);
                    currentRow--;
                    currentCol = buffer[currentRow].size();
                    buffer[currentRow].insert(
                        buffer[currentRow].end(), rightSide.begin(),
                        rightSide.end());
                    leftSide.clear();
                    rightSide.clear();
                }
            }
            else if (inputChar == '\t')
            {
                buffer[currentRow].insert(
                    buffer[currentRow].begin() + currentCol, ' ');
                buffer[currentRow].insert(
                    buffer[currentRow].begin() + currentCol, ' ');
                buffer[currentRow].insert(
                    buffer[currentRow].begin() + currentCol, ' ');
                buffer[currentRow].insert(
                    buffer[currentRow].begin() + currentCol, ' ');
                currentCol = currentCol + 4;
            }
            else if (inputChar != (inputChar & 0x1f) &&
                     inputChar < 128)
            {
                buffer[currentRow].insert(buffer[currentRow].begin() +
                                              currentCol,
                                          inputChar);
                currentCol++;
            }
        }
        else if (currentMode == Mode_Replace)
        {
            buffer[currentRow][currentCol] = inputChar;
            currentMode = Mode_Normal;
        }
        else if (currentMode == Mode_ReplaceContinuous)
        {
            if (inputChar != (inputChar & 0x1f) && inputChar < 128 &&
                currentCol < buffer[currentRow].size())
            {
                buffer[currentRow][currentCol] = inputChar;
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
            else if (inputChar != (inputChar & 0x1f) &&
                     inputChar < 128)
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

exitprog:
    endwin();
    buffer.clear();
    yankedBuffer.clear();
    return 0;
}
