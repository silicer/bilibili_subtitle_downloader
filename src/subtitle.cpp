#include <cpr/cpr.h>
#include <chrono>
#include <codecvt>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <nlohmann/json.hpp>
#include <ostream>
#include <regex>
#include <string>

#include "utils/cookie_utils.h"

#ifdef _WIN32
#define HOME_ENV "USERPROFILE"
#include <Windows.h>
#else
#define HOME_ENV "HOME"
#endif

using json = nlohmann::json;

std::pair<std::string, std::string> get_subtitle(std::string ep_id);
std::string json2srt(const std::string & data);
std::string cht2chs(const std::string & content);
void write_desktop(const std::string & content, std::string filename);

const std::string MEDIA_INFO_HOST =
    "https://bangumi.bilibili.com/view/web_api/season?ep_id=";
const std::string SUBTITLE_INFO_HOST = "https://api.bilibili.com/x/player/v2";
const std::string CONVERTER_URL = "https://api.zhconvert.org/convert";

int main() {
  #ifdef _WIN32
  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);
  #endif
  std::string ep_id_str;
  std::cout << "Please input the episode id: ";
  std::cin >> ep_id_str;
  auto result_tmp = get_subtitle(ep_id_str);
  std::string title = result_tmp.first;
  std::string data = result_tmp.second;
  std::string cht_subtitle = json2srt(data);
  std::string chs_subtitle = cht2chs(cht_subtitle);
  title = cht2chs(title);
  write_desktop(chs_subtitle, title);
  return 0;
}

std::pair<std::string, std::string> get_subtitle(std::string ep_id) {
  std::string cookie = get_cookie(".bilibili.com");
  cpr::Session session;
  session.SetCookies(cpr::Cookies{{"SESSDATA", cookie}});
  session.SetHeader(cpr::Header{{"User-Agent", "curl/8.0.1"}, {"Cookie", "SESSDATA=" + cookie}});
  session.SetUrl(cpr::Url{MEDIA_INFO_HOST + ep_id});
  cpr::Response res = session.Get();
  int ep_id_int = std::stoi(ep_id);
  json media_info = json::parse(res.text)["result"];
  int season_id = media_info["season_id"];
  int media_id = media_info["media_id"];
  json episodes = media_info["episodes"];
  for (auto &episode : episodes) {
    if (ep_id_int == episode["ep_id"]) {
      cpr::Parameters param =
          cpr::Parameters{{"cid", std::to_string(episode["cid"].get<int>())},
                          {"aid", std::to_string(episode["aid"].get<int>())},
                          {"bvid", episode["bvid"]},
                          {"season_id", std::to_string(season_id)},
                          {"ep_id", ep_id}};
      std::string title = episode["index_title"];
      session.SetUrl(cpr::Url{SUBTITLE_INFO_HOST});
      session.SetParameters(param);
      cpr::Response subtitle_info_res = session.Get();
      json subtitle_info = json::parse(subtitle_info_res.text);
      std::string subtitle_url =
          "https:" +
          subtitle_info["data"]["subtitle"]["subtitles"][0]["subtitle_url"]
              .get<std::string>();
      session.SetUrl(cpr::Url{subtitle_url});
      session.SetParameters({});
      return {title, session.Get().text};
    }
  }
  std::cerr << "Error: Failed to get subtitle. Cannot find the episode." << std::endl;
  exit(-1);
}

std::string json2srt(const std::string & data) {
  constexpr auto timestamp_to_time = [](float myTime) -> std::string {
    auto timestamp = static_cast<unsigned long long>(myTime * 1000);
    // Convert timestamp to time_point
    std::chrono::system_clock::time_point time_point =
        std::chrono::system_clock::time_point(
            std::chrono::milliseconds(timestamp));

    // Convert time_point to time_t
    std::time_t time_t = std::chrono::system_clock::to_time_t(time_point);

    // Convert time_t to tm struct
    std::tm *timeinfo = std::gmtime(&time_t);

    // Format tm struct as string in HH:mm:ss format
    std::ostringstream oss;
    oss << std::put_time(timeinfo, "%H:%M:%S");
    short militime = timestamp % 1000;
    oss << "," << std::setfill('0') << std::setw(3) << militime;
    return oss.str();
  };

  json raw = json::parse(data)["body"];
  std::string result = "";
  for (short i = 0; i < raw.size(); i++) {
    std::string start = timestamp_to_time(raw[i]["from"].get<float>());
    std::string end = timestamp_to_time(raw[i]["to"].get<float>());
    std::string newline = std::to_string(i) + "\n" + start + " --> " + end +
                          "\n" + raw[i]["content"].get<std::string>();
    result += newline + "\n\n";
  }
  return result;
}

std::string cht2chs(const std::string & content) {
  cpr::Payload param = cpr::Payload{{"text", content}, {"converter", "China"}};
  cpr::Response res = cpr::Post(cpr::Url{CONVERTER_URL}, param);
  auto response = json::parse(res.text);
  return response["data"]["text"].get<std::string>();
}

void write_desktop(const std::string & content, std::string filename) {
  std::regex pattern("[/\\:*?\"<>|]");
  filename = std::regex_replace(filename, pattern, "_");

  std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
  std::wstring output_path_str =
      converter.from_bytes(static_cast<std::string>(getenv(HOME_ENV)) +
                           "/Desktop/" + filename + ".srt");
  std::filesystem::path path = output_path_str;
  std::filesystem::path absolute_path = std::filesystem::absolute(path);
  std::ofstream outfile(absolute_path);
  if (!outfile.is_open()) {
    std::cerr << "Error: Failed to create file." << std::endl;
    exit(-2);
  }
  outfile << content;
  outfile.close();
  return;
}
