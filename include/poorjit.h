#include <poorjit_version.h>
#include <boost/dll.hpp>
#include <boost/process.hpp>
#include <filesystem>
#include <fstream>
#include <future>
#include <mutex>
#include <string>
#include <zlib.h>

namespace poorjit {

namespace bp = ::boost::process;
namespace dll = ::boost::dll;
namespace fs = ::std::filesystem;

template <class plugin_api> class jitlib {
public:
  jitlib(boost::shared_ptr<plugin_api> &&sym) : symbol(sym) {}
  plugin_api *operator->() { return symbol.get(); }
  plugin_api &operator*() { return *symbol; }

private:
  boost::shared_ptr<plugin_api> symbol;
};

std::mutex jitmtx; // protects file_mtx
std::map<std::string, std::mutex> file_mtx; //ensures each file is only compiled once

template <class plugin_api> class jitmgr {

public:
  ~jitmgr() {
    for (auto const& p : cleanup) {
      fs::remove(p);
    }
  }
  std::future<jitlib<plugin_api>> jit(std::string source_code, std::vector<std::string>extra_args={}) {
    std::unique_lock<std::mutex> lock(jitmtx);
    fs::create_directory(jit_workdir);
    auto jit_path = (jit_workdir / hash(source_code));

    return std::async(
        std::launch::async, [this, my_source_code = std::move(source_code),
                             my_extra_args = std::move(extra_args),
                             my_jit_path = std::move(jit_path)] {
          auto source_path = my_jit_path;
          source_path.replace_extension(".cc");
          auto lib_path = my_jit_path;
          lib_path.replace_extension(".so");

          std::unique_lock<std::mutex> jit_lock(jitmtx);
          std::unique_lock<std::mutex> file_lock(file_mtx[lib_path]);

          //cache compilation
          if(!fs::exists(lib_path)) {
            cleanup.emplace_back(lib_path);
            cleanup.emplace_back(source_path);
            jit_lock.unlock();
            {
              std::ofstream source_file(source_path);
              source_file.write(my_source_code.c_str(), my_source_code.size());
            }

            std::vector<std::string> args {
              compiler,
              source_path.string(),
              "-o", lib_path.string(),
              "-fPIC", "-fvisibility=hidden", "-shared"
            };
            std::copy(my_extra_args.begin(), my_extra_args.end(), std::back_inserter(args));

            bp::child c(args, bp::std_out > stdout, bp::std_err > stderr);
            c.wait();
          } else {
            jit_lock.unlock();
          }

          return jitlib<plugin_api>{dll::import_symbol<plugin_api>(
              lib_path.string(), "plugin", dll::load_mode::append_decorations)};
        });
  }
  void disable_cleanup() {
    do_cleanup = false;
  }

private:
  std::string hash(std::string const &data) {
    std::size_t crc = crc32_z(size_t{0}, nullptr, 0);
    crc = crc32_z(crc, reinterpret_cast<const unsigned char *>(data.data()),
                  data.size());
    return std::to_string(crc);
  }

  bool do_cleanup = true;
  std::vector<fs::path> cleanup;
  fs::path jit_workdir =
      fs::temp_directory_path() / "poorjit" / std::to_string(getpid());
  std::string compiler = POORJIT_DEFAULT_COMPILER;
};

} // namespace poorjit
