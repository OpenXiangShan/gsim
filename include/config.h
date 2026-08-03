#ifndef CONFIG_H
#define CONFIG_H

struct Config {
  bool EnableDumpGraph;
  bool DumpGraphDot;
  bool DumpGraphJson;
  bool DumpGraphStats;
  bool DumpAssignTree;
  bool DumpConstStatus;
  bool FlattenNodes;
  bool NoCoarsen;
  std::string OutputDir;
  std::string StopAfterStage;
  std::string ExportPreCoarsenGrh;
  std::string ExportExecutableGrh;
  std::string ExportTopoProj;
  std::string ExecutableGrhProfile;
  std::string InputFile;
  uint64_t InputFileBytes;
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
