#include "ManifestReader.h"
#include "SceneComposer.h"

#include <Logging.h>
#include <boost/program_options.hpp>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/spdlog.h>

#include <iostream>

std::shared_ptr<spdlog::logger> gLogger;

namespace po = boost::program_options;

int main(int argc, char* argv[]) {
        gLogger = spdlog::stdout_logger_mt("Init");

        po::options_description desc("compose_scene — assemble engine scenes from a manifest\n\nUsage");
        desc.add_options()
                ("help,h",    "print help")
                ("manifest",  po::value<std::string>()->required(),
                 "path to .seassembly manifest file")
                ("output,o",  po::value<std::string>(),
                 "override output path from manifest")
                ("level",     po::value<std::string>()->default_value("info"),
                 "log level: debug|info|warn|error")
                ("log",       po::value<std::string>()->default_value("stdout"),
                 "log destination: stdout|stderr|<file>")
                ("print,p",   "print node tree of each loaded scene");

        po::positional_options_description pos;
        pos.add("manifest", 1);

        po::variables_map vm;
        try {
                po::store(po::command_line_parser(argc, argv)
                                .options(desc).positional(pos).run(), vm);
                if (vm.count("help")) { std::cout << desc << "\n"; return 0; }
                po::notify(vm);
        }
        catch (const std::exception& e) {
                std::cerr << "error: " << e.what() << "\n" << desc << "\n";
                return 1;
        }

        const std::string& log_dest = vm["log"].as<std::string>();
        if (log_dest == "stdout")
                gLogger = spdlog::stdout_logger_mt("G");
        else if (log_dest == "stderr")
                gLogger = spdlog::stderr_logger_mt("G");
        else
                gLogger = spdlog::rotating_logger_mt("G", log_dest, 1024*1024*1024, 10);

        const std::string& level = vm["level"].as<std::string>();
        if      (level == "debug") gLogger->set_level(spdlog::level::debug);
        else if (level == "warn")  gLogger->set_level(spdlog::level::warn);
        else if (level == "error") gLogger->set_level(spdlog::level::err);
        else                       gLogger->set_level(spdlog::level::info);

        SE::TOOLS::Manifest oManifest;
        if (!SE::TOOLS::ReadManifest(vm["manifest"].as<std::string>(), oManifest))
                return 1;

        if (vm.count("output"))
                oManifest.output = vm["output"].as<std::string>();

        if (oManifest.output.empty()) {
                log_e("no output path (set 'output' in manifest or use -o)");
                return 1;
        }

        SE::TOOLS::SceneComposer oComposer(oManifest, vm.count("print") > 0);
        return oComposer.Compose() ? 0 : 1;
}
