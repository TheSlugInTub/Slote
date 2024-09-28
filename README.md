# Slote

Minimal and fully customizable vi-like text editor in C++.

Slote uses ncurses for advanced terminal stuff.

Forked off of https://github.com/maksimKorzh/v.

# Features

- Start screen, you can change the start screen in the config/start.txt file.
- Cursor movement
- Modal, with normal and insert modes

# Commands

Press h, j, k and l to move around in normal mode like in vim.

Press w in normal mode to save file.

Press Escape in insert mode to switch to normal mode.

Press i to enter insert mode, the cursor will be set back one letter.
Press a to enter insert mode, the cursor will be set forward one letter.

Press q in normal mode to quit.
