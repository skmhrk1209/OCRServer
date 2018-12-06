#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <iostream>

class Server {
    boost::asio::io_context& io_context;
    boost::asio::ip::tcp::acceptor acceptor;
    std::vector<boost::asio::ip::tcp::socket> sockets;
    boost::asio::streambuf receive_buffer;

   public:
    Server(boost::asio::io_context& io_context_, const boost::asio::ip::tcp::endpoint& endpoint)
        : io_context(io_context_), acceptor(io_context_, endpoint) {}

    void accept() {
        acceptor.async_accept([this](boost::system::error_code error_code, boost::asio::ip::tcp::socket socket) {
            if (error_code) {
                std::cout << "accept failed: " << error_code.message() << std::endl;
            } else {
                std::cout << "accept succeeded: " << socket.remote_endpoint() << std::endl;
            }

            sockets.emplace_back(std::move(socket));
            receive(sockets.back());

            accept();
        });
    }
    void receive(boost::asio::ip::tcp::socket& socket) {
        boost::asio::async_read_until(socket, receive_buffer, '\n', [&](const boost::system::error_code& error_code, std::size_t size) {
            if (error_code) {
                std::cout << "receive failed: " << error_code.message() << std::endl;
                socket.close();

            } else {
                std::string data(boost::asio::buffer_cast<const char*>(receive_buffer.data()), size);
                std::cout << "receive succeeded: " << data << std::endl;

                receive_buffer.consume(receive_buffer.size());
                receive(socket);
            }
        });
    }
};

int main(int argc, char* argv[]) {
    boost::asio::io_context io_context;

    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), std::atoi(argv[1]));
    Server server(io_context, endpoint);

    server.accept();

    io_context.run();
}