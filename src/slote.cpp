#include <codecvt>
#include <cwchar>
#include <ncurses.h>
#include <locale.h>
#include <algorithm>
#include <ctype.h>
#include <fstream>
#include <string>
#include <vector>

using namespace std;

int R, C, r, c, y, x, com, indent;

string src = "noname.txt", stat = "", msg = "", mod = "n", cnt = "";
vector<vector<int>> b = {}, bf = {};

const int LINE_NUMBER_WIDTH = 5; // Width reserved for line numbers
                                
vector<string> ReadStartScreen(const string &fileName) {
    vector<string> startScreenContents;
    ifstream file(fileName);
    if (file.is_open()) {
        string line;
        while (getline(file, line)) {
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

void DisplayStartScreen(const vector<string> &startScreenContents) {
    clear();
    int startY = (R - startScreenContents.size()) / 2;
    int startX;
    for (int i = 0; i < startScreenContents.size(); i++) {
        startX = (C - startScreenContents[i].length()) / 2;
        mvprintw(startY + i, startX, "%s", startScreenContents[i].c_str());
    }

    mvprintw(R - 1, (C - 26) / 2, "Press any key to continue...");
    refresh();
}

int main(int argc, char **argv) 
{
  setlocale(LC_ALL, ""); // Set the locale to the default environment locale
  setlocale(LC_CTYPE, ""); // Set locale for UTF-8 support
  initscr(); 
  start_color();
  nodelay(stdscr, TRUE); 
  noecho(); 
  raw(); 

  getmaxyx(stdscr, R, C);
  R--; 
  vector<int> row;

  std::string startFile = "config/start.txt";
  vector<string> startScreenContents = ReadStartScreen(startFile);
  
  int chS = -1;
    
  DisplayStartScreen(startScreenContents);
  
    while (chS == -1)
    {
        chS = getch();
    }
  
    clear();


  if (argc == 2) 
  {
      src = argv[1]; 
  }else 
  {
      b.push_back(row);
  }
  
  try 
  { 
    vector<int> row;

    ifstream ifs(src); 
    string cont((istreambuf_iterator<char>(ifs)), (istreambuf_iterator<char>())); 
    for (int i = 0; i < cont.size(); i++) 
    {
        if (cont[i] == '\n') 
        { 
            b.push_back(row); row.clear(); 
        }
        else 
        { 
            row.push_back(cont[i]); 
        } 
    } 

    if (row.size()) { b.push_back(row); } 
    ifs.close(); 
  } 

  catch (exception &e) {}

  if (src != "noname.txt" && b.size() == 0) { b.push_back(row); }
  
  // Main loop of program
  while (TRUE) 
  {
    if (r < y) { y = r; }
    if (r >= y + R) { y = r - R+1; }
    if (c < x) { x = c; }
    if (c >= x + C) { x = c - C+1; }

    move(0, 0); 

    for (int row = 0; row < R; row++) 
    {
      int brw = row + y; 

       // Print line numbers
        if (brw < b.size()) 
        {
            mvprintw(row, 0, "%*d", LINE_NUMBER_WIDTH - 1, brw + 1);
        } 
        else 
        {
            mvprintw(row, 0, "%*s", LINE_NUMBER_WIDTH - 1, "~");
        }


      for (int col = 0; col < C; col++) 
      { 
        int bcl = col + x;
        if (brw < b.size() && bcl < b[brw].size()) 
        { 
            mvaddch(row, col + LINE_NUMBER_WIDTH, b[brw][bcl]); 
        }
      } 

      clrtoeol(); 
      addstr(brw < b.size()-1 ? "\n" : "\n~"); 
    }

    stat = mod + " \"" + src + "\" " + to_string(r+1) + "/" + to_string(b.size());

    stat += b.size() ? " --" + to_string((int)((r+1)*100/b.size())) + "%-- " : "";

    stat += "col " + to_string(c+1) + " --x" + (cnt.length() ? cnt : "0") + "--";

    move(R, 0);

    if (msg == "") 
        for (int i = 0; i < stat.length(); i++) 
            addch(stat[i]);
    else 
    { 
        for (int i = 0; i < msg.length(); i++) 
        {
            addch(msg[i]); msg = "";
        }
    }

    clrtoeol(); 
    curs_set(0); 
    move(r - y, c - x + LINE_NUMBER_WIDTH); 
    curs_set(1); 
    refresh();
    
    int ch = -1; 

    while (ch == -1) 
    {
        ch = getch();
    }
    
    string modes = "irR";

    if (isdigit(ch) && !(modes.find(mod[0]) != string::npos)) 
    {
        cnt += ch;
    }

    if (ch == ('[' & 0x1f)) 
    { 
        if (c) 
        {
            c--; 
        }
        mod = 'n'; 
        cnt = ""; 
        continue; 
    }

    int times = atoi(cnt.c_str()); 

    if (mod == "n") 
    {
        if (ch == 'i') 
        { 
            mod = "i"; 
            
            if (c >= b[r].size()) 
                c = 0; 
            curs_set(1);
            continue; 
        }else if (ch == 'a') 
        { 
            mod = "i"; 
            if (c < b[r].size()) // Move one letter forward if not at the end of the line
                c++; 
            continue; 
        }
        else if (ch == 'o') 
        { 
            vector<int> row; 
            b.insert(b.begin() + r+1, row); 
            r++; 
            c = 0; 
            mod = "i"; 
        }
        else if (ch == 'O') 
        { 
            vector<int> row; 
            b.insert(b.begin() + r, row); 
            c = 0; 
            mod = "i"; 
        }
        else if (ch == 'A') 
        { 
            mod = "i"; 
            c = b[r].size(); 
        }
        else if (ch == 'r') 
        { 
            mod = "r"; 
        } 
        else if (ch == 'R') 
        { 
            mod = "R"; 
        }
        else if (ch == 'G') 
        { 
            r = (times-1 <= b.size()-1 ? times-1 : b.size() - 1); 
            cnt = ""; 
        }
        else if (ch =='p' and bf.size()) 
        { 
            for (int i = 0; i < bf.size(); i++) 
            {
                b.insert(b.begin() + r+i+1, bf[i]);
            } 
            r += bf.size(); 
        }
        else if (ch == 'y' || ch == 'd') 
        {
            bf.clear(); 
            for (int i = 0; i < (cnt.length() ? times : 1); i++) 
            {
                if (r+i < b.size()) 
                { 
                    vector<int> row; 
                   
                    for (int j = 0; j < b[r+i].size(); j++) 
                    {
                        row.insert(row.begin() + j, b[r+i][j]); 
                    } 
                    bf.push_back(row); 
                }
            }

            if (ch == 'd') 
            {
                for (int i = 0; i < (cnt.length() ? times : 1); i++) 
                {
                    if (b.size() > 1 && r < b.size()) 
                        b.erase(b.begin() + r);
                    if (r == b.size()) 
                        r--;
                } 
            }
            cnt = "";
            msg = (ch == 'y' ? "Yank " : "Delete ") + to_string(bf.size()) + " line(s)";
        } 
        else if (ch == ' ' || ch == 127) 
        {
            for (int i = 0; i < (cnt.length() ? times : 1); i++) 
            {
                if (r+i < b.size()) 
                { 
                    if (ch == ' ' && c < b[r+i].size()) 
                        b[r+i].insert(b[r+i].begin() + c, 32);
                    else if (ch == 127 && c && c < b[r+i].size()) 
                        b[r+i].erase(b[r+i].begin() + c-1); 
                }
            }

        if (ch == ' ') 
            c++; 
        else if (c) 
            c--;
        } 
      else if (ch == 'q') 
         goto exitprog; 
      else if (ch == 'w') 
      {
        ofstream ofs(src, ofstream::out); 
        string cont = "";

        for (int row = 0; row < b.size(); row++) 
        {
          for (int col = 0; col < b[row].size(); col++) 
          {
            char c = b[row][col]; if (c) cont += c;
          } 
          cont += "\n"; 
        }

        ofs << cont; ofs.close();
        msg = to_string(b.size()) + " line(s) written to " + "\"" + src + "\""; 
      } 
      else {
        switch (ch) {
          case '#': 
              c = 0; 
              break;
          case '$': 
              c = b[r].size(); 
              break;
          case 'x': 
              if (b[r].size()) 
              {
                  b[r].erase(b[r].begin() + c); 
              }
              break;
          case 'h': 
              c ? c-- : c;
              break;
          case 'j': 
              r < b.size()-1 ? r++ : r;
              break;
          case 'k': 
              r ? r-- : r; 
              break;
          case 'l': 
              c < b[r].size()-1 ? c++ : c; 
              break;
        } 
        int rwl = r < b.size() ? b[r].size() : 0;
        if (c > rwl -1) 
        {
            c = rwl ? rwl-1 : rwl;
        }
      } 
      continue;
    } 
    else if (mod == "i") 
    {
      indent = cnt.length() ? times : 0;
      if (ch == '\n') 
      {
        vector<int> right(b[r].size() - c);
        vector<int> left(c);
        copy(b[r].begin() + c, b[r].begin() + b[r].size(), right.begin());
        copy(b[r].begin(), b[r].begin() + c, left.begin()); b[r].clear();

        b[r] = left; 
        r++; 
        c = 0; 
        
        b.insert(b.begin() + r, right);
        left.clear();
        right.clear();

        for (int i = 0; i < indent; i++) 
        { 
            b[r].insert(b[r].begin() + c, 32);
            c += 1;
        }
      } 
      else if (ch == KEY_BACKSPACE || ch == '\b' || ch == 127) 
      {
        if (c) 
        { 
            c--; 
            b[r].erase(b[r].begin() + c); 
        }
        else if (r) 
        {
          vector<int> right(b[r].size() - c);
          vector<int> left(c);
          copy(b[r].begin() + c, b[r].begin() + b[r].size(), right.begin());
          copy(b[r].begin(), b[r].begin() + c, left.begin());
          b.erase(b.begin() + r);
          r--;
          c = b[r].size();
          b[r].insert(b[r].end(), right.begin(), right.end());
          left.clear(); right.clear();
        }
      }
      else if (ch == '\t')
      {
          b[r].insert(b[r].begin() + c, ch);
          b[r].insert(b[r].begin() + c, ch);
          b[r].insert(b[r].begin() + c, ch);
          b[r].insert(b[r].begin() + c, ch);
          c = c + 4;
      }
      else if (ch != (ch & 0x1f) && ch < 128) 
      { 
          b[r].insert(b[r].begin() + c, ch); 
          c++;
      }
    } 
    else if (mod == "r") 
    { 
        b[r][c] = ch; 
        mod = "n"; 
    } 
    else if (mod == "R")
    {
        if (ch != (ch & 0x1f) && ch < 128 && c < b[r].size()) 
        { 
            b[r][c] = ch; 
            c++; 
        }
        if (ch == KEY_RESIZE) 
        { 
            getmaxyx(stdscr, R, C); 
            R--; 
            r = c = 0; 
            refresh();
        }
    }
  } 
 
exitprog:
  endwin(); 
  b.clear(); 
  bf.clear(); 
  system("clear"); 
  return 0;
}
