##############################################
### Compiler Detection (clang-19 preferred)
##############################################
# Clang-19 is recommended and other versions/compilers may fail to build.
# We prefer clang++-19 if available; otherwise fall back to clang++ and
# emit a strong warning that non-19 compilers can cause compilation failures.

# Progress info
$(info [gsim] Compiler detection: start)

# Record whether user explicitly set CXX (environment or command line)
ORIGIN_CXX := $(origin CXX)
$(info [gsim] CXX is setting by: $(ORIGIN_CXX))

# Detect availability of clang++-19
HAVE_CLANG19 := $(shell command -v clang++-19 >/dev/null 2>&1 && echo yes || echo no)
ifeq ($(HAVE_CLANG19),yes)
  $(info [gsim] Found clang++-19)
else
  $(info [gsim] clang++-19 not found)
endif

# Detect default clang++ major version (if any)
DEFAULT_CLANG_VERSION := $(shell clang++ --version 2>/dev/null)
DEFAULT_CLANG_MAJOR := $(shell echo '$(DEFAULT_CLANG_VERSION)' | sed -n 's/.*clang version \([0-9][0-9]*\).*/\1/p')
ifneq ($(strip $(DEFAULT_CLANG_VERSION)),)
  $(info [gsim] System clang++ version detected: $(DEFAULT_CLANG_VERSION))
else
  $(info [gsim] System clang++ not found or not clang)
endif

# Enforce: if clang-19 is missing AND CXX not explicitly set AND default clang < 19, hard error.
ifeq ($(HAVE_CLANG19),no)
ifeq ($(filter environment command line,$(ORIGIN_CXX)),)
ifneq ($(DEFAULT_CLANG_MAJOR),)
ifeq ($(shell [ $(DEFAULT_CLANG_MAJOR) -ge 19 ] && echo ok),)
$(error clang-19 not found, and default clang ($(DEFAULT_CLANG_MAJOR)) is < 19. Please install clang-19 or provide CXX pointing to clang >= 19.)
endif
else
$(error clang-19 not found, and no suitable default clang detected. Please install clang-19 or provide CXX pointing to clang >= 19.)
endif
endif
endif

# Pick CXX: use clang++-19 if present; otherwise leave as provided or fall back to clang++
ifeq ($(ORIGIN_CXX),default)
  $(info [gsim] CXX not explicitly set, force CXX with auto-detection)
  ifeq ($(HAVE_CLANG19),yes)
  CXX = clang++-19
  else
    HAVE_CLANGPP := $(shell command -v clang++ >/dev/null 2>&1 && echo yes || echo no)
    ifeq ($(HAVE_CLANGPP),yes)
      CXX = clang++
    else
      $(warn Neither clang++-19 nor clang++ found. Please install clang++ or set CXX to a valid clang compiler.)
    endif
  endif
  ORIGIN_CXX := force
else
  $(info [gsim] CXX explicitly set by user; use as is)
endif

$(info [gsim] Using CXX=$(CXX) (origin: $(ORIGIN_CXX)))

# Read compiler version string (best-effort)
CXX_VERSION_STR := $(shell $(CXX) --version 2>/dev/null)
ifneq ($(strip $(CXX_VERSION_STR)),)
  $(info [gsim] $(CXX) version: $(CXX_VERSION_STR))
endif

# If compiler is clang, extract major version; otherwise warn
ifneq ($(findstring clang,$(CXX_VERSION_STR)),)
CLANG_MAJOR := $(shell echo '$(CXX_VERSION_STR)' | sed -n 's/.*clang version \([0-9][0-9]*\).*/\1/p')
$(info [gsim] Detected clang major from CXX: $(CLANG_MAJOR))
ifneq ($(CLANG_MAJOR),19)
$(warning Detected clang $(CLANG_MAJOR); clang-19 is strongly recommended and other versions may fail to compile. Install clang-19 or set CXX=clang++-19.)
endif
else
$(warning CXX ($(CXX)) does not appear to be clang; clang-19 is strongly recommended and other compilers may fail to compile.)
endif

# If user explicitly set CXX (env or command line), enforce: must be clang >= 19
ifneq ($(filter environment command line,$(ORIGIN_CXX)),)
  $(info [gsim] CXX explicitly provided by user; validating...)
  ifeq ($(findstring clang,$(CXX_VERSION_STR)),)
    $(error CXX is set to '$(CXX)', which is not clang. Please set CXX=clang++-19 (or a clang >= 19).)
  endif
  ifneq ($(CLANG_MAJOR),)
    ifeq ($(shell [ $(CLANG_MAJOR) -ge 19 ] && echo ok),)
      $(error CXX is set to clang $(CLANG_MAJOR) (<19). Please use clang >= 19, preferably clang-19.)
    endif
  endif
endif

$(info [gsim] Compiler detection: done)

