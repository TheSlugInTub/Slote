function Start()
end

function DisplayPane()
    local numPanes = get_panes_size()
    local activePane = get_active_pane() 
    DebugLogInt(activePane)
    DebugLog("yo how ya doin\n");
    DebugLog("\n");
    local activePaneObj = get_pane(activePane)

    local currentRow, currentCol = activePaneObj:get_cursor_position()
    
    -- Process each pane
    for i = 0, numPanes - 1 do
        DebugLogInt(i)
        DebugLog("\n");
        local pane = get_pane(i)
        local win = pane:get_window()
        local rows, cols = pane:get_size()
        local viewportTopRow, viewportLeftCol = pane:get_viewport_top_col_and_left_col()
        
        win:erase() -- Clear window first
        
        -- Go row by row on pane
        for row = 0, rows - 1 do
            -- Get the index of the row we're on
            local bufferRowIndex = row + viewportTopRow
            
            -- Line numbers
            win:move(row, 0)

            -- Text content
            local textWidth = cols - LINE_NUMBER_WIDTH
            for col = 0, textWidth - 1 do
                local bufferColIndex = col + viewportLeftCol
                win:move(row, col + LINE_NUMBER_WIDTH)
                
                if bufferRowIndex < get_pane_buffer_size(i) and 
                   bufferColIndex < get_pane_buffer_row_size(i, bufferRowIndex) then
                    -- Get character from buffer
                    local ch = get_pane_buffer_char(i, bufferRowIndex, bufferColIndex)
                    win:addch(ch)
                else
                    win:addch(string.byte(' ')) -- Clear with space
                end
            end
        end
        
        -- Set cursor position for active pane
        if i == activePaneObj then
            local cursorRow = currentRow - viewportTopRow
            local cursorCol = currentCol - viewportLeftCol + LINE_NUMBER_WIDTH
            
            if cursorRow >= 0 and cursorRow < rows and
               cursorCol >= LINE_NUMBER_WIDTH and cursorCol < cols then
                win:move(cursorRow, cursorCol)
            end
        end
        
        win:refresh()
    end
end

function Display()
end
