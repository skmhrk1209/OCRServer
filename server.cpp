#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <iostream>

namespace asio = boost::asio;
using asio::ip::tcp;

class Server {
    asio::io_service& io_service;
    tcp::acceptor acceptor;
    tcp::socket socket;
    asio::streambuf receive_buffer;

   public:
    Server(asio::io_service& io_service_) : io_service(io_service_), acceptor(io_service_), socket(io_service_) {}

    void accept(unsigned short port) {
        auto end_point = tcp::endpoint(tcp::v4(), port);
        acceptor.open(end_point.protocol());
        acceptor.bind(end_point);
        acceptor.listen();

        acceptor.async_accept(socket, boost::bind(&Server::on_accept, this, asio::placeholders::error));
    }

    void receive() {
        boost::asio::async_read(socket, receive_buffer, asio::transfer_all(),
                                boost::bind(&Server::on_receive, this, asio::placeholders::error, asio::placeholders::bytes_transferred));
    }

   private:
    void on_accept(const boost::system::error_code& error_code) {
        if (error_code) {
            std::cout << "accept failed: " << error_code.message() << std::endl;
            return;
        }

        receive();
    }

    void on_receive(const boost::system::error_code& error_code, size_t bytes_transferred) {
        if (error_code && error_code != boost::asio::error::eof) {
            std::cout << "receive failed: " << error_code.message() << std::endl;
            return;
        }

        const char* data = asio::buffer_cast<const char*>(receive_buffer.data());
        std::cout << data << std::endl;

        receive_buffer.consume(receive_buffer.size());
    }
};

int main() {
    asio::io_service io_service;
    Server server(io_service);

    server.accept(31400);

    io_service.run();
}
