
#ifdef VERILATOR
#include <verilated.h>
#include <verilated_vcd_c.h>
#include HEADER
#endif

#if defined(GSIM)
#include "../obj/ysyx6.h"
#endif

#include <sys/mman.h>

#include <fstream>
#include <cassert>
#include <tuple>
#include <chrono>
#include <iostream>
#include <format>
#include <cstdio>

struct MMapDeleter {
  MMapDeleter(off_t size) : size(size) {}

  void operator()(void* p) {
    if (munmap(p, size) == -1) { perror("munmap"); }
  }
  off_t size;
};

class Model {
 public:
  Model() {}

  virtual void eval() {}
  virtual void set_clock(bool clock) {}
  virtual void set_reset(bool reset) {}
  virtual bool get_trap() { return false; }
};

#ifdef VERILATOR
class VerilatorModel : public Model {
 public:
  VerilatorModel(void* ram, std::size_t size) {
    // Initialize Context
    Context = std::make_unique<VerilatedContext>();

    // Initialize verilated c model
    ref = std::make_unique<REF_NAME>(Context.get());

    // Initialize verilated vcd
    //FIXME: Add `--trace` to `VERI_VFLAGS`
    Verilated::traceEverOn(true);
    Wave = std::make_unique<VerilatedVcdC>();

    ref->trace(Wave.get(), 99, 99);
    Wave->open("./model.vcd");

    // Initialize memory
    memcpy(&ref->rootp->ysyx6__DOT__Mem__DOT__mem_ext__DOT__Memory, ram, size);
  }

  void eval() override { ref->eval(); }
  void set_clock(bool clock) override { ref->clock = clock; }
  void set_reset(bool reset) override { ref->reset = reset; }
  void clock() {
    set_clock(false);
    eval();
    Context->timeInc(1);
    Wave->dump(Context->time());

    set_clock(true);
    eval();
    Context->timeInc(1);
    Wave->dump(Context->time());

    ++Cycles;
  }

  void reset() {
    for (int i = 0; i < 100; ++i) {
      set_reset(i < 10);
      clock();
    }
  }

  bool get_trap() override { return ref->rootp->io_debug_trap; }
  std::size_t get_cycle() { return Cycles; }

  REF_NAME* get() { return ref.get(); }

 private:
  std::unique_ptr<VerilatedContext> Context{nullptr};
  std::unique_ptr<VerilatedVcdC> Wave{nullptr};
  std::unique_ptr<REF_NAME> ref{nullptr};

  std::size_t Cycles{0};
};
#endif

#ifdef GSIM
class GsimModel : public Model {
 public:
  GsimModel(void* ram, std::size_t size) { memcpy(ref.Mem$mem, ram, size); }

  void eval() override { ref.step(); }
  // No need to do: void set_clock(bool clock) {}
  void set_reset(bool reset) override { ref.set_reset(reset); }

  void clock() { eval(); }

  void reset() {
    for (int i = 0; i < 100; ++i) {
      set_reset(i < 10);
      clock();
    }
  }

  bool get_trap() override { return ref.io$$debug_trap; }
  std::size_t get_cycle() { return ref.cycles; }

  MOD_NAME& get() { return ref; }

 private:
  MOD_NAME ref;
};
#endif

#if (defined(VERILATOR) && defined(GSIM))
class ComparingModel : public Model {
 public:
  ComparingModel(void* ram, std::size_t size) : VtorModel(ram, size), GModel(ram, size) {}

  void eval() override {
    VtorModel.eval();

    GModel.eval();
  }

  void set_reset(bool reset) override {
    VtorModel.set_reset(reset);
    GModel.set_reset(reset);
  }

  void clock() {
    VtorModel.clock();

    GModel.clock();

    check();
  }

  void reset() {
    VtorModel.reset();

    GModel.reset();
  }

  void check() {
    bool exists{false};
    auto report = [&](auto name, auto miss, auto right) {
      std::cerr << std::format("mismatching @({:#x}) {:#x}: {}\n\texpected: {:#x} unexpected: {:#x}\n",
                               VtorModel.get_cycle(), GModel.get_cycle(), name, miss, right);
      exists = true;
    };

    auto diff = [&](auto name, auto left, auto right) {
      if (left != right) report(name, left, right);
    };

#define VTOR_GET(X) VtorModel.get()->rootp->X
#define GSIM_GET(X) GModel.get().X
#define DIFF(X, Y) diff(#X, VTOR_GET(Y), GSIM_GET(X))
    DIFF(Tile$FakeICache$state, ysyx6__DOT__Tile__DOT__FakeICache__DOT__state);
    DIFF(Tile$FakeDCache$state, ysyx6__DOT__Tile__DOT__FakeDCache__DOT__state);
    DIFF(Tile$Core$frontend$valid, ysyx6__DOT__Tile__DOT__Core__DOT__frontend__DOT__valid);
    DIFF(Tile$Arb$state, ysyx6__DOT__Tile__DOT__Arb__DOT__state);
    DIFF(Tile$Arb$inflightSrc, ysyx6__DOT__Tile__DOT__Arb__DOT__inflightSrc);

    // Frontend
    DIFF(Tile$Core$frontend$fetchUnit$f1$MemReady,
         ysyx6__DOT__Tile__DOT__Core__DOT__frontend__DOT__fetchUnit__DOT__f1__DOT__MemReady);
    DIFF(Tile$Core$frontend$fetchUnit$f1$Stall,
         ysyx6__DOT__Tile__DOT__Core__DOT__frontend__DOT__fetchUnit__DOT__f1__DOT__Stall);
    DIFF(Tile$Core$frontend$fetchUnit$valid, ysyx6__DOT__Tile__DOT__Core__DOT__frontend__DOT__fetchUnit__DOT__valid);
    DIFF(Tile$FakeICache$needFlush, ysyx6__DOT__Tile__DOT__FakeICache__DOT__needFlush);
    DIFF(Tile$Core$frontend$fetchUnit$f0$pc,
         ysyx6__DOT__Tile__DOT__Core__DOT__frontend__DOT__fetchUnit__DOT__f0__DOT__pc);

    // Backend
    DIFF(Tile$Core$backend$valid, ysyx6__DOT__Tile__DOT__Core__DOT__backend__DOT__valid);
    DIFF(Tile$Core$backend$valid_1, ysyx6__DOT__Tile__DOT__Core__DOT__backend__DOT__valid_1);
    DIFF(Tile$Core$backend$valid_2, ysyx6__DOT__Tile__DOT__Core__DOT__backend__DOT__valid_2);
    DIFF(Tile$Core$backend$EXU$LSU$Stall, ysyx6__DOT__Tile__DOT__Core__DOT__backend__DOT__EXU__DOT__LSU__DOT__Stall);

    // Xbar
    DIFF(xbar$outSelRespVec__0, ysyx6__DOT__xbar__DOT__outSelRespVec_0);
    DIFF(xbar$outSelRespVec__1, ysyx6__DOT__xbar__DOT__outSelRespVec_1);
    DIFF(xbar$outSelRespVec__2, ysyx6__DOT__xbar__DOT__outSelRespVec_2);
    DIFF(xbar$outSelRespVec__3, ysyx6__DOT__xbar__DOT__outSelRespVec_3);

#undef GSIM_GET
#undef VTOR_GET
    if (exists) assert(0);
  }

  bool get_trap(void) override { return GModel.get_trap(); }

 private:
  VerilatorModel VtorModel;
  GsimModel GModel;
};
#endif

auto loadProgram(const std::string& Path) {
  std::fstream fs(Path, std::ios::binary | std::ios::in | std::ios::ate);
  if (!fs.is_open()) throw std::logic_error(std::format("Failed to open: {}\n", Path));

  std::cout << std::format("Using image: {}\n", Path);

  std::size_t ImgSize = fs.tellg();
  fs.seekg(0, std::ios::beg);

  std::cout << std::format("Using {} image\n", ImgSize);

  // Initialize memory
  auto* ptr =
      static_cast<uint64_t*>(mmap(NULL, 4 * 1024 * 1024, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0));
  if (ptr == MAP_FAILED) {
    perror("mmap");
    exit(EXIT_FAILURE);
  }

  auto ram = std::shared_ptr<uint64_t>(ptr, MMapDeleter(4 * 1024 * 1024));
  if (!fs.read((char*)ram.get(), ImgSize)) throw std::logic_error("Failed to read");

  fs.close();
  return std::make_tuple(ram, ImgSize);
}

int main(int argc, char** argv) {
  auto image = std::string(argv[1]);
  auto [ram, size] = loadProgram(image);
  assert(ram != nullptr);

#if (defined(VERILATOR) && defined(GSIM))
  ComparingModel Model(ram.get(), size);
#elif (defined(VERILATOR))
  VerilatorModel Model(ram.get(), size);
#elif (defined(GSIM))
  GsimModel Model(ram.get(), size);
#endif

  // Reset it.
  Model.reset();

  auto Before = std::chrono::high_resolution_clock::now();

  for (;;) {
    Model.clock();

    // Hit GOOD/BAD Trap?
    if (Model.get_trap()) break;
  }

  auto After = std::chrono::high_resolution_clock::now();

#if !(defined(VERILATOR) && defined(GSIM))
  std::cerr << "----------------------------------------\n";
  std::cout << std::format("Total cycle {}\n", Model.get_cycle());

  auto Seconds = std::chrono::duration_cast<std::chrono::microseconds>(After - Before).count();
  double FreqMHz = Model.get_cycle() / (double)Seconds;
  std::cout << std::format("frequency {} MHz\n", FreqMHz);
#endif

  return 0;
}
