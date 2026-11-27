---
trigger: always_on
---

# C++ Coding Standards

- [Purpose](#purpose)
- [Conventions](#conventions)
  - [1. General Style](#1-general-style)
  - [2. Files](#2-files)
  - [3. Includes and Declarations](#3-includes-and-declarations)
  - [4. Comments](#4-comments)
  - [5. Classes & Structs](#5-classes--structs)
  - [6. Functions](#6-functions)
  - [7. Variables](#7-variables)
  - [8. Enums & Constants](#8-enums--constants)
  - [9. Lambdas](#9-lambdas)
  - [10. Casts](#10-casts)
  - [11. Auto](#11-auto)
  - [12. Control Flow](#12-control-flow)
  - [13. Other Language Features](#13-other-language-features)
  - [14. Engine Changes](#14-engine-changes)

## Purpose

This document summarizes the high-level coding conventions for writing C++ code on Pangea Dawn. They are based on the official [Unreal Coding Standard](https://dev.epicgames.com/documentation/en-us/unreal-engine/epic-cplusplus-coding-standard-for-unreal-engine) which should be read first. Below is just a list of addendums and emphasized points.

The goal is to have our code blend in with existing Unreal API code. The majority of C++ for Pangea Dawn is written in close relation to the Unreal Engine, therefore this document is directed primarily towards that use case. It also applies, in the absence of any other standard, to non-Unreal C++ in the project.

The decision process used in determining the standards are based upon the following priorities (roughly in order):

- Have our code blend in with existing Unreal API code as much as possible.
- Help write functionally correct code within UE5 while avoiding common problems.
- Help write code that is understandable and debuggable to the dozens of people who will have to read it.
- Help write code that is maintainable over the lifetime of the codebase.

## Conventions

We are providing a complete summary here in order to allow people to understand the conventions at a glance, instead of having to open multiple documents. Our coding conventions are numbered, which makes it easier to refer to them in code reviews. Each begins with one of the following:

- ✅ **ALWAYS**: This rule should be followed in all circumstances unless given sign off by the team leadership.
- 👍 **PREFER**: This is not a hard rule but you should have a good reason not to follow it.
- ⚠️ **AVOID**: You should not use this particular paradigm unless you have a good reason to do so.
- ❌ **NEVER**: Do not do this thing under any circumstances unless given sign off by the team leadership.

### 1. General Style

- **1.1.** ✅ **ALWAYS** keep Tabs as Tabs; they should be four spaces in width.
- **1.2.** ❌ **NEVER** put multiple statements on one line.
- **1.3.** 👍 **PREFER** keeping line lengths short enough so code is easier to review when side by side. Wrap the code to a new line when necessary.
- **1.4.** ✅ **ALWAYS** put each block in braces even if the block is only one line. This avoids subtle bugs and minimizes the necessary change when you add more functionality to a one line statement.
- **1.5.** ❌ **NEVER** use any swearing in symbol names, comments, or log output.
- **1.6.** 👍 **PREFER** to follow Epic's "Inclusive Word Choice" guidance (`AllowList`/`DenyList` vs `WhiteList`/`BlackList`, etc).
- **1.7.** ✅ **ALWAYS** format Pangea-owned C++ with the repository `.clang-format` (clang-format 19 or newer) before review. It settles indentation, brace placement, wrapping at 160 columns and include order inside each group. Never run it on vendor plugins.
- **1.8.** ✅ **ALWAYS** separate logical blocks inside a function with one blank line. The formatter cannot do this for you:
  - after the closing brace of a block, unless `else`, `catch` or another closing brace follows;
  - before an `if` / `for` / `while` / `switch` that does not test the value set on the line above;
  - before a `return` that ends a block of two or more statements;
  - before a comment that introduces the next step.

  ```cpp
  UFoo* const Foo = GetFoo();
  if (Foo == nullptr) // Tests the line above: keep together.
  {
      return;
  }

  Foo->Prepare();
  Foo->Run();

  // Publish the result once the run has finished.
  OnFinished.Broadcast(Foo);
  ```

### 2. Files

- **2.1.** ✅ **ALWAYS** use PascalCase for file names, but without the type prefix (e.g. `AExampleName` would be in the file named `ExampleName.h`).
- **2.2.** ✅ **ALWAYS** start every original source file we create with a copyright notification. Use the same notice as the other files in the module you are adding to. Do not rewrite the notice of an existing file.
- **2.3.** ✅ **ALWAYS** use `#pragma once` at the top of all headers to protect against multiple includes just below the copyright.
- **2.4.** 👍 **PREFER** defining only one public type per file. Multiple types can cause a user to pull in more than intended when including a header, can cause the file size to bloat, and can become too hard to read.

### 3. Includes and Declarations

Reference: https://dev.epicgames.com/documentation/en-us/unreal-engine/include-what-you-use-iwyu-for-unreal-engine-programming

- **3.1.** ❌ **NEVER** include unused headers.
- **3.2.** ⚠️ **AVOID** relying on a header that is included indirectly by another header you include.
- **3.3.** ✅ **ALWAYS** make the first include within a header file `CoreMinimal.h`. Then follow it immediately with all other required includes.
- **3.4.** ✅ **ALWAYS** make the first include in a source file the class header file contained within. Then follow it immediately with all the other required includes.
- **3.5.** 👍 **PREFER** being as fine grained as possible with includes. For example, do not include `Core.h` when you only need one specific header from it or `CoreMinimal.h` will do.
- **3.6.** 👍 **PREFER** to group your includes so the engine level headers are before headers within your plugin or game code. Then group headers for your plugin or game code after it.
- **3.7.** 👍 **PREFER** to alphabetize the includes within each grouping.
- **3.8.** 👍 **PREFER** to keep conditionally included files together in their own preprocessor block.

  ```cpp
  #include "CoreMinimal.h"

  #include "Engine/DataTable.h"
  #include "Engine/StreamableManager.h"
  #include "Subsystems/GameInstanceSubsystem.h"

  #include "SomeGameHeader.h"
  #include "SomeOtherGameHeader.h"

  #if !UE_BUILD_SHIPPING
  #include "DrawDebugHelpers.h"
  #include "SomeOtherDebugHeader.h"
  #endif // !UE_BUILD_SHIPPING

  #include "MyFilename.generated.h"
  ```

- **3.9.** ✅ **ALWAYS** use forward declarations instead of including a header when you can.
- **3.10.** 👍 **PREFER** to list your forward declarations near the beginning of the file while keeping them alphabetized.
- **3.11.** ⚠️ **AVOID** doing inline class forward declarations.

  ```cpp
  class AExampleClass; // GOOD
  class AExampleClass * Example; // Bad
  ```

### 4. Comments

- **4.1.** ✅ **ALWAYS** write self-documenting code.

  ```cpp
  int32 T = S + L - B; // BAD
  int32 TotalLeaves = SmallLeaves + LargeLeaves - SmallAndLargeLeaves; // GOOD
  ```

- **4.2.** ✅ **ALWAYS** write useful comments that do not contradict the code.

  ```cpp
  ++Leaves; // increment Leaves - BAD b/c it doesn't add anything useful
  ++Leaves; // never increment Leaves - BAD b/c it contradicts the code
  ++Leaves; // we know there's another tea leaf - GOOD b/c it adds additional detail you might have to dig for
  ```

- **4.3.** ✅ **ALWAYS** comment all `UCLASS()`, `USTRUCT()`, `UENUM()`, `UPROPERTY()`, and `UFUNCTION()` markup that is exposed to blueprint with proper informative comments so they generate good tooltips within the editor. This is the user's only view into the blackbox of C++ functionality. Alternatively you can use the `"ToolTip"` metaproperty to add your tooltips.

### 5. Classes & Structs

- **5.1.** ✅ **ALWAYS** use PascalCase for class names, e.g. `AExampleName`.
- **5.2.** ✅ **ALWAYS** add type prefixes to class names to distinguish them from variable names. For example, `FSkin` is a type name, and `Skin` is an instance of a `FSkin`. UnrealHeaderTool requires the correct prefixes in most cases, so it's important to provide them.
  - Template classes are prefixed by `T`.
  - Classes that inherit from `UObject` are prefixed by `U`.
  - Classes that inherit from `AActor` are prefixed by `A`.
  - Classes that inherit from `SWidget` are prefixed by `S`.
  - Classes that are abstract interfaces are prefixed by `I`.
  - Enums are prefixed by `E`.
- **5.3.** ⚠️ **AVOID** public member variables in classes.
- **5.4.** ✅ **ALWAYS** mark each constructor that takes exactly one argument as `explicit`, unless it's a copy constructor or the whole point of the constructor is to allow implicit casting and then leave a comment as to why. This minimizes wrong use of the constructor.
- **5.5.** ✅ **ALWAYS** comment on all classes and structs with a description of why the class was created and what problem it solves.
- **5.6.** ❌ **NEVER** name a class such that it ends with `Actor`, `Object`, `Component`, `Widget`, `Subsystem`, `Controller`, `Pawn`, `Character`, or other foundational UE5 class names UNLESS it actually inherits from those base classes.

### 6. Functions

- **6.1.** ✅ **ALWAYS** use PascalCase for function names.
- **6.2.** ✅ **ALWAYS** use `virtual` and `override` on appropriate member functions in child classes.
- **6.3.** ❌ **NEVER** pass complex types by value unless you absolutely mean for them to be copied and you're certain they have proper copy constructors.

  ```cpp
  bool DoAThing(const TArray<FHitResult>& MyArray); // GOOD
  bool DoAThing(TArray<FHitResult> MyArray); // BAD unless you were just going to copy it off anyways
  ```

- **6.4.** 👍 **PREFER** prefixing function parameter names with `Out` if they are passed by reference and the function is expected to write to that value. This makes it obvious that the value passed in this argument will be replaced by the function.
- **6.5.** ✅ **ALWAYS** flag reference and pointer parameters as `const` if they are not going to be modified within the method.
- **6.6.** 👍 **PREFER** to flag methods as `const` if they do not modify the object.
- **6.7.** ✅ **ALWAYS** prefix RPCs with `Server_`, `Client_`, or `Multi_` to identify it as an RPC and tell us what the proper calling channels are.
- **6.8.** ⚠️ **AVOID** providing function implementations in header files. Use inline functions judiciously, as they force rebuilds even in files which don't use them. Inlining should only be used for trivial accessors and when profiling shows there is a benefit to doing so. Be even more conservative in the use of `FORCEINLINE`. All code and local variables will be expanded out into the calling function and will cause the same build time problems caused by large functions.
- **6.9.** 👍 **PREFER** commenting public functions with enough information so the future readers can decide if it might be what they're looking for without having to read the function body.
- **6.10.** ✅ **ALWAYS** validate all input parameters for blueprint exposed functions in such a way that it provides the editor user with enough information to debug the problem. This means you should not use `check`s or `ensure`s to validate parameters. At times, functions and parameters may not need validation, but if they do, do it in this manner.
- **6.11.** 👍 **PREFER** validating all input parameters performing conditional error handling while communicating invalid inputs appropriately through logging or other means.
- **6.12.** 👍 **PREFER** using `check()` to verify your input parameters in cases where you can guarantee the parameters are valid b/c the function is private or we absolutely need the extra performance of avoiding more costly error handling.

  ```cpp
  void ExampleHighlyPerformantPrivateFunction(const UObject* InObj, float SomeFloat)
  {
      check(IsValid(InObj));
      check(SomeFloat >= 0.f && SomeFloat <= 1.f);
      ...
  }
  ```

### 7. Variables

- **7.1.** ✅ **ALWAYS** use PascalCase for member, global, local, and parameter variables. The exception is boolean variables which should be prefixed with a `b`.

  ```cpp
  float ExplosionRadius = 1.0f;
  bool bExploded = false;

  void ExampleFunction(const bool bSetEnabled, float ExampleParam)
  {
      bool bWasEnabled = bEnabled;
      bEnabled = bSetEnabled;

      float OldExample = ExampleMember;
      ExampleMember = ExampleParam;
  }
  ```

- **7.2.** ✅ **ALWAYS** initialize your member variables unless there's a good reason for the optimization and you can show it will always get initialized before it is used.
- **7.3.** ❌ **NEVER** use short or meaningless names (e.g. `"a"`, `"foo"`, etc.). Single character variable names (such as `"i"` or `"j"`) are only okay for counters and temporaries, where the purpose of the variable is obvious. The person reviewing your code can tell you if it's obvious.
- **7.4.** ⚠️ **AVOID** using negative names for boolean variables.

  ```cpp
  if (!bNotInfinite) // BAD: Double negation can be hard to read.
  if (bInfinite) // GOOD
  ```

- **7.5.** ✅ **ALWAYS** declare each variable on a separate line.
- **7.6.** ✅ **ALWAYS** use portable aliases for basic C++ types:
  - `bool` for boolean values (*never* assume the size of `bool`). `BOOL` will not compile.
  - `TCHAR` for a character (*never* assume the size of `TCHAR`).
  - `uint8` for unsigned bytes (1 byte).
  - `int8` for signed bytes (1 byte).
  - `uint16` for unsigned "shorts" (2 bytes).
  - `int16` for signed "shorts" (2 bytes).
  - `uint32` for unsigned ints (4 bytes).
  - `int32` for signed ints (4 bytes).
  - `uint64` for unsigned "quad words" (8 bytes).
  - `int64` for signed "quad words" (8 bytes).
  - `float` for single precision floating point (4 bytes).
  - `double` for double precision floating point (8 bytes).
  - `PTRINT` for an integer that may hold a pointer (*never* assume the size of `PTRINT`).
- **7.7.** 👍 **PREFER** to test whether a pointer is valid before dereferencing it. `nullptr` should be used instead of the C-style `NULL` macro in all cases.
- **7.8.** 👍 **PREFER** if a pointer points to any `UObject`, use `IsValid` to ensure the pointer is not null and the referenced object is not marked for destruction.
- **7.9.** ✅ **ALWAYS** comment all `UPROPERTY` member variables exposed to the editor. This will ensure the user has proper tooltips for what the members do.
- **7.10.** 👍 **PREFER** using `const` if a variable is not going to be modified.
- **7.11.** ✅ **ALWAYS** use `TObjectPtr<T>` when defining member variables of type `T` that are or inherit from `UObject` and need `UPROPERTY` markup.
- **7.12.** 👍 **PREFER** to minimize your usage of `UPROPERTY` markup. No need to just blanket a class's (or struct's) members with it.
- **7.13.** ⚠️ **AVOID** using no specifiers for a `UPROPERTY`. If it isn't exposed to the editor, `Transient` is usually appropriate.

### 8. Enums & Constants

- **8.1.** 👍 **PREFER** using `enum class` over `static constexpr` over `static const` variables over `#define` when defining constants. There are valid uses for each but expect to be questioned as to why you used a lesser preferred method.
  - `enum` provides a compile time constant that can group multiple values that can be used for type checking.
  - `static constexpr` also provides a compile time constant and produces a compiler error if it can't determine the value.
  - `static const` is usually a compile time constant but it will silently change to a runtime constant if it can't resolve the value which produces slightly slower code.
  - `#define` is the absolute last resort for constants as it could silently have a naming conflict and it is not type checked like the others.
- **8.2.** ✅ **ALWAYS** comment enum values by either using the `ToolTip` metaproperty or by adding a comment above the value. Do not use an end-of-line comment. This will ensure the UnrealHeaderTool picks up the comment as a tooltip within the editor.
- **8.3.** ✅ **ALWAYS** ensure enums exposed to blueprint are based on `uint8`.

### 9. Lambdas

- **9.1.** ✅ **ALWAYS** name variables assigned to Lambdas following the variables naming convention.
- **9.2.** ⚠️ **AVOID** writing Lambdas more than a few lines long. You should break them out into helper functions to provide consistent callstacks for crash logging.
- **9.3.** ❌ **NEVER** capture more than you need with a Lambda. This means you should almost never use `[=]` or `[&]`.
- **9.4.** ❌ **NEVER** capture `UObject` pointers in Lambdas used as callbacks. This goes for capturing the `"this"` pointer as well. You can assign them to `TWeakObjectPtr`s and pass those in instead, or use `BindWeakLambda` which will avoid calling the lambda if the weakptr is invalid at the time of the callback.

### 10. Casts

- **10.1.** ✅ **ALWAYS** use the specialized casts (`static_cast`, `reinterpret_cast`, `const_cast`, `dynamic_cast`) when dealing with non-`UObject` types.
- **10.2.** ✅ **ALWAYS** use the engine provided `Cast<>` functions when dealing with `UObject` types where you are going to immediately check against `nullptr`.
- **10.3.** ✅ **ALWAYS** use the engine provided `CastChecked<>` functions when dealing with `UObject` types where you can safely assume it is the correct type and is non-null. This will be faster in shipping builds.
- **10.4.** ❌ **NEVER** use C-style casts except when casting from POD type to POD type. You must be sure you're not casting away constness when exercising the exception. Be extra careful when casting to the size of your pointer as it can confuse the compiler and compile without warning or error (i.e. `uint64` instead of `uint64*`).

### 11. Auto

Additional reading in the Epic Coding Standards document. Search for "You shouldn't use auto".

- **11.1.** ⚠️ **AVOID** using the `auto` keyword unless it is eliminating repetition in templated code, assigning iterator types, or needing to bind a Lambda to a variable. Using `auto` should never make the code less readable.
- **11.2.** ✅ **ALWAYS** use `auto&` for references to prevent unnecessary copy construction.
- **11.3.** ✅ **ALWAYS** use `auto*` instead of just `auto` for pointers.
- **11.4.** ✅ **ALWAYS** use `const` with the appropriate form of `auto` when the target is unchanged. This will add additional guidance to the reader.

### 12. Control Flow

- **12.1.** ✅ **ALWAYS** use a new line for the body of a control flow statement. This makes it more debuggable.

  ```cpp
  if (i==0) { return 0; } // BAD
  for (int i=0; i < 5; ++i) { ++x; } // BAD

  // GOOD
  if (i==0)
  {
      return 0;
  }
  ```

- **12.2.** ✅ **ALWAYS** add a `break` or `return` statement at the end of every `case`, or a comment to indicate that there's intentionally no break, unless another case follows immediately within `switch` statements.

  ```cpp
  switch (myEnumValue)
  {
      case Value1:
          DoSomething();
          break;
      case Value2:
      case Value3:
          DoSomethingElse();
          // Fall through.
      default:
          DefaultHandling();
          break;
  }
  ```

- **12.3.** 👍 **PREFER** use early outs to avoid unnecessary scope depth.

  ```cpp
  if (ThisOrThat)
  {
      return;
  }

  DoSomethingElse();
  ```

- **12.4.** ✅ **ALWAYS** use range based for loops wherever possible. It shows clear intent to iterate from beginning to end without modifying the size of the container which is by far the most common case for iterating. There are also additional checks within the UE5 engine to error when the container is resized while iterating using range based for loops.
- **12.5.** ❌ **NEVER** use unsigned types for array index storage. Instead, document a variable is non-negative using assertions.
  - For more information, read the Google C++ style guide "On Unsigned Integers" here: https://google.github.io/styleguide/cppguide.html.
- **12.6.** ❌ **NEVER** nest ternary statements.

  ```cpp
  int32 Foo = Bar1 ? Bar2 ? 1 : 0 : 0; // BAD
  ```

- **12.7.** ❌ **NEVER** mix `const` and non-`const` iterators. For example, `const_iterator` uses `cbegin`/`cend`, not `begin`/`end`.

### 13. Other Language Features

- **13.1.** ✅ **ALWAYS** use proprietary types, such as `TArray` or `TMap` where possible. This avoids unnecessary and repeated type conversions while interacting with the Unreal Engine APIs.
- **13.2.** ✅ **ALWAYS** use the `TEXT()` macro around string literals. Without it, code which constructs `FString`s from literals will cause an undesirable string conversion process.
- **13.3.** ✅ **ALWAYS** use `static_assert` for compile time asserts.
- **13.4.** ✅ **ALWAYS** give a reference to container objects to fill out when you need to construct, filter the contents of, or copy a container. This can prevent an extra copy that would occur if you return a container by value and it can't be optimized by the compiler.
- **13.5.** 👍 **PREFER** to return a `const` reference to a container object when possible to prevent copying the container that is guaranteed to live beyond the caller's lifetime.
- **13.6.** ✅ **ALWAYS** use `#if 1` and `#if 0` instead of `#if true` and `#if false`. The latter can cause compiler errors with some compilers.
- **13.7.** ✅ **ALWAYS** ensure the project compiles without warnings. If the compiler can't be set to warnings as errors, any warnings should be treated as high priority bugs and fixed as soon as possible.
- **13.8.** 👍 **PREFER** to have your function take an `FStringView` over a `const FString&` to prevent promotion of c-style strings via dynamic memory allocation.
- **13.9.** ✅ **ALWAYS** use `UE_DEFINE_GAMEPLAY_TAG_COMMENT` instead of `UE_DEFINE_GAMEPLAY_TAG`.

### 14. Engine Changes

Pangea Dawn builds against an installed (launcher) engine and treats vendor plugins as read only. See the root `AGENTS.md` for the vendor boundary rules.

- **14.1.** ❌ **NEVER** modify engine code or vendor plugin code or assets.
- **14.2.** ✅ **ALWAYS** use inheritance, components, delegates, interfaces, configuration or project-owned plugins to change engine or vendor behavior. If no supported extension point exists, explain the limitation instead of patching.
