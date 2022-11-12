#include "gtest/gtest.h"
#include <fmt/format.h>
#include <poorjit.h>

using namespace std::string_literals;
class plugin_api {
  public:
  virtual std::string hello() = 0;
  virtual ~plugin_api();
};
TEST(poorjit, integation1) {
  poorjit::jitmgr<plugin_api> mgr;

  std::string src_template = R"(
    #include <boost/config.hpp>
    #include <string>
    class plugin_api {{
      public:
      virtual std::string hello() = 0;
      virtual ~plugin_api()=default;
    }};
    class test_plugin: public plugin_api {{
      std::string hello() override {{
        return "{}";
      }}
    }};

    extern "C" BOOST_SYMBOL_EXPORT test_plugin plugin;
    test_plugin plugin;
  )";

  std::string a = fmt::format(src_template, "a");
  std::string b = fmt::format(src_template, "b");
  
  auto jit_a = mgr.jit(a);
  auto jit_b = mgr.jit(b);
  ASSERT_EQ(jit_a.get()->hello(), "a"s);
  ASSERT_EQ(jit_b.get()->hello(), "b"s);
}
