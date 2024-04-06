#include "SingleServer.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <utility>

#include <boost/asio.hpp>
#include <boost/process.hpp>

namespace asio = boost::asio;
namespace bp = boost::process;
namespace fs = std::filesystem;
using std::string, std::vector, std::move, std::exception;
using std::endl, std::cerr, std::cout, std::cin;
using std::to_string, std::unique_ptr, std::make_unique, std::make_shared;

class SingleServer {
    bool type_is_java;
    string executor;
    int xms, xmx;
    string server_path;
    string server_file;
    vector<string> server_options;
    string config_name;

    unique_ptr<bp::child> server_process{};
    unique_ptr<bp::opstream> stdin_stream{};
    unique_ptr<bp::async_pipe> stdout_pipe{}, stderr_pipe{};
    asio::io_context io_context;

public:
    explicit SingleServer(bool type_is_java = true,
                          string executor = "java",
                          int xms = 1,
                          int xmx = 4,
                          string server_path = "",
                          string server_file = "server.jar",
                          vector<string> server_options = {
                              "-XX:+UnlockExperimentalVMOptions",
                              "-XX:MaxGCPauseMillis=100",
                              "-XX:+DisableExplicitGC",
                              "-XX:TargetSurvivorRatio=90",
                              "-XX:G1NewSizePercent=50",
                              "-XX:G1MaxNewSizePercent=80",
                              "-XX:G1MixedGCLiveThresholdPercent=35",
                              "-XX:+AlwaysPreTouch",
                              "-XX:+ParallelRefProcEnabled",
                              "-Dusing.aikars.flags=mcflags.emc.gs"
                          },
                          string config_name = "Default.json")
        : type_is_java(type_is_java),
          executor(move(executor)),
          xms(xms),
          xmx(xmx),
          server_path(move(server_path)),
          server_file(move(server_file)),
          server_options(move(server_options)),
          config_name(move(config_name)) {
    }

    int start() {
        if (xms > xmx) {
            return 2;
        }

        fs::path server_file_path = fs::path(server_path) / server_file;
        if (server_file.find(".jar") == string::npos && type_is_java) {
            server_file_path += ".jar";
        }

        stdin_stream = make_unique<bp::opstream>();
        stdout_pipe = make_unique<bp::async_pipe>(io_context);
        stderr_pipe = make_unique<bp::async_pipe>(io_context);

        vector<string> args;
        args.emplace_back(executor);
        args.emplace_back("-Xms" + to_string(xms) + "G");
        args.emplace_back("-Xmx" + to_string(xmx) + "G");
        args.insert(args.end(), server_options.begin(), server_options.end());
        if (type_is_java) {
            args.emplace_back("-jar");
        }
        args.emplace_back(server_file_path.string());

        try {
            server_process = make_unique<bp::child>(
                args,
                bp::std_in < *stdin_stream,
                bp::std_out > *stdout_pipe,
                bp::std_err > *stderr_pipe,
                bp::start_dir = server_path
            );

            start_async_read(stdout_pipe, false);
            start_async_read(stderr_pipe, true);

        } catch (const exception &e) {
            cerr << "Failed to start server: " << e.what() << endl;
            return 1;
        }

        io_context.run();

        return 0;
    }

    void exit() {
        if (server_process) {
            server_process->terminate();
            server_process->wait();
            cleanup();
        }
    }

    void stop() {
        command("stop");
        server_process->wait();
        cleanup();
    }

    void command(const string &cmd) {
        if (stdin_stream) {
            *stdin_stream << cmd << endl;
            stdin_stream->flush();
        }
    }

private:
    void start_async_read(unique_ptr<bp::async_pipe> &pipe, bool is_stderr) {
        auto buffer = make_shared<vector<char>>(1024);
        pipe->async_read_some(asio::buffer(*buffer),
            [this, buffer, &pipe, is_stderr](const boost::system::error_code &ec, size_t bytes_transferred) {
                if (!ec) {
                    string output(buffer->data(), bytes_transferred);
                    if (is_stderr) {
                        cerr << output;
                    } else {
                        cout << output;
                    }
                    this->start_async_read(pipe, is_stderr);
                }
            }
        );
    }

    void cleanup() {
        server_process.reset();
        stdin_stream.reset();
        stdout_pipe.reset();
        stderr_pipe.reset();
    }
};
