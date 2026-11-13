#ifndef CONFIG_H
#define CONFIG_H

struct Config {
  bool EnableDumpGraph;
  bool DumpGraphDot;
  bool DumpGraphJson;
  bool DumpGraphStats;
  bool DumpAssignTree;
  bool DumpExpressions;
  std::set<std::string> ExpressionKinds;
  std::string ExpressionIdPath;
  std::string ExpressionClosure;
  size_t ExpressionMaxNodes;
  bool DumpConstStatus;
  std::string OutputDir;
  std::string StopAfterStage;
  std::string SplitArrayMapPath;
  std::string NodeEvolutionTracePath;
  std::string NodeEvolutionInputSha256;
  int SuperNodeMaxSize;
  uint32_t cppMaxSizeKB;
  std::string sep_module;
  std::string sep_aggr;
  int MergeWhenSize;
  int When2muxBound;
  int LogLevel;
  std::set<std::string> DumpStages;
  Config();
};

extern Config globalConfig;

#endif
