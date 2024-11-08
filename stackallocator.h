#include <iostream>
#include <memory>
#include <iterator>
#include <list>

template <size_t N>
struct StackStorage {
    int8_t data[N];
    size_t size;

    // NOLINTNEXTLINE
    StackStorage() : size(0) {}
};

template <typename T, size_t N>
struct StackAllocator {
    StackStorage<N>* st;

    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    template <typename U>
    // NOLINTNEXTLINE
    struct rebind {
        using other = StackAllocator<U, N>;
    };

    template <typename U>
    StackAllocator(const StackAllocator<U, N>& copy) : st(copy.st) {}

    StackAllocator(StackStorage<N>& st) : st(&st) {}
    StackAllocator(StackStorage<N>* st) : st(st) {}

    T* allocate(size_t count) const {
        size_t padding = st->size % alignof(T);
        if (padding > 0) {
            padding = alignof(T) - padding;
        }
        if (st->size + count * sizeof(T) + padding > N) {
            throw std::bad_alloc();
        }
        st->size += padding;
        T *tmp = reinterpret_cast<T *>(st->data + st->size);
        st->size += count * sizeof(T);
        return tmp;
    }

    void deallocate(T* ptr, size_t count) const {
        if (reinterpret_cast<int8_t*>(ptr + count) == st->data + st->size) {
            st->size -= count * sizeof(T);
        }
    }

};


template <typename T, typename Allocator = std::allocator<T>>
class List {
public:
    using value_type = T;
    struct BaseNode {
        BaseNode* prev = this;
        BaseNode* next = this;
    };

    struct Node : BaseNode {
        T key;

        Node(BaseNode* prev, BaseNode* next, const T& key) : BaseNode{prev, next}, key(key) {}
        Node(BaseNode* prev, BaseNode* next) : BaseNode{prev, next}, key() {}
    };


private:
    using NodeAlloc = typename std::allocator_traits<Allocator>::template rebind_alloc<Node>;
    using NodeTraits = typename std::allocator_traits<NodeAlloc>;

    [[ no_unique_address ]] NodeAlloc alloc;
    mutable BaseNode data;
    size_t sz = 0;

    void emplace_back() {
        Node *it = NodeTraits::allocate(alloc, 1);
        try {
            NodeTraits::construct(alloc, it, data.prev, &data);
        } catch (...) {
            NodeTraits::deallocate(alloc, it, 1);
            throw;
        }

        data.prev->next = it;
        data.prev = it;
        ++sz;
    }

    void destroy() {
        BaseNode* it = data.next;
        while (it != &data) {
            BaseNode* tmp = it->next;
            NodeTraits::destroy(alloc, static_cast<Node*>(it));
            NodeTraits::deallocate(alloc, static_cast<Node*>(it), 1);
            it = tmp;
        }
    }


public:

    List() = default;

    explicit List(const Allocator& allocator) :  alloc(allocator) {}

    List(size_t count, const T& value, const Allocator& allocator = Allocator()) : alloc(allocator) {
        try {
            for (size_t i = 0; i < count; ++i) {
                push_back(value);
            }
        } catch (...) {
            destroy();
            throw;
        }
    }

    explicit List(size_t count, const Allocator& allocator = Allocator()) : alloc(allocator) {
        try {
            for (size_t i = 0; i < count; ++i) {
                emplace_back();
            }
        } catch (...) {
            destroy();
            throw;
        }
    }

    List(const List& copy, const Allocator& alloc) : alloc(alloc) {
        try {
            for (auto it = copy.begin(); it != copy.end(); ++it) {
                push_back(*it);
            }
        } catch (...) {
            destroy();
            throw;
        }
    }

    List(const List& copy) : List(copy, NodeTraits::select_on_container_copy_construction(copy.alloc)) {}

    List& operator=(const List& copy) {
        List res(copy, std::allocator_traits<Allocator>::propagate_on_container_copy_assignment::value ?
                       copy.alloc : alloc);
        std::swap(alloc, res.alloc);
        std::swap(data, res.data);
        sz = res.sz;
        data.next->prev = &data;
        data.prev->next = &data;
        res.data.prev->next = &res.data;
        return *this;
    }

    using allocator_type = Allocator;

    allocator_type get_allocator() const {
        return alloc;
    }

    size_t size() const {
        return sz;
    }

    template <typename Value>
    struct BaseIterator {
    public:
        friend class List;
        using value_type = Value;
        using pointer = value_type*;
        using reference = value_type&;
        using difference_type = ptrdiff_t;
        using const_reference = const value_type&;
        using iterator_category = std::bidirectional_iterator_tag;

    private:
        BaseNode* item;

    public:
        BaseIterator() = default;

        BaseIterator(BaseNode* item) : item(item) {}

        operator BaseIterator<const Value>() const {
            return BaseIterator<const Value>(item);
        }

        reference operator*() const {
            return static_cast<Node*>(item)->key;
        }

        pointer operator->() const {
            return &operator*();
        }

        BaseIterator& operator++() {
            item = item->next;
            return *this;
        }

        BaseIterator operator++(int) {
            auto copy = *this;
            ++*this;
            return copy;
        }

        BaseIterator& operator--() {
            item = item->prev;
            return *this;
        }

        BaseIterator operator--(int) {
            auto copy = *this;
            --*this;
            return copy;
        }

        bool operator==(const BaseIterator<const T>& other) const {
            return item == other.item;
        }

        bool operator==(const BaseIterator<T>& other) const {
            return item == other.item;
        }
    };

    using iterator = BaseIterator<T>;
    using const_iterator = BaseIterator<const T>;
    using reverse_iterator = std::reverse_iterator<BaseIterator<T>>;
    using const_reverse_iterator = std::reverse_iterator<BaseIterator<const T>>;

    iterator begin() {
        return iterator(data.next);
    }
    iterator end() {
        return iterator(&data);
    }

    const_iterator begin() const {
        return const_iterator(data.next);
    }
    const_iterator end() const {
        return const_iterator(&data);
    }

    const_iterator cbegin() const {
        return const_iterator(data.next);
    }
    const_iterator cend() const {
        return const_iterator(&data);
    }

    reverse_iterator rbegin() {
        return reverse_iterator(end());
    }
    reverse_iterator rend() {
        return reverse_iterator(begin());
    }

    const_reverse_iterator rbegin() const {
        return const_reverse_iterator(cend());
    }
    const_reverse_iterator rend() const {
        return const_reverse_iterator(begin());
    }

    const_reverse_iterator crbegin() const {
        return const_reverse_iterator(cend());
    }
    const_reverse_iterator crend() const {
        return const_reverse_iterator(cbegin());
    }


    void push_back(const T& value) {
        insert(end(), value);
    }

    void pop_back() {
        erase(--end());
    }

    void push_front(const T& value) {
        insert(begin(), value);
    }

    void pop_front() {
        erase(begin());
    }

    iterator insert(const_iterator iter, const T& value) {
        Node* it = NodeTraits::allocate(alloc, 1);
        try {
            NodeTraits::construct(alloc, it, iter.item->prev, iter.item, value);
        } catch (...) {
            NodeTraits::deallocate(alloc, it, 1);
            throw;
        }

        iter.item->prev->next = it;
        iter.item->prev = it;
        ++sz;
        return iterator(it);
    }

    iterator erase(const_iterator iter) {
        BaseNode* it = iter.item->next;
        it->prev = iter.item->prev;
        iter.item->prev->next = it;

        NodeTraits::destroy(alloc, static_cast<Node*>(iter.item));
        NodeTraits::deallocate(alloc, static_cast<Node*>(iter.item), 1);
        --sz;
        return it;
    }

    void clear() {
        destroy();
        data.prev = data.next = &data;
        sz = 0;
    }

    ~List() {
        destroy();
    }
};
