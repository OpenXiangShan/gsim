#ifndef CONFIG_H
#define CONFIG_H

struct Config {
  bool EnableDumpGraph;
  std::string OutputDir;
  int SuperNodeMaxSize;
  Config();
};

extern Config globalConfig;

#endif
