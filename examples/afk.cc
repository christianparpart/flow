// AWK-alike implementation with Flow
// usage: afk [-f FILE] [--help, -h | --version, -v]

#include <flow/util/Flags.h>
#include <flow/lang/Interpreter.h>
#include <flow/vm/Instruction.h>
#include <flow/Diagnostics.h>

#include <string>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

class AfkProcessor : public flow::lang::Interpreter {
 public:
  explicit AfkProcessor(bool trace);

  void initialize();
  void process(std::string line);
  void finalize();

 private:
  void flow_line(flow::Params& args);
  void flow_print(flow::Params& args);

 private:
  bool trace_;
  std::string currentLine_;
};

AfkProcessor::AfkProcessor(bool debug)
    : trace_{debug},
      currentLine_{} {
  registerFunction("LINE")
      .bind(&AfkProcessor::flow_line, this)
      .returnType(flow::LiteralType::String);

  registerFunction("print")
      .bind(&AfkProcessor::flow_print, this)
      .param<std::string>("text");
}

void AfkProcessor::flow_line(flow::Params& args) {
  args.setResult(currentLine_);
}

void AfkProcessor::flow_print(flow::Params& args) {
  std::cout << args.getString(1) << '\n';
}

void AfkProcessor::process(std::string line) {
  currentLine_ = std::move(line);

  if (!trace_) {
    run("process");
  } else {
    run("process", nullptr,
        [this](flow::Instruction instr, size_t ip, size_t sp) {
          std::cerr << flow::disassemble(instr, ip, sp, &program()->constants()) << "\n";
        });
  }
}

void AfkProcessor::initialize() {
  run("initially");
}

void AfkProcessor::finalize() {
  run("finally");
}

int main(int argc, const char* argv[]) {
  flow::util::Flags flags;
  flags.defineString("file", 'f', "PROGRAM_FILE", "Path to program to execute");
  flags.defineBool("help", 'h', "Shows this help and then exits");
  flags.defineBool("dump-tc", 'd', "Prints program target code and exits.");
  flags.defineBool("dump-ir", 0, "Prints program IR at the beginning and exits.");
  flags.defineBool("trace", 't', "Prints program target code during execution");
  flags.enableParameters("INPUT_FILE ...", "Files to be processed");
  flags.parse(argc, argv);

  if (flags.getBool("help")) {
    std::cerr << "afk [-f PROGRAM_FILE] INPUT_FILEL" << "\n\n";
    std::cerr << flags.helpText() << "\n";
    return EXIT_SUCCESS;
  }

  std::string programFileName = flags.getString("file");

  AfkProcessor afk{flags.getBool("trace")};
  {
    flow::diagnostics::Report report;
    if (!afk.compileLocalFile(programFileName, &report, 1)) {
      std::cerr << report << "\n";
      return EXIT_FAILURE;
    }
  }

  if (flags.getBool("dump-ir")) {
    afk.programIR()->dump();
    return EXIT_SUCCESS;
  }

  if (flags.getBool("dump-tc")) {
    afk.program()->dump();
    return EXIT_SUCCESS;
  }

  afk.initialize();

  for (const std::string& inputFileName: flags.parameters()) {
    std::ifstream source{inputFileName};
    std::string line;
    for (std::getline(source, line); !source.eof(); std::getline(source, line)) {
      afk.process(std::move(line));
    }
  }

  afk.finalize();

  return EXIT_SUCCESS;
}
