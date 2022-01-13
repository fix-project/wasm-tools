/*
 * Copyright 2016 WebAssembly Community Group participants
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cassert>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <iostream>

#include "src/apply-names.h"
#include "src/binary-reader.h"
#include "src/binary-reader-ir.h"
#include "src/error-formatter.h"
#include "src/feature.h"
#include "src/generate-names.h"
#include "src/ir.h"
#include "src/option-parser.h"
#include "src/resolve-names.h"
#include "src/stream.h"
#include "src/validator.h"
#include "src/wast-lexer.h"
#include "src/wat-writer.h"

#include "combine-modules.h"
#include "generate-prefix-names.h"
#include "rebase-index.h"
#include "resolve-imports.h"

using namespace wabt;

static int s_verbose;
static std::string s_infile;
static std::string s_lib_infile;
static std::string s_outfile;
static Features s_features;
static WriteWatOptions s_write_wat_options;
static bool s_resolve_names = true;
static bool s_read_debug_names = true;
static bool s_fail_on_custom_section_error = true;
static std::unique_ptr<FileStream> s_log_stream;
static bool s_validate = true;

static const char s_description[] =
R"(  Read two files in the WebAssembly binary format, and convert it to
  the WebAssembly binary format.

examples:
  # parse binary file test.wasm and lib file lib.wasm write binary file output.wasm
  $ wasmlink test.wasm lib.wasm -o output.wasm
)";

static void ParseOptions(int argc, char** argv) {
  OptionParser parser("wasmlink", s_description);

  parser.AddOption('v', "verbose", "Use multiple times for more info", []() {
    s_verbose++;
    s_log_stream = FileStream::CreateStderr();
  });
  parser.AddOption(
      'o', "output", "FILENAME",
      "Output file for the generated wast file, by default use stdout",
      [](const char* argument) {
        s_outfile = argument;
        ConvertBackslashToSlash(&s_outfile);
      });
  parser.AddOption('f', "fold-exprs", "Write folded expressions where possible",
                   []() { s_write_wat_options.fold_exprs = true; });
  s_features.AddOptions(&parser);
  parser.AddOption("inline-exports", "Write all exports inline",
                   []() { s_write_wat_options.inline_export = true; });
  parser.AddOption("inline-imports", "Write all imports inline",
                   []() { s_write_wat_options.inline_import = true; });
  parser.AddOption("no-debug-names", "Ignore debug names in the binary file",
                   []() { s_read_debug_names = false; });
  parser.AddOption("no-resolve-names", "Do not resolve names to index",
                   []() { s_resolve_names = false; });
  parser.AddOption("ignore-custom-section-errors",
                   "Ignore errors in custom sections",
                   []() { s_fail_on_custom_section_error = false; });
  parser.AddOption("no-check", "Don't check for invalid modules",
                   []() { s_validate = false; });
  parser.AddArgument("filename", OptionParser::ArgumentCount::One,
                     [](const char* argument) {
                       s_infile = argument;
                       ConvertBackslashToSlash(&s_infile);
                     });
  parser.AddArgument("lib_filename", OptionParser::ArgumentCount::One,
                     [](const char* argument) {
                       s_lib_infile = argument;
                       ConvertBackslashToSlash(&s_lib_infile);
                     });
  parser.Parse(argc, argv);
}

std::string StripWasm(const std::string& file_name) {
  return file_name.substr(0, file_name.length() - 5);
}

int ProgramMain(int argc, char** argv) {
  Result result;
  Result result_lib;

  InitStdio();
  ParseOptions(argc, argv);

  std::vector<uint8_t> file_data;
  std::vector<uint8_t> lib_file_data;
  result = ReadFile(s_infile.c_str(), &file_data);
  result_lib = ReadFile(s_lib_infile.c_str(), &lib_file_data);
  if (Succeeded(result) && Succeeded(result_lib)) {
    Errors errors;
    Module module;
    Module libmodule;
    Module output;
    const bool kStopOnFirstError = true;
    ReadBinaryOptions options(s_features, s_log_stream.get(),
                              s_read_debug_names, kStopOnFirstError,
                              s_fail_on_custom_section_error);
    result = ReadBinaryIr(s_infile.c_str(), file_data.data(), file_data.size(),
                          options, &errors, &module);
    result_lib = ReadBinaryIr(s_lib_infile.c_str(), lib_file_data.data(), lib_file_data.size(),
                              options, &errors, &libmodule);
    module.name = StripWasm(s_infile);
    libmodule.name = StripWasm(s_lib_infile);
    if (Succeeded(result) && Succeeded(result_lib)) {
      if (s_validate) {
        ValidateOptions options(s_features);
        result = ValidateModule(&module, &errors, options);
        result_lib = ValidateModule(&libmodule, &errors, options);
      }

      if (Succeeded(result) && Succeeded(result_lib)) {
        result = GeneratePrefixNames(&module);
        result_lib = GeneratePrefixNames(&libmodule);
      }

      if (Succeeded(result) && Succeeded(result_lib)) {
        result = ApplyNames(&module);
        result_lib = ApplyNames(&libmodule);
      }

      if (Succeeded(result) && Succeeded(result_lib)) {
        std::unordered_map<std::string, std::string> import_map;
        result = ResolveImports(&module, &libmodule, &import_map);
      }

      if (Succeeded(result)) {
        result = CombineModules(&module, &libmodule, &output);
      }
      
      if (Succeeded(result) && s_resolve_names) {
        result = ResolveNamesModule(&output, &errors);
      }
  
      if (Succeeded(result)) {
        FileStream stream(!s_outfile.empty() ? FileStream(s_outfile)
            : FileStream(stdout));
        result = WriteWat(&stream, &output, s_write_wat_options);
      }
    }
    FormatErrorsToFile(errors, Location::Type::Binary);
  }
  return result != Result::Ok || result_lib != Result::Ok;
}

int main(int argc, char** argv) {
  WABT_TRY
  return ProgramMain(argc, argv);
  WABT_CATCH_BAD_ALLOC_AND_EXIT
}
