function Start()
    local scr = stdscr()
    local y, x = scr:getmaxyx()

    newWindow = newwin(y - 20, x - 10, 5, 5);
    newWindow:refresh()
end

function Display()
    local scr = stdscr()

    local y, x = scr:getmaxyx()

    DebugLog("Hey there chump!\n");
    newWindow:move(0, 0)
    newWindow:clear()
    newWindow:printw("Hi from lua!!!!")
    newWindow:refresh()
end
