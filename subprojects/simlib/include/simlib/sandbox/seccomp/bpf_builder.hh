#pragma once

#include <cstdint>
#include <seccomp.h>
#include <simlib/errmsg.hh>
#include <simlib/file_descriptor.hh>
#include <simlib/macros/throw.hh>
#include <sys/mman.h>

namespace sandbox::seccomp {

#define DECLARE_FOR_ARG(arg_num)      \
    struct ARG##arg_num##_EQ {        \
        uint64_t datum;               \
    };                                \
    struct ARG##arg_num##_NE {        \
        uint64_t datum;               \
    };                                \
    struct ARG##arg_num##_LT {        \
        uint64_t datum;               \
    };                                \
    struct ARG##arg_num##_LE {        \
        uint64_t datum;               \
    };                                \
    struct ARG##arg_num##_GT {        \
        uint64_t datum;               \
    };                                \
    struct ARG##arg_num##_GE {        \
        uint64_t datum;               \
    };                                \
    struct ARG##arg_num##_MASKED_EQ { \
        uint64_t mask;                \
        uint64_t datum;               \
    };

DECLARE_FOR_ARG(0)
DECLARE_FOR_ARG(1)
DECLARE_FOR_ARG(2)
DECLARE_FOR_ARG(3)
DECLARE_FOR_ARG(4)
DECLARE_FOR_ARG(5)
#undef DECLARE_FOR_ARG

class BpfBuilder {
    scmp_filter_ctx seccomp_ctx;

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc99-extensions"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#endif

#define DEFINE_FOR_ARG(arg_num)                                                                  \
    static constexpr auto arg_cmp_to_seccomp_native(const ARG##arg_num##_EQ& arg_cmp) noexcept { \
        return SCMP_A##arg_num(SCMP_CMP_EQ, arg_cmp.datum);                                      \
    }                                                                                            \
    static constexpr auto arg_cmp_to_seccomp_native(const ARG##arg_num##_NE& arg_cmp) noexcept { \
        return SCMP_A##arg_num(SCMP_CMP_NE, arg_cmp.datum);                                      \
    }                                                                                            \
    static constexpr auto arg_cmp_to_seccomp_native(const ARG##arg_num##_LT& arg_cmp) noexcept { \
        return SCMP_A##arg_num(SCMP_CMP_LT, arg_cmp.datum);                                      \
    }                                                                                            \
    static constexpr auto arg_cmp_to_seccomp_native(const ARG##arg_num##_LE& arg_cmp) noexcept { \
        return SCMP_A##arg_num(SCMP_CMP_LE, arg_cmp.datum);                                      \
    }                                                                                            \
    static constexpr auto arg_cmp_to_seccomp_native(const ARG##arg_num##_GT& arg_cmp) noexcept { \
        return SCMP_A##arg_num(SCMP_CMP_GT, arg_cmp.datum);                                      \
    }                                                                                            \
    static constexpr auto arg_cmp_to_seccomp_native(const ARG##arg_num##_GE& arg_cmp) noexcept { \
        return SCMP_A##arg_num(SCMP_CMP_GE, arg_cmp.datum);                                      \
    }                                                                                            \
    static constexpr auto arg_cmp_to_seccomp_native(const ARG##arg_num##_MASKED_EQ& arg_cmp      \
    ) noexcept {                                                                                 \
        return SCMP_A##arg_num(SCMP_CMP_MASKED_EQ, arg_cmp.mask, arg_cmp.datum);                 \
    }

    DEFINE_FOR_ARG(0)
    DEFINE_FOR_ARG(1)
    DEFINE_FOR_ARG(2)
    DEFINE_FOR_ARG(3)
    DEFINE_FOR_ARG(4)
    DEFINE_FOR_ARG(5)
#undef DEFINE_FOR_ARG

#ifdef __clang__
#pragma clang diagnostic pop
#endif

public:
    explicit BpfBuilder(uint32_t def_action) : seccomp_ctx{seccomp_init(def_action)} {
        if (!seccomp_ctx) {
            THROW("seccom_init() failed");
        }

        // Enable binary tree sorted syscalls in the filter
        int err = seccomp_attr_set(seccomp_ctx, SCMP_FLTATR_CTL_OPTIMIZE, 2);
        if (err) {
            THROW("seccomp_attr_set()", errmsg(-err));
        }
    }

    BpfBuilder(const BpfBuilder&) = delete;
    BpfBuilder(BpfBuilder&&) = delete;
    BpfBuilder& operator=(const BpfBuilder&) = delete;
    BpfBuilder& operator=(BpfBuilder&&) = delete;

    template <class... Args>
    void allow_syscall(int syscall, Args&&... args) {
        int err = seccomp_rule_add(
            seccomp_ctx,
            SCMP_ACT_ALLOW,
            syscall,
            sizeof...(args),
            arg_cmp_to_seccomp_native(std::forward<Args>(args))...
        );
        if (err) {
            THROW("seccomp_rule_add()", errmsg(-err));
        }
    }

    template <class... Args>
    void err_syscall(int errnum, int syscall, Args&&... args) {
        int err = seccomp_rule_add(
            seccomp_ctx,
            SCMP_ACT_ERRNO(errnum),
            syscall,
            sizeof...(args),
            arg_cmp_to_seccomp_native(std::forward<Args>(args))...
        );
        if (err) {
            THROW("seccomp_rule_add()", errmsg(-err));
        }
    }

    [[nodiscard]] FileDescriptor export_to_fd() const {
        auto mfd = FileDescriptor{memfd_create("seccomp bpf", MFD_CLOEXEC)};
        if (!mfd.is_open()) {
            THROW("memfd_create()", errmsg());
        }

        int err = seccomp_export_bpf(seccomp_ctx, mfd);
        if (err) {
            THROW("seccomp_export_bpf()", errmsg(-err));
        }

        return mfd;
    }

    ~BpfBuilder() { seccomp_release(seccomp_ctx); }
};

} // namespace sandbox::seccomp
