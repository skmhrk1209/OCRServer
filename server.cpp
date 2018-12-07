#include <pybind11/embed.h>
#include <pybind11/pybind11.h>
#include <algorithm>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/regex.hpp>
#include <iostream>
#include <string>
#include <vector>

namespace py = pybind11;

class Server {
   private:
    boost::asio::io_context& io_context;
    boost::asio::ip::tcp::acceptor acceptor;
    std::vector<boost::asio::ip::tcp::socket> sockets;
    std::string buffer;
    py::scoped_interpreter interpreter;

   public:
    Server(boost::asio::io_context& io_context_, const boost::asio::ip::tcp::endpoint& endpoint)
        : io_context(io_context_), acceptor(io_context_, endpoint) {}

    void accept() {
        acceptor.async_accept([this](auto error_code, auto socket) {
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

    void send(boost::asio::ip::tcp::socket& socket, const std::string& string) {
        boost::asio::async_write(socket, boost::asio::buffer(string), [this, &socket, string](auto error_code, auto size) {
            if (error_code) {
                std::cout << "send failed: " << error_code.message() << std::endl;
            } else {
                std::cout << "send succeeded: " << string << std::endl;

                receive(socket);
            }
        });
    }

    void receive(boost::asio::ip::tcp::socket& socket) {
        boost::asio::async_read_until(
            socket, boost::asio::dynamic_buffer(buffer), boost::regex("<s>.*</s>"), [this, &socket](auto error_code, auto size) {
                if (error_code) {
                    std::cout << "receive failed: " << error_code.message() << std::endl;

                    socket.close();

                } else {
                    std::string start_token = "<s>";
                    std::string end_token = "</s>";

                    std::string string(std::search(buffer.begin(), buffer.end(), start_token.begin(), start_token.end()) + start_token.size(),
                                       std::search(buffer.begin(), buffer.end(), end_token.begin(), end_token.end()));
                    std::cout << "receive succeeded: " << string << std::endl;

                    buffer.erase(0, size);

                    py::module::import("sys").attr("path").cast<py::list>().append(".");
                    std::string prediction = py::module::import("predict").attr("predict")(string).cast<std::string>();

                    send(socket, "<s>" + prediction + "</s>");
                }
            });
    }

    const std::string& get_buffer() const { return buffer; }
};

int main(int argc, char* argv[]) {
    boost::asio::io_context io_context;

    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), std::atoi(argv[1]));
    Server server(io_context, endpoint);

    server.accept();

    io_context.run();
}