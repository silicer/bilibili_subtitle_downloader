#include "cookie_utils.h"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <cryptopp/aes.h>
#include <cryptopp/gcm.h>
#include <cryptopp/filters.h>
#include <iostream>

#ifdef _WIN32
#include <sqlite3.h>
#define HOME_ENV "USERPROFILE"
#else
#include <libsecret/secret.h>
#define HOME_ENV "HOME"
#endif

using json = nlohmann::json;

std::string base64_decode(const std::string &in) {
  std::string out;

  std::vector<int> T(256, -1);
  for (int i = 0; i < 64; i++) {
    T["ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i]] =
        i;
  }

  int val = 0, valb = -8;
  for (unsigned char c : in) {
    if (T[c] == -1)
      break;
    val = (val << 6) + T[c];
    valb += 6;
    if (valb >= 0) {
      out.push_back(char((val >> valb) & 0xFF));
      valb -= 8;
    }
  }
  return out;
}

#ifdef _WIN32
std::vector<byte> get_decrypted_aes_key() {
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
  std::string aes_key_decoded = base64_decode(aes_key);
  aes_key_decoded = aes_key_decoded.substr(5);

  DATA_BLOB input;
  DATA_BLOB output;
  input.pbData = reinterpret_cast<BYTE *>(aes_key_decoded.data());
  input.cbData = aes_key_decoded.size();

  if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, 0, &output)) {
    std::cerr << "Error: Failed to decrypt data." << std::endl;
    exit(-1);
  }
  
  std::vector<byte> aes_key_decrypted(output.pbData, output.pbData + output.cbData);
  LocalFree(output.pbData);
  return aes_key_decrypted;
}
#else
std::vector<byte> get_decrypted_aes_key() {
  const SecretSchema schema = {
        "microsoft-edge_libsecret_os_crypt_password", SECRET_SCHEMA_NONE,
        {
            { "application", SECRET_SCHEMA_ATTRIBUTE_STRING },
            { NULL, 0 },
        },
    };

    GError *error = NULL;

    // 使用libsecret查找Edge的加密密钥
    gchar *encrypted_key = secret_password_lookup_sync(&schema, NULL, &error,
                                                       "application", "microsoft-edge",
                                                       NULL);

    if (error != NULL) {
        std::cerr << "Error: " << error->message << std::endl;
        g_error_free(error);
        return "";
    }

    if (!encrypted_key) {
        std::cerr << "Encrypted key not found." << std::endl;
        return "";
    }

    // Edge在存储密钥之前可能会添加一个前缀，我们需要删除这个前缀来获取真正的加密密钥
    std::string decrypted_key = std::string(encrypted_key + 3); // 假设前缀长度为3
    secret_password_free(encrypted_key);

    return decrypted_key;
}
#endif

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
  auto aes_key = get_decrypted_aes_key();
  std::string cookie = decrypt_with_aes_gcm(encrypted_data, aes_key, iv);
  return cookie;
}