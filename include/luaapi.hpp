#include <sol/sol.hpp>
#include <stdarg.h>
#include <curses.h>
#include <format>

struct LuaWindow
{
    WINDOW* window;
    bool    owning;

    int lua_move(int y, int x) { return wmove(window, y, x); }

    int lua_printw(const std::string& format, sol::variadic_args va)
    {
        std::string result = format;

        for (auto arg : va)
        {
            if (arg.is<std::string>())
            {
                // Replace first %s with string value
                size_t pos = result.find("%s");
                if (pos != std::string::npos)
                {
                    result.replace(pos, 2, arg.as<std::string>());
                }
            }
            else if (arg.is<int>())
            {
                // Replace first %d with int value
                size_t pos = result.find("%d");
                if (pos != std::string::npos)
                {
                    result.replace(pos, 2,
                                   std::to_string(arg.as<int>()));
                }
            }
            else if (arg.is<float>())
            {
                // Replace first %f with float value
                size_t pos = result.find("%f");
                if (pos != std::string::npos)
                {
                    result.replace(pos, 2,
                                   std::to_string(arg.as<float>()));
                }
            }
            else if (arg.is<char>())
            {
                // Replace first %c with char value
                size_t pos = result.find("%c");
                if (pos != std::string::npos)
                {
                    result.replace(pos, 2,
                                   std::to_string(arg.as<float>()));
                }
            }
        }

        return wprintw(window, "%s", result.c_str());
    }

    int lua_refresh() { return wrefresh(window); }

    std::tuple<int, int> lua_getmaxyx()
    {
        int y, x;
        getmaxyx(window, y, x);
        return std::make_tuple(y, x);
    }

    int lua_bkgd(int colorPair) { return wbkgd(window, colorPair); }

    int lua_nodelay(bool val) { return nodelay(window, val); }

    int lua_erase() { return werase(window); }

    int lua_addch(int ch) { return waddch(window, ch); }

    int lua_attron(int colorPair)
    {
        return wattron(window, colorPair);
    }

    int lua_attroff(int colorPair)
    {
        return wattroff(window, colorPair);
    }

    int lua_getch() { return wgetch(window); }

    int lua_resize(int rows, int cols)
    {
        return wresize(window, rows, cols);
    }

    int lua_keypad(bool val) { return keypad(window, val); }

    int lua_delwin() { return delwin(window); }

    int lua_clear() { return wclear(window); }
};

struct Pane
{
    std::vector<std::vector<int>> buffer =
        {};             // Buffer of text which you will edit
    int     rows, cols; // Height and width
    int     x, y;       // Top-right anchor position
    WINDOW* window;     // Ncurses window

    int currentRow = 0; // Cursor pos
    int currentCol = 0; // Cursor pos
    int viewportTopRow = 0;
    int viewportLeftCol = 0;

    std::string filename = "noname.txt";
};

struct LuaPane
{
    Pane* pane;

    LuaWindow lua_get_window()
    {
        return {.window = pane->window, .owning = true};
    }

    std::tuple<int, int> lua_get_size()
    {
        return std::tuple<int, int>(pane->rows, pane->cols);
    }

    std::tuple<int, int> lua_get_position()
    {
        return std::tuple<int, int>(pane->y, pane->x);
    }

    std::tuple<int, int> lua_get_cursor_position()
    {
        return std::tuple<int, int>(pane->currentRow,
                                    pane->currentCol);
    }

    std::tuple<int, int> lua_get_viewport_top_row_and_left_col()
    {
        return std::tuple<int, int>(pane->viewportTopRow,
                                    pane->viewportLeftCol);
    }

    const char* lua_get_filename()
    {
        return pane->filename.c_str();
    }

    int lua_get_buffer_size()
    {
        return pane->buffer.size();
    }
    
    int lua_get_buffer_row_size(int bufferRowIndex)
    {
        return pane->buffer[bufferRowIndex].size();
    }
    
    int lua_get_buffer_char(int row, int col)
    {
        return pane->buffer[row][col];
    }
};

inline sol::state luaState;

void InitLua();
