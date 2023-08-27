#include <cpr/cpr.h>
#include <chrono>
#include <codecvt>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
// #include <iterator>
#include <locale>
#include <nlohmann/json.hpp>
#include <ostream>
#include <regex>
#include <string>
#include <sqlite3.h>
#include <cpp-base64/base64.cpp>
#include <cryptopp/aes.h>
#include <cryptopp/gcm.h>
#include <cryptopp/filters.h>
#include <vector>
// #include <utility>

#ifdef _WIN32
#include <cstdlib>
#define HOME_ENV "USERPROFILE"
#include <Windows.h>
#else
#include <unistd.h>
#define HOME_ENV "HOME"
#endif

using json = nlohmann::json;

const std::string MEDIA_INFO_HOST =
    "https://bangumi.bilibili.com/view/web_api/season?ep_id=";
const std::string SUBTITLE_INFO_HOST = "https://api.bilibili.com/x/player/v2";
const std::string CONVERTER_URL = "https://api.zhconvert.org/convert";

std::vector<byte> extract_aes_key_frin_local_state() {
  std::string local_state_path = static_cast<std::string>(getenv(HOME_ENV)) +
                                 "/AppData/Local/Microsoft/Edge/User Data/"
                                 "Local State";
  std::ifstream local_state_file(local_state_path);
  if (!local_state_file.is_open()) {
    std::cerr << "Error: Failed to open file." << std::endl;
    exit(-1);
  }
  json local_state = json::parse(local_state_file);
  std::string aes_key = local_state["os_crypt"]["encrypted_key"];
  // aes_key = aes_key.substr(5);
  // base64 decode aes_key to bytes
  std::string aes_key_decoded = base64_decode(aes_key);
  aes_key_decoded = aes_key_decoded.substr(5);
  // decrypt aes_key using DPAPI
  DATA_BLOB input;
  DATA_BLOB output;
  LPWSTR pDescrOut = NULL;
  input.pbData = reinterpret_cast<BYTE *>(aes_key_decoded.data());
  input.cbData = aes_key_decoded.size();
  if (!CryptUnprotectData(&input, &pDescrOut, nullptr, nullptr, nullptr, 0, &output)) {
    std::cerr << "Error: Failed to decrypt data." << std::endl;
    exit(-1);
  }
  std::vector<byte> aes_key_decrypted(output.pbData, output.pbData + output.cbData);
  std::wcout << pDescrOut << std::endl;
  LocalFree(pDescrOut);
  LocalFree(output.pbData);
  return aes_key_decrypted;
}

std::string decrypt_with_aes_gcm(const std::string &encrypted_data, const std::vector<byte> &key, const std::string &iv) {
    try {
        CryptoPP::GCM<CryptoPP::AES>::Decryption decryption;
        decryption.SetKeyWithIV(key.data(), key.size(), (byte*)iv.data(), iv.size());

        std::string decrypted_data;
        CryptoPP::StringSource ss(encrypted_data, true,
            new CryptoPP::AuthenticatedDecryptionFilter(decryption,
                new CryptoPP::StringSink(decrypted_data)
            )
        );

        return decrypted_data;
    } catch (const CryptoPP::Exception& e) {
        // Handle decryption error
        std::cerr << "Decryption error: " << e.what() << std::endl;
        return "";
    }
}

// A function that returns a cookie string reading of a specific domain from a cookie file of Chrome
std::string get_cookie(const std::string &domain) {
  std::string cookie_path = static_cast<std::string>(getenv(HOME_ENV)) +
                            "/AppData/Local/Microsoft/Edge/User Data/"
                            "Default/Network/Cookies";
  sqlite3 *db;
  sqlite3_stmt *stmt;
  int rc = sqlite3_open(cookie_path.c_str(), &db);
  if (rc != SQLITE_OK) {
    std::cerr << "Error: Failed to open database." << std::endl;
    exit(-1);
  }
  std::string sql = "SELECT encrypted_value FROM cookies WHERE host_key = '" +
                    domain + "' AND name = 'SESSDATA'";
  rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    std::cerr << "Error: Failed to prepare statement." << std::endl;
    exit(-1);
  }
  rc = sqlite3_step(stmt);
  if (rc != SQLITE_ROW) {
    std::cerr << "Error: Failed to step." << std::endl;
    exit(-1);
  }
  std::string encrypted_value(
      reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)));
  std::string encrypted_data = encrypted_value.substr(3);
  std::string iv = encrypted_data.substr(0, 12);
  encrypted_data = encrypted_data.substr(12);
  sqlite3_finalize(stmt);
  sqlite3_close(db);
  auto aes_key = extract_aes_key_frin_local_state();
  std::string cookie = decrypt_with_aes_gcm(encrypted_data, aes_key, iv);
  return cookie;
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

int main() {
  #ifdef _WIN32
  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);
  #endif
  std::string ep_id_str;
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
