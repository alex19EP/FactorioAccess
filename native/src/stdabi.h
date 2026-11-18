#pragma once

// The game's standard library containers as its compiler lays them out, for reading them in the
// game's memory and for building the few the game reads from us. Every layout rule of the game's
// standard library (MSVC's on Windows) lives here; the readers elsewhere go through these types.
// Each type is the container object itself, so `*reinterpret_cast<const String*>(address)` reads
// the std::string at `address`.

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>
#include <utility>

namespace fa::stdabi {

// std::basic_string: a 16-byte small buffer or a heap pointer, then size and capacity. The text
// is on the heap once the capacity no longer fits the small buffer.
template <class Char>
struct BasicString {
   static constexpr size_t kSmall = 16 / sizeof(Char);

   union {
      Char buffer[kSmall];
      Char* pointer;
   };
   size_t size;
   size_t capacity;

   bool onHeap() const { return capacity >= kSmall; }
   Char* data() { return onHeap() ? pointer : buffer; }
   const Char* data() const { return onHeap() ? pointer : buffer; }
   std::basic_string_view<Char> view() const { return {data(), size}; }

   // The block the game's operator delete takes to free the text, or null when it is in the small
   // buffer. The allocator over-aligns blocks of 4 KiB and more and keeps their true start just
   // before the text.
   void* allocation() const {
      if (!onHeap()) return nullptr;
      void* block = pointer;
      if ((capacity + 1) * sizeof(Char) >= 0x1000) block = reinterpret_cast<void**>(block)[-1];
      return block;
   }

   // `text` (null-terminated, `length` characters) as a string for a game function that takes a
   // std::string const& and copies it. A short text is copied into the small buffer; a long one is
   // pointed at, so it must stay alive and unchanged for the call. The game never frees or grows it.
   static BasicString borrow(const Char* text, size_t length) {
      BasicString out{};
      out.size = length;
      if (length < kSmall) {
         for (size_t i = 0; i <= length; ++i) out.buffer[i] = text[i];
         out.capacity = kSmall - 1;
      } else {
         out.pointer = const_cast<Char*>(text);
         out.capacity = length;
      }
      return out;
   }
};

using String = BasicString<char>;
using WString = BasicString<wchar_t>;
static_assert(sizeof(String) == 32 && sizeof(WString) == 32);

// std::vector: first element, past the last, past the end of the storage. T may be a const
// std::byte for elements whose size the PDB gives; then the pointers count bytes.
template <class T>
struct Vector {
   T* first;
   T* last;
   T* storageEnd;

   T* begin() const { return first; }
   T* end() const { return last; }
   size_t size() const { return static_cast<size_t>(last - first); }
   bool empty() const { return first == last; }
   T& operator[](size_t index) const { return first[index]; }
};
static_assert(sizeof(Vector<int>) == 24);

// A node of std::map and std::set (a red-black tree): its links, its colour, whether it is the
// head (the end marker, which every leaf links to), then the value, which for a map is the
// std::pair<const Key, T>. Value's own alignment places it, as the game's compiler placed it.
template <class Value>
struct TreeNode {
   TreeNode* left;
   TreeNode* parent;
   TreeNode* right;
   char color;
   char isNil;
   Value value;
};

// A node whose value the DLL reads at offsets from the node's start that the PDB gives (the
// _Myval members of std::_Tree_node).
template <>
struct TreeNode<void> {
   TreeNode* left;
   TreeNode* parent;
   TreeNode* right;
   char color;
   char isNil;
};

// Where a node's value starts when the DLL knows only a type with the value's alignment.
template <class SameAlignment>
constexpr uint32_t treeValueOffset() {
   return static_cast<uint32_t>(offsetof(TreeNode<SameAlignment>, value));
}
static_assert(treeValueOffset<double>() == 0x20 && treeValueOffset<uint16_t>() == 0x1a);

// std::map and std::set: the head node, then the element count. The head's parent is the root,
// its left the first node and its right the last. Iterates in key order. Tree<void> is a tree
// whose value type the DLL knows only by the PDB's offsets: iterating it yields each node's
// address, from which those offsets count.
template <class Value>
struct Tree {
   using Node = TreeNode<Value>;

   class Iterator {
   public:
      explicit Iterator(const Node* node) : _node(node) {}

      const Node* node() const { return _node; }
      // The node's own address, from which the PDB's _Myval offsets count.
      const std::byte* address() const { return reinterpret_cast<const std::byte*>(_node); }
      // The value, or for Value void the node's address.
      decltype(auto) operator*() const {
         if constexpr (std::is_void_v<Value>)
            return address();
         else
            return (std::as_const(_node->value));
      }
      const auto* operator->() const
         requires(!std::is_void_v<Value>)
      {
         return &_node->value;
      }

      // The in-order successor: the leftmost node of the right subtree, or the first ancestor
      // this subtree is on the left of; the head after the last node.
      Iterator& operator++() {
         if (!_node->right->isNil) {
            _node = _node->right;
            while (!_node->left->isNil) _node = _node->left;
            return *this;
         }
         const Node* parent = _node->parent;
         while (!parent->isNil && _node == parent->right) {
            _node = parent;
            parent = parent->parent;
         }
         _node = parent;
         return *this;
      }
      bool operator==(const Iterator&) const = default;

   private:
      const Node* _node;
   };

   Node* head;
   size_t count;

   Iterator begin() const { return Iterator(head->left); }
   Iterator end() const { return Iterator(head); }
   size_t size() const { return count; }
   bool empty() const { return count == 0; }

   // The node `order` finds by a search down from the root, or null. order(node) compares the
   // node's key with the one sought: negative when the node's key is less, positive when more,
   // zero for the node sought.
   template <class Order>
   const Node* search(Order order) const {
      for (const Node* node = head->parent; !node->isNil;) {
         const auto result = order(node);
         if (result == 0) return node;
         node = result < 0 ? node->right : node->left;
      }
      return nullptr;
   }
};
static_assert(sizeof(Tree<void>) == 16);

// std::deque from its _Map member on: the block map, its size (a power of two), the index of the
// first element counted from the map's first block, and the element count. The elements are in
// blocks of kBlock, a constant of the library that depends on the element's size.
template <class T>
struct Deque {
   static constexpr size_t kBlock = sizeof(T) <= 1   ? 16
                                    : sizeof(T) <= 2 ? 8
                                    : sizeof(T) <= 4 ? 4
                                    : sizeof(T) <= 8 ? 2
                                                     : 1;

   T** map;
   size_t mapSize;
   size_t offset;
   size_t count;

   size_t size() const { return count; }
   bool empty() const { return count == 0; }
   const T& operator[](size_t index) const {
      const size_t place = offset + index;
      return map[(place / kBlock) & (mapSize - 1)][place % kBlock];
   }
};

} // namespace fa::stdabi
