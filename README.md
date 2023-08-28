# Simple Bilibili Bangumi Subtitle Downloader

A very simple Bilibili bangumi subtitle downlodaer written in C++. You need these libraries to compile the program.

- [nlohmann/json](https://github.com/nlohmann/json)
- [libcpr/cpr](https://github.com/libcpr/cpr)
- [Sqlite3](https://www.sqlite.org)
- [Crypto++](https://www.cryptopp.com)

## Usage

Just run the program and enter the episode id. The program will read Bilibili cookies from the browsers and use it to find corresponding subtitle, transcript it into Simplified Chinese and write the srt format on the desktop.

```Edge is the only supported browser for now. Adding cookie manually and other browsers will be supported soon.```
