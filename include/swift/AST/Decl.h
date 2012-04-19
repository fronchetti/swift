//===--- Decl.h - Swift Language Declaration ASTs ---------------*- C++ -*-===//
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
// This file defines the Decl class and subclasses.
//
//===----------------------------------------------------------------------===//

#ifndef SWIFT_DECL_H
#define SWIFT_DECL_H

#include "swift/AST/DeclContext.h"
#include "swift/AST/Identifier.h"
#include "swift/AST/Type.h"
#include "swift/Basic/SourceLoc.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/PointerUnion.h"
#include <cstddef>

namespace swift {
  class ASTContext;
  class ASTWalker;
  class Type;
  class Expr;
  class FuncDecl;
  class FuncExpr;
  class BraceStmt;
  class Component;
  class DeclAttributes;
  class OneOfElementDecl;
  class NameAliasType;
  class Pattern;
  enum class Resilience : unsigned char;
  class TypeAliasDecl;
  class Stmt;
  
enum class DeclKind {
#define DECL(Id, Parent) Id,
#define DECL_RANGE(Id, FirstId, LastId) \
  First_##Id##Decl = FirstId, Last_##Id##Decl = LastId,
#include "swift/AST/DeclNodes.def"
};

/// Decl - Base class for all declarations in Swift.
class Decl {
  class DeclBitfields {
    friend class Decl;
    unsigned Kind : 8;
  };
  enum { NumDeclBits = 8 };
  static_assert(NumDeclBits <= 32, "fits in an unsigned");

  enum { NumNamedDeclBits = NumDeclBits };
  static_assert(NumNamedDeclBits <= 32, "fits in an unsigned");

  class ValueDeclBitfields {
    friend class ValueDecl;
    unsigned : NumNamedDeclBits;

    // The following flags are not necessarily meaningful for all
    // kinds of value-declarations.

    // NeverUsedAsLValue - Whether this decl is ever used as an lvalue 
    // (i.e. used in a context where it could be modified).
    unsigned NeverUsedAsLValue : 1;

    // HasFixedLifetime - Whether the lifetime of this decl matches its
    // scope (i.e. the decl isn't captured, so it can be allocated as part of
    // the stack frame.)
    unsigned HasFixedLifetime : 1;
  };
  enum { NumValueDeclBits = NumNamedDeclBits + 2 };
  static_assert(NumValueDeclBits <= 32, "fits in an unsigned");

protected:
  union {
    DeclBitfields DeclBits;
    ValueDeclBitfields ValueDeclBits;
  };

private:
  DeclContext *Context;

  Decl(const Decl&) = delete;
  void operator=(const Decl&) = delete;

protected:
  Decl(DeclKind kind, DeclContext *DC) : Context(DC) {
    DeclBits.Kind = unsigned(kind);
  }

public:
  /// Alignment - The required alignment of Decl objects.
  enum { Alignment = 8 };

  DeclKind getKind() const { return DeclKind(DeclBits.Kind); }

  DeclContext *getDeclContext() const { return Context; }
  void setDeclContext(DeclContext *DC) { Context = DC; }

  /// getASTContext - Return the ASTContext that this decl lives in.
  ASTContext &getASTContext() const {
    assert(Context && "Decl doesn't have an assigned context");
    return Context->getASTContext();
  }
  
  SourceLoc getLocStart() const;
 
  void dump() const;
  void print(raw_ostream &OS, unsigned Indent = 0) const;

  bool walk(ASTWalker &walker);

  // Implement isa/cast/dyncast/etc.
  static bool classof(const Decl *) { return true; }
  
  // Make vanilla new/delete illegal for Decls.
  void *operator new(size_t Bytes) = delete;
  void operator delete(void *Data) = delete;

  // Only allow allocation of Decls using the allocator in ASTContext
  // or by doing a placement new.
  void *operator new(size_t Bytes, ASTContext &C,
                     unsigned Alignment = Decl::Alignment);
  void *operator new(size_t Bytes, void *Mem) { 
    assert(Mem); 
    return Mem; 
  }
};

/// ImportDecl - This represents a single import declaration, e.g.:
///   import swift
///   import swift.int
class ImportDecl : public Decl {
public:
  typedef std::pair<Identifier, SourceLoc> AccessPathElement;

private:
  SourceLoc ImportLoc;

  /// The number of elements in this path.
  unsigned NumPathElements;

  AccessPathElement *getPathBuffer() {
    return reinterpret_cast<AccessPathElement*>(this+1);
  }
  const AccessPathElement *getPathBuffer() const {
    return reinterpret_cast<const AccessPathElement*>(this+1);
  }
  
  ImportDecl(DeclContext *DC, SourceLoc ImportLoc,
             ArrayRef<AccessPathElement> Path);

public:
  static ImportDecl *create(ASTContext &C, DeclContext *DC,
                            SourceLoc ImportLoc,
                            ArrayRef<AccessPathElement> Path);

  ArrayRef<AccessPathElement> getAccessPath() const {
    return ArrayRef<AccessPathElement>(getPathBuffer(), NumPathElements);
  }
  
  SourceLoc getLocStart() const { return ImportLoc; }

  // Implement isa/cast/dyncast/etc.
  static bool classof(const Decl *D) {
    return D->getKind() == DeclKind::Import;
  }
  static bool classof(const ImportDecl *D) { return true; }
};

/// ExtensionDecl - This represents a type extension containing methods
/// associated with the type.  This is not a ValueDecl and has no Type because
/// there are no runtime values of the Extension's type.  
class ExtensionDecl : public Decl, public DeclContext {
  SourceLoc ExtensionLoc;  // Location of 'extension' keyword.
  
  /// ExtendedType - The type being extended.
  Type ExtendedType;
  ArrayRef<Decl*> Members;
public:

  ExtensionDecl(SourceLoc ExtensionLoc, Type ExtendedType,
                ArrayRef<Decl*> Members, DeclContext *Parent)
    : Decl(DeclKind::Extension, Parent),
      DeclContext(DeclContextKind::ExtensionDecl, Parent),
      ExtensionLoc(ExtensionLoc),
      ExtendedType(ExtendedType), Members(Members) {
  }
  
  SourceLoc getExtensionLoc() const { return ExtensionLoc; }
  SourceLoc getLocStart() const { return ExtensionLoc; }
  Type getExtendedType() const { return ExtendedType; }
  ArrayRef<Decl*> getMembers() const { return Members; }

  // Implement isa/cast/dyncast/etc.
  static bool classof(const Decl *D) {
    return D->getKind() == DeclKind::Extension;
  }
  static bool classof(const DeclContext *C) {
    return C->getContextKind() == DeclContextKind::ExtensionDecl;
  }
  static bool classof(const ExtensionDecl *D) { return true; }
};

// PatternBindingDecl - This decl contains a pattern and optional initializer
// for a set of one or more VarDecls declared together.  (For example, in
// "var (a,b) = foo()", this contains the pattern "(a,b)" and the intializer
// "foo()".  The same applies to simpler declarations like "var a = foo()".)
class PatternBindingDecl : public Decl {
  SourceLoc VarLoc; // Location of the 'var' keyword
  Pattern *Pat; // The pattern which this decl binds
  Expr *Init; // Initializer for the variables

  friend class Decl;
  
public:
  PatternBindingDecl(SourceLoc VarLoc, Pattern *Pat, Expr *E,
                     DeclContext *Parent)
    : Decl(DeclKind::PatternBinding, Parent), VarLoc(VarLoc), Pat(Pat),
      Init(E) {
  }

  SourceLoc getVarLoc() const { return VarLoc; }
  SourceLoc getLocStart() const { return VarLoc; }

  Pattern *getPattern() const { return Pat; }

  Expr *getInit() const { return Init; }
  void setInit(Expr *E) { Init = E; }

  static bool classof(const Decl *D) {
    return D->getKind() == DeclKind::PatternBinding;
  }
  static bool classof(const PatternBindingDecl *D) { return true; }

};

/// TopLevelCodeDecl - This decl is used as a container for top-level
/// expressions and statements in the main module.  It is always a direct
/// child of the body of a TranslationUnit.  The primary reason for
/// building these is to give top-level statements a DeclContext which is
/// distinct from the TranslationUnit itself.  This, among other things,
/// makes it easier to distinguish between local top-level variables (which
/// are not live past the end of the statement) and global variables.
class TopLevelCodeDecl : public Decl, public DeclContext {
public:
  typedef llvm::PointerUnion<Expr*, Stmt*> ExprOrStmt;

private:
  ExprOrStmt Body;

public:
  TopLevelCodeDecl(DeclContext *Parent)
    : Decl(DeclKind::TopLevelCode, Parent),
      DeclContext(DeclContextKind::TopLevelCodeDecl, Parent) {}

  ExprOrStmt getBody() { return Body; }
  void setBody(Expr *E) { Body = E; }
  void setBody(Stmt *S) { Body = S; }

  static bool classof(const Decl *D) {
    return D->getKind() == DeclKind::TopLevelCode;
  }
  static bool classof(const DeclContext *C) {
    return C->getContextKind() == DeclContextKind::TopLevelCodeDecl;
  }
  static bool classof(const TopLevelCodeDecl *D) { return true; }
};

/// SubscriptDecl - Declares a subscripting operator for a type.
///
/// A subscript declaration is defined as a get/set pair that produces a
/// specific type. For example:
///
/// \code
/// subscript (i : Int) -> String {
///   get { /* return ith String */ }
///   set { /* set ith string to value */ }
/// }
/// \endcode
///
/// A type with a subscript declaration can be used as the base of a subscript
/// expression a[i], where a is of the subscriptable type and i is the type
/// of the index. A subscript can have multiple indices:
///
/// struct Matrix {
///   subscript (i : Int, j : Int) -> Double {
///     get { /* return element at position (i, j) */ }
///     set { /* set element at position (i, j) */ }
///   }
/// }
///
/// A given type can have multiple subscript declarations, so long as the
/// signatures (indices and element type) are distinct.
class SubscriptDecl : public Decl {
  SourceLoc SubscriptLoc;
  SourceLoc ArrowLoc;
  Pattern *Indices;
  Type ElementTy;
  SourceRange Braces;
  FuncDecl *Get;
  FuncDecl *Set;
  
public:
  SubscriptDecl(SourceLoc SubscriptLoc, Pattern *Indices, SourceLoc ArrowLoc,
                Type ElementTy, SourceRange Braces, FuncDecl *Get,
                FuncDecl *Set, DeclContext *Parent)
    : Decl(DeclKind::Subscript, Parent), SubscriptLoc(SubscriptLoc),
      ArrowLoc(ArrowLoc), Indices(Indices), ElementTy(ElementTy),
      Braces(Braces), Get(Get), Set(Set) { }
  
  SourceLoc getLocStart() const { return SubscriptLoc; }
  
  /// \brief Retrieve the indices for this subscript operation.
  Pattern *getIndices() const { return Indices; }
  
  /// \brief Retrieve the type of the element referenced by a subscript
  /// operation.
  Type getElementType() const { return ElementTy; }
  
  /// \brief Retrieve the subscript getter, a function that takes the indices
  /// and produces a value of the element type.
  FuncDecl *getGetter() const { return Get; }
  
  /// \brief Retrieve the subscript setter, a function that takes the indices
  /// and a new value of the lement type and updates the corresponding value.
  ///
  /// The subscript setter is optional.
  FuncDecl *getSetter() const { return Set; }
  
  static bool classof(const Decl *D) {
    return D->getKind() == DeclKind::Subscript;
  }
  
  static bool classof(const SubscriptDecl *D) { return true; }
};

/// NamedDecl - An abstract base class for declarations with names.
class NamedDecl : public Decl {
  Identifier Name;
  const DeclAttributes *Attrs;
  static const DeclAttributes EmptyAttrs;

protected:
  NamedDecl(DeclKind K, DeclContext *DC, Identifier name)
    : Decl(K, DC), Name(name), Attrs(&EmptyAttrs) {
  }
  
public:
  Identifier getName() const { return Name; }
  bool isOperator() const { return Name.isOperator(); }

  DeclAttributes &getMutableAttrs();
  const DeclAttributes &getAttrs() const { return *Attrs; }

  Resilience getResilienceFrom(Component *C) const;

  // Implement isa/cast/dyncast/etc.
  static bool classof(const Decl *D) {
    return D->getKind() >= DeclKind::First_NamedDecl &&
           D->getKind() <= DeclKind::Last_NamedDecl;
  }
  static bool classof(const NamedDecl *D) { return true; }
};

/// ValueDecl - All named decls that are values in the language.  These can
/// have a type, etc.
class ValueDecl : public NamedDecl {
  Type Ty;

protected:
  ValueDecl(DeclKind K, DeclContext *DC, Identifier name, Type ty)
    : NamedDecl(K, DC, name), Ty(ty) {
    ValueDeclBits.NeverUsedAsLValue = false;
    ValueDeclBits.HasFixedLifetime = false;
  }

public:

  /// isDefinition - Return true if this is a definition of a decl, not a
  /// forward declaration (e.g. of a function) that is implemented outside of
  /// the swift code.
  bool isDefinition() const;
  
  bool hasType() const { return !Ty.isNull(); }
  Type getType() const {
    assert(!Ty.isNull() && "declaration has no type set yet");
    return Ty;
  }

  /// Set the type of this declaration for the first time.
  void setType(Type T) {
    assert(Ty.isNull() && "changing type of declaration");
    Ty = T;
  }

  /// Overwrite the type of this declaration.
  void overwriteType(Type T) {
    Ty = T;
  }

  /// getTypeOfReference - Returns the type that would arise from a
  /// normal reference to this declaration.  For isReferencedAsLValue()'d decls,
  /// this returns a reference to the value's type.  For non-lvalue decls, this
  /// just returns the decl's type.
  Type getTypeOfReference() const;

  /// isReferencedAsLValue - Returns 'true' if references to this
  /// declaration are l-values.
  bool isReferencedAsLValue() const {
    return getKind() == DeclKind::Var;
  }

  void setHasFixedLifetime(bool flag) {
    ValueDeclBits.HasFixedLifetime = flag;
  }
  void setNeverUsedAsLValue(bool flag) {
    ValueDeclBits.NeverUsedAsLValue = flag;
  }

  bool hasFixedLifetime() const {
    return ValueDeclBits.HasFixedLifetime;
  }
  bool isNeverUsedAsLValue() const {
    return ValueDeclBits.NeverUsedAsLValue;
  }

  /// isInstanceMember - Determine whether this value is an instance member
  /// of a oneof or protocol.
  bool isInstanceMember() const;
  
  // Implement isa/cast/dyncast/etc.
  static bool classof(const Decl *D) {
    return D->getKind() >= DeclKind::First_ValueDecl &&
           D->getKind() <= DeclKind::Last_ValueDecl;
  }
  static bool classof(const ValueDecl *D) { return true; }
};  

/// TypeAliasDecl - This is a declaration of a typealias, for example:
///
///    typealias foo : int
///
/// TypeAliasDecl's always have 'MetaTypeType' type.
///
class TypeAliasDecl : public ValueDecl {
  /// The type that represents this (sugared) name alias.
  mutable NameAliasType *AliasTy;

  SourceLoc TypeAliasLoc;
  Type UnderlyingTy;
  
public:
  TypeAliasDecl(SourceLoc TypeAliasLoc, Identifier Name,
                Type Underlyingty, DeclContext *DC);
  
  SourceLoc getTypeAliasLoc() const { return TypeAliasLoc; }
  void setTypeAliasLoc(SourceLoc loc) { TypeAliasLoc = loc; }

  /// hasUnderlyingType - Returns whether the underlying type has been set.
  bool hasUnderlyingType() const {
    return !UnderlyingTy.isNull();
  }

  /// getUnderlyingType - Returns the underlying type, which is
  /// assumed to have been set.
  Type getUnderlyingType() const {
    assert(!UnderlyingTy.isNull() && "getting invalid underlying type");
    return UnderlyingTy;
  }

  /// setUnderlyingType - Set the underlying type.  This is meant to
  /// be used when resolving an unresolved type name during name-binding.
  void setUnderlyingType(Type T) {
    assert(UnderlyingTy.isNull() && "changing underlying type of type-alias");
    UnderlyingTy = T;
  }

  /// overwriteUnderlyingType - Actually change the underlying type.
  /// Typically it is overwritten to an error type.  It's possible for
  /// type canonicalization to not see these changes.
  void overwriteUnderlyingType(Type T) {
    UnderlyingTy = T;
  }

  SourceLoc getLocStart() const { return TypeAliasLoc; }

  /// getAliasType - Return the sugared version of this decl as a Type.
  NameAliasType *getAliasType() const;
  
  // Implement isa/cast/dyncast/etc.
  static bool classof(const Decl *D) {
    return D->getKind() == DeclKind::TypeAlias;
  }
  static bool classof(const TypeAliasDecl *D) { return true; }
};
  

/// VarDecl - 'var' declaration.
class VarDecl : public ValueDecl {
private:
  SourceLoc VarLoc;    // Location of the 'var' token.
  
  struct GetSetRecord {
    SourceRange Braces;
    FuncDecl *Get;       // User-defined getter
    FuncDecl *Set;       // User-defined setter
  };
  
  GetSetRecord *GetSet;
  
public:
  VarDecl(SourceLoc VarLoc, Identifier Name, Type Ty, DeclContext *DC)
    : ValueDecl(DeclKind::Var, DC, Name, Ty),
      VarLoc(VarLoc), GetSet() {}

  /// getVarLoc - The location of the 'var' token.
  SourceLoc getVarLoc() const { return VarLoc; }
  
  SourceLoc getLocStart() const { return VarLoc; }

  /// \brief Determine whether this variable is actually a property, which
  /// has no storage but does have a user-defined getter or setter.
  bool isProperty() const { return GetSet != nullptr; }
  
  /// \brief Make this variable into a property, providing a getter and
  /// setter.
  void setProperty(ASTContext &Context, SourceLoc LBraceLoc, FuncDecl *Get,
                   FuncDecl *Set, SourceLoc RBraceLoc);

  /// \brief Retrieve the getter used to access the value of this variable.
  FuncDecl *getGetter() const { return GetSet? GetSet->Get : nullptr; }

  /// \brief Retrieve the setter used to mutate the value of this variable.
  FuncDecl *getSetter() const { return GetSet? GetSet->Set : nullptr; }

  // Implement isa/cast/dyncast/etc.
  static bool classof(const Decl *D) { return D->getKind() == DeclKind::Var; }
  static bool classof(const VarDecl *D) { return true; }
};
  

/// FuncDecl - 'func' declaration.
class FuncDecl : public ValueDecl {
  SourceLoc StaticLoc;  // Location of the 'static' token or invalid.
  SourceLoc FuncLoc;    // Location of the 'func' token.
  FuncExpr *Body;
  llvm::PointerIntPair<Decl *, 1, bool> GetOrSetDecl;
  
public:
  FuncDecl(SourceLoc StaticLoc, SourceLoc FuncLoc, Identifier Name,
           Type Ty, FuncExpr *Body, DeclContext *DC)
    : ValueDecl(DeclKind::Func, DC, Name, Ty), StaticLoc(StaticLoc),
      FuncLoc(FuncLoc), Body(Body) {
  }
  
  bool isStatic() const { return StaticLoc.isValid(); }

  FuncExpr *getBody() const { return Body; }
  void setBody(FuncExpr *NewBody) { Body = NewBody; }

  
  /// getExtensionType - If this is a method in a type extension for some type,
  /// return that type, otherwise return Type().
  Type getExtensionType() const;
  
  /// computeThisType - If this is a method in a type extension for some type,
  /// compute and return the type to be used for the 'this' argument of the
  /// type (which varies based on whether the extended type is a reference type
  /// or not), or an empty Type() if no 'this' argument should exist.  This can
  /// only be used after name binding has resolved types.
  Type computeThisType() const;
  
  /// getImplicitThisDecl - If this FuncDecl is a non-static method in an
  /// extension context, it will have a 'this' argument.  This method returns it
  /// if present, or returns null if not.
  VarDecl *getImplicitThisDecl();
  const VarDecl *getImplicitThisDecl() const {
    return const_cast<FuncDecl*>(this)->getImplicitThisDecl();
  }
  
  SourceLoc getStaticLoc() const { return StaticLoc; }
  SourceLoc getFuncLoc() const { return FuncLoc; }
    
  SourceLoc getLocStart() const {
    return StaticLoc.isValid() ? StaticLoc : FuncLoc;
  }
  
  /// makeGetter - Note that this function is the getter for the given
  /// declaration, which may be either a variable or a subscript declaration.
  void makeGetter(Decl *D) {
    GetOrSetDecl.setPointer(D);
    GetOrSetDecl.setInt(false);
  }
  
  /// makeSetter - Note that this function is the setter for the given
  /// declaration, which may be either a variable or a subscript declaration.
  void makeSetter(Decl *D) {
    GetOrSetDecl.setPointer(D);
    GetOrSetDecl.setInt(true);
  }
  
  /// getGetterVar - If this function is a getter, retrieve the declaration for
  /// which it is a getter. Otherwise, returns null.
  Decl *getGetterDecl() const {
    return GetOrSetDecl.getInt()? nullptr : GetOrSetDecl.getPointer();
  }

  /// getSetterVar - If this function is a setter, retrieve the declaration for
  /// which it is a setter. Otherwise, returns null.
  Decl *getSetterDecl() const {
    return GetOrSetDecl.getInt()? GetOrSetDecl.getPointer() : nullptr;
  }

  // Implement isa/cast/dyncast/etc.
  static bool classof(const Decl *D) { return D->getKind() == DeclKind::Func; }
  static bool classof(const FuncDecl *D) { return true; }
};

/// OneOfElementDecl - This represents an element of a 'oneof' declaration, e.g.
/// X and Y in:
///   oneof d { X : int, Y : int, Z }
/// The type of a OneOfElementDecl is always the OneOfType for the containing
/// oneof.
class OneOfElementDecl : public ValueDecl {
  SourceLoc IdentifierLoc;
  
  /// ArgumentType - This is the type specified with the oneof element.  For
  /// example 'int' in the Y example above.  This is null if there is no type
  /// associated with this element (such as in the Z example).
  Type ArgumentType;
    
public:
  OneOfElementDecl(SourceLoc IdentifierLoc, Identifier Name, Type Ty,
                   Type ArgumentType, DeclContext *DC)
  : ValueDecl(DeclKind::OneOfElement, DC, Name, Ty),
    IdentifierLoc(IdentifierLoc), ArgumentType(ArgumentType) {}

  Type getArgumentType() const { return ArgumentType; }

  SourceLoc getIdentifierLoc() const { return IdentifierLoc; }
  SourceLoc getLocStart() const { return IdentifierLoc; }
  
  // Implement isa/cast/dyncast/etc.
  static bool classof(const Decl *D) {
    return D->getKind() == DeclKind::OneOfElement;
  }
  static bool classof(const OneOfElementDecl *D) { return true; }
 
};

} // end namespace swift

#endif
