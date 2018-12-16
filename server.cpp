#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/process.hpp>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace asio = boost::asio;
namespace process = boost::process;

class Server {
    template <typename Stream, typename Buffer>
    struct BufferStream {
        using Ptr = std::unique_ptr<BufferStream>;
        template <typename... Args>
        BufferStream(Args&&... args) : stream(std::forward<Args>(args)...) {}
        template <typename... Args>
        static auto create(Args&&... args) {
            return std::make_unique<BufferStream>(std::forward<Args>(args)...);
        }
        auto& read_stream() { return stream.read_stream(); }
        auto& write_stream() { return stream.write_stream(); }

        Stream stream;
        Buffer read_buffer;
        Buffer write_buffer;
    };

    template <typename... Streams>
    struct StreamWrapper;

    template <typename ReadWriteStream>
    struct StreamWrapper<ReadWriteStream> {
        template <typename... Args>
        StreamWrapper(Args&&... args) : stream(std::forward<Args>(args)...) {}
        auto& read_stream() { return stream; }
        auto& write_stream() { return stream; }

        ReadWriteStream stream;
    };

    template <typename ReadStream, typename WriteStream>
    struct StreamWrapper<ReadStream, WriteStream> {
        template <typename... Args>
        StreamWrapper(Args&&... args) : streams(std::forward<Args>(args)...) {}
        auto& read_stream() { return streams.first; }
        auto& write_stream() { return streams.second; }

        std::pair<ReadStream, WriteStream> streams;
    };

    using BufferSocket = BufferStream<StreamWrapper<asio::ip::tcp::socket>, std::string>;
    using BufferPipe = BufferStream<StreamWrapper<process::async_pipe, process::async_pipe>, std::string>;

   private:
    asio::io_context& io_context;
    asio::ip::tcp::acceptor acceptor;

   public:
    Server(asio::io_context& context, const asio::ip::tcp::endpoint& endpoint) : io_context(context), acceptor(context, endpoint) {}

    void accept() {
        acceptor.async_accept([this](const auto& error_code, auto&& socket) {
            if (error_code) {
                std::cout << "accept failed: " << error_code.message() << std::endl;
            } else {
                std::cout << "accept succeeded: " << socket.remote_endpoint() << std::endl;
                auto buffer_socket = BufferSocket::create(std::forward<decltype(socket)>(socket));
                auto buffer_pipe = BufferPipe::create(std::piecewise_construct, std::forward_as_tuple(io_context), std::forward_as_tuple(io_context));
                auto child = std::make_unique<process::child>(process::search_path("python"), "server_app.py",
                                                              process::std_in<buffer_pipe->write_stream(), process::std_out> buffer_pipe->read_stream());
                read(std::move(buffer_socket), std::move(buffer_pipe), std::move(child));
            }
            accept();
        });
    }

    template <typename BufferStream, typename NextBufferStream, typename Child>
    void read(BufferStream&& buffer_stream, NextBufferStream&& next_buffer_stream, Child&& child) {
        asio::async_read_until(
            buffer_stream->read_stream(), asio::dynamic_buffer(buffer_stream->read_buffer), '\n',
            [this, buffer_stream = std::forward<BufferStream>(buffer_stream), next_buffer_stream = std::forward<NextBufferStream>(next_buffer_stream),
             child = std::forward<Child>(child)](const auto& error_code, ...) mutable {
                if (error_code) {
                    std::cout << "read failed: " << error_code.message() << std::endl;
                } else {
                    std::cout << "read succeeded" << std::endl;
                    next_buffer_stream->write_buffer = std::move(buffer_stream->read_buffer);
                    write(std::move(next_buffer_stream), std::move(buffer_stream), std::move(child));
                }
            });
    }

    template <typename BufferStream, typename NextBufferStream, typename Child>
    void write(BufferStream&& buffer_stream, NextBufferStream&& next_buffer_stream, Child&& child) {
        asio::async_write(
            buffer_stream->write_stream(), asio::buffer(buffer_stream->write_buffer),
            [this, buffer_stream = std::forward<BufferStream>(buffer_stream), next_buffer_stream = std::forward<NextBufferStream>(next_buffer_stream),
             child = std::forward<Child>(child)](const auto& error_code, ...) mutable {
                if (error_code) {
                    std::cout << "write failed: " << error_code.message() << std::endl;
                } else {
                    std::cout << "write succeeded" << std::endl;
                    read(std::move(buffer_stream), std::move(next_buffer_stream), std::move(child));
                }
            });
    }
};

int main(int argc, char* argv[]) {
    asio::io_context io_context;
    asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v4(), std::atoi(argv[1]));
    Server server(io_context, endpoint);
    server.accept();

    std::vector<std::thread> threads;
    for (auto i = 0; i < std::thread::hardware_concurrency(); ++i) {
        threads.emplace_back([&io_context]() { io_context.run(); });
    }

    for (auto& thread : threads) {
        thread.join();
    }
}