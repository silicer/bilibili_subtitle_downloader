# Simple Bilibili Bangumi Subtitle Downloader

A very simple Bilibili bangumi subtitle downlodaer written in C++. You need these libraries to compile the program.

- [nlohmann/json](https://github.com/nlohmann/json)
- [libcpr/cpr](https://github.com/libcpr/cpr)
- [Sqlite3](https://www.sqlite.org)
- [Crypto++](https://www.cryptopp.com)

## Usage

Just run the program and enter the episode id. The program will read Bilibili cookies from the browsers and use it to find corresponding subtitle, transcript it into Simplified Chinese and write the srt format on the desktop.

|Argument|Description|Type|Default|Required|
|-|-|-|-|-|
|-b/--browser| Specify which browser to read cookies from | string | "Edge" | No |
|-c/--cookie| Specify the cookie of Bilibili| string | "" | No |
|-e/--episode| Specify the episode id of the subtitle you want to download | string | "" | Yes|

**You are required to input it while the program is running if you don't specify it in the command line.**

