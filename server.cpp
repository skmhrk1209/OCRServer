#include <pybind11/embed.h>
#include <pybind11/pybind11.h>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/regex.hpp>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace py = pybind11;
namespace asio = boost::asio;

class Server {
    struct Socket {
        using Ptr = std::shared_ptr<Socket>;
        template <typename... Args>
        Socket(Args&&... args) : socket(std::forward<Args>(args)...) {}
        template <typename... Args>
        static Ptr create(Args&&... args) {
            return std::make_shared<Socket>(std::forward<Args>(args)...);
        }
        asio::ip::tcp::socket socket;
        std::string send_buffer;
        std::string receive_buffer;
    };

   private:
    asio::io_context& io_context;
    asio::ip::tcp::acceptor acceptor;
    std::vector<Socket::Ptr> sockets;

    const std::string sos;
    const std::string eos;

    py::scoped_interpreter interpreter;
    py::module server_app;

   public:
    Server(asio::io_context& context, const asio::ip::tcp::endpoint& endpoint) : io_context(context), acceptor(context, endpoint), sos("<s>"), eos("</s>") {
        py::module::import("sys").attr("path").cast<py::list>().append(".");
        server_app = py::module::import("server_app");
    }

    void accept() {
        acceptor.async_accept([=](const auto& error_code, auto&& sock) {
            if (error_code) {
                std::cout << "accept failed: " << error_code.message() << std::endl;
            } else {
                std::cout << "accept succeeded: " << sock.remote_endpoint() << std::endl;
                auto socket = Socket::create(std::move(sock));
                sockets.emplace_back(socket);
                receive(socket);
            }
            accept();
        });
    }

    void send(const Socket::Ptr& socket, const std::string& string) {
        socket->send_buffer = sos + string + eos;

        asio::async_write(socket->socket, asio::buffer(socket->send_buffer), [=](const auto& error_code, ...) {
            if (error_code) {
                std::cout << "send failed: " << error_code.message() << std::endl;
                socket->socket.close();
            } else {
                std::cout << "send succeeded" << std::endl;
                receive(socket);
            }
        });
    }

    void receive(const Socket::Ptr& socket) {
        asio::async_read_until(socket->socket, asio::dynamic_buffer(socket->receive_buffer), boost::regex(sos + ".*" + eos), [=](const auto& error_code, ...) {
            if (error_code) {
                std::cout << "receive failed: " << error_code.message() << std::endl;
                socket->socket.close();
            } else {
                std::cout << "receive succeeded" << std::endl;
                socket->receive_buffer.erase(socket->receive_buffer.begin(), socket->receive_buffer.begin() + sos.size());
                socket->receive_buffer.erase(socket->receive_buffer.end() - eos.size(), socket->receive_buffer.end());

                send(socket, server_app.attr("process")(socket->receive_buffer).cast<std::string>());
            }
        });
    }
};

int main(int argc, char* argv[]) {
    asio::io_context io_context;
    asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v4(), std::atoi(argv[1]));
    Server server(io_context, endpoint);

    server.accept();
    io_context.run();
}