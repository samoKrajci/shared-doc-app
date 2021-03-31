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
    // typedef boost::shared_ptr<Tcp_connection> pointer;

    static std::unique_ptr<Tcp_connection> create(
        boost::asio::io_context& io_context,
        const std::function<void(std::string)>& send_all, int id)
    {
        return std::unique_ptr<Tcp_connection> { new Tcp_connection(
            io_context, send_all, id) };
    }

    void start()
    {
        // debug output
        debug_output("Client connected");

        // prida cursor do dokumentu
        Document::Document_handler::add_new_cursor(id);

        // connection je up, da sa posielat
        alive = true;

        // Send id
        std::stringstream ss;
        ss << id;

        send(ss.str());

        // zacne receive loop
        boost::asio::async_read(socket,
            boost::asio::buffer(rec_buff_, DEFAULT_MESSAGE_LENGTH),
            boost::bind(&Tcp_connection::handle_read, this,
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
    }

    // posli daco clientovi
    void send(std::string message)
    {
        mtx.lock();
        if (!is_alive())
            debug_output("Connection not alive. Skipping sending...");
        else
            boost::asio::async_write(socket, boost::asio::buffer(message),
                boost::bind(&Tcp_connection::handle_write_empty, this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
        mtx.unlock();
    }

    bool is_alive() const { return alive; }

    bool is_expired() const { return expired; }

    int get_id() const { return id; }

    Tcp_connection& operator=(const Tcp_connection&)
        = delete; // nekopirovatelne

    ~Tcp_connection() { Document::Document_handler::remove_cursor(id); }

    tcp::socket socket;

private:
    Tcp_connection(boost::asio::io_context& io_context,
        const std::function<void(std::string)>& send_all, int id)
        : socket(io_context)
        , rec_buff_(std::string(DEFAULT_MESSAGE_LENGTH, ' '))
        , send_all(send_all)
        , id(id)
        , alive(false)
        , expired(false)
    {
    }

    void handle_read(
        const boost::system::error_code& error, size_t /*bytes_transferred*/)
    {
        // error handling
        if (error.failed()) {
            debug_output("Read error: " + error.message());
            debug_output("Connection closed...");
            alive = false;
            expired = true;
            return;
        }

        std::cout << "\n";
        debug_output("Data received: " + rec_buff_);

        // ak je message "DD" tak nerob nic, update sa posle pri klasickom
        //  broadcaste
        bool message_valid = true;
        if (rec_buff_ != "DD")
            message_valid
                = Document::Document_handler::process_message(id, rec_buff_);
        // debug vec
        Document::Document_handler::print();

        // broadcastni stav dokumentu
        if (message_valid)
            send_all(Document::Document_handler::serialize());

        // citaj dalsi message (rekurzivne sa loopuje)
        boost::asio::async_read(socket,
            boost::asio::buffer(rec_buff_, DEFAULT_MESSAGE_LENGTH),
            boost::bind(&Tcp_connection::handle_read, this,
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
    }

    void handle_write_empty(
        const boost::system::error_code& error, size_t /*bytes_transferred*/)
    {
        /*
            Ked netreba special handling writu, typicky pri broadcaste
        */
        debug_output(error.message());
    }

    void debug_output(const std::string& message) const
    {
        std::cout << "[" << id << "] " << message << "\n";
    }

    std::string send_buff_, rec_buff_;
    std::function<void(std::string)> send_all;
    int id;
    bool alive;
    bool expired;
    static const int DEFAULT_MESSAGE_LENGTH = 2;

    std::mutex mtx;
};

class Tcp_server {
public:
    Tcp_server(boost::asio::io_context& io_context, size_t port)
        : io_context_(io_context)
        , acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
        , next_client_id(0)
    {
        start_accept();
    }

    Tcp_server& operator=(const Tcp_server&) = delete; // nekopirovatelne

    void send_all(const std::string& message)
    {
        auto it = connections.begin();
        while (it != connections.end()) {
            if (it->second->is_expired()) {
                connections.erase(it++);
            } else {
                it->second->send(message);
                ++it;
            }
        }
    }

private:
    void start_accept()
    {
        std::cout << "Start accept\n";

        // Tcp_connection::pointer new_connection = Tcp_connection::create(
        //     io_context_, dh,
        //     [myself](std::string message) { myself->send_all(message); },
        //     next_client_id);
        connections[next_client_id] = Tcp_connection::create(
            io_context_,
            [this](const std::string& message) { this->send_all(message); },
            next_client_id);

        Tcp_connection& new_connection_ref = *connections[next_client_id];

        // std::unique_ptr<Tcp_connection> up = std::move(new_connection);
        // connections[next_client_id]
        //     = std::make_unique<Tcp_connection>(*new_connection);
        std::cout << "Number of connections: " << connections.size() << "\n";

        ++next_client_id;

        acceptor_.async_accept(new_connection_ref.socket,
            boost::bind(&Tcp_server::handle_accept, this,
                boost::ref(new_connection_ref),
                boost::asio::placeholders::error));
    }

    void handle_accept(
        Tcp_connection& new_connection, const boost::system::error_code& error)
    {
        if (!error) {
            new_connection.start();
        }

        start_accept();
    }

    boost::asio::io_context& io_context_;
    tcp::acceptor acceptor_;
    int next_client_id;

    std::map<int, std::unique_ptr<Tcp_connection>> connections;
};

int main()
{
    try {
        boost::asio::io_context io_context;
        Tcp_server server(io_context, 6969);

        io_context.run();
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}