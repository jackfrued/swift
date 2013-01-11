//===--- StructLayout.h - Structure layout ----------------------*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2015 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See http://swift.org/LICENSE.txt for license information
// See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
// This file defines some routines that are useful for performing
// structure layout.
//
//===----------------------------------------------------------------------===//

#ifndef SWIFT_IRGEN_STRUCTLAYOUT_H
#define SWIFT_IRGEN_STRUCTLAYOUT_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Twine.h"
#include "IRGen.h"

namespace llvm {
  class StructType;
  class Type;
  class Value;
}

namespace swift {
namespace irgen {
  class Address;
  class IRGenFunction;
  class IRGenModule;
  class TypeInfo;

/// An algorithm for laying out a structure.
enum class LayoutStrategy {
  /// Compute an optimal layout;  there are no constraints at all.
  Optimal,

  /// The 'universal' strategy: all translation units must agree on
  /// the layout.
  Universal
};

/// The kind of object being laid out.
enum class LayoutKind {
  /// A non-heap object does not require a heap header.
  NonHeapObject,

  /// A heap object is destined to be allocated on the heap and must
  /// be emitted with the standard heap header.
  HeapObject,
};

/// An element layout is the layout for a single element of a type.
struct ElementLayout {
  /// A constant value used to record that there is no structure index.
  enum : unsigned { NoStructIndex = unsigned(-1) };

  /// The offset in bytes from the start of the struct.
  Size ByteOffset;

  /// The index of this element in the LLVM struct.
  unsigned StructIndex;

  /// The swift type information for this element.
  const TypeInfo *Type;

  Address project(IRGenFunction &IGF, Address addr,
                  const llvm::Twine &suffix = "") const;
};

/// A class for building a structure layout.
class StructLayoutBuilder {
protected:
  IRGenModule &IGM;
private:
  llvm::SmallVector<llvm::Type*, 8> StructFields;
  Size CurSize = Size(0);
  Alignment CurAlignment = Alignment(1);
  bool IsKnownLayout = true;
public:
  StructLayoutBuilder(IRGenModule &IGM) : IGM(IGM) {}

  /// Add a swift heap header to the layout.  This must be the first
  /// call to the layout.
  void addHeapHeader();

  /// Add a number of fields to the layout.  The field layouts need
  /// only have the TypeInfo set; the rest will be filled out.
  ///
  /// Returns true if the fields may have increased the storage
  /// requirements of the layout.
  bool addFields(llvm::MutableArrayRef<ElementLayout> fields,
                 LayoutStrategy strategy);

  /// Return whether the layout is known to be empty.
  bool empty() const { return IsKnownLayout && CurSize == Size(0); }

  /// Return the current set of fields.
  llvm::ArrayRef<llvm::Type*> getStructFields() const { return StructFields; }

  /// Return whether the structure has a known layout.
  bool hasKnownLayout() const { return IsKnownLayout; }

  /// Return the size of the structure built so far.
  Size getSize() const { return CurSize; }

  /// Return the alignment of the structure built so far.
  Alignment getAlignment() const { return CurAlignment; }

  /// Build the current elements as a new anonymous struct type.
  llvm::StructType *getAsAnonStruct() const;

  /// Build the current elements as a new anonymous struct type.
  void setAsBodyOfStruct(llvm::StructType *type) const;
};

/// A struct layout is the result of laying out a complete structure.
class StructLayout {
  Alignment Align;
  Size TotalSize;
  llvm::Type *Ty;
  llvm::SmallVector<ElementLayout, 8> Elements;

public:
  /// Create a structure layout.
  ///
  /// \param strategy - how much leeway the algorithm has to rearrange
  ///   and combine the storage of fields
  /// \param kind - the kind of layout to perform, including whether the
  ///   layout must include the reference-counting header
  /// \param typeToFill - if present, must be an opaque type whose body
  ///   will be filled with this layout
  StructLayout(IRGenModule &IGM, LayoutKind kind, LayoutStrategy strategy,
               llvm::ArrayRef<const TypeInfo *> fields,
               llvm::StructType *typeToFill = 0);

  /// Create a structure layout from a builder.
  StructLayout(const StructLayoutBuilder &builder,
               llvm::Type *type,
               llvm::ArrayRef<ElementLayout> elements)
    : Align(builder.getAlignment()), TotalSize(builder.getSize()),
      Ty(type), Elements(elements.begin(), elements.end()) {}

  /// Return the element layouts.  This is parallel to the fields
  /// passed in the constructor.
  llvm::ArrayRef<ElementLayout> getElements() const { return Elements; }
  llvm::Type *getType() const { return Ty; }
  Size getSize() const { return TotalSize; }
  Alignment getAlignment() const { return Align; }
  bool empty() const { return TotalSize.isZero(); }

  bool hasStaticLayout() const { return true; }
  llvm::Value *emitSize(IRGenFunction &IGF) const;
  llvm::Value *emitAlign(IRGenFunction &IGF) const;

  /// Bitcast the given pointer to this type.
  Address emitCastTo(IRGenFunction &IGF, llvm::Value *ptr,
                     const llvm::Twine &name = "") const;
};

Size getHeapHeaderSize(IRGenModule &IGM);

void addHeapHeaderToLayout(IRGenModule &IGM, Size &size, Alignment &align,
                           SmallVectorImpl<llvm::Type*> &fieldTypes);

} // end namespace irgen
} // end namespace swift

#endif
