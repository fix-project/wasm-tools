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
#include <unordered_set>

#include "wabt/apply-names.h"
#include "wabt/binary-reader-ir.h"
#include "wabt/binary-reader.h"
#include "wabt/binary-writer.h"
#include "wabt/error-formatter.h"
#include "wabt/feature.h"
#include "wabt/generate-names.h"
#include "wabt/ir.h"
#include "wabt/option-parser.h"
#include "wabt/resolve-names.h"
#include "wabt/stream.h"
#include "wabt/validator.h"
#include "wabt/wast-lexer.h"

using namespace wabt;

static int s_verbose;
static std::string s_infile;
static std::string s_outfile;
static Features s_features;
static bool s_resolve_names = true;
static bool s_read_debug_names = true;
static bool s_fail_on_custom_section_error = true;
static std::unique_ptr<FileStream> s_log_stream;
static bool s_validate = true;
static WriteBinaryOptions s_write_binary_options;
static std::unordered_set<std::string> s_allowed_exports;
static std::unordered_set<std::string> s_not_allowed_exports;

static const char s_description[] = "XXX TBD";

static void ParseOptions( int argc, char** argv )
{
  OptionParser parser( "export-audit", s_description );

  parser.AddOption( 'v', "verbose", "Use multiple times for more info", []() {
    s_verbose++;
    s_log_stream = FileStream::CreateStderr();
  } );
  s_features.AddOptions( &parser );

  parser.AddOption( 'e',
                    "export",
                    "allowed export name",
                    "Name of an export that will be retained in the output file",
                    []( const char* argument ) { s_allowed_exports.insert( argument ); } );

  parser.AddOption( 'r',
                    "remove-export",
                    "not allowed export name",
                    "Name of an export that will be removed in the output file",
                    []( const char* argument ) { s_not_allowed_exports.insert( argument ); } );

  parser.AddOption(
    "no-debug-names", "Ignore debug names in the binary file", []() { s_read_debug_names = false; } );
  parser.AddOption( "no-resolve-names", "Do not resolve names to index", []() { s_resolve_names = false; } );
  parser.AddOption( "ignore-custom-section-errors", "Ignore errors in custom sections", []() {
    s_fail_on_custom_section_error = false;
  } );
  parser.AddOption( "no-check", "Don't check for invalid modules", []() { s_validate = false; } );
  parser.AddArgument( "filename", OptionParser::ArgumentCount::One, []( const char* argument ) {
    s_infile = argument;
    ConvertBackslashToSlash( &s_infile );
  } );
  parser.AddArgument( "output", OptionParser::ArgumentCount::One, []( const char* argument ) {
    s_outfile = argument;
    ConvertBackslashToSlash( &s_outfile );
  } );
  parser.Parse( argc, argv );
}

std::string StripWasm( const std::string& file_name )
{
  return file_name.substr( 0, file_name.length() - 5 );
}

int ProgramMain( int argc, char** argv )
{
  Result result;

  InitStdio();
  ParseOptions( argc, argv );
  
  if ( s_allowed_exports.size() > 0 && s_not_allowed_exports.size() > 0 ) {
    std::cerr << "Specifying -e and -r at the same time\n";
    return 1;
  }

  std::vector<uint8_t> file_data;
  result = ReadFile( s_infile.c_str(), &file_data );

  if ( Succeeded( result ) ) {
    Errors errors;
    Module module;
    const bool kStopOnFirstError = true;
    ReadBinaryOptions options(
      s_features, s_log_stream.get(), s_read_debug_names, kStopOnFirstError, s_fail_on_custom_section_error );
    result = ReadBinaryIr( s_infile.c_str(), file_data.data(), file_data.size(), options, &errors, &module );
    if ( Succeeded( result ) ) {
      if ( s_validate ) {
        ValidateOptions options( s_features );
        result = ValidateModule( &module, &errors, options );
      }

      for ( auto it = module.exports.begin(); it != module.exports.end(); ) {
        std::cerr << "found export \"" << ( *it )->name << "\" ";

        const auto& name = ( *it )->name;
        
        if ( s_allowed_exports.size() > 0 ) {
          if ( s_allowed_exports.count( name ) ) {
            std::cerr << "\n";
            ++it;
          } else {
            std::cerr << "(suppressing)\n";
            it = module.exports.erase( it );
          }
        } else if ( s_not_allowed_exports.size() > 0 ) {
          if ( s_not_allowed_exports.count( name ) ) {
            std::cerr << "(suppressing)\n";
            it = module.exports.erase( it );
          } else {
            std::cerr << "\n";
            ++it;
          }
        }
      }

      MemoryStream memory_stream;
      if ( Succeeded( result ) ) {
        // have to serialize to MemoryStream because FileStream::Truncate is not implemented
        s_write_binary_options.features = s_features;
        result = WriteBinaryModule( &memory_stream, &module, s_write_binary_options );
      }

      if ( Succeeded( result ) ) {
        memory_stream.WriteToFile( s_outfile );
      }
    }
    FormatErrorsToFile( errors, Location::Type::Binary );
  }
  return result != Result::Ok;
}

int main( int argc, char** argv )
{
  WABT_TRY
  return ProgramMain( argc, argv );
  WABT_CATCH_BAD_ALLOC_AND_EXIT
}
