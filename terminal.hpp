#pragma once

#include <ncurses.h>
#include <string>

namespace os
{
    constexpr auto escape_key = 27;

    class terminal
    {
    public:
        terminal()
        {
            setlocale(LC_ALL, "");
            initscr();
            noecho();
            keypad(stdscr, true);
            nodelay(stdscr, true);
            curs_set(0);
        }

        ~terminal()
        {
            endwin();
        }

        void print(const int x, const int y, const wchar_t* s) const
        {
            mvaddwstr(y, x, s);
        }

        void print_char(const int x, const int y, const char* c, const bool is_reversed = false) const
        {
            if (is_reversed)
                attron(A_REVERSE);

            mvaddstr(y, x, c);

            if (is_reversed)
                attroff(A_REVERSE);
        }

        auto screen_size() const
        {
            std::pair<int, int> result;
            getmaxyx(stdscr, result.second, result.first);
            return result;
        }
    };
}