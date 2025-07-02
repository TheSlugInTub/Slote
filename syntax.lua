-- C/C++ Keywords and types
local cpp_keywords = {
    ["auto"] = true, ["break"] = true, ["case"] = true, ["char"] = true, ["const"] = true,
    ["continue"] = true, ["default"] = true, ["do"] = true, ["double"] = true, ["else"] = true,
    ["enum"] = true, ["extern"] = true, ["float"] = true, ["for"] = true, ["goto"] = true,
    ["if"] = true, ["inline"] = true, ["long"] = true, ["register"] = true,
    ["restrict"] = true, ["return"] = true, ["short"] = true, ["signed"] = true, ["sizeof"] = true,
    ["static"] = true, ["struct"] = true, ["switch"] = true, ["typedef"] = true, ["union"] = true,
    ["unsigned"] = true, ["void"] = true, ["volatile"] = true, ["while"] = true,
    -- C++ specific
    ["alignas"] = true, ["alignof"] = true, ["and"] = true, ["and_eq"] = true, ["asm"] = true,
    ["atomic_cancel"] = true, ["atomic_commit"] = true, ["atomic_noexcept"] = true, ["bitand"] = true,
    ["bitor"] = true, ["bool"] = true, ["catch"] = true, ["class"] = true, ["compl"] = true,
    ["concept"] = true, ["const_cast"] = true, ["consteval"] = true, ["constexpr"] = true,
    ["constinit"] = true, ["co_await"] = true, ["co_return"] = true, ["co_yield"] = true,
    ["decltype"] = true, ["delete"] = true, ["dynamic_cast"] = true, ["explicit"] = true,
    ["export"] = true, ["false"] = true, ["friend"] = true, ["mutable"] = true, ["namespace"] = true,
    ["new"] = true, ["noexcept"] = true, ["not"] = true, ["not_eq"] = true, ["nullptr"] = true,
    ["operator"] = true, ["or"] = true, ["or_eq"] = true, ["private"] = true, ["protected"] = true,
    ["public"] = true, ["reflexpr"] = true, ["reinterpret_cast"] = true, ["requires"] = true,
    ["static_assert"] = true, ["static_cast"] = true, ["synchronized"] = true, ["template"] = true,
    ["this"] = true, ["thread_local"] = true, ["throw"] = true, ["true"] = true, ["try"] = true,
    ["typeid"] = true, ["typename"] = true, ["using"] = true, ["virtual"] = true, ["wchar_t"] = true,
    ["xor"] = true, ["xor_eq"] = true
}

local cpp_types = {
    ["int8_t"] = true, ["int16_t"] = true, ["int32_t"] = true, ["int64_t"] = true,
    ["uint8_t"] = true, ["uint16_t"] = true, ["uint32_t"] = true, ["uint64_t"] = true,
    ["size_t"] = true, ["ssize_t"] = true, ["ptrdiff_t"] = true, ["intptr_t"] = true,
    ["uintptr_t"] = true, ["std"] = true, ["string"] = true, ["vector"] = true,
    ["map"] = true, ["set"] = true, ["list"] = true, ["deque"] = true, ["queue"] = true,
    ["stack"] = true, ["pair"] = true, ["tuple"] = true, ["array"] = true, ["bitset"] = true,
    ["shared_ptr"] = true, ["unique_ptr"] = true, ["weak_ptr"] = true, ["int"] = true,
    ["short"] = true, ["long"] = true, ["char"] = true, ["double"] = true,
}

function isAlphaNumUnderscore(ch)
    return (ch >= string.byte('a') and ch <= string.byte('z')) or
           (ch >= string.byte('A') and ch <= string.byte('Z')) or
           (ch >= string.byte('0') and ch <= string.byte('9')) or
           ch == string.byte('_')
end

function isDigit(ch)
    return ch >= string.byte('0') and ch <= string.byte('9')
end

function isAlpha(ch)
    return (ch >= string.byte('a') and ch <= string.byte('z')) or
           (ch >= string.byte('A') and ch <= string.byte('Z'))
end

function getWordAt(paneIndex, row, col)
    local rowSize = get_pane_buffer_row_size(paneIndex, row)
    if col >= rowSize then return "" end
    
    local ch = get_pane_buffer_char(paneIndex, row, col)
    if not isAlphaNumUnderscore(ch) then return "" end
    
    -- Find start of word
    local startCol = col
    while startCol > 0 do
        local prevCh = get_pane_buffer_char(paneIndex, row, startCol - 1)
        if not isAlphaNumUnderscore(prevCh) then break end
        startCol = startCol - 1
    end
    
    -- Find end of word
    local endCol = col
    while endCol < rowSize - 1 do
        local nextCh = get_pane_buffer_char(paneIndex, row, endCol + 1)
        if not isAlphaNumUnderscore(nextCh) then break end
        endCol = endCol + 1
    end
    
    -- Extract word
    local word = ""
    for i = startCol, endCol do
        word = word .. string.char(get_pane_buffer_char(paneIndex, row, i))
    end
    
    return word, startCol, endCol
end

function getTokenType(paneIndex, row, col)
    local rowSize = get_pane_buffer_row_size(paneIndex, row)
    if col >= rowSize then return "default" end
    
    local ch = get_pane_buffer_char(paneIndex, row, col)
    
    -- Handle comments
    if ch == string.byte('/') and col < rowSize - 1 then
        local nextCh = get_pane_buffer_char(paneIndex, row, col + 1)
        if nextCh == string.byte('/') then
            return "comment"  -- Line comment
        elseif nextCh == string.byte('*') then
            return "comment"  -- Block comment start
        end
    end
    
    -- Continue block comment
    if col > 0 then
        local prevCh = get_pane_buffer_char(paneIndex, row, col - 1)
        if prevCh == string.byte('*') and ch == string.byte('/') then
            return "comment"  -- Block comment end
        end
    end
    
    -- Handle strings
    if ch == string.byte('"') or ch == string.byte("'") then
        return "string"
    end
    
    -- Handle numbers
    if isDigit(ch) then
        -- Check if it's not part of an identifier
        if col == 0 or not isAlpha(get_pane_buffer_char(paneIndex, row, col - 1)) then
            return "number"
        end
    end
    
    -- Handle words (keywords, types, functions)
    if isAlphaNumUnderscore(ch) then
        local word, startCol, endCol = getWordAt(paneIndex, row, col)
        if word ~= "" then
            if cpp_keywords[word] then
                return "keyword"
            elseif cpp_types[word] then
                return "type"
            else
                -- Check if it's a function call (followed by parentheses)
                local nextNonSpace = endCol + 1
                while nextNonSpace < rowSize do
                    local nextCh = get_pane_buffer_char(paneIndex, row, nextNonSpace)
                    if nextCh == string.byte(' ') or nextCh == string.byte('\t') then
                        nextNonSpace = nextNonSpace + 1
                    elseif nextCh == string.byte('(') then
                        return "function"
                    else
                        break
                    end
                end
            end
        end
    end
    
    return "default"
end

function getColorPair(tokenType)
    if tokenType == "keyword" then return 5
    elseif tokenType == "function" then return 6
    elseif tokenType == "number" then return 7
    elseif tokenType == "string" then return 8
    elseif tokenType == "type" then return 9
    elseif tokenType == "comment" then return 10
    else return 0 end  -- Default color
end

function Start()
    keyword_col = 110
    function_col = 111
    number_col = 112
    string_col = 113
    type_col = 114
    comment_col = 115
    background_col = 102

    init_color(keyword_col, 600, 400, 900)
    init_color(function_col, 300, 650, 999)
    init_color(number_col, 850, 400, 0)
    init_color(string_col, 200, 800, 100)
    init_color(type_col, 800, 700, 200)
    init_color(comment_col, 600, 600, 600)
    init_pair(5, keyword_col , background_col)
    init_pair(6, function_col, background_col)
    init_pair(7, number_col  , background_col)
    init_pair(8, string_col  , background_col)
    init_pair(9, type_col    , background_col)
    init_pair(10, comment_col, background_col)
end

function DisplayPane()
    local numPanes = get_panes_size()
    local activePane = get_active_pane() 
    local activePaneObj = get_pane(activePane)

    local currentRow, currentCol = activePaneObj:get_cursor_position()
    
    -- Track multi-line comment state per pane
    local inBlockComment = {}
    
    -- Process each pane
    for i = 0, numPanes - 1 do
        local pane = get_pane(i)
        local win = pane:get_window()
        local rows, cols = pane:get_size()
        local viewportTopRow, viewportLeftCol = pane:get_viewport_top_col_and_left_col()
        
        win:erase() -- Clear window first
        inBlockComment[i] = false
        
        -- Go row by row on pane
        for row = 0, rows - 1 do
            -- Get the index of the row we're on
            local bufferRowIndex = row + viewportTopRow
            
            -- Line numbers
            win:move(row, 0)
            if bufferRowIndex < pane:get_buffer_size() then 
                local lineNumStr = string.format("%" .. (LINE_NUMBER_WIDTH - 1) .. "d", bufferRowIndex + 1)
                win:printw(lineNumStr)
            else
                local tildaStr = string.format("%" .. (LINE_NUMBER_WIDTH - 1) .. "s", "~")
                win:printw(tildaStr)
            end

            -- Text content with syntax highlighting
            local textWidth = cols - LINE_NUMBER_WIDTH
            local inString = false
            local stringChar = 0
            local inLineComment = false
            
            for col = 0, textWidth - 1 do
                local bufferColIndex = col + viewportLeftCol
                win:move(row, col + LINE_NUMBER_WIDTH)
                
                if bufferRowIndex < pane:get_buffer_size() and 
                   bufferColIndex < pane:get_buffer_row_size(bufferRowIndex) then
                    -- Get character from buffer
                    local ch = pane:get_buffer_char(bufferRowIndex, bufferColIndex)
                    
                    -- Determine color based on context
                    local colorPair = 0
                    
                    -- Handle string state
                    if not inBlockComment[i] and not inLineComment then
                        if not inString then
                            if ch == string.byte('"') or ch == string.byte("'") then
                                inString = true
                                stringChar = ch
                                colorPair = getColorPair("string")
                            end
                        else
                            colorPair = getColorPair("string")
                            if ch == stringChar then
                                -- Check for escape character
                                if bufferColIndex == 0 or pane:get_buffer_char(i, bufferRowIndex, bufferColIndex - 1) ~= string.byte('\\') then
                                    inString = false
                                end
                            end
                        end
                    end
                    
                    -- Handle comments
                    if not inString then
                        if not inBlockComment[i] and not inLineComment then
                            if ch == string.byte('/') and bufferColIndex < pane:get_buffer_row_size(i, bufferRowIndex) - 1 then
                                local nextCh = pane:get_buffer_char(i, bufferRowIndex, bufferColIndex + 1)
                                if nextCh == string.byte('/') then
                                    inLineComment = true
                                    colorPair = getColorPair("comment")
                                elseif nextCh == string.byte('*') then
                                    inBlockComment[i] = true
                                    colorPair = getColorPair("comment")
                                end
                            end
                        elseif inBlockComment[i] then
                            colorPair = getColorPair("comment")
                            if ch == string.byte('*') and bufferColIndex < pane:get_buffer_row_size(i, bufferRowIndex) - 1 then
                                local nextCh = pane:get_buffer_char(i, bufferRowIndex, bufferColIndex + 1)
                                if nextCh == string.byte('/') then
                                    inBlockComment[i] = false
                                end
                            end
                        elseif inLineComment then
                            colorPair = getColorPair("comment")
                        end
                    end
                    
                    -- Handle other tokens if not in string or comment
                    if not inString and not inBlockComment[i] and not inLineComment and colorPair == 0 then
                        local tokenType = getTokenType(i, bufferRowIndex, bufferColIndex)
                        colorPair = getColorPair(tokenType)
                    end
                    
                    -- Apply color and add character
                    if colorPair > 0 then
                        win:attron(COLOR_PAIR(colorPair))
                    end
                    win:addch(ch)
                    if colorPair > 0 then
                        win:attroff(COLOR_PAIR(colorPair))
                    end
                else
                    win:addch(string.byte(' ')) -- Clear with space
                end
            end
            
            -- Reset line-specific states
            inString = false
            inLineComment = false
        end
        
        -- Set cursor position for active pane
        if i == activePane then
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
