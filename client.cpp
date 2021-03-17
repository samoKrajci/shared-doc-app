#include "document.h"

#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <boost/thread/thread.hpp>
#include <functional>
#include <iostream>
#include <ncurses.h>
#include <string>

using boost::asio::ip::tcp;

struct Tcp_client {

public:
    Tcp_client(char* address)
        : socket(tcp::socket(io_context))
        , connected(false)
        , address(address)
        , buff(std::vector<char>(MAX_BUFF_LENGTH))
    {
    }

    bool is_connected() const { return connected; }

    bool connect()
    {
        try {
            tcp::resolver resolver(io_context);
            tcp::resolver::results_type endpoints
                = resolver.resolve(address, PORT);

            boost::asio::connect(socket, endpoints);

            // receive my id
            std::vector<char> temp_buff(32);
            size_t l = socket.read_some(boost::asio::buffer(temp_buff));

            // set id
            std::stringstream s1;
            s1.write(temp_buff.data(), l);
            std::stringstream s2(s1.str());
            s2 >> id;

            send_message("DD");

            connected = true;
        } catch (std::exception& e) {
            std::cerr << e.what() << std::endl;
            return false;
        }
        return true;
    }

    void send_message(std::string message)
    {
        socket.write_some(boost::asio::buffer(message));
    }

    void receive_loop(
        std::function<void(std::vector<char>, int)>* handle_received_message)
    {
        boost::system::error_code error;

        for (;;) {
            // size_t len = socket.read_some(boost::asio::buffer(buff), error);
            socket.read_some(boost::asio::buffer(buff), error);

            if (error == boost::asio::error::eof) {
                connected = false;
                break; // Connection closed cleanly by peer.
            } else if (error)
                throw boost::system::system_error(error); // Some other error.

            (*handle_received_message)(buff, id);
        }
    }

    std::vector<char> get_buffer() { return buff; }

    // private:
    boost::asio::io_context io_context;
    tcp::socket socket;

    bool connected;

    const char* address;
    static const std::string PORT;

    std::vector<char> buff;
    static const size_t MAX_BUFF_LENGTH;

    size_t id;
};

const std::string Tcp_client::PORT = "6969";
const size_t Tcp_client::MAX_BUFF_LENGTH = 10000;

struct Window {
    Window()
    {
        initscr();
        noecho();
        cbreak();
        keypad(stdscr, true);
        start_color();
        curs_set(0);
        init_color_pairs();
    }

    void init_color_pairs()
    {
        init_pair(1, COLOR_BLACK, COLOR_GREEN);
        init_pair(2, COLOR_BLACK, COLOR_MAGENTA);
        init_pair(3, COLOR_BLACK, COLOR_YELLOW);
        init_pair(4, COLOR_BLACK, COLOR_RED);
        init_pair(5, COLOR_BLACK, COLOR_CYAN);
    }

    chtype get_cursor_attribute(int cursor_id, int my_id)
    {
        if (cursor_id == my_id)
            return A_REVERSE;
        return COLOR_PAIR((cursor_id % 5) + 1);
    }

    ~Window() { endwin(); }

    void input_loop(Tcp_client* tcp_client)
    {
        for (;;) {
            printw("%d", KEY_UP);
            int ch = getch();
            std::string message;

            message = "S";
            switch (ch) {
            case KEY_UP:
                message += "U";
                break;
            case KEY_DOWN:
                message += "D";
                break;
            case KEY_RIGHT:
                message += "R";
                break;
            case KEY_LEFT:
                message += "L";
                break;
            case '\n':
                message += "B";
                break;
            case KEY_HOME:
                message += "H";
                break;
            case KEY_END:
                message += "E";
                break;
            case KEY_DC:
                message += "X";
                break;
            case KEY_BACKSPACE:
                message += "A";
                break;
            case '\t':
                message += "T";
                break;
            default:
                message = "W";
                message += ch;
                break;
            }

            tcp_client->send_message(message);
        }
    }

    void print_document_image(Document::Document_image& document_image, int id)
    {
        clear();

        auto closest_cursor = document_image.cursors.begin();
        bool no_cursors_left = false;
        for (size_t line_index = 0; line_index < document_image.data.size();
             ++line_index) {
            std::string& line = document_image.data.at(line_index);
            for (size_t col = 0; col < line.size() + 1; ++col) {
                int index_of_cursor_on_this_place = -1;
                if (!no_cursors_left)
                    while ((closest_cursor->line == line_index)
                        and (closest_cursor->column == col)) {
                        if (index_of_cursor_on_this_place != id)
                            index_of_cursor_on_this_place = closest_cursor->id;
                        if (closest_cursor != document_image.cursors.end())
                            ++closest_cursor;
                        else
                            no_cursors_left = true;
                    }

                chtype cursor_attribute
                    = get_cursor_attribute(index_of_cursor_on_this_place, id);
                attron(cursor_attribute);
                if (col == line.size())
                    printw(" ");
                else
                    printw("%c", line[col]);
                attroff(cursor_attribute);
            }
            printw("\n");
        }

        refresh();
    }

    void printw_buff(std::vector<char> buff, int id)
    {
        std::stringstream ss;
        ss.write(buff.data(), buff.size());
        Document::Document_image document_image(ss.str());

        print_document_image(document_image, id);
    }
};

int main(int argc, char* argv[])
{
    try {
        if (argc != 2) {
            std::cerr << "Usage: client <host>" << std::endl;
            return 1;
        }

        Tcp_client tcp_client(argv[1]);
        if (!tcp_client.connect())
            return 1;

        Window window;

        std::function<void(std::vector<char>, int)> f
            = [&window](std::vector<char> buff, int id) {
                  window.printw_buff(buff, id);
              };
        boost::thread t1(
            boost::bind(&Tcp_client::receive_loop, &tcp_client, &f));
        boost::thread t2(
            boost::bind(&Window::input_loop, &window, &tcp_client));

        t1.join();
        t2.join();
        getch();

    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        std::cout << "fail\n";
    }

    return 0;
}