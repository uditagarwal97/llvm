//===- llvm/unittest/Support/PropertySetIO.cpp - Property set I/O tests ---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Support/PropertySetIO.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"

#include "gtest/gtest.h"

using namespace llvm;
using namespace llvm::util;

namespace {

TEST(PropertySet, IntValuesIO) {
  // '1' in '1|20' means 'integer property'
  auto Content = "[Staff/Ages]\n"
                 "person1=1|20\n"
                 "person2=1|25\n"
                 "[Staff/Experience]\n"
                 "person1=1|1\n"
                 "person2=1|2\n"
                 "person3=1|12\n";
  auto MemBuf = MemoryBuffer::getMemBuffer(Content);
  // Parse a property set registry
  auto PropSetsPtr = PropertySetRegistry::read(MemBuf.get());

  if (!PropSetsPtr)
    FAIL() << "PropertySetRegistry::read failed\n";

  std::string Serialized;
  {
    llvm::raw_string_ostream OS(Serialized);
    // Serialize
    PropSetsPtr->get()->write(OS);
  }
  // Check that the original and the serialized version are equal
  ASSERT_EQ(Serialized, Content);
}

TEST(PropertySet, ByteArrayValuesIO) {
  // '2' in '2|...' means 'byte array property', Base64-encoded
  // encodes the following byte arrays:
  //   { 8, 0, 0, 0, 0, 0, 0, 0, 0x1 };
  //   { 40, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF, 0x7F, 0xFF, 0x70 };
  // first 8 bytes are the size in bits (40) of what follows (5 bytes).

  auto Content = "[Opt/Param]\n"
                 "kernel1=2|IAAAAAAAAAQA\n"
                 "kernel2=2|oAAAAAAAAAw///3/wB\n";
  auto MemBuf = MemoryBuffer::getMemBuffer(Content);
  // Parse a property set registry
  auto PropSetsPtr = PropertySetRegistry::read(MemBuf.get());

  if (!PropSetsPtr)
    FAIL() << "PropertySetRegistry::read failed\n";

  std::string Serialized;
  {
    llvm::raw_string_ostream OS(Serialized);
    // Serialize
    PropSetsPtr->get()->write(OS);
  }
  // Check that the original and the serialized version are equal
  ASSERT_EQ(Serialized, Content);
}

TEST(PropertySet, EmptyByteArrayValue) {
  // An empty container has a null data pointer, which must not be passed to
  // memcpy.
  PropertyValue Prop{std::vector<char>{}};
  ASSERT_EQ(Prop.getType(), PropertyValue::BYTE_ARRAY);
  ASSERT_EQ(Prop.getByteArraySize(), 0u);
}

TEST(PropertySet, ByteArrayReassignment) {
  // Assigning over a byte array used to leak the old allocation, and
  // self-assignment used to clear the value. Aliases are used to keep clang
  // from diagnosing the self-assignments.
  PropertyValue A{std::vector<char>{'a', 'b', 'c'}};
  PropertyValue B{std::vector<char>{'d', 'e'}};
  PropertyValue &AliasA = A;

  A = B;
  ASSERT_EQ(A.getByteArraySize(), 2u);
  ASSERT_EQ(A.asByteArray()[0], 'd');

  A = AliasA;
  ASSERT_EQ(A.getByteArraySize(), 2u);
  ASSERT_EQ(A.asByteArray()[1], 'e');

  A = std::move(B);
  ASSERT_EQ(A.getByteArraySize(), 2u);

  A = std::move(AliasA);
  ASSERT_EQ(A.getType(), PropertyValue::BYTE_ARRAY);
  ASSERT_EQ(A.getByteArraySize(), 2u);
}

TEST(PropertySet, ByteArrayRemove) {
  // remove() shifts the trailing properties down with move assignment, which
  // used to leak the removed slot's byte array.
  auto Content = "[Opt/Param]\n"
                 "kernel1=2|IAAAAAAAAAQA\n"
                 "kernel2=2|oAAAAAAAAAw///3/wB\n";
  auto MemBuf = MemoryBuffer::getMemBuffer(Content);
  auto PropSetsPtr = PropertySetRegistry::read(MemBuf.get());

  if (!PropSetsPtr)
    FAIL() << "PropertySetRegistry::read failed\n";

  PropSetsPtr->get()->remove("Opt/Param", "kernel1");

  std::string Serialized;
  {
    llvm::raw_string_ostream OS(Serialized);
    PropSetsPtr->get()->write(OS);
  }
  ASSERT_EQ(Serialized, "[Opt/Param]\nkernel2=2|oAAAAAAAAAw///3/wB\n");
}
} // namespace
