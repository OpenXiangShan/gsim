#ifndef CONFIG_H
#define CONFIG_H

struct Config {
  bool EnableDumpGraph;
  std::string OutputDir;
  int SuperNodeMaxSize;
  uint32_t cppMaxSizeKB;
  std::string sep_module;
  std::string sep_aggr;
  Config();
};

extern Config globalConfig;

#endif
