#pragma once

#define STRINGIFY(...) PRIMITIVE_STRINGIFY(__VA_ARGS__)
#define PRIMITIVE_STRINGIFY(...) #__VA_ARGS__

#define EAT(...)

#define CAT(...) PRIMITIVE_CAT(__VA_ARGS__)
#define PRIMITIVE_CAT(a, ...) a##__VA_ARGS__

#define REV_CAT(...) PRIMITIVE_REV_CAT(__VA_ARGS__)
#define PRIMITIVE_REV_CAT(b, ...) __VA_ARGS__##b

#define PRIMITIVE_DOUBLE_CAT(a, b, ...) a##b##__VA_ARGS__

#define EMPTY()
#define EXPAND(...) __VA_ARGS__
#define DEFER1(...) __VA_ARGS__ EMPTY()
#define DEFER2(...) __VA_ARGS__ EMPTY EMPTY()()
#define DEFER3(...) __VA_ARGS__ EMPTY EMPTY EMPTY()()()

#define LPAREN() (
#define RPAREN() )
#define COMMA() ,

// Syntax: FOLDR(macro, sequence, accumulator)
// E.g.
//   FOLDR(XX, (1)(2)(3,4)(5)()(6), 42)
// expands to:
//   XX((1), XX((2), XX((3,4), XX((5), XX((), XX((6), 42))))))
#define FOLDR(macro, seq, ...)                                         \
    IMPL_FOLDR_EXTRACT_RES(EXPAND(EXPAND(IMPL_FOLDR_PREPARE_PREFIX(seq \
    ) DEFER2(IMPL_FOLDR_EVAL_USING_START)(macro, __VA_ARGS__) IMPL_FOLDR_PREPARE_SUFFIX(seq))))
#define IMPL_FOLDR_EXTRACT_RES(...) IMPL_FOLDR_EXTRACT_RES2(__VA_ARGS__)
#define IMPL_FOLDR_EXTRACT_RES2(macro, ...) __VA_ARGS__
#define IMPL_FOLDR_EVAL_USING(...) IMPL_FOLDR_EVAL_USING_IMPL(__VA_ARGS__)
#define IMPL_FOLDR_EVAL_USING_IMPL(args, macro, ...) macro, macro(args, __VA_ARGS__)
#define IMPL_FOLDR_EVAL_USING_START(macro, ...) macro, __VA_ARGS__

#define IMPL_FOLDR_PREPARE_PREFIX(seq) REV_CAT(_END, IMPL_FOLDR_PREPARE_PREFIX_1 seq)
#define IMPL_FOLDR_PREPARE_PREFIX_1_END
#define IMPL_FOLDR_PREPARE_PREFIX_2_END
#define IMPL_FOLDR_PREPARE_PREFIX_1(...) \
    IMPL_FOLDR_PREPARE_PREFIX_3(__VA_ARGS__) IMPL_FOLDR_PREPARE_PREFIX_2
#define IMPL_FOLDR_PREPARE_PREFIX_2(...) \
    IMPL_FOLDR_PREPARE_PREFIX_3(__VA_ARGS__) IMPL_FOLDR_PREPARE_PREFIX_1
#define IMPL_FOLDR_PREPARE_PREFIX_3(...) \
    IMPL_FOLDR_EVAL_USING DEFER2(LPAREN)()(__VA_ARGS__)DEFER2(COMMA)()

#define IMPL_FOLDR_PREPARE_SUFFIX(seq) REV_CAT(_END, IMPL_FOLDR_PREPARE_SUFFIX_1 seq)
#define IMPL_FOLDR_PREPARE_SUFFIX_1_END
#define IMPL_FOLDR_PREPARE_SUFFIX_2_END
#define IMPL_FOLDR_PREPARE_SUFFIX_1(...) IMPL_FOLDR_PREPARE_SUFFIX_3 IMPL_FOLDR_PREPARE_SUFFIX_2
#define IMPL_FOLDR_PREPARE_SUFFIX_2(...) IMPL_FOLDR_PREPARE_SUFFIX_3 IMPL_FOLDR_PREPARE_SUFFIX_1
#define IMPL_FOLDR_PREPARE_SUFFIX_3 DEFER2(RPAREN)()

// MAP(XX, (1)(2, 3)(4)) expands to: XX(1) XX(2, 3) XX(4)
#define MAP(macro, seq) IMPL_MAP_EXTRACT_RES(FOLDR(IMPL_MAP_APPLY, seq, macro, ))
#define IMPL_MAP_APPLY(args, macro, ...) macro, macro args __VA_ARGS__
#define IMPL_MAP_EXTRACT_RES(...) IMPL_MAP_EXTRACT_RES2(__VA_ARGS__)
#define IMPL_MAP_EXTRACT_RES2(a, ...) __VA_ARGS__

// MAP_DELIM(XX, delim, (1)(2, 3)(4)) expands to: XX(1) delim XX(2, 3) delim X(4)
#define MAP_DELIM(macro, delimiter, seq) MAP_DELIM_FUNC(macro, delimiter EAT, seq)
// MAP_DELIM_FUNC(XX, delim, (1)(2, 3)(4)) expands to: XX(1) delim() XX(2, 3) delim() X(4)
#define MAP_DELIM_FUNC(macro, delimiter_func, seq) \
    IMPL_MAP_DELIM_FUNC(macro, delimiter_func, FOLDR(IMPL_MAP_DELIM_FUNC_SPLIT_FIRST_ARG, seq, , ))

#define IMPL_MAP_DELIM_FUNC(...) IMPL_MAP_DELIM_FUNC2(__VA_ARGS__)
#define IMPL_MAP_DELIM_FUNC_SPLIT_FIRST_ARG(arg, prev_arg, acc) arg, prev_arg acc
#define IMPL_MAP_DELIM_FUNC2(macro, delimiter_func, first_args, rest_args)   \
    MAP(macro, first_args)                                                   \
    IMPL_MAP_DELIM_FUNC_EXTRACT_RES(                                         \
        FOLDR(IMPL_MAP_DELIM_FUNC_APPLY, rest_args, macro, delimiter_func, ) \
    )
#define IMPL_MAP_DELIM_FUNC_APPLY(args, macro, delimiter_func, ...) \
    macro, delimiter_func, delimiter_func() macro args __VA_ARGS__
#define IMPL_MAP_DELIM_FUNC_EXTRACT_RES(...) IMPL_MAP_DELIM_FUNC_EXTRACT_RES2(__VA_ARGS__)
#define IMPL_MAP_DELIM_FUNC_EXTRACT_RES2(macro, delimiter_func, ...) __VA_ARGS__
