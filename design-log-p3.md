# Project 3 Design Log
**Author:** David Welch · **P2 code used:** my own
**Tools used:** Claude (AI assistant). It fixed `Workspace::resolve`, `calc_evaluate`, and the `tests/p3/test_p3.cpp` suite. I reviewed the code and checked the results by building with `-Werror` and the sanitizers.

## 1. Hash table
I chose **separate chaining**: `std::vector<std::vector<std::unique_ptr<Tool>>>`, one small vector per bucket. P3 never removes a tool, so open addressing's main weakness (tombstones) does not matter, but chaining still wins on robustness: a bad cluster costs one longer chain instead of slowing every probe sequence that crosses it, and the load factor can never reach a state where `find` loops forever. The spec's reference numbers agree: about 6 for FNV-1a with chaining against about 30 for linear probing at 0.75.

The hash is **64-bit FNV-1a**. It uses every character, is order-sensitive (`"ab"` and `"ba"` differ), and returns the full-width value; the index is `hash % bucket_count()` computed separately. The table starts with 16 buckets. Before each insert, if `(size + 1) / buckets > 0.75`, it doubles the bucket count and re-inserts every entry using the **new** count. Checking before inserting keeps `load_factor()` at or below 1.0 whenever `add()` returns.

Measured with `tool_0` … `tool_9999`:

| Hash | Longest chain at 16,384 buckets |
|---|---|
| FNV-1a (used) | **6** |
| djb2 | 4 |
| `s[0] + s.size()` | **9,000** |

For the real table: `bucket_count = 16384`, `load_factor = 0.610`, `max_probe_length = 6`, and 10,000 distinct hash values. The weak hash fails because the names share a first character and similar lengths, so most land in one bucket. djb2 matches FNV-1a under chaining; I kept FNV-1a, which the spec recommends.

`add()` is **amortized O(1)**. A resize at size *n* costs O(n), but after doubling the table has room for about *n* more entries before the next resize. So the O(n) cost is spread over roughly *n* cheap inserts, which is O(1) each.

## 2. Pointer stability
The `Tool` objects are created on the heap by `make_unique` and never copied or moved. The table stores only `unique_ptr<Tool>`. A rehash moves those smart pointers into the new bucket vectors, which copies an address and leaves the `Tool` where it was; likewise, when an inner bucket vector grows, only pointers move. `find()` returns `get()`, the address of the original object, so it is the same before and after any number of resizes. If buckets held `Tool` objects by value, every rehash would invalidate outstanding pointers. Tests `reg_pointer_stable_across_resizes` and `reg_10000_tools` check this.

## 3. Calculator trace
`2 * (3 + 4) - 5`, with stacks written bottom to top:

| Token | What happens | `values` | `ops` |
|---|---|---|---|
| `2` | number, push | 2 | |
| `*` | `ops` is empty, push | 2 | `*` |
| `(` | always push | 2 | `* (` |
| `3` | push | 2 3 | `* (` |
| `+` | top is `(`, so nothing to apply; push | 2 3 | `* ( +` |
| `4` | push | 2 3 4 | `* ( +` |
| `)` | apply `+`: pop 4 (right), then 3 (left), push 7; top is now `(`, pop it | 2 7 | `*` |
| `-` | top `*` has precedence 2 ≥ 1, so apply it: 7 is right, 2 is left, push 14; then push `-` | 14 | `-` |
| `5` | push | 14 5 | `-` |
| end | apply `-`: 5 is right, 14 is left, push 9 | **9** | |

Two details matter: `>=` makes equal precedence apply left to right (so `10 - 4 - 3` is 3), and the first value popped is the right operand (otherwise `10 - 4` would be −6).

## 4. Errors as values
A failing tool is normal in an agent loop: the file does not exist, the path is outside the workspace, the expression divides by zero. The model reads the message and tries something else. So `resolve` and `calc_evaluate` return `false` with a reason, and tools return `ToolResult::failure(...)`, which the dispatcher turns into an `"ok":false` result in the conversation. A return value forces the caller to handle the failure at the boundary and keeps the harness running; an exception would have to unwind through the dispatcher, and every caller would need to remember to catch it. Exceptions are for **programmer errors**, bugs in the C++ itself such as calling `as_string()` on a JSON number (`std::logic_error`). Those should be loud.

## 5. What I would design differently
`find()` calls `tool->name()` for every entry in a chain, and `rehash` calls it again to recompute every hash. Each call is a virtual call plus a `std::string` copy. In hindsight I would store a small entry struct in each bucket holding the name, the cached hash, and the `unique_ptr<Tool>`. Lookups could then compare the cached hash first, and a resize would not need to touch the tools at all.
