#include "qcompiler/arm64.h"
#include "qcompiler/ir.h"
#include "qcompiler/lexer.h"
#include "qcompiler/parser.h"
#include "qcompiler/qast.h"
#include "qcompiler/sigma_dfa.h"
#include "qcompiler/slr.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <system_error>

using namespace qcompiler;

namespace {

std::string read_file(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("cannot open input file: " + path.string());
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

void write_file(const std::filesystem::path &path, const std::string &content) {
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("cannot write output file: " + path.string());
    }
    out << content;
}

void remove_file_if_exists(const std::filesystem::path &path) {
    std::error_code error;
    std::filesystem::remove(path, error);
    if (error) {
        throw std::runtime_error("cannot remove stale output file: " + path.string());
    }
}

std::string tokens_to_text(const std::vector<Token> &tokens) {
    std::ostringstream out;
    for (const auto &token : tokens) {
        if (token.type == TokenType::End) {
            continue;
        }
        out << '(' << token_type_name(token.type) << ", " << token.lexeme << ")"
            << " @" << token.line << ':' << token.column << '\n';
    }
    return out.str();
}

struct CliOptions {
    std::filesystem::path input_path;
    std::filesystem::path output_prefix;
    char route = 'C';
};

char parse_route(const std::string &value) {
    if (value == "A" || value == "a") {
        return 'A';
    }
    if (value == "B" || value == "b") {
        return 'B';
    }
    if (value == "C" || value == "c") {
        return 'C';
    }
    throw std::runtime_error("route must be A, B, or C");
}

CliOptions parse_options(int argc, char **argv) {
    if (argc < 4) {
        throw std::runtime_error("missing arguments");
    }
    CliOptions options;
    options.input_path = argv[1];
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--out") {
            if (i + 1 >= argc) {
                throw std::runtime_error("missing value for --out");
            }
            options.output_prefix = argv[++i];
        } else if (arg == "--route") {
            if (i + 1 >= argc) {
                throw std::runtime_error("missing value for --route");
            }
            options.route = parse_route(argv[++i]);
        } else {
            throw std::runtime_error("unknown argument: " + arg);
        }
    }
    if (options.output_prefix.empty()) {
        throw std::runtime_error("missing --out <output-prefix>");
    }
    return options;
}

IRProgram parse_by_route(char route, const std::vector<Token> &tokens, const std::filesystem::path &output_prefix) {
    if (route == 'A') {
        remove_file_if_exists(output_prefix.string() + ".slr");
        const QastNode qast = parse_qast_recursive_descent(tokens);
        write_file(output_prefix.string() + ".qast", qast_to_text(qast));
        return qast_to_ir(qast);
    }
    if (route == 'B') {
        const QastNode qast = parse_qast_slr(tokens);
        write_file(output_prefix.string() + ".qast", qast_to_text(qast));
        write_file(output_prefix.string() + ".slr", slr_table_to_text(build_qsort_slr_table()));
        return qast_to_ir(qast);
    }
    remove_file_if_exists(output_prefix.string() + ".qast");
    Parser parser(tokens);
    write_file(output_prefix.string() + ".slr", slr_table_to_text(build_qsort_slr_table()));
    return parser.parse_program();
}

void print_usage() {
    std::cerr << "usage: qcompiler <input.ql> [--route A|B|C] --out <output-prefix>\n";
}

} // namespace

int main(int argc, char **argv) {
    try {
        CliOptions options;
        try {
            options = parse_options(argc, argv);
        } catch (const std::exception &ex) {
            print_usage();
            std::cerr << "qcompiler: " << ex.what() << '\n';
            return 2;
        }

        Lexer lexer(read_file(options.input_path));
        auto tokens = lexer.tokenize();
        IRProgram program = parse_by_route(options.route, tokens, options.output_prefix);
        program = elide_temp_moves_program(std::move(program));
        IRProgram optimized = optimize_program_basic(program);

        Arm64Generator generator;
        write_file(options.output_prefix.string() + ".tokens", tokens_to_text(tokens));
        write_file(options.output_prefix.string() + ".qtac", program_to_qtac(program));
        write_file(options.output_prefix.string() + ".symtab", program_to_symtab(program));
        write_file(options.output_prefix.string() + ".s", generator.generate(program));
        write_file(options.output_prefix.string() + ".opt.s", generator.generate(optimized));
        write_file(options.output_prefix.string() + ".sigma", SigmaDfa::instance().to_text());
        return 0;
    } catch (const std::exception &ex) {
        std::cerr << "qcompiler: " << ex.what() << '\n';
        return 1;
    }
}
