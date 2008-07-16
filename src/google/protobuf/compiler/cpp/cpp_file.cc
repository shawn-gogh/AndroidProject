// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.
// http://code.google.com/p/protobuf/
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Author: kenton@google.com (Kenton Varda)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.

#include <google/protobuf/compiler/cpp/cpp_file.h>
#include <google/protobuf/compiler/cpp/cpp_enum.h>
#include <google/protobuf/compiler/cpp/cpp_service.h>
#include <google/protobuf/compiler/cpp/cpp_extension.h>
#include <google/protobuf/compiler/cpp/cpp_helpers.h>
#include <google/protobuf/compiler/cpp/cpp_message.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/stubs/strutil.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace cpp {

// ===================================================================

FileGenerator::FileGenerator(const FileDescriptor* file,
                             const string& dllexport_decl)
  : file_(file),
    message_generators_(
      new scoped_ptr<MessageGenerator>[file->message_type_count()]),
    enum_generators_(
      new scoped_ptr<EnumGenerator>[file->enum_type_count()]),
    service_generators_(
      new scoped_ptr<ServiceGenerator>[file->service_count()]),
    extension_generators_(
      new scoped_ptr<ExtensionGenerator>[file->extension_count()]) {

  for (int i = 0; i < file->message_type_count(); i++) {
    message_generators_[i].reset(
      new MessageGenerator(file->message_type(i), dllexport_decl));
  }

  for (int i = 0; i < file->enum_type_count(); i++) {
    enum_generators_[i].reset(
      new EnumGenerator(file->enum_type(i), dllexport_decl));
  }

  for (int i = 0; i < file->service_count(); i++) {
    service_generators_[i].reset(
      new ServiceGenerator(file->service(i), dllexport_decl));
  }

  for (int i = 0; i < file->extension_count(); i++) {
    extension_generators_[i].reset(
      new ExtensionGenerator(file->extension(i), dllexport_decl));
  }

  SplitStringUsing(file_->package(), ".", &package_parts_);
}

FileGenerator::~FileGenerator() {}

void FileGenerator::GenerateHeader(io::Printer* printer) {
  string filename_identifier = FilenameIdentifier(file_->name());

  // Generate top of header.
  printer->Print(
    "// Generated by the protocol buffer compiler.  DO NOT EDIT!\n"
    "\n"
    "#ifndef PROTOBUF_$filename_identifier$__INCLUDED\n"
    "#define PROTOBUF_$filename_identifier$__INCLUDED\n"
    "\n"
    "#include <string>\n"
    "\n",
    "filename_identifier", filename_identifier);

  printer->Print(
    "#include <google/protobuf/stubs/common.h>\n"
    "\n");

  // Verify the protobuf library header version is compatible with the protoc
  // version before going any further.
  printer->Print(
    "#if GOOGLE_PROTOBUF_VERSION < $min_header_version$\n"
    "#error This file was generated by a newer version of protoc which is\n"
    "#error incompatible with your Protocol Buffer headers.  Please update\n"
    "#error your headers.\n"
    "#endif\n"
    "#if $protoc_version$ < GOOGLE_PROTOBUF_MIN_PROTOC_VERSION\n"
    "#error This file was generated by an older version of protoc which is\n"
    "#error incompatible with your Protocol Buffer headers.  Please\n"
    "#error regenerate this file with a newer version of protoc.\n"
    "#endif\n"
    "\n",
    "min_header_version",
      SimpleItoa(protobuf::internal::kMinHeaderVersionForProtoc),
    "protoc_version", SimpleItoa(GOOGLE_PROTOBUF_VERSION));

  // OK, it's now safe to #include other files.
  printer->Print(
    "#include <google/protobuf/generated_message_reflection.h>\n"
    "#include <google/protobuf/repeated_field.h>\n"
    "#include <google/protobuf/extension_set.h>\n");

  if (file_->service_count() > 0) {
    printer->Print(
      "#include <google/protobuf/service.h>\n");
  }

  for (int i = 0; i < file_->dependency_count(); i++) {
    printer->Print(
      "#include \"$dependency$.pb.h\"\n",
      "dependency", StripProto(file_->dependency(i)->name()));
  }

  // Open namespace.
  GenerateNamespaceOpeners(printer);

  printer->Print("\n");

  // Generate forward declarations of classes.
  for (int i = 0; i < file_->message_type_count(); i++) {
    message_generators_[i]->GenerateForwardDeclaration(printer);
  }

  printer->Print("\n");

  // Generate enum definitions.
  for (int i = 0; i < file_->message_type_count(); i++) {
    message_generators_[i]->GenerateEnumDefinitions(printer);
  }
  for (int i = 0; i < file_->enum_type_count(); i++) {
    enum_generators_[i]->GenerateDefinition(printer);
  }

  printer->Print(kThickSeparator);
  printer->Print("\n");

  // Generate class definitions.
  for (int i = 0; i < file_->message_type_count(); i++) {
    if (i > 0) {
      printer->Print("\n");
      printer->Print(kThinSeparator);
      printer->Print("\n");
    }
    message_generators_[i]->GenerateClassDefinition(printer);
  }

  printer->Print("\n");
  printer->Print(kThickSeparator);
  printer->Print("\n");

  // Generate service definitions.
  for (int i = 0; i < file_->service_count(); i++) {
    if (i > 0) {
      printer->Print("\n");
      printer->Print(kThinSeparator);
      printer->Print("\n");
    }
    service_generators_[i]->GenerateDeclarations(printer);
  }

  printer->Print("\n");
  printer->Print(kThickSeparator);
  printer->Print("\n");

  // Declare extension identifiers.
  for (int i = 0; i < file_->extension_count(); i++) {
    extension_generators_[i]->GenerateDeclaration(printer);
  }

  printer->Print("\n");
  printer->Print(kThickSeparator);
  printer->Print("\n");

  // Generate class inline methods.
  for (int i = 0; i < file_->message_type_count(); i++) {
    if (i > 0) {
      printer->Print(kThinSeparator);
      printer->Print("\n");
    }
    message_generators_[i]->GenerateInlineMethods(printer);
  }

  // Close up namespace.
  GenerateNamespaceClosers(printer);

  printer->Print(
    "#endif  // PROTOBUF_$filename_identifier$__INCLUDED\n",
    "filename_identifier", filename_identifier);
}

void FileGenerator::GenerateSource(io::Printer* printer) {
  printer->Print(
    "// Generated by the protocol buffer compiler.  DO NOT EDIT!\n"
    "\n"
    "#include \"$basename$.pb.h\"\n"
    "#include <google/protobuf/descriptor.h>\n"
    "#include <google/protobuf/io/coded_stream.h>\n"
    "#include <google/protobuf/reflection_ops.h>\n"
    "#include <google/protobuf/wire_format_inl.h>\n",
    "basename", StripProto(file_->name()));

  // For each dependency, write a prototype for that dependency's
  // BuildDescriptors() function.  We don't expose these in the header because
  // they are internal implementation details, and since this is generated code
  // we don't have the usual risks involved with declaring external functions
  // within a .cc file.
  for (int i = 0; i < file_->dependency_count(); i++) {
    const FileDescriptor* dependency = file_->dependency(i);
    // Open the dependency's namespace.
    vector<string> dependency_package_parts;
    SplitStringUsing(dependency->package(), ".", &dependency_package_parts);
    for (int i = 0; i < dependency_package_parts.size(); i++) {
      printer->Print("namespace $name$ { ",
                     "name", dependency_package_parts[i]);
    }
    // Declare its BuildDescriptors() function.
    printer->Print(
      "void $function$();",
      "function", GlobalBuildDescriptorsName(dependency->name()));
    // Close the namespace.
    for (int i = 0; i < dependency_package_parts.size(); i++) {
      printer->Print(" }");
    }
    printer->Print("\n");
  }

  GenerateNamespaceOpeners(printer);

  printer->Print(
    "\n"
    "namespace {\n"
    "\n");
  for (int i = 0; i < file_->message_type_count(); i++) {
    message_generators_[i]->GenerateDescriptorDeclarations(printer);
  }
  for (int i = 0; i < file_->enum_type_count(); i++) {
    printer->Print(
      "const ::google::protobuf::EnumDescriptor* $name$_descriptor_ = NULL;\n",
      "name", ClassName(file_->enum_type(i), false));
  }
  for (int i = 0; i < file_->service_count(); i++) {
    printer->Print(
      "const ::google::protobuf::ServiceDescriptor* $name$_descriptor_ = NULL;\n",
      "name", file_->service(i)->name());
  }

  printer->Print(
    "\n"
    "}  // namespace\n"
    "\n");

  // Define our externally-visible BuildDescriptors() function.
  GenerateBuildDescriptors(printer);

  // Generate enums.
  for (int i = 0; i < file_->enum_type_count(); i++) {
    enum_generators_[i]->GenerateMethods(printer);
  }

  // Generate classes.
  for (int i = 0; i < file_->message_type_count(); i++) {
    printer->Print("\n");
    printer->Print(kThickSeparator);
    printer->Print("\n");
    message_generators_[i]->GenerateClassMethods(printer);
  }

  // Generate services.
  for (int i = 0; i < file_->service_count(); i++) {
    if (i == 0) printer->Print("\n");
    printer->Print(kThickSeparator);
    printer->Print("\n");
    service_generators_[i]->GenerateImplementation(printer);
  }

  // Define extensions.
  for (int i = 0; i < file_->extension_count(); i++) {
    extension_generators_[i]->GenerateDefinition(printer);
  }

  GenerateNamespaceClosers(printer);
}

void FileGenerator::GenerateBuildDescriptors(io::Printer* printer) {
  // BuildDescriptors() is a file-level procedure which initializes all of
  // the Descriptor objects for this file.  It runs the first time one of the
  // descriptors is accessed.  This will always be at static initialization
  // time, because every message has a statically-initialized default instance,
  // and the constructor for a message class accesses its descriptor.  See the
  // constructor and the descriptor() method of message classes.
  printer->Print(
    "\n"
    "void $builddescriptorsname$() {\n"
    "  static bool already_here = false;\n"
    "  if (already_here) return;\n"
    "  already_here = true;\n"
    "  GOOGLE_PROTOBUF_VERIFY_VERSION;\n"
    "  ::google::protobuf::DescriptorPool* pool =\n"
    "    ::google::protobuf::DescriptorPool::internal_generated_pool();\n"
    "\n",
    "builddescriptorsname", GlobalBuildDescriptorsName(file_->name()));
  printer->Indent();

  // Call the BuildDescriptors() methods for all of our dependencies, to make
  // sure they get initialized first.
  for (int i = 0; i < file_->dependency_count(); i++) {
    const FileDescriptor* dependency = file_->dependency(i);
    // Print the namespace prefix for the dependency.
    vector<string> dependency_package_parts;
    SplitStringUsing(dependency->package(), ".", &dependency_package_parts);
    printer->Print("::");
    for (int i = 0; i < dependency_package_parts.size(); i++) {
      printer->Print("$name$::",
                     "name", dependency_package_parts[i]);
    }
    // Call its BuildDescriptors function.
    printer->Print(
      "$name$();\n",
      "name", GlobalBuildDescriptorsName(dependency->name()));
  }

  // Embed the descriptor.  We simply serialize the entire FileDescriptorProto
  // and embed it as a string literal, which is parsed and built into real
  // descriptors at initialization time.
  FileDescriptorProto file_proto;
  file_->CopyTo(&file_proto);
  string file_data;
  file_proto.SerializeToString(&file_data);

  printer->Print(
    "const ::google::protobuf::FileDescriptor* file = pool->InternalBuildGeneratedFile(");

  // Only write 40 bytes per line.
  static const int kBytesPerLine = 40;
  for (int i = 0; i < file_data.size(); i += kBytesPerLine) {
    printer->Print("\n  \"$data$\"",
      "data", CEscape(file_data.substr(i, kBytesPerLine)));
  }
  printer->Print(
    ", $size$);\n",
    "size", SimpleItoa(file_data.size()));

  // Assign all global descriptors.
  for (int i = 0; i < file_->message_type_count(); i++) {
    message_generators_[i]->GenerateDescriptorInitializer(printer, i);
  }
  for (int i = 0; i < file_->enum_type_count(); i++) {
    enum_generators_[i]->GenerateDescriptorInitializer(printer, i);
  }
  for (int i = 0; i < file_->service_count(); i++) {
    service_generators_[i]->GenerateDescriptorInitializer(printer, i);
  }

  printer->Outdent();
  printer->Print(
    "}\n"
    "\n"
    "// Force BuildDescriptors() to be called at static initialization time.\n"
    "struct StaticDescriptorInitializer_$filename$ {\n"
    "  StaticDescriptorInitializer_$filename$() {\n"
    "    $builddescriptorsname$();\n"
    "  }\n"
    "} static_descriptor_initializer_$filename$_;\n"
    "\n",
    "builddescriptorsname", GlobalBuildDescriptorsName(file_->name()),
    "filename", FilenameIdentifier(file_->name()));
}

void FileGenerator::GenerateNamespaceOpeners(io::Printer* printer) {
  if (package_parts_.size() > 0) printer->Print("\n");

  for (int i = 0; i < package_parts_.size(); i++) {
    printer->Print("namespace $part$ {\n",
                   "part", package_parts_[i]);
  }
}

void FileGenerator::GenerateNamespaceClosers(io::Printer* printer) {
  if (package_parts_.size() > 0) printer->Print("\n");

  for (int i = package_parts_.size() - 1; i >= 0; i--) {
    printer->Print("}  // namespace $part$\n",
                   "part", package_parts_[i]);
  }
}

}  // namespace cpp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google