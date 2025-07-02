#include <luaapi.hpp>

#pragma comment(lib, "luajit.lib")

void InitLua()
{
    luaState.open_libraries(sol::lib::base, sol::lib::io,
                            sol::lib::math, sol::lib::table,
                            sol::lib::string);

    auto window_type = luaState.new_usertype<LuaWindow>(
        "Window", sol::constructors<LuaWindow>(), "move",
        &LuaWindow::lua_move,                 // BLAH BLAH
        "refresh", &LuaWindow::lua_refresh,   // BLAH BLAH
        "getmaxyx", &LuaWindow::lua_getmaxyx, // BLAH BLAH
        "nodelay", &LuaWindow::lua_nodelay,   // BLAH BLAH
        "erase", &LuaWindow::lua_erase,       // BLAH BLAH
        "printw", &LuaWindow::lua_printw,     // BLAH BLAH
        "addch", &LuaWindow::lua_addch,       // BLAH BLAH
        "attron", &LuaWindow::lua_attron,     // BLAH BLAH
        "getch", &LuaWindow::lua_getch,       // BLAH BLAH
        "resize", &LuaWindow::lua_resize,     // BLAH BLAH
        "delwin", &LuaWindow::lua_delwin,     // BLAH BLAH
        "keypad", &LuaWindow::lua_keypad,     // BLAH BLAH
        "resize", &LuaWindow::lua_resize,     // BLAH BLAH
        "clear", &LuaWindow::lua_clear,       // BLAH BLAH
        "attroff", &LuaWindow::lua_attroff);  // BLAH BLAH

    auto pane_type = luaState.new_usertype<LuaPane>(
        "Pane", sol::constructors<LuaWindow>(),     // BLAH
        "get_window", &LuaPane::lua_get_window,     // BLAH BLAH
        "get_size", &LuaPane::lua_get_size,         // BLAH BLAH
        "get_position", &LuaPane::lua_get_position, // BLAH BLAH
        "get_cursor_position", &LuaPane::lua_get_cursor_position,
        "get_viewport_top_row_and_left_col",
        &LuaPane::lua_get_viewport_top_row_and_left_col, // BLAH BLAH
        "get_buffer_size", &LuaPane::lua_get_buffer_size,
        "get_buffer_row_size", &LuaPane::lua_get_buffer_row_size,
        "get_buffer_char", &LuaPane::lua_get_buffer_char,
        "get_filename", &LuaPane::lua_get_filename);

    luaState["newwin"] =
        [](int nlines, int ncols, int begin_y, int begin_x)
    {
        WINDOW* w = newwin(nlines, ncols, begin_y, begin_x);
        return LuaWindow(w, true); // owns the window
    };

    luaState["stdscr"] = []()
    {
        return LuaWindow(stdscr, false); // doesn't own stdscr
    };

    luaState["endwin"] = []() { return endwin(); };

    luaState["noecho"] = []() { return noecho(); };

    luaState["initscr"] = []() { return initscr(); };

    luaState["COLOR_PAIR"] = [](int index)
    { return COLOR_PAIR(index); };

    luaState["setlocale"] = [](int category, const char* locale)
    { return setlocale(category, locale); };

    luaState["start_color"] = []() { return start_color(); };

    luaState["can_change_color"] = []()
    { return can_change_color(); };

    luaState["init_color"] = [](int col, int r, int g, int b)
    { return init_color(col, r, g, b); };

    luaState["init_pair"] = [](int index, int col1, int col2)
    { return init_pair(index, col1, col2); };

    luaState["curs_set"] = [](int set) { return curs_set(set); };
}
