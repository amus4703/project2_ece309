# design log - project 2

## growth factor

`Conversation` uses a allocated array of `Message` objects. Its
capacity begins at zero, becomes one for each append and doubles each
time the array is full: 1, 2, 4.... Doubling is a great trade off
because it avoids unnecessary allocations while keeping unused capacity
below the size of the live conversation after a growth event.

The container never uses `std::vector`, `Conversation` is responsible
for its own array and is the only class in the project that uses `new[]`
and `delete[]`.

Although one append that triggers a resize can copy every existing
message, the average cost of a sequence of appends is constant.
Consider the resize costs through capacity `2^k`: the number of copied
messages is

`1 + 2 + 4 + ... + 2^(k-1) = 2^k - 1`.

If at least `2^k` messages have been appended, that number is still
less than the total number of appends. The normal placement of each new
message adds one more assignment, so the total work for `n` appends is
at most a multiple of `n`. Therefore `append` is amortized O(1), even
though a particular resizing call is O(n). A fixed growth amount, such
as adding one slot, would instead copy `1 + 2 + ... + (n - 1)` messages
and make the total cost O(n^2).

## rule of five

When you make a new object, it starts with nothing inside: no memory
and with a capacity of 0. The cleanup function will clear all of this
memory even when there is no contents to delete. When you make a copy,
it makes a new space in memory and copies everything one by one so it
ends up in a different spot than the original. If the copying fails
midway, the half already copied gets deleted and moves the error along,
so no bad data gets saved. Finally, when you assign a copy to an
existing object, it builds a temporary copy in the background and then
quickly swaps the new copy's parts with the old ones, which keeps your
data safe if an error happens and prevents issues if you assign an
object to itself.

The move constructor and move assignment don't copy individual
messages. They take the source pointer, size and capacity and set the
source to a null pointer and zero values. The moved-from object is then
valid, empty, and safe to destroy. Move assignment deletes its current
buffer before taking ownership, unless when it is assigned to itself.
The test suite checks distinct storage for copies, pointer transfer for
moves, a moved-from empty object, copy assignment, move assignment, and
a clean destruction path under AddressSanitizer.

## sentinel scanner

The scanner watches for `<|end_conversation|>` as the reply streams in
even if it's split across chunks. It keeps a small holding area,
`pending_`, for the last few characters that could still be the start
of the sentinel. Each new character goes into `pending_`. If it now
matches the full sentinel, the scanner returns the safe text before it,
marks it found, and drops everything after. If not, it releases
characters from the front as safe text until what's left could still be
the start of the sentinel.

`pending_` can never hold the full sentinel, since a full match gets
caught right away. So it stays at most `sentinel.length() - 1`
characters after every character and every chunk, wherever the splits
land. The tests check this by splitting the sentinel at every point,
making sure `<|end_world|>` doesn't trigger it, and feeding 4 MiB one
byte at a time while checking the limit.

## what i would do if done again

If I changed this project, I would move the resizing work/code to a
private grow() helper to keep it inside append(). The version we have
right now works but append() is responsible for checking the capacity
along with inserting a message. If I were to separate these jobs, it
would help make the code easier to read and also give one place to test
for the copying or cleanup logic. I would also add a different way to
inspect capacity.
