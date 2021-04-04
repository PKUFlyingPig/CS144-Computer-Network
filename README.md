# CS144 : Computer Networks

This repository contains all the learning materials for Stanford CS143 course : Computer Networks, you can go to the [initial website](https://cs144.github.io) to get more guidance.

## Lab Session

The most wonderful part of this class is the labs. There are 8 labs in total, which guides you to implement a complete Internet infrastructure step by step. Here is a brief overview of each lab :

- Lab 0 : simple Internet applications 
- Lab 1 - 4 : implementation of the TCP protocols
  - Lab 1 : stream assembler
  - Lab 2 : TCP receiver
  - Lab 3 : TCP sender
  - Lab 4 : TCP connection
- Lab 5 : Network Interface 
- Lab 6 : IP routing
- Lab 7 : Putting it all together (Use your own Internet infrastructures to talk with each other, so cool ! )

#### Lab 1 : stream assembler 

Related files : stream_reassembler.cc, stream_reassembler.hh

My implementation uses the idea of a fix-sized sliding window. I use a variable `unass_base` to record the index of the first unassembled byte, which is also the first byte of the sliding window. To simplify the implementation, I use a bitmap to indicate the current state of each byte in my slipping window. So when a substring being pushed into the assembler, I first mark the intersection of the substring and the window as assembled (in the bitmap). Then I call the `check_contiguous()` function to check if I can move the window further, i.e., assemble more bytes. A tricky part of my code is that to keep the sliding window's size fixed, whenever you kick one element out of the window, you should also push one placeholder into the window.

Caveat : eof of the incoming substrings does not mean all the substrings have been all assembled, because these substrings may come in any order, which means the last part of the string may come first, indicating the eof as true, but you should wait for all the substrings to come to finally end the input.

#### Lab 2 : TCP receiver

Related files : wrapping_integers.cc, wrapping_integers.hh, tcp_receiver.cc, tcp_receiver.hh

The code is quite short, but it's tricky to implement correctly. My implementation is a little bit messy : (, but it finally works : ).

#### Lab 3 : TCP sender

In debugging ... ...





## Sponge quickstart

For build prereqs, see [the CS144 VM setup instructions](https://web.stanford.edu/class/cs144/vm_howto).

To set up your build directory:

	$ mkdir -p <path/to/sponge>/build
	$ cd <path/to/sponge>/build
	$ cmake ..

**Note:** all further commands listed below should be run from the `build` dir.

To build:

    $ make

You can use the `-j` switch to build in parallel, e.g.,

    $ make -j$(nproc)

To test (after building; make sure you've got the [build prereqs](https://web.stanford.edu/class/cs144/vm_howto) installed!)

    $ make check_lab0

or

	$ make check_lab1

etc.

The first time you run a `make check`, it may run `sudo` to configure two
[TUN](https://www.kernel.org/doc/Documentation/networking/tuntap.txt) devices for use during testing.

### build options

You can specify a different compiler when you run cmake:

    $ CC=clang CXX=clang++ cmake ..

You can also specify `CLANG_TIDY=` or `CLANG_FORMAT=` (see "other useful targets", below).

Sponge's build system supports several different build targets. By default, cmake chooses the `Release`
target, which enables the usual optimizations. The `Debug` target enables debugging and reduces the
level of optimization. To choose the `Debug` target:

    $ cmake .. -DCMAKE_BUILD_TYPE=Debug

The following targets are supported:

- `Release` - optimizations
- `Debug` - debug symbols and `-Og`
- `RelASan` - release build with [ASan](https://en.wikipedia.org/wiki/AddressSanitizer) and
  [UBSan](https://developers.redhat.com/blog/2014/10/16/gcc-undefined-behavior-sanitizer-ubsan/)
- `RelTSan` - release build with
  [ThreadSan](https://developer.mozilla.org/en-US/docs/Mozilla/Projects/Thread_Sanitizer)
- `DebugASan` - debug build with ASan and UBSan
- `DebugTSan` - debug build with ThreadSan

Of course, you can combine all of the above, e.g.,

    $ CLANG_TIDY=clang-tidy-6.0 CXX=clang++-6.0 .. -DCMAKE_BUILD_TYPE=Debug

**Note:** if you want to change `CC`, `CXX`, `CLANG_TIDY`, or `CLANG_FORMAT`, you need to remove
`build/CMakeCache.txt` and re-run cmake. (This isn't necessary for `CMAKE_BUILD_TYPE`.)

### other useful targets

To generate documentation (you'll need `doxygen`; output will be in `build/doc/`):

    $ make doc

To lint (you'll need `clang-tidy`):

    $ make -j$(nproc) tidy

To run cppcheck (you'll need `cppcheck`):

    $ make cppcheck

To format (you'll need `clang-format`):

    $ make format

To see all available targets,

    $ make help
