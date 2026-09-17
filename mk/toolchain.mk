##############################################
### Compiler Detection (clang >= 19 required)
##############################################
# Clang 18 and older have known _BitInt correctness issues. Clang 19 and
# newer are supported. Prefer a suitable system clang++, then fall back to
# the versioned clang++-19 executable if necessary.

MIN_CLANG_MAJOR := 19

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
DEFAULT_CLANG_OK := $(shell [ -n '$(DEFAULT_CLANG_MAJOR)' ] && [ $(DEFAULT_CLANG_MAJOR) -ge $(MIN_CLANG_MAJOR) ] && echo yes || echo no)
ifneq ($(strip $(DEFAULT_CLANG_VERSION)),)
  $(info [gsim] System clang++ version detected: $(DEFAULT_CLANG_VERSION))
else
  $(info [gsim] System clang++ not found or not clang)
endif

# Enforce: if CXX is not explicitly set, a supported default clang++ or
# clang++-19 must be available.
ifeq ($(HAVE_CLANG19),no)
ifeq ($(filter environment command line,$(ORIGIN_CXX)),)
ifneq ($(DEFAULT_CLANG_OK),yes)
$(error No suitable clang found. Please install clang >= $(MIN_CLANG_MAJOR) or provide CXX pointing to it.)
endif
endif
endif

# Pick CXX: keep a supported default clang++, otherwise fall back to clang++-19.
ifeq ($(ORIGIN_CXX),default)
  $(info [gsim] CXX not explicitly set, force CXX with auto-detection)
  ifeq ($(DEFAULT_CLANG_OK),yes)
    CXX = clang++
  else ifeq ($(HAVE_CLANG19),yes)
    CXX = clang++-19
  else
    $(error No suitable clang found. Please install clang >= $(MIN_CLANG_MAJOR) or set CXX to a valid clang compiler.)
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
ifneq ($(CLANG_MAJOR),)
ifeq ($(shell [ $(CLANG_MAJOR) -ge $(MIN_CLANG_MAJOR) ] && echo ok),)
$(warning Detected clang $(CLANG_MAJOR); clang $(MIN_CLANG_MAJOR) or newer is required.)
endif
endif
else
$(warning CXX ($(CXX)) does not appear to be clang; clang $(MIN_CLANG_MAJOR) or newer is required.)
endif

# If user explicitly set CXX (env or command line), enforce: must be clang >= 19
ifneq ($(filter environment command line,$(ORIGIN_CXX)),)
  $(info [gsim] CXX explicitly provided by user; validating...)
  ifeq ($(findstring clang,$(CXX_VERSION_STR)),)
    $(error CXX is set to '$(CXX)', which is not clang. Please set CXX to clang >= $(MIN_CLANG_MAJOR).)
  endif
  ifneq ($(CLANG_MAJOR),)
    ifeq ($(shell [ $(CLANG_MAJOR) -ge $(MIN_CLANG_MAJOR) ] && echo ok),)
      $(error CXX is set to clang $(CLANG_MAJOR) (<$(MIN_CLANG_MAJOR)). Please use clang >= $(MIN_CLANG_MAJOR).)
    endif
  endif
endif

$(info [gsim] Compiler detection: done)

