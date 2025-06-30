# Slote

Minimal and fully customizable vi-like text editor in C++.

Slote uses ncurses for advanced terminal stuff.

Forked off of https://github.com/maksimKorzh/v.

# Features

- Start screen, you can change the start screen in C:\Users\[user]\slotestart.txt
OR ~/slotestart.txt on Linux.
- Cursor movement.
- Modal, with normal, insert, command and replace modes. 
- Multiple panes! Type :vsp to make a vertical split or :sp to make a horizontal split.
Press Ctrl-W in normal mode to switch between panes

# Commands

Type :h and hit enter for help.

Press h, j, k and l to move around in normal mode like in vim.

Type :w in normal mode to save file.

Press Escape in insert mode to switch to normal mode.

Press i to enter insert mode, the cursor will be set back one letter.
Press a to enter insert mode, the cursor will be set forward one letter.

Type :q in normal mode to quit.

Type :e [file-name] in normal mode to open a file.

Type :vsp to make a vertical split.
