#ifndef MT_COST_MODEL_H
#define MT_COST_MODEL_H

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <climits>
#include <string_view>

// Static costs shared by MTask partitioning and worker scheduling. No task
// names, profiling samples or compiler-generated symbol sizes are inputs.
namespace mtcost {
enum Feature { Assign, Simple, Multiply, Divide, Index, Call, Branch, Count };
using Features = std::array<double, Count>;

inline constexpr Features defaultWeights{1.0, 0.5, 4.0, 16.0, 0.5, 6.0, 8.0};

inline bool identifier(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '$';
}

inline Features operations(std::string_view text) {
  Features features{};
  for (size_t i = 0; i < text.size();) {
    const char c = text[i];
    if (i + 1 < text.size() && c == '/' && text[i + 1] == '/') break;
    if (i + 1 < text.size() && c == '/' && text[i + 1] == '*') {
      const size_t end = text.find("*/", i + 2);
      i = end == text.npos ? text.size() : end + 2;
      continue;
    }
    if (c == '"' || c == '\'') {
      const char quote = c;
      ++i;
      while (i < text.size()) {
        if (text[i] == '\\') { i = std::min(text.size(), i + 2); continue; }
        if (text[i++] == quote) break;
      }
      continue;
    }
    if (identifier(c)) {
      const size_t begin = i++;
      while (i < text.size() && identifier(text[i])) ++i;
      const std::string_view word = text.substr(begin, i - begin);
      size_t next = i;
      while (next < text.size() && std::isspace(static_cast<unsigned char>(text[next]))) ++next;
      if (next < text.size() && text[next] == '(') {
        if (word == "if" || word == "for" || word == "while") {
          ++features[Branch];
        } else if (word != "sizeof" && word != "alignof" && word != "decltype" &&
                   word != "static_cast" && word != "reinterpret_cast" &&
                   word != "_BitInt" && word != "uint8_t" && word != "uint16_t" &&
                   word != "uint32_t" && word != "uint64_t" && word != "uint128_t" &&
                   word != "int" && word != "bool") {
          ++features[Call];
        }
      }
      continue;
    }
    const std::string_view pair = text.substr(i, 2);
    if (pair == "==" || pair == "!=" || pair == "<=" || pair == ">=" ||
        pair == "<<" || pair == ">>" || pair == "&&" || pair == "||" ||
        pair == "++" || pair == "--") {
      ++features[Simple]; i += 2; continue;
    }
    if (c == '=') ++features[Assign];
    else if (c == '*') ++features[Multiply];
    else if (c == '/' || c == '%') ++features[Divide];
    else if (c == '[') ++features[Index];
    else if (c == '+' || c == '-' || c == '&' || c == '|' || c == '^' ||
             c == '~' || c == '!' || c == '<' || c == '>' || c == '?') {
      ++features[Simple];
    }
    ++i;
  }
  return features;
}

struct Accumulator {
  Features all{};
  void statement(std::string_view text) {
    const Features features = operations(text);
    for (size_t i = 0; i < Count; ++i) all[i] += features[i];
  }
  void begin(std::string_view condition) { statement(condition); }
  void end() {}
};

inline int bounded(double value) {
  if (!std::isfinite(value) || value >= INT_MAX/4) return INT_MAX/4;
  return std::max(1, static_cast<int>(std::ceil(value)));
}

// Relative work units, NOT ns.  The weights use all-path counts because that
// ranked both instrumented task time and uninstrumented perf samples better
// than a fixed 50/50 branch prior on the XiangShan calibration workload.  The
// deliberately conservative multiply/divide ratios avoid fitting the handful
// of such tasks as design-specific outliers.
inline double score(const Features& f) {
  double result = 0;
  for (size_t i = 0; i < Count; ++i) result += f[i]*defaultWeights[i];
  return result;
}
// Relative units fixed from the XiangShan trace/perf calibration. They are
// static inputs to all later designs; generated simulators read no profile.
inline constexpr int taskOverhead = 1400;
inline constexpr int remoteCheck = 192;
inline constexpr int remotePublish = 32;
inline constexpr int tokenLatency = 32;
}
#endif
