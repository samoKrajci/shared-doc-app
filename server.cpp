#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/thread.hpp>
#include <ctime>
#include <functional>
#include <iostream>
#include <string>

#include "document.h"

using boost::asio::ip::tcp;

class Tcp_connection : public boost::enable_shared_from_this<Tcp_connection> {
public:
    typedef boost::shared_ptr<Tcp_connection> pointer;

    static pointer create(boost::asio::io_context& io_context,
        Document::Document_handler& dh,
        std::function<void(std::string)> send_all, int id)
    {
        return pointer(new Tcp_connection(io_context, dh, send_all, id));
    }

    tcp::socket& socket() { return socket_; }

    void start()
    {
        // debug output
        std::cout << "Client connected. Id: " << id << "\n";

        // prida cursor do dokumentu
        dh.add_new_cursor(id);

        // connection je up, da sa posielat
        alive = true;

        // Send id
        std::stringstream ss;
        ss << id;
        send(ss.str());

        // zacne receive loop
        boost::asio::async_read(socket_,
            boost::asio::buffer(rec_buff_, DEFAULT_MESSAGE_LENGTH),
            boost::bind(&Tcp_connection::handle_read, shared_from_this(),
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
    }

    // posli daco clientovi
    void send(std::string message)
    {
        mtx.lock();
        if (!is_alive())
            std::cout << "Connection " << id
                      << " is not alive. Skipping sending...";
        else
            boost::asio::async_write(socket_, boost::asio::buffer(message),
                boost::bind(&Tcp_connection::handle_write_empty,
                    shared_from_this(), boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
        mtx.unlock();
    }

    bool is_alive() { return alive; }

    int get_id() { return id; }

    ~Tcp_connection()
    {
        dh.remove_cursor(id);
        std::cout << "destructor called on id:" << id << "\n";
    }

private:
    Tcp_connection(boost::asio::io_context& io_context,
        Document::Document_handler& dh,
        std::function<void(std::string)> send_all, int id)
        : socket_(io_context)
        , rec_buff_(std::string(DEFAULT_MESSAGE_LENGTH, ' '))
        , dh(dh)
        , send_all(send_all)
        , id(id)
        , alive(false)
    {
    }

    void handle_read(
        const boost::system::error_code& error, size_t /*bytes_transferred*/)
    {
        // error handling
        if (error.failed()) {
            std::cout << "Read error: " << error << "\n";
            std::cout << "Connection closed...\n";
            alive = false;
            return;
        }

        // debug output
        std::cout << "data received: \"" << rec_buff_ << "\""
                  << "\n\t"
                  << "from: " << id << "\n";

        // ak je message "DD" tak nerob nic, update sa posle pri klasickom
        //  broadcaste
        bool message_valid = true;
        if (rec_buff_ != "DD")
            message_valid = dh.process_message(id, rec_buff_);
        // debug vec
        dh.print();

        // broadcastni stav dokumentu
        if (message_valid)
            send_all(dh.serialize());

        // citaj dalsi message (rekurzivne sa loopuje)
        boost::asio::async_read(socket_,
            boost::asio::buffer(rec_buff_, DEFAULT_MESSAGE_LENGTH),
            boost::bind(&Tcp_connection::handle_read, shared_from_this(),
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
    }

    void handle_write_empty(
        const boost::system::error_code& error, size_t /*bytes_transferred*/)
    {
        /*
            Ked netreba special handling writu, typicky pri broadcaste
        */
        std::cout << error << "\n";
    }

    tcp::socket socket_;
    std::string send_buff_, rec_buff_;
    Document::Document_handler& dh;
    std::function<void(std::string)> send_all;
    int id;
    bool alive;
    static const int DEFAULT_MESSAGE_LENGTH = 2;

    std::mutex mtx;
};

class tcp_server {
public:
    tcp_server(boost::asio::io_context& io_context, size_t port,
        Document::Document_handler& dh)
        : io_context_(io_context)
        , acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
        , dh(dh)
        , next_client_id(0)
    {
        start_accept();
    }

    void send_all(std::string message)
    {
        auto it = connections.begin();
        while (it != connections.end()) {
            if (it->expired()) {
                connections.erase(it++);
            } else {
                auto cp = it->lock();
                std::cout << cp->get_id() << "\n";
                cp->send(message);
                ++it;
            }
        }
    }

private:
    void start_accept()
    {
        std::cout << "Start accept\n";

        auto myself = this;
        Tcp_connection::pointer new_connection = Tcp_connection::create(
            io_context_, dh,
            [myself](std::string message) { myself->send_all(message); },
            next_client_id);

        boost::weak_ptr<Tcp_connection> wp = new_connection;
        connections.insert(wp);
        std::cout << "pocet connectionov: " << connections.size() << "\n";

        ++next_client_id;

        acceptor_.async_accept(new_connection->socket(),
            boost::bind(&tcp_server::handle_accept, this, new_connection,
                boost::asio::placeholders::error));
    }

    void handle_accept(Tcp_connection::pointer new_connection,
        const boost::system::error_code& error)
    {
        std::cout << "Handle accept\n";
        if (!error) {
            new_connection->start();
        }

        start_accept();
    }

    boost::asio::io_context& io_context_;
    tcp::acceptor acceptor_;
    Document::Document_handler& dh;
    int next_client_id;

    std::set<boost::weak_ptr<Tcp_connection>> connections;
};

void local_loop()
{
    std::string s;
    for (;;) {
        std::cin >> s;
        if (s == "Hello") {
            std::cout << "world!\n";
        }
    }
}

int main()
{
    Document::Document_handler dh;
    try {
        boost::asio::io_context io_context;
        tcp_server server(io_context, 6969, dh);
        boost::thread t1(&local_loop);

        io_context.run();
        t1.join();
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}