// SPDX-License-Identifier: BSD-2-Clause

// This code is part of the sfizz library and is licensed under a BSD 2-clause
// license. You should have receive a LICENSE.md file along with the code.
// If not, contact the sfizz maintainers at https://github.com/sfztools/sfizz

/*
  This program reads a SFZ file, and outputs it back into a single file
  with all the includes and definitions processed.

  It can serve to facilitate identifying problems, whether these are related to
  the parser or complicated instrument structures.
 */

#include "parser/Parser.h"
#include "../tests/cxxopts.hpp"
#include "absl/strings/string_view.h"
#include <iostream>

class MyParserListener : public sfz::Parser::Listener {
public:
    explicit MyParserListener(sfz::Parser& parser)
        : _parser(parser)
    {
    }

protected:
    void onParseFullBlock(const std::string& header, const std::vector<sfz::Opcode>& opcodes) override
    {
        std::cout << '\n';
        std::cout << '<' << header << '>' << '\n';
        for (const sfz::Opcode& opc : opcodes)
            std::cout << opc.opcode << '=' << opc.value << '\n';
    }

    void onParseError(const sfz::SourceRange& range, const std::string& message) override
    {
        const auto relativePath = range.start.filePath->lexically_relative(_parser.originalDirectory());
        std::cerr << "Parse error in " << relativePath << " at line " << range.start.lineNumber + 1 << ": " << message << '\n';
    }

    void onParseWarning(const sfz::SourceRange& range, const std::string& message) override
    {
        const auto relativePath = range.start.filePath->lexically_relative(_parser.originalDirectory());
        std::cerr << "Parse warning in " << relativePath << " at line " << range.start.lineNumber + 1 << ": " << message << '\n';
    }

private:
    sfz::Parser& _parser;
};

int main(int argc, char *argv[])
{
    cxxopts::Options options("sfizz_preprocess", "Preprocess SFZ files");

    options.positional_help("<sfz-file>");

    options.add_options()
        ("D,define", "Add external definition", cxxopts::value<std::vector<std::string>>())
        ("i,input", "Input SFZ file", cxxopts::value<std::string>())
        ("h,help", "Print usage");

    options.parse_positional({"input"});

    std::unique_ptr<cxxopts::ParseResult> resultPtr;
    try {
        resultPtr = absl::make_unique<cxxopts::ParseResult>(options.parse(argc, argv));
    } catch (cxxopts::OptionException& ex) {
        std::cerr << ex.what() << "\n";
        return 1;
    }
    cxxopts::ParseResult& result = *resultPtr;

    if (result.count("help")) {
        std::cerr << options.help() << "\n";
        return 0;
    }

    if (!result.count("input")) {
        std::cerr << "Please indicate the SFZ file path.\n";
        return 1;
    }

    const fs::path sfzFilePath { result["input"].as<std::string>() };

    sfz::Parser parser;
    MyParserListener listener(parser);
    parser.setListener(&listener);

    if (result.count("define")) {
        auto& definitions = result["define"].as<std::vector<std::string>>();
        for (absl::string_view definition : definitions) {
            size_t pos = definition.find('=');
            if (pos == definition.npos) {
                std::cerr << "The definition is malformed, should be key=value.\n";
                return 1;
            }
            absl::string_view key = definition.substr(0, pos);
            absl::string_view val = definition.substr(pos + 1);
            parser.addExternalDefinition(key, val);
        }
    }

    parser.parseFile(sfzFilePath);

    if (parser.getErrorCount() > 0)
        return 1;

    return 0;
}
