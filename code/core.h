#if !defined(CORE_H_)
#define CORE_H_

#if defined(__cplusplus)
extern "C" {
#endif

//
// --------------------------------------------------------------------------------
// :Core_Types
// --------------------------------------------------------------------------------
//

#include <stdint.h>
#include <stdbool.h>

typedef uint8_t  U8;
typedef uint16_t U16;
typedef uint32_t U32;
typedef uint64_t U64;

typedef int8_t  S8;
typedef int16_t S16;
typedef int32_t S32;
typedef int64_t S64;

typedef int8_t  B8;
typedef int16_t B16;
typedef int32_t B32;
typedef int64_t B64;

typedef float  F32;
typedef double F64;

typedef struct Str8 Str8;
struct Str8 {
    S64 count;
    U8 *data;
};

typedef struct Buffer Buffer;
struct Buffer {
    union {
        struct {
            S64 used;
            U8 *data;
        };

        Str8 str;
    };

    S64 limit;
};

#define U8_MAX  ((U8)  -1)
#define U16_MAX ((U16) -1)
#define U32_MAX ((U32) -1)
#define U64_MAX ((U64) -1)

#define S8_MIN  ((S8)  0x80)
#define S16_MIN ((S16) 0x8000)
#define S32_MIN ((S32) 0x80000000)
#define S64_MIN ((S64) 0x8000000000000000)

#define S8_MAX  ((U8)  0x7F)
#define S16_MAX ((U16) 0x7FFF)
#define S32_MAX ((U32) 0x7FFFFFFF)
#define S64_MAX ((U64) 0x7FFFFFFFFFFFFFFF)

#define F32_MAX ((F32) 3.40282346638528859811704183484516925e+038F)
#define F64_MAX ((F64) 1.79769313486231570814527423731704357e+308 )

//
// --------------------------------------------------------------------------------
// :Platform_Macros
// --------------------------------------------------------------------------------
//

// Operating system
//

#define OS_WINDOWS 0
#define OS_MACOS   0
#define OS_LINUX   0
#define OS_SWITCH  0

#if defined(_WIN32)
    #undef  OS_WINDOWS
    #define OS_WINDOWS 1
#elif defined(__APPLE__)
    #include <TargetConditionals.h>
    #if TARGET_OS_OSX
        #undef  OS_MACOS
        #define OS_MACOS 1
    #else
        #error "macos is the only supported apple platform."
    #endif
#elif defined(__linux__)
    #undef  OS_LINUX
    #define OS_LINUX 1
#elif defined(__SWITCH__)
    #undef  OS_SWITCH
    #define OS_SWITCH 1
#else
    #error "undetermined or unsupported operating system"
#endif

// CPU architecture
//

#define ARCH_AMD64   0
#define ARCH_AARCH64 0

#if defined(__amd64__) || defined(_M_AMD64)
    #undef  ARCH_AMD64
    #define ARCH_AMD64 1
#elif defined(__aarch64__) || defined(_M_ARM64)
    #undef  ARCH_AARCH64
    #define ARCH_AARCH64 1
#else
    #error "undetermined or unsupported cpu architecture"
#endif

// Compiler type
//

#define COMPILER_CLANG 0
#define COMPILER_MSVC  0
#define COMPILER_GCC   0

#if defined(__clang__)
    #undef  COMPILER_CLANG
    #define COMPILER_CLANG 1
#elif defined(_MSC_VER)
    #undef  COMPILER_MSVC
    #define COMPILER_MSVC 1
#elif defined(__GNUC__)
    #undef  COMPILER_GCC
    #define COMPILER_GCC 1
#else
    #error "undetermined or unsupported compiler"
#endif

//
// --------------------------------------------------------------------------------
// :Utility_Macros
// --------------------------------------------------------------------------------
//

#include <assert.h>

#define Assert(exp) assert(exp)
#define StaticAssert(exp) static_assert(exp, #exp)

#define cast(x) (x)

#define ArraySize(x) (sizeof(x) / sizeof((x)[0]))
#define OffsetTo(T, m) ((U64) &(((T *) 0)->m))

#define DeferLoop(s, e) for (int __defer__ = ((s), 0); __defer__ == 0; __defer__ += 1, (e))

#define Min(a, b) ((a) < (b) ?  (a) : (b))
#define Max(a, b) ((a) > (b) ?  (a) : (b))
#define Abs(x)    ((x) < (0) ? -(x) : (x))

#define Clamp(min, x, max) Min(Max(x, min), max)
#define Clamp01(x) Clamp(0, x, 1)

#define __Glue(a, b) a##b
#define Glue(a, b) __Glue(a, b)

#define __Stringify(s) #s
#define Stringify(s) __Stringify(s)

#define FourCC(a, b, c, d) (((U32) (d) << 24) | ((U32) (c) << 16) | ((U32) (b) << 8) | ((U32) (a) << 0))

#define StackBuffer(name, size) \
    U8 __data_for_##name[size]; \
    Buffer name = { 0, __data_for_##name, size }

// single-linked queue/stack macros with user-named 'next' member
//
#define QueuePushNamed(head, tail, node, next)      ((head) == 0 ? (head) = (tail) = (node) : ((tail)->next = (node), (tail) = (node)), (node)->next = 0)
#define QueuePushFrontNamed(head, tail, node, next) ((head) == 0 ? (head) = (tail) = (node), (node)->next = 0 : ((node)->next = (head), (head) = (node)))
#define QueuePopNamed(head, tail, next)             ((head) == (tail) ? (head) = (tail) = 0 : ((head) = (head)->next))

#define StackPushNamed(top, bottom, node, next)       QueuePushFrontNamed(top, bottom, node, next)
#define StackPushBottomNamed(top, bottom, node, next) QueuePushNamed(top, bottom, node, next)
#define StackPopNamed(top, bottom, next)              QueuePopNamed(top, bottom, next)

// single-linked queue/stack macros with default 'next' member name
//
#define QueuePush(head, tail, node)      QueuePushNamed(head, tail, node, next)
#define QueuePushFront(head, tail, node) QueuePushFrontNamed(head, tail, node, next)
#define QueuePop(head, tail)             QueuePopNamed(head, tail, next)

#define StackPush(top, bottom, node)       QueuePushFrontNamed(top, bottom, node, next)
#define StackPushBottom(top, bottom, node) QueuePushNamed(top, bottom, node, next)
#define StackPop(top, bottom)              QueuePopNamed(top, bottom, next)

#if defined(CORE_LIB)
    #if COMPILER_MSVC
        #if defined(CORE_IMPL)
            #define Function extern __declspec(dllexport)
        #else
            #define Function extern __declspec(dllimport)
        #endif
    #else
        #define Function extern
    #endif
#else
    #define Function static
#endif

#if COMPILER_MSVC
    #define ThreadVar __declspec(thread)
#elif (COMPILER_CLANG || COMPILER_GCC)
    #define ThreadVar __thread
#endif

#define Internal     static
#define GlobalVar    static
#define LocalPersist static

//
// --------------------------------------------------------------------------------
// :Intrinsics
// --------------------------------------------------------------------------------
//
//

// Returns the operand width in bits if the input is zero
//
Function U32 U32LeadingZeroCount(U32 x);
Function U64 U64LeadingZeroCount(U64 x);

Function U32 U32TrailingZeroCount(U32 x);
Function U64 U64TrailingZeroCount(U64 x);

// Input count is masked to the bit width of the operand
//
Function U32 U32RotateLeft(U32 x, U32 count);
Function U64 U64RotateLeft(U64 x, U32 count);

Function U32 U32RotateRight(U32 x, U32 count);
Function U64 U64RotateRight(U64 x, U32 count);

Function U32 U32PopCount(U32 x);
Function U64 U64PopCount(U64 x);

// Atomics
//

// All return the value stored in 'ptr' before operation
//
Function U32 U32AtomicAdd(volatile U32 *ptr, U32 value);
Function U64 U64AtomicAdd(volatile U64 *ptr, U64 value);

Function U32   U32AtomicExchange(volatile U32 *ptr, U32 exchange);
Function U64   U64AtomicExchange(volatile U64 *ptr, U64 exchange);
Function void *PtrAtomicExchange(void *volatile *ptr, void *exchange);

// Return true if operation succeeded, otherwise false
//
Function B32 U32AtomicCompareExchange(volatile U32 *ptr, U32 exchange, U32 comparand);
Function B32 U64AtomicCompareExchange(volatile U64 *ptr, U64 exchange, U64 comparand);
Function B32 PtrAtomicCompareExchange(void *volatile *ptr, void *exchange, void *comparand);

// :note these are here instead of some maths header because they are relatively useful to have about and
// are implemented using instruction intrinsics
//
Function F32 F32Sqrt(F32 x);
Function F64 F64Sqrt(F64 x);

Function F32 F32InvSqrt(F32 x);
Function F64 F64InvSqrt(F64 x);

Function F32 F32ApproxInvSqrt(F32 x);
Function F64 F64ApproxInvSqrt(F64 x);

//
// --------------------------------------------------------------------------------
// :Memory_Arena
// --------------------------------------------------------------------------------
//

#define KB(x) ((U64) (x) << 10ULL)
#define MB(x) ((U64) (x) << 20ULL)
#define GB(x) ((U64) (x) << 30ULL)
#define TB(x) ((U64) (x) << 40ULL)

#define AlignUp(x, a) (((x) + ((a) - 1)) & ~((a) - 1))
#define AlignDown(x, a) ((x) & ~((a) - 1))

#if defined(__cplusplus)
    // :note C uses _Alignof as an operator, provides alignof in stdalign.h however msvc just doesn't
    // define the macros when including it because "C11" is not available, even though _Alignof works
    // so instead I'm just using _Alignof in C++ through a macro so everyone is happy
    //
    #define _Alignof(x) alignof(x)
#endif

#if !defined(ARENA_DEFAULT_COMMIT_SIZE)
    #if OS_SWITCH
        // :note Switchbrew doesn't currently support userspace reserve/commit virtual memory semantics so
        // we have to force chained arenas thus the default commit size is the size of each 'arena block'
        // and not the amount to align commit requests to
        //
        // see: https://github.com/switchbrew/libnx/issues/503
        //
        #define ARENA_DEFAULT_COMMIT_SIZE MB(4)
    #else
        // Othewise we have proper virtual memory support so we can use a smaller value to align to
        //
        #define ARENA_DEFAULT_COMMIT_SIZE KB(64)
    #endif
#endif

typedef U32 ArenaFlags;
enum {
    // the arena is made up of internally chained blocks of memory rather than a
    // single large contiguous memory region
    //
    ARENA_FLAG_CHAINED = (1 << 0),

    // allocations are not cleared to zero, can be provided per allocation
    //
    ARENA_FLAG_NO_ZERO = (1 << 1)
};

typedef struct Arena Arena;
struct Arena {
    Arena *current;   // arena to allocate from,  considered opaque
    Arena *prev;      // previous arena in chain, considered opaque

    U64 base;         // base offset of this arena
    U64 offset;       // current offset into this arena to start next allocation at
    U64 limit;        // limit of this arena

    U64 committed;    // size of this arena that has been virtually committed
    U64 commit_size;  // size to align commit requests to @todo: U32 instead? then we can have 'last_offset'

    ArenaFlags flags; // any permanent flags to apply to the arena
    U32 pad;          // pad to 64-bytes, may be used for something else later
};

StaticAssert(sizeof(Arena) == 64);

Function Arena *ArenaAllocArgs(U64 limit, U64 commit_size, ArenaFlags flags);
Function Arena *ArenaAlloc(U64 limit);

Function void ArenaReset(Arena *arena);   // sets usage to 0 but arena remains valid to use
Function void ArenaRelease(Arena *arena); // releases all backing memory for arena making it invalid to use

Function void *ArenaPushFrom(Arena *arena, U64 size, ArenaFlags flags, U32 alignment);
Function void *ArenaPushCopyFrom(Arena *arena, void *src, U64 size, ArenaFlags flags, U32 alignment);

// basic macros to push from/copy to arenas
//
#define ArenaPush(...)     ArenaPushExpand((__VA_ARGS__, ArenaPush5, ArenaPush4, ArenaPush3, ArenaPush2))(__VA_ARGS__)
#define ArenaPushCopy(...) ArenaPushCopyExpand((__VA_ARGS__, ArenaPushCopy6, ArenaPushCopy5, ArenaPushCopy4, ArenaPushCopy3))(__VA_ARGS__)

// support macros to allow default arguments when using the generic push/push copy macros
//
// :note have to do dumb expansion macro first because msvc doesn't have a conformant preprocessor
//
#define ArenaPushExpand(args) ArenaPushBase args
#define ArenaPushBase(a, b, c, d, e, f, ...) f

#define ArenaPush2(arena, T)          (T *) ArenaPushFrom((arena),       sizeof(T), 0, _Alignof(T))
#define ArenaPush3(arena, T, n)       (T *) ArenaPushFrom((arena), (n) * sizeof(T), 0, _Alignof(T))
#define ArenaPush4(arena, T, n, f)    (T *) ArenaPushFrom((arena), (n) * sizeof(T), f, _Alignof(T))
#define ArenaPush5(arena, T, n, f, a) (T *) ArenaPushFrom((arena), (n) * sizeof(T), f, (a))

#define ArenaPushCopyExpand(args) ArenaPushCopyBase args
#define ArenaPushCopyBase(a, b, c, d, e, f, g, ...) g

#define ArenaPushCopy3(arena, src, T)          (T *) ArenaPushCopyFrom((arena), (src),       sizeof(T), 0, _Alignof(T))
#define ArenaPushCopy4(arena, src, T, n)       (T *) ArenaPushCopyFrom((arena), (src), (n) * sizeof(T), 0, _Alignof(T))
#define ArenaPushCopy5(arena, src, T, n, f)    (T *) ArenaPushCopyFrom((arena), (src), (n) * sizeof(T), f, _Alignof(T))
#define ArenaPushCopy6(arena, src, T, n, f, a) (T *) ArenaPushCopyFrom((arena), (src), (n) * sizeof(T), f, (a))

// Pop allocation calls from the end of an arena
//

Function void ArenaPopTo(Arena *arena, U64 offset);
Function void ArenaPopSize(Arena *arena, U64 size);

#define ArenaPop(...) ArenaPopExpand((__VA_ARGS__, ArenaPop3, ArenaPop2))(__VA_ARGS__)

#define ArenaPopExpand(args) ArenaPopBase args
#define ArenaPopBase(a, b, c, d, ...) d

#define ArenaPop2(arena, T)    ArenaPopSize((arena),       sizeof(T))
#define ArenaPop3(arena, T, n) ArenaPopSize((arena), (n) * sizeof(T))

// Temporary memory arenas
//

#if !defined(TEMP_ARENA_DEFAULT_LIMIT)
    #define TEMP_ARENA_DEFAULT_LIMIT GB(1)
#endif

#if !defined(TEMP_ARENA_COUNT)
    #define TEMP_ARENA_COUNT 2
#endif

typedef struct TempArena TempArena;
struct TempArena {
    Arena *arena;
    U64 offset;
};

// Create a temporary arena from an exising arena
//
Function TempArena TempFrom(Arena *arena);
Function TempArena TempGet(U32 count, Arena **conflicts);

#define TempGetAny() TempGet(0, 0)

Function void TempRelease(TempArena *temp);

#if defined(__cplusplus)
}
#endif

#endif  // CORE_H_

#if defined(CORE_IMPL)

//
// --------------------------------------------------------------------------------
// :Impl_Intrinsics
// --------------------------------------------------------------------------------
//
// @todo: maybe we want these to be implemented in the header regardless of CORE_IMPL being defined
// because we will likely want all of these to be inlined by the compiler, especially the atomics
//

#if COMPILER_MSVC

#include <intrin.h>

U32 U32LeadingZeroCount(U32 x) {
    unsigned long index; // msvc++ complains that uint32_t and unsigned long are too different??? :ulong

    U32 result = _BitScanForward(&index, x) ? (31 - index) : 32;
    return result;
}

U64 U64LeadingZeroCount(U64 x) {
    unsigned long index; // :ulong

    U64 result = _BitScanForward64(&index, x) ? (63 - index) : 64;
    return result;
}

U32 U32TrailingZeroCount(U32 x) {
    unsigned long index; // :ulong

    U32 result = _BitScanForward(&index, x) ? index : 32;
    return result;
}

U64 U64TrailingZeroCount(U64 x) {
    unsigned long index; // :ulong

    U64 result = _BitScanForward64(&index, x) ? index : 64;
    return result;
}

#if ARCH_AMD64

U32 U32PopCount(U32 x) {
    U32 result = __popcnt(x);
    return result;
}

U64 U64PopCount(U64 x) {
    U64 result = __popcnt64(x);
    return result;
}

#elif ARCH_AARCH64

U32 U32PopCount(U32 x) {
    U32 result = _CountOneBits(x);
    return result;
}

U64 U64PopCount(U64 x) {
    U64 result = _CountOneBits64(x);
    return result;
}

#endif

#elif (COMPILER_CLANG || COMPILER_GCC)

#if ARCH_AMD64
    #include <x86intrin.h>
#elif ARCH_AARCH64
    #include <arm_neon.h>
    // @todo: sve? not that any platform we're aiming for supports it
    // aarch64 is only for Nintendo switch at the moment
    //
#endif

U32 U32LeadingZeroCount(U32 x) {
    U32 result = x ? __builtin_clz(x) : 32;
    return result;
}

U64 U64LeadingZeroCount(U64 x) {
    U64 result = x ? __builtin_clzll(x) : 64;
    return result;
}

U32 U32TrailingZeroCount(U32 x) {
    U32 result = x ? __builtin_ctz(x) : 32;
    return result;
}

U64 U64TrailingZeroCount(U64 x) {
    U64 result = x ? __builtin_ctzll(x) : 64;
    return result;
}

U32 U32PopCount(U32 x) {
    U32 result = __builtin_popcount(x);
    return result;
}

U64 U64PopCount(U64 x) {
    U64 result = __builtin_popcountll(x);
    return result;
}

#endif

// :note msvc has intrinsic functions for rotate but all compilers (under -O2) will
// produce the correct instructions for these, both x64 and aarch64
//

U32 U32RotateLeft(U32 x, U32 count) {
    count &= 31;

    U32 result = (x << count) | (x >> (32 - count));
    return result;
}

U64 U64RotateLeft(U64 x, U32 count) {
    count &= 63;

    U64 result = (x << count) | (x >> (64 - count));
    return result;
}

U32 U32RotateRight(U32 x, U32 count) {
    count &= 31;

    U32 result = (x >> count) | (x << (32 - count));
    return result;
}

U64 U64RotateRight(U64 x, U32 count) {
    count &= 63;

    U64 result = (x >> count) | (x << (64 - count));
    return result;
}

// Atomics
//
#if COMPILER_MSVC

U32 U32AtomicAdd(volatile U32 *ptr, U32 value) {
    U32 result = _InterlockedExchangeAdd((volatile long *) ptr, value);
    return result;
}

U64 U64AtomicAdd(volatile U64 *ptr, U64 value) {
    U64 result = _InterlockedExchangeAdd64((volatile __int64 *) ptr, value);
    return result;
}

U32 U32AtomicExchange(volatile U32 *ptr, U32 exchange) {
    U32 result = _InterlockedExchange((volatile long *) ptr, exchange);
    return result;
}

U64 U64AtomicExchange(volatile U64 *ptr, U64 exchange) {
    U64 result = _InterlockedExchange64((volatile __int64 *) ptr, exchange);
    return result;
}

void *PtrAtomicExchange(void * volatile *ptr, void *exchange) {
    void *result = _InterlockedExchangePointer(ptr, exchange);
    return result;
}

B32 U32AtomicCompareExchange(volatile U32 *ptr, U32 exchange, U32 comparand) {
    B32 result = _InterlockedCompareExchange((volatile long *) ptr, exchange, comparand) == (long) comparand;
    return result;
}

B32 U64AtomicCompareExchange(volatile U64 *ptr, U64 exchange, U64 comparand) {
    B32 result = _InterlockedCompareExchange64((volatile __int64 *) ptr, exchange, comparand) == (long) comparand;
    return result;
}

B32 PtrAtomicCompareExchange(void *volatile *ptr, void *exchange, void *comparand) {
    B32 result = _InterlockedCompareExchangePointer(ptr, exchange, comparand) == comparand;
    return result;
}

#elif (COMPILER_CLANG || COMPILER_GCC)

// :note armv8 has acquire-release atomic semantics and __ATOMIC_SEQ_CST is technically stronger than that
// however, compilers will produce the same asm regardless of which is used so just going to use SEQ_CST
//

U32 U32AtomicAdd(volatile U32 *ptr, U32 value) {
    U32 result = __atomic_fetch_add(ptr, value, __ATOMIC_SEQ_CST);
    return result;
}

U64 U64AtomicAdd(volatile U64 *ptr, U64 value) {
    U64 result = __atomic_fetch_add(ptr, value, __ATOMIC_SEQ_CST);
    return result;
}

U32 U32AtomicExchange(volatile U32 *ptr, U32 exchange) {
    U32 result;

    __atomic_exchange(ptr, &exchange, &result, __ATOMIC_SEQ_CST);
    return result;
}

U64 U64AtomicExchange(volatile U64 *ptr, U64 exchange) {
    U64 result;

    __atomic_exchange(ptr, &exchange, &result, __ATOMIC_SEQ_CST);
    return result;
}

void *PtrAtomicExchange(void *volatile *ptr, void *exchange) {
    void *result;

    __atomic_exchange(ptr, &exchange, &result, __ATOMIC_SEQ_CST);
    return result;
}

B32 U32AtomicCompareExchange(volatile U32 *ptr, U32 exchange, U32 comparand) {
    B32 result = __atomic_compare_exchange(ptr, &comparand, &exchange, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return result;
}

B32 U64AtomicCompareExchange(volatile U64 *ptr, U64 exchange, U64 comparand) {
    B32 result = __atomic_compare_exchange(ptr, &comparand, &exchange, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return result;
}

B32 PtrAtomicCompareExchange(void *volatile *ptr, void *exchange, void *comparand) {
    B32 result = __atomic_compare_exchange(ptr, &comparand, &exchange, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return result;
}

#endif

#if ARCH_AMD64

F32 F32Sqrt(F32 x) {
    F32 result = _mm_cvtss_f32(_mm_sqrt_ss(_mm_set1_ps(x)));
    return result;
}

F64 F64Sqrt(F64 x) {
    __m128d temp;

    temp = _mm_set1_pd(x);
    temp = _mm_sqrt_sd(temp, temp);

    F64 result = _mm_cvtsd_f64(temp);
    return result;
}

F32 F32ApproxInvSqrt(F32 x) {
    F32 result = _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set1_ps(x)));
    return result;
}

F64 F64ApproxInvSqrt(F64 x) {
    // :note sse/avx doesn't have an instruction for double precision rsqrt for some reason
    //
    F64 result = F64InvSqrt(x);
    return result;
}

#elif ARCH_AARCH64

F32 F32Sqrt(F32 x) {
    F32 result = vgetq_lane_f32(vsqrtq_f32(vdupq_n_f32(x)), 0);
    return result;
}

F64 F64Sqrt(F64 x) {
    F64 result = vgetq_lane_f64(vsqrtq_f64(vdupq_n_f64(x)), 0);
    return result;
}

// :note for these vrsqrteq calls there are analogous vrsqrtsq calls which will do
// newton-raphson iteration to converge on a more accurate result.
//
// with some testing, 2 iterations is enough for single precision and 3 iterations for double precision
// to converge on a bit-identical result
//
// @todo: maybe worth testing to see if these are actually faster than doing an fdiv for 1.0 / sqrt
//

F32 F32ApproxInvSqrt(F32 x) {
    F32 result = vgetq_lane_f32(vrsqrteq_f32(vdupq_n_f32(x)), 0);
    return result;
}

F64 F64ApproxInvSqrt(F64 x) {
    F64 result = vgetq_lane_f64(vrsqrteq_f64(vdupq_n_f64(x)), 0);
    return result;
}

#endif

F32 F32InvSqrt(F32 x) {
    F32 result = 1.0f / F32Sqrt(x);
    return result;
}

F64 F64InvSqrt(F64 x) {
    F64 result = 1.0 / F64Sqrt(x);
    return result;
}

//
// --------------------------------------------------------------------------------
// :Impl_Memory_Arena
// --------------------------------------------------------------------------------
//

#include <string.h> // for memcpy

#define ARENA_MIN_LIMIT       (KB(64))
#define ARENA_MIN_COMMIT_SIZE (KB(4))
#define ARENA_MIN_OFFSET      (64)

#if OS_SWITCH
    #define ARENA_DEFAULT_FLAGS (ARENA_FLAG_CHAINED)
#else
    #define ARENA_DEFAULT_FLAGS (0)
#endif

Internal void *OS_MemoryReserve(U64 size);
Internal B32   OS_MemoryCommit(void *base, U64 size);
Internal void  OS_MemoryDecommit(void *base, U64 size);
Internal void  OS_MemoryRelease(void *base, U64 size);

Arena *ArenaAllocArgs(U64 limit, U64 commit_size, ArenaFlags flags) {
    Arena *result = 0;

    B32 chained = (flags & ARENA_FLAG_CHAINED) != 0;
    U64 reserve = AlignUp(limit,       ARENA_MIN_LIMIT);
    U64 commit  = AlignUp(commit_size, ARENA_MIN_COMMIT_SIZE);

    result = cast(Arena *) OS_MemoryReserve(chained ? commit : reserve);
    if (result) {
        if (OS_MemoryCommit(result, commit)) {
            // we have successfully reserved and committed the memory required to store the arena
            // metadata and the memory placed directly after the metadata is used for allocations
            //
            result->current     = result;
            result->prev        = 0;

            result->base        = 0;
            result->offset      = ARENA_MIN_OFFSET;
            result->limit       = reserve;

            result->committed   = commit;
            result->commit_size = commit;

            result->flags       = flags;
            result->pad         = 0;
        }
    }

    return result;
}

Arena *ArenaAlloc(U64 limit) {
    Arena *result = ArenaAllocArgs(limit, ARENA_DEFAULT_COMMIT_SIZE, ARENA_DEFAULT_FLAGS);
    return result;
}

void ArenaReset(Arena *arena) {
    if ((arena->flags & ARENA_FLAG_CHAINED) != 0) {
        // release all in chain apart from the last one as we want the arena to remain valid
        //
        Arena *current = arena->current;
        while (current->prev != 0) {
            void *base = cast(void *) current;
            U64   size = current->limit;

            current = current->prev;

            OS_MemoryRelease(base, size);
        }

        arena->current = current;
    }
    else {
        U64   decommit_size = arena->committed - arena->commit_size;
        void *decommit_base = cast(U8 *) arena + arena->commit_size;

        OS_MemoryDecommit(decommit_base, decommit_size);
    }

    arena->offset    = ARENA_MIN_OFFSET;
    arena->committed = arena->commit_size;
}

void ArenaRelease(Arena *arena) {
    if ((arena->flags & ARENA_FLAG_CHAINED) != 0) {
        // release all arenas in chain including the bottom one as we no longer need any of the
        // backing memory
        //
        Arena *current = arena->current;
        while (current != 0) {
            void *base = (void *) current;
            U64   size = current->limit;

            current = current->prev;

            OS_MemoryRelease(base, size);
        }
    }
    else {
        void *release_base = cast(void *) arena;
        U64   release_size = arena->limit;

        OS_MemoryRelease(release_base, release_size);
    }
}

void *ArenaPushFrom(Arena *arena, U64 size, ArenaFlags flags, U32 alignment) {
    void *result = 0;

    alignment = Clamp(1, alignment, 4096);

    U64 offset = arena->offset;
    U64 align  = AlignUp(offset, cast(U64) alignment) - offset; // @todo: should the U64 cast be done in macro?
    U64 end    = offset + align + size;

    B32 chained = (arena->flags & ARENA_FLAG_CHAINED);

    if (end <= arena->limit) {
        // we have enough space in the arena to put this allocation
        //
        Arena *base = arena->current;

        if (end > arena->committed) {
            // we have not committed enough memory to hold this allocation so commit more
            //
            U64 commit_size = 0;
            U8 *commit_base = 0;

            if (chained) {
                arena->offset += (base->commit_size - base->offset); // we waste the end of this 'block'

                Assert(arena->offset == arena->committed);
                Assert((arena->offset & (alignment - 1)) == 0);

                arena->offset += AlignUp(ARENA_MIN_OFFSET, alignment); // for arena metadata

                base   = 0;
                offset = arena->offset;
                align  = 0;
                end    = offset + size;

                // have to reserve a small amount of space for the arena metadata
                //
                U64 total = size + ARENA_MIN_OFFSET;

                commit_size = AlignUp(total, arena->commit_size);
                if (arena->committed + commit_size > arena->limit) {
                    // don't overcommit memory due to rounding up to commit size
                    //
                    commit_size = arena->limit - arena->committed;
                }

                if (total <= commit_size) {
                    // we still have enough space to reserve/commit the new memory block
                    //
                    commit_base = cast(U8 *) OS_MemoryReserve(commit_size);
                    if (commit_base) {
                        if (OS_MemoryCommit(commit_base, commit_size)) {
                            Arena *current = arena->current;

                            base = cast(Arena *) commit_base;

                            base->current     = arena;
                            base->prev        = current;

                            base->base        = (current->base + current->commit_size);
                            base->offset      = ARENA_MIN_OFFSET;
                            base->limit       = arena->limit;

                            base->committed   = commit_size;
                            base->commit_size = commit_size;

                            base->flags       = arena->flags;

                            arena->current = base;
                        }
                    }
                }
            }
            else {
                commit_size = AlignUp(end, base->commit_size) - base->committed;
                commit_base = cast(U8 *) base + base->committed;

                if (!OS_MemoryCommit(commit_base, commit_size)) {
                    // we failed to commit more memory so force the allocation to fail thus
                    // returning null to the user
                    //
                    base = 0;
                }
            }

            arena->committed += commit_size;
        }

        if (base) {
            result = cast(U8 *) base + (offset - base->base) + align;

            arena->offset = end;              // global offset in total arena space
            base->offset  = end - base->base; // local offset in current arena (if chained), otherwise above

            if ((flags & ARENA_FLAG_NO_ZERO) == 0) {
                // clear the memory to zero unless otherwise specified
                //
                memset(result, 0, size);
            }

            Assert(arena->offset <= arena->limit);
            Assert(arena->offset <= arena->committed);

            Assert(((U64) result & (alignment - 1)) == 0);
        }
    }

    return result;
}

void *ArenaPushCopyFrom(Arena *arena, void *src, U64 size, ArenaFlags flags, U32 alignment) {
    void *result = ArenaPushFrom(arena, size, flags, alignment);

    memcpy(result, src, size);
    return result;
}

void ArenaPopTo(Arena *arena, U64 offset) {
    Assert(arena->offset >= offset);

    U64 mark = Max(offset, ARENA_MIN_OFFSET);

    if ((arena->flags & ARENA_FLAG_CHAINED) != 0) {
        Arena *current = arena->current;
        while (current->base > mark) {
            void *base = cast(void *) current;
            U64   size = current->limit;

            current = current->prev;
            arena->committed -= size;

            OS_MemoryRelease(base, size);
        }

        arena->current  = current;
        current->offset = mark - current->base;
    }
    else {
       U64 decommit_size = AlignDown(arena->offset - mark, arena->commit_size);
       if (decommit_size != 0) {
           void *decommit_base = cast(U8 *) arena + (arena->committed - decommit_size);
           arena->committed   -= decommit_size;

           OS_MemoryDecommit(decommit_base, decommit_size);
       }
    }

    arena->offset = mark;
}

void ArenaPopSize(Arena *arena, U64 size) {
    Assert(arena->offset > ARENA_MIN_OFFSET);
    Assert((arena->offset - ARENA_MIN_OFFSET) >= size);

    ArenaPopTo(arena, arena->offset - size);
}

Internal ThreadVar Arena *__tls_temp[TEMP_ARENA_COUNT];

TempArena TempFrom(Arena *arena) {
    TempArena result;

    result.arena  = arena;
    result.offset = arena->offset;
    return result;
}

TempArena TempGet(U32 count, Arena **conflicts) {
    TempArena result = { 0 };

    for (U32 t = 0; t < TEMP_ARENA_COUNT; ++t) {
        Arena *temp = __tls_temp[t];
        if (!temp) {
            __tls_temp[t] = temp = ArenaAlloc(TEMP_ARENA_DEFAULT_LIMIT);
        }

        for (U32 c = 0; c < count; ++c) {
            if (conflicts[c] == temp) {
                temp = 0;
                break;
            }
        }

        if (temp != 0) {
            result.arena  = temp;
            result.offset = temp->offset;

            break;
        }
    }

    return result;
}

void TempRelease(TempArena *temp) {
    ArenaPopTo(temp->arena, temp->offset);
}

//
// --------------------------------------------------------------------------------
// :Impl_OS_Memory
// --------------------------------------------------------------------------------
//

#if OS_WINDOWS

#if !defined(_WINDOWS_)
    #define MEM_COMMIT   0x00001000
    #define MEM_RESERVE  0x00002000
    #define MEM_DECOMMIT 0x00004000
    #define MEM_RELEASE  0x00008000

    #define PAGE_NOACCESS  0x01
    #define PAGE_READWRITE 0x04

    // msvc/clang properly use the actual windows sdk include definitions however, mingw has its
    // own definitions for windows.h which conflict if you include windows.h after the fact so we
    // have to typedef some stuff...
    //
    typedef int BOOL;
    typedef void *LPVOID;
    typedef unsigned long long SIZE_T;

    #if defined(__CYGWIN__)
        typedef unsigned int DWORD;
    #else
        typedef unsigned long DWORD;
    #endif

    #if defined(__cplusplus)
        extern "C" __declspec(dllimport) LPVOID VirtualAlloc(LPVOID, SIZE_T, DWORD, DWORD);
        extern "C" __declspec(dllimport) BOOL    VirtualFree(LPVOID, SIZE_T, DWORD);
    #else
        extern __declspec(dllimport) LPVOID VirtualAlloc(LPVOID, SIZE_T, DWORD, DWORD);
        extern __declspec(dllimport) BOOL    VirtualFree(LPVOID, SIZE_T, DWORD);
    #endif
#endif

void *OS_MemoryReserve(U64 size) {
    void *result = VirtualAlloc(0, size, MEM_RESERVE, PAGE_NOACCESS);
    return result;
}

B32 OS_MemoryCommit(void *base, U64 size) {
    B32 result = VirtualAlloc(base, size, MEM_COMMIT, PAGE_READWRITE) != 0;
    return result;
}

void OS_MemoryDecommit(void *base, U64 size) {
    VirtualFree(base, size, MEM_DECOMMIT);
}

void OS_MemoryRelease(void *base, U64 size) {
    (void) size; // size must be zero for MEM_RELEASE
    VirtualFree(base, 0, MEM_RELEASE);
}

#elif (OS_MACOS || OS_LINUX)

// @todo: macos does provide mmap but has lower-level virtual memory apis that we will probably want
// to use instead. especially if they allow us to explicitly commit/decommit memory pages. looks like they're
// for kernel development though so probably can't use them in userspace
//

#include <sys/mman.h>

#if !defined(MAP_ANONYMOUS)
    #define MAP_ANONYMOUS MAP_ANON
#endif

void *OS_MemoryReserve(U64 size) {
    void *addr = mmap(0, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    void *result = (addr == MAP_FAILED) ? 0 : addr;
    return result;
}

B32 OS_MemoryCommit(void *base, U64 size) {
    B32 result = mprotect(base, size, PROT_READ | PROT_WRITE) == 0;
    return result;
}

void OS_MemoryDecommit(void *base, U64 size) {
    mprotect(base, size, PROT_NONE);
    madvise (base, size, MADV_DONTNEED);
}

void OS_MemoryRelease(void *base, U64 size) {
    munmap(base, size);
}

#elif OS_SWITCH

// we unfortunatley have to use malloc/free as there are no other allocators that provide general
// purpose address space on switchbrew
//
#include <stdlib.h>

void *OS_MemoryReserve(U64 size) {
    void *result = malloc(size);
    return result;
}

B32 OS_MemoryCommit(void *base, U64 size) {
    (void) base;
    (void) size;

    B32 result = true;
    return result;
}

void OS_MemoryDecommit(void *base, U64 size) {
    (void) base;
    (void) size;
}

void OS_MemoryRelease(void *base, U64 size) {
    (void) size;

    free(base);
}

#endif

#endif  // CORE_IMPL
