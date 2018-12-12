#include <pybind11/embed.h>
#include <pybind11/pybind11.h>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/regex.hpp>
#include <iostream>
#include <string>
#include <vector>

namespace py = pybind11;
namespace asio = boost::asio;

class Server {
    struct Socket {
        template <typename... Args>
        Socket(Args&&... args) : socket(std::forward<Args>(args)...) {}
        asio::ip::tcp::socket socket;
        std::string send_buffer;
        std::string receive_buffer;
    };

   private:
    boost::asio::io_context& io_context;
    boost::asio::ip::tcp::acceptor acceptor;
    std::vector<Socket> sockets;

    const std::string sos = "<s>";
    const std::string eos = "</s>";

    py::scoped_interpreter interpreter;
    py::module server_app;

   public:
    Server(boost::asio::io_context& context, const boost::asio::ip::tcp::endpoint& endpoint) : io_context(context), acceptor(context, endpoint) {
        py::module::import("sys").attr("path").cast<py::list>().append(".");
        server_app = py::module::import("server_app");
    }

    void accept() {
        acceptor.async_accept([this](auto error_code, auto&& socket) {
            if (error_code) {
                std::cout << "accept failed: " << error_code.message() << std::endl;
            } else {
                std::cout << "accept succeeded: " << socket.remote_endpoint() << std::endl;

                sockets.emplace_back(std::move(socket));
                receive(sockets.back());
            }

            accept();
        });
    }

    void send(Socket& socket, const std::string& string) {
        socket.send_buffer = sos + string + eos;

        asio::async_write(socket.socket, asio::buffer(socket.send_buffer), [&](auto error_code, ...) {
            if (error_code) {
                std::cout << "send failed: " << error_code.message() << std::endl;

                socket.socket.close();
            } else {
                std::cout << "send succeeded" << std::endl;

                receive(socket);
            }
        });
    }

    void receive(Socket& socket) {
        asio::async_read_until(socket.socket, asio::dynamic_buffer(socket.receive_buffer), boost::regex(sos + ".*" + eos), [&](auto error_code, ...) {
            if (error_code) {
                std::cout << "receive failed: " << error_code.message() << std::endl;

                socket.socket.close();
            } else {
                std::cout << "receive succeeded" << std::endl;

                socket.receive_buffer.erase(socket.receive_buffer.begin(), socket.receive_buffer.begin() + sos.size());
                socket.receive_buffer.erase(socket.receive_buffer.end() - eos.size(), socket.receive_buffer.end());

                send(socket, server_app.attr("process")(socket.receive_buffer).cast<std::string>());
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