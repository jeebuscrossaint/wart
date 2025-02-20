# wart
*a wallpaper fetcher and applier for linux & bsd*

## Why does this exist
I like the microsoft bing wallpapers as much as people crap on microsoft whoever they have taking those pictures are doing great and I wanted to have them on linux too so I just wrote this program in C++ to do it for me.

## How does it work?
It downloads a wallpaper from like an api you can read more about here: [https://github.com/TimothyYe/bing-wallpaper](TimothyYe/bing-wallpaper) and then it sets the wallpaper based on whatever the desktop environment is or through a method of your choice in the config file. You can also add hooks like pywal to change the colorscheme of your terminal and other applications.

## How do I use it?
It is on the AUR or you can compile it yourself the dependencies are [https://github.com/nlohmann/json](nlohmann/json) and [https://github.com/curl/curl](libcurl) and you can compile it with `make` and install it with `make install` or `make install PREFIX=/usr` if you want to install it to /usr instead of /usr/local. You also need CMake of course.
```
git clone https://github.com/jeebuscrossaint/wart.git
cd wart
cmake -S . -B build
cd build
make
```

## How do I configure it?
You can configure it by either creating the config file in ~/.wart/wartrc or by just running wart which on start will create a default config file if none exists. It is very easy to understand.

## License
```
BSD 2-Clause License

Copyright (c) 2025, Amarnath P.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```
