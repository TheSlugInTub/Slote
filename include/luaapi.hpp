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
};

inline sol::state luaState;

void InitLua();
