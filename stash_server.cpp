#include <iostream>
#include <fstream>
#include <string>
#include <boost/asio.hpp>
#include <vector>
#include <optional>
#include <filesystem>

// g++ -I ~/HomeExt/boost_1_76_0 stash_server.cpp -o server.out -std=c++17 -pthread

/*
    To Do:
        - receive_files currently outputs dummy content
        - switch send and receive to async
        - switch send and receive to use streambuf?
        - Add smart pointers?
*/

class Stash_Session : public std::enable_shared_from_this<Stash_Session>
{
    private:
        boost::asio::ip::tcp::socket m_session_sock;
        boost::asio::streambuf m_streambuf;

        std::vector <std::string> parse_header ()
        {
            std::vector<char> header(m_streambuf.size());
            buffer_copy(boost::asio::buffer(header), m_streambuf.data());

            std::string current_arg {""};
            std::vector <std::string> args;
            for (int i{0}; i < header.size(); i++)
            {
                if (header[i] == ' ' or i == header.size()-1)
                {
                    args.push_back(current_arg);
                    current_arg = "";
                }
                else
                {
                    current_arg += header[i];
                }
            }

            std::cout << "Identified " << args.size() << " argument(s) in header\n";
            for (std::string arg : args)
                std::cout << "\t" << arg <<"\n";

            // Check if call makes sense
            if (args.size() < 1)
                args[0] = "Invalid Operation";
            else if (args[0] == "push" and args.size() > 1)
                ;
            else if (args[0] == "pull")
                ;
            else
                args[0] = "Invalid Operation";

            return args;
        }

        void receive_files(const std::filesystem::path folder, const std::size_t num_files)
        {
            std::cout << "In receive files\n";
            if (not std::filesystem::exists(folder))
                std::filesystem::create_directory(folder);
            for (std::size_t i{0}; i < num_files; i++)
            {
                std::cout << "\tReceiving file " << i << "\n";
                std::vector <std::byte> current_file;
                boost::asio::read(m_session_sock, boost::asio::buffer(current_file));
                std::ofstream fout;
                fout.open(folder/std::to_string(i));
                fout << "Writing something";
                fout.close();
            }
        }

        std::size_t send(const std::filesystem::directory_entry& file)
        {
            //return boost::asio::write(m_session_sock, boost::asio::buffer(file));
            std::cout << "In send\n";
            return 2;
        }

        void send_files(const std::filesystem::path folder)
        {
            for (auto& file : std::filesystem::recursive_directory_iterator(folder))
            {
                send(file);
                std::filesystem::remove(file);
            }
        }

        void header_read_completion_handler (boost::system::error_code error, std::size_t bytes_transferred)
        {
            if ( (!error or error == boost::asio::error::eof) and bytes_transferred > 0)
            {
                std::cout << "Read successful (" << bytes_transferred << " bytes transferred)\n";
                std::vector <std::string> args {parse_header()};
                if (args[0] == "push")
                {
                    std::cout << "Pushing\n";
                    receive_files("test", static_cast<std::size_t>(std::stoi(args[1])));
                }
                else if (args[0] == "pull")
                {
                    std::cout << "Pulling\n";
                    //send_files();
                }
            }
            else
                std::cerr << "Read failed with " << error.message() << "\n";
            std::cout << "Leaving read_completion_handler\n";
        }

    public:
        Stash_Session(boost::asio::ip::tcp::socket&& session_sock)
            : m_session_sock(std::move(session_sock))
        {
        }

        void start()
        {
            boost::asio::async_read(m_session_sock, m_streambuf, std::bind(&Stash_Session::header_read_completion_handler, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
        }

};


class Stash_Server
{
    private:
        int m_port;
        boost::asio::io_context* m_io_context_ptr;
        boost::asio::ip::tcp::acceptor m_acceptor;
        std::optional<boost::asio::ip::tcp::socket> m_socket;

    public:
        Stash_Server(boost::asio::io_context* io_context_ptr, std::uint16_t port = 3490)
            // Create acceptor for any ipv4 endpoint; opens, binds, and listens on endpoint
            : m_port{port}, m_io_context_ptr{io_context_ptr}, m_acceptor(*m_io_context_ptr, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), m_port))
        {
        }

        void async_accept()
        {
            m_socket.emplace(*m_io_context_ptr);
            m_acceptor.async_accept(*m_socket, [&] (boost::system::error_code error)
            {
                std::make_shared<Stash_Session>(std::move(*m_socket))->start();
                async_accept();
            });
        }

};

int main()
{
  try
  {
    boost::asio::io_context io_context;
    Stash_Server server(&io_context);
    server.async_accept();
    io_context.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
