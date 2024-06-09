#ifndef FLOAT16_H
#define FLOAT16_H

// CAUTION: __is_identifier behaves opposite how you would expect!
// '__is_identifier' returns '0' if '__x' is a reserved identifier provided by
// the compiler and '1' otherwise.
// borrowed from LLVM __config header under Apache license 2. 
// (https://www.mend.io/blog/top-10-apache-license-questions-answered/)
#ifndef __is_identifier       // Optional of course.
#define __is_identifier(x) 1  // Compatibility with non-clang compilers.
#endif

// More sensible macro for keyword detection
#define __has_keyword(__x) !(__is_identifier(__x))

// map a half float type, if available, to _OptionalHalfFloatType
#if __has_keyword(_Float16)
#define HAVE_FLOAT16
#endif

#endif
