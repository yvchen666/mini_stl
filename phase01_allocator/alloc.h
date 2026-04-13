#pragma once
// alloc.h — SGI STL 风格双层分配器（C++11 重写）
//
// 架构：
//   MallocAlloc  (一级) — 直接 malloc/free，处理大块内存和 OOM
//   PoolAlloc    (二级) — freelist 内存池，处理 <= 128 字节的小块
//   Allocator<T> — 标准 allocator 接口，包装上面两个
//
// 二级分配器核心设计：
//   - 将 [1,128] 字节分为 16 档，步长 8（即 8,16,24,...,128）
//   - 每档维护一条单向空闲链表（free_list）
//   - 内存池（memory_pool）统一向 malloc 申请大块，切分后分发给 free_list
//   - 归还时只挂回 free_list，不还给 OS（进程结束才释放）

#include <cstdlib>    // malloc, free
#include <cstring>    // memcpy
#include <stdexcept>  // std::bad_alloc
#include <new>        // std::bad_alloc

namespace mystl {

// ============================================================
// 一级分配器：MallocAlloc
// ============================================================
class MallocAlloc {
public:
    static void* allocate(std::size_t n) {
        void* p = std::malloc(n);
        if (!p) {
            p = oom_malloc(n);  // OOM 处理
        }
        return p;
    }

    static void deallocate(void* p, std::size_t /*n*/) {
        std::free(p);
    }

    static void* reallocate(void* p, std::size_t /*old_sz*/, std::size_t new_sz) {
        void* r = std::realloc(p, new_sz);
        if (!r) r = oom_realloc(p, new_sz);
        return r;
    }

    // 设置 OOM 处理函数（类似 set_new_handler）
    using OomHandler = void (*)();
    static OomHandler set_malloc_handler(OomHandler f) {
        OomHandler old = oom_handler_;
        oom_handler_ = f;
        return old;
    }

private:
    static OomHandler oom_handler_;

    static void* oom_malloc(std::size_t n) {
        for (;;) {
            if (!oom_handler_) throw std::bad_alloc();
            oom_handler_();
            void* p = std::malloc(n);
            if (p) return p;
        }
    }

    static void* oom_realloc(void* p, std::size_t n) {
        for (;;) {
            if (!oom_handler_) throw std::bad_alloc();
            oom_handler_();
            void* r = std::realloc(p, n);
            if (r) return r;
        }
    }
};

MallocAlloc::OomHandler MallocAlloc::oom_handler_ = nullptr;

// ============================================================
// 二级分配器：PoolAlloc
// ============================================================
class PoolAlloc {
    // 内存块大小对齐到 8 字节，最大 128 字节
    static constexpr std::size_t ALIGN      = 8;
    static constexpr std::size_t MAX_BYTES  = 128;
    static constexpr std::size_t N_FREELISTS = MAX_BYTES / ALIGN;  // 16

    // freelist 节点：用 union 复用内存
    // 当节点空闲时，用 free_list_link 指向下一个节点
    // 当节点已分配时，这块内存完全归用户使用
    union FreeNode {
        FreeNode*   free_list_link;
        char        client_data[1];  // 占位，实际大小由分配时决定
    };

    // 将 n 字节向上对齐到 ALIGN 的倍数
    static std::size_t round_up(std::size_t n) {
        return (n + ALIGN - 1) & ~(ALIGN - 1);
    }

    // 根据大小计算落在哪条 freelist（0-based index）
    static std::size_t freelist_index(std::size_t n) {
        return (n + ALIGN - 1) / ALIGN - 1;
    }

    // 16 条 freelist，每条对应一档大小
    static FreeNode* free_list_[N_FREELISTS];

    // 内存池的游标：[pool_start_, pool_end_) 是当前可用的原始内存
    static char*       pool_start_;
    static char*       pool_end_;
    static std::size_t heap_size_;  // 向 malloc 累计申请的字节数（用于扩容策略）

    // 从内存池切出 n_obj 个大小为 size 的块，挂到 free_list[idx]
    // 如果内存池不足，先向 OS 申请补充
    static void* refill(std::size_t size) {
        int n_obj = 20;
        char* chunk = chunk_alloc(size, n_obj);

        if (n_obj == 1) return chunk;  // 只分到一个，直接返回，不挂链

        std::size_t idx = freelist_index(size);
        FreeNode* result = reinterpret_cast<FreeNode*>(chunk);

        // 把剩余 n_obj-1 个块串成链表，挂到 free_list
        FreeNode* cur = reinterpret_cast<FreeNode*>(chunk + size);
        free_list_[idx] = cur;
        for (int i = 1; ; ++i) {
            FreeNode* next = reinterpret_cast<FreeNode*>(
                reinterpret_cast<char*>(cur) + size);
            if (i == n_obj - 1) {
                cur->free_list_link = nullptr;
                break;
            }
            cur->free_list_link = next;
            cur = next;
        }
        return result;
    }

    // 从内存池分配 n_obj 个 size 字节的块（尽力而为，实际可能少于 n_obj）
    // n_obj 通过引用传入，返回时反映实际分配数量
    static char* chunk_alloc(std::size_t size, int& n_obj) {
        std::size_t total    = size * n_obj;
        std::size_t left     = pool_end_ - pool_start_;

        if (left >= total) {
            // 内存池有足够空间
            char* r = pool_start_;
            pool_start_ += total;
            return r;
        } else if (left >= size) {
            // 至少能分出一个块
            n_obj = static_cast<int>(left / size);
            char* r = pool_start_;
            pool_start_ += size * n_obj;
            return r;
        } else {
            // 内存池残余连一个块都放不下，先把残余挂回合适的 freelist
            if (left > 0) {
                std::size_t idx = freelist_index(left);
                reinterpret_cast<FreeNode*>(pool_start_)->free_list_link =
                    free_list_[idx];
                free_list_[idx] = reinterpret_cast<FreeNode*>(pool_start_);
            }

            // 向 malloc 申请新内存，申请量 = 2 * total + 历史增量/16
            std::size_t bytes_to_get = 2 * total + round_up(heap_size_ >> 4);
            pool_start_ = static_cast<char*>(std::malloc(bytes_to_get));
            if (!pool_start_) {
                // malloc 失败：在更大档位的 freelist 里找一块应急
                for (std::size_t s = size + ALIGN; s <= MAX_BYTES; s += ALIGN) {
                    std::size_t idx = freelist_index(s);
                    if (free_list_[idx]) {
                        pool_start_ = reinterpret_cast<char*>(free_list_[idx]);
                        free_list_[idx] = free_list_[idx]->free_list_link;
                        pool_end_ = pool_start_ + s;
                        // 递归重试，此时至少能分出一个块
                        return chunk_alloc(size, n_obj);
                    }
                }
                // 彻底没内存，交给一级分配器触发 OOM 处理
                pool_end_ = nullptr;
                pool_start_ = static_cast<char*>(
                    MallocAlloc::allocate(bytes_to_get));
            }
            heap_size_  += bytes_to_get;
            pool_end_    = pool_start_ + bytes_to_get;
            return chunk_alloc(size, n_obj);  // 递归，现在肯定有内存了
        }
    }

public:
    static void* allocate(std::size_t n) {
        if (n > MAX_BYTES) {
            return MallocAlloc::allocate(n);
        }
        std::size_t idx = freelist_index(n);
        FreeNode* node = free_list_[idx];
        if (!node) {
            // freelist 为空，向内存池要一批
            return refill(round_up(n));
        }
        // 从 freelist 头部摘出一个块
        free_list_[idx] = node->free_list_link;
        return node;
    }

    static void deallocate(void* p, std::size_t n) {
        if (n > MAX_BYTES) {
            MallocAlloc::deallocate(p, n);
            return;
        }
        std::size_t idx = freelist_index(n);
        FreeNode* node = reinterpret_cast<FreeNode*>(p);
        // 头插法挂回 freelist
        node->free_list_link = free_list_[idx];
        free_list_[idx] = node;
    }

    static void* reallocate(void* p, std::size_t old_sz, std::size_t new_sz) {
        if (old_sz > MAX_BYTES && new_sz > MAX_BYTES) {
            return MallocAlloc::reallocate(p, old_sz, new_sz);
        }
        if (round_up(old_sz) == round_up(new_sz)) return p;  // 同一档，不用动
        void* r = allocate(new_sz);
        std::size_t copy_sz = old_sz < new_sz ? old_sz : new_sz;
        std::memcpy(r, p, copy_sz);
        deallocate(p, old_sz);
        return r;
    }
};

// 静态成员定义
PoolAlloc::FreeNode* PoolAlloc::free_list_[PoolAlloc::N_FREELISTS] = {};
char*       PoolAlloc::pool_start_ = nullptr;
char*       PoolAlloc::pool_end_   = nullptr;
std::size_t PoolAlloc::heap_size_  = 0;

// ============================================================
// 标准 Allocator<T> 接口 — 包装 PoolAlloc
// ============================================================
template <typename T>
class Allocator {
public:
    using value_type      = T;
    using pointer         = T*;
    using const_pointer   = const T*;
    using reference       = T&;
    using const_reference = const T&;
    using size_type       = std::size_t;
    using difference_type = std::ptrdiff_t;

    template <typename U>
    struct rebind { using other = Allocator<U>; };

    Allocator() noexcept = default;
    template <typename U>
    Allocator(const Allocator<U>&) noexcept {}

    pointer allocate(size_type n) {
        return static_cast<pointer>(PoolAlloc::allocate(n * sizeof(T)));
    }

    void deallocate(pointer p, size_type n) {
        PoolAlloc::deallocate(p, n * sizeof(T));
    }

    pointer address(reference x) const noexcept { return &x; }
    const_pointer address(const_reference x) const noexcept { return &x; }
    size_type max_size() const noexcept {
        return std::size_t(-1) / sizeof(T);
    }

    template <typename U, typename... Args>
    void construct(U* p, Args&&... args) {
        ::new (static_cast<void*>(p)) U(std::forward<Args>(args)...);
    }

    template <typename U>
    void destroy(U* p) { p->~U(); }
};

template <typename T, typename U>
bool operator==(const Allocator<T>&, const Allocator<U>&) noexcept { return true; }
template <typename T, typename U>
bool operator!=(const Allocator<T>&, const Allocator<U>&) noexcept { return false; }

} // namespace mystl
