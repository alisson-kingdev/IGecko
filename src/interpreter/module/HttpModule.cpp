#include "HttpModule.h"

#ifndef CPPHTTPLIB_OPENSSL_SUPPORT
#define CPPHTTPLIB_OPENSSL_SUPPORT
#endif
#include <algorithm>
#include <cctype>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <future>
#include <httplib.h>
#include <iostream>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <thread>

#include "../Interpreter.h"
#include "../runtime/Runtime.h"

namespace module
{

namespace
{

// ──────────────────────────────────────────────
// HTTP Client helpers (existing)
// ──────────────────────────────────────────────

struct Url
{
  std::string protocol;
  std::string host;
  std::string path;
};

Url parseUrl(std::string_view url)
{
  std::string protocol;
  if (url.starts_with("https://")) {
    protocol = "https://";
    url.remove_prefix(8);
  }
  else if (url.starts_with("http://")) {
    protocol = "http://";
    url.remove_prefix(7);
  }
  else {
    throw std::runtime_error("Invalid or unsupported protocol: Only http and https are allowed.");
  }

  size_t slashPos = url.find('/');
  std::string hostPort(url.substr(0, slashPos));
  std::string path = (slashPos != std::string_view::npos) ? std::string(url.substr(slashPos)) : "/";

  return {protocol + hostPort, hostPort, path};
}

bool isValidHeaderKey(std::string_view key)
{
  return !key.empty() && std::all_of(key.begin(), key.end(), [](char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '-';
  });
}

std::string sanitizeHeaderValue(std::string_view value)
{
  std::string sanitized;
  for (char c : value) {
    if (c != '\r' && c != '\n') {
      sanitized += c;
    }
  }
  size_t first = sanitized.find_first_not_of(" \t");
  if (first == std::string::npos) {
    return "";
  }
  size_t last = sanitized.find_last_not_of(" \t");
  return sanitized.substr(first, (last - first + 1));
}

gecko::Value handleResponse(httplib::Result& res)
{
  if (!res) {
    throw std::runtime_error("HTTP request failed: Connection error");
  }

  auto responseObj = std::make_shared<Environment>();
  responseObj->define("status", static_cast<double>(res->status));
  responseObj->define("body", res->body);
  responseObj->exportName("status");
  responseObj->exportName("body");

  return responseObj;
}

// ──────────────────────────────────────────────
// HTTP Server
// ──────────────────────────────────────────────

struct MiddlewareEntry
{
  std::string pathPrefix;
  gecko::Value handler;
  bool isErrorHandler;
};

struct ServerRoute
{
  std::string method;
  std::string pattern;
  std::vector<std::string> paramNames;
  std::regex patternRegex;
  gecko::Value handler;
};

struct ServerData
{
  std::vector<ServerRoute> routes;
  std::vector<MiddlewareEntry> middlewares;
  std::shared_ptr<std::mutex> mtx;
  Interpreter* creatorInterpreter;
  std::shared_ptr<Environment> globals;
  std::shared_ptr<httplib::Server> svr;
  bool listening;

  ServerData()
      : mtx(std::make_shared<std::mutex>())
      , creatorInterpreter(nullptr)
      , svr(std::make_shared<httplib::Server>())
      , listening(false)
  {}
};

std::regex makeRouteRegex(const std::string& pattern,
                          std::vector<std::string>& paramNames)
{
  std::string escaped;
  paramNames.clear();
  size_t i = 0;
  while (i < pattern.size()) {
    if (pattern[i] == ':') {
      size_t start = i + 1;
      size_t end = start;
      while (end < pattern.size() && pattern[end] != '/' && pattern[end] != '\0') {
        ++end;
      }
      paramNames.push_back(pattern.substr(start, end - start));
      escaped += "([^/]+)";
      i = end;
    } else {
      if (pattern[i] == '*' || pattern[i] == '?' || pattern[i] == '+' ||
          pattern[i] == '.' || pattern[i] == '^' || pattern[i] == '$' ||
          pattern[i] == '|' || pattern[i] == '(' || pattern[i] == ')' ||
          pattern[i] == '[' || pattern[i] == ']' || pattern[i] == '{' ||
          pattern[i] == '}') {
        escaped += '\\';
      }
      escaped += pattern[i];
      ++i;
    }
  }
  return std::regex("^" + escaped + "$");
}

nlohmann::json serverValueToJson(const gecko::Value& val)
{
  return std::visit(gecko::overloaded{[](gecko::Nil) -> nlohmann::json {
                                        return nullptr;
                                      },
                                      [](bool b) -> nlohmann::json {
                                        return b;
                                      },
                                      [](double d) -> nlohmann::json {
                                        return d;
                                      },
                                      [](const std::string& s) -> nlohmann::json {
                                        return s;
                                      },
                                      [](const gecko::Array& arr) -> nlohmann::json {
                                        nlohmann::json j = nlohmann::json::array();
                                        for (const auto& v : *arr) {
                                          j.push_back(serverValueToJson(v));
                                        }
                                        return j;
                                      },
                                      [](const gecko::Object& obj) -> nlohmann::json {
                                        nlohmann::json j = nlohmann::json::object();
                                        for (const auto& name : obj->getDeclaredNames()) {
                                          j[name] = serverValueToJson(obj->get(name));
                                        }
                                        return j;
                                      },
                                      [](const auto&) -> nlohmann::json {
                                        throw std::runtime_error("res.json: unsupported type");
                                      }},
                    val.data);
}

std::shared_ptr<Environment> createReqObject(
    const httplib::Request& req,
    const std::vector<std::string>& paramNames = {},
    const std::smatch& match = std::smatch{})
{
  auto reqObj = std::make_shared<Environment>();
  reqObj->define("method", req.method);
  reqObj->define("url", req.path);
  reqObj->define("path", req.path);
  reqObj->define("body", req.body);
  reqObj->define("protocol", "http");
  reqObj->define("hostname", req.remote_addr);
  reqObj->define("ip", req.remote_addr);
  reqObj->define("secure", false);
  reqObj->define("fresh", false);

  auto headersObj = std::make_shared<Environment>();
  for (const auto& [key, val] : req.headers) {
    std::string lower;
    for (char c : key) lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    headersObj->define(lower, val);
  }
  reqObj->define("headers", headersObj);

  auto paramsObj = std::make_shared<Environment>();
  for (size_t idx = 0; idx < paramNames.size() && (idx + 1) < match.size(); ++idx) {
    paramsObj->define(paramNames[idx], match[idx + 1].str());
  }
  if (!req.params.empty()) {
    for (const auto& [k, v] : req.params) {
      paramsObj->define(k, v);
    }
  }
  reqObj->define("params", paramsObj);

  auto queryObj = std::make_shared<Environment>();
  size_t qpos = req.target.find('?');
  if (qpos != std::string::npos) {
    std::string qs = req.target.substr(qpos + 1);
    size_t s = 0;
    while (s < qs.size()) {
      size_t amp = qs.find('&', s);
      std::string pair = qs.substr(s, amp == std::string::npos ? amp : amp - s);
      size_t eq = pair.find('=');
      if (eq != std::string::npos) {
        auto decode = [](const std::string& src) -> std::string {
          std::string out;
          for (size_t i = 0; i < src.size(); ++i) {
            if (src[i] == '%' && i + 2 < src.size()) {
              int hi = std::isxdigit(static_cast<unsigned char>(src[i+1])) ?
                       std::stoi(src.substr(i+1, 2), nullptr, 16) : 0;
              out += static_cast<char>(hi);
              i += 2;
            } else if (src[i] == '+') {
              out += ' ';
            } else {
              out += src[i];
            }
          }
          return out;
        };
        queryObj->define(decode(pair.substr(0, eq)),
                        decode(pair.substr(eq + 1)));
      }
      s = (amp == std::string::npos) ? qs.size() : amp + 1;
    }
  }
  reqObj->define("query", queryObj);

  std::string remoteAddr = req.remote_addr + ":" + std::to_string(req.remote_port);
  reqObj->define("remoteAddress", remoteAddr);

  reqObj->exportName("method");
  reqObj->exportName("url");
  reqObj->exportName("path");
  reqObj->exportName("body");
  reqObj->exportName("headers");
  reqObj->exportName("params");
  reqObj->exportName("query");
  reqObj->exportName("protocol");
  reqObj->exportName("hostname");
  reqObj->exportName("ip");
  reqObj->exportName("remoteAddress");

  return reqObj;
}

std::shared_ptr<Environment> createResObject(httplib::Response& httplibRes,
                                             bool* sentFlag = nullptr)
{
  auto resObj = std::make_shared<Environment>();
  resObj->define("body", "");
  resObj->define("statusCode", static_cast<double>(httplibRes.status));
  resObj->define("statusText", "OK");
  resObj->define("sent", false);
  resObj->define("headersSent", false);

  auto resHeaders = std::make_shared<Environment>();
  resObj->define("headers", resHeaders);
  resObj->define("locals", std::make_shared<Environment>());

  auto markSent = [sentFlag, &resObj]() {
    resObj->define("sent", true);
    resObj->define("headersSent", true);
    if (sentFlag) *sentFlag = true;
  };

  resObj->define(
      "status",
      gecko::NativeFunction(
          [&httplibRes, resObj](const std::vector<gecko::Value>& args) -> gecko::Value {
            if (args.empty() || !args[0].isNumber()) {
              throw std::runtime_error("res.status expects a number");
            }
            httplibRes.status = static_cast<int>(args[0].as<double>());
            resObj->define("statusCode", args[0]);
            return resObj;
          }));

  auto sendFn =
      gecko::NativeFunction(
          [&httplibRes, resObj, markSent](const std::vector<gecko::Value>& args) -> gecko::Value {
            if (args.empty()) {
              throw std::runtime_error("res.send expects a value");
            }
            std::string body;
            std::string contentType;
            auto& arg = args[0];
            if (arg.isString()) {
              body = arg.as<std::string>();
              contentType = "text/html; charset=utf-8";
            } else if (arg.isObject() || arg.isArray()) {
              body = serverValueToJson(arg).dump();
              contentType = "application/json";
            } else if (arg.isNumber()) {
              body = std::to_string(arg.as<double>());
              contentType = "text/plain; charset=utf-8";
            } else if (arg.isBool()) {
              body = arg.as<bool>() ? "true" : "false";
              contentType = "text/plain; charset=utf-8";
            } else {
              body = serverValueToJson(arg).dump();
              contentType = "application/json";
            }
            resObj->define("body", body);
            httplibRes.set_content(body, contentType);
            markSent();
            return nullptr;
          });

  resObj->define("send", sendFn);

  resObj->define("json",
                 gecko::NativeFunction(
                     [&httplibRes, resObj, markSent](const std::vector<gecko::Value>& args) -> gecko::Value {
                       if (args.empty()) {
                         throw std::runtime_error("res.json expects a value");
                       }
                       std::string body = serverValueToJson(args[0]).dump();
                       resObj->define("body", body);
                       httplibRes.set_content(body, "application/json");
                       markSent();
                       return nullptr;
                     }));

  resObj->define("jsonp",
                 gecko::NativeFunction(
                     [&httplibRes, resObj, markSent](const std::vector<gecko::Value>& args) -> gecko::Value {
                       if (args.empty()) {
                         throw std::runtime_error("res.jsonp expects a value");
                       }
                       std::string callback = "callback";
                       if (args.size() > 1 && args[1].isString()) {
                         callback = args[1].as<std::string>();
                       }
                       std::string body = callback + "(" + serverValueToJson(args[0]).dump() + ")";
                       resObj->define("body", body);
                       httplibRes.set_content(body, "application/javascript");
                       markSent();
                       return nullptr;
                     }));

  resObj->define("setHeader",
                 gecko::NativeFunction([&httplibRes, resHeaders](
                                           const std::vector<gecko::Value>& args) -> gecko::Value {
                   if (args.size() < 2 || !args[0].isString() || !args[1].isString()) {
                     throw std::runtime_error("res.setHeader expects (name, value) strings");
                   }
                   httplibRes.set_header(args[0].as<std::string>(), args[1].as<std::string>());
                   resHeaders->define(args[0].as<std::string>(), args[1].as<std::string>());
                   return nullptr;
                 }));

  resObj->define("append",
                 gecko::NativeFunction([&httplibRes, resHeaders](
                                           const std::vector<gecko::Value>& args) -> gecko::Value {
                   if (args.size() < 2 || !args[0].isString() || !args[1].isString()) {
                     throw std::runtime_error("res.append expects (name, value) strings");
                   }
                   std::string name = args[0].as<std::string>();
                   std::string val = args[1].as<std::string>();
                   std::string existing = httplibRes.get_header_value(name);
                   if (!existing.empty()) {
                     val = existing + ", " + val;
                   }
                   httplibRes.set_header(name, val);
                   resHeaders->define(name, val);
                   return nullptr;
                 }));

  resObj->define("cookie",
                 gecko::NativeFunction([&httplibRes](const std::vector<gecko::Value>& args) -> gecko::Value {
                   if (args.empty() || !args[0].isString() || args.size() < 2) {
                     throw std::runtime_error("res.cookie expects (name, value, options?)");
                   }
                   std::string cookie = args[0].as<std::string>() + "=" + args[1].as<std::string>();
                   if (args.size() > 2 && args[2].isObject()) {
                     auto opts = args[2].as<gecko::Object>();
                     auto addOpt = [&](const std::string& key, const std::string& prefix) {
                       try {
                         if (opts->isExported(key)) {
                           cookie += "; " + prefix + opts->get(key).as<std::string>();
                         }
                       } catch (...) {}
                     };
                     addOpt("domain", "Domain=");
                     addOpt("path", "Path=");
                     try {
                       if (opts->isExported("maxAge")) {
                         cookie += "; Max-Age=" + std::to_string(static_cast<int>(opts->get("maxAge").as<double>()));
                       }
                     } catch (...) {}
                     try {
                       if (opts->isExported("httpOnly") && opts->get("httpOnly").as<bool>()) {
                         cookie += "; HttpOnly";
                       }
                     } catch (...) {}
                     try {
                       if (opts->isExported("secure") && opts->get("secure").as<bool>()) {
                         cookie += "; Secure";
                       }
                     } catch (...) {}
                     try {
                       if (opts->isExported("sameSite")) {
                         cookie += "; SameSite=" + opts->get("sameSite").as<std::string>();
                       }
                     } catch (...) {}
                   }
                   httplibRes.set_header("Set-Cookie", cookie);
                   return nullptr;
                 }));

  resObj->define("clearCookie",
                 gecko::NativeFunction([&httplibRes](const std::vector<gecko::Value>& args) -> gecko::Value {
                   if (args.empty() || !args[0].isString()) {
                     throw std::runtime_error("res.clearCookie expects a name string");
                   }
                   httplibRes.set_header("Set-Cookie",
                                         args[0].as<std::string>() + "=; Max-Age=0");
                   return nullptr;
                 }));

  resObj->define(
      "redirect",
      gecko::NativeFunction([&httplibRes, markSent](const std::vector<gecko::Value>& args) -> gecko::Value {
        if (args.empty() || !args[0].isString()) {
          throw std::runtime_error("res.redirect expects a URL string");
        }
        int status = 302;
        if (args.size() > 1 && args[1].isNumber()) {
          status = static_cast<int>(args[1].as<double>());
        }
        httplibRes.set_redirect(args[0].as<std::string>(), status);
        markSent();
        return nullptr;
      }));

  resObj->define(
      "type",
      gecko::NativeFunction([&httplibRes](const std::vector<gecko::Value>& args) -> gecko::Value {
        if (args.empty() || !args[0].isString()) {
          throw std::runtime_error("res.type expects a MIME type string");
        }
        httplibRes.set_header("Content-Type", args[0].as<std::string>());
        return nullptr;
      }));

  resObj->define(
      "format",
      gecko::NativeFunction([&httplibRes, resObj, markSent](const std::vector<gecko::Value>& args) -> gecko::Value {
        if (args.empty() || !args[0].isObject()) {
          throw std::runtime_error("res.format expects an object");
        }
        auto obj = args[0].as<gecko::Object>();
        std::string accept;
        try {
          if (resObj->isExported("headers")) {
            auto h = resObj->get("headers");
            if (h.isObject()) {
              auto hObj = h.as<gecko::Object>();
              if (hObj->isExported("accept")) {
                accept = hObj->get("accept").as<std::string>();
              }
            }
          }
        } catch (...) {}
        std::string chosenType = "text/html";
        if (!accept.empty()) {
          if (accept.find("application/json") != std::string::npos) chosenType = "application/json";
          else if (accept.find("text/plain") != std::string::npos) chosenType = "text/plain";
          else if (accept.find("text/xml") != std::string::npos) chosenType = "text/xml";
        }
        auto names = obj->getDeclaredNames();
        for (const auto& n : names) {
          std::string lookup;
          if (n == "json") lookup = "application/json";
          else if (n == "html") lookup = "text/html";
          else if (n == "text") lookup = "text/plain";
          else if (n == "xml") lookup = "text/xml";
          else lookup = n;
          if (chosenType == lookup || chosenType.find(lookup) != std::string::npos) {
            auto val = obj->get(n);
            if (val.isNativeFunction()) {
              val.as<gecko::NativeFunction>()({});
            } else {
              httplibRes.set_content(serverValueToJson(val).dump(), chosenType);
            }
            markSent();
            return nullptr;
          }
        }
        httplibRes.status = 406;
        markSent();
        return nullptr;
      }));

  resObj->define(
      "sendFile",
      gecko::NativeFunction([&httplibRes, markSent](const std::vector<gecko::Value>& args) -> gecko::Value {
        if (args.empty() || !args[0].isString()) {
          throw std::runtime_error("res.sendFile expects a path string");
        }
        std::string filePath = args[0].as<std::string>();
        std::ifstream file(filePath, std::ios::binary | std::ios::ate);
        if (!file) {
          httplibRes.status = 404;
          httplibRes.set_content("File not found", "text/plain");
          markSent();
          return nullptr;
        }
        auto size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::string content(static_cast<size_t>(size), '\0');
        file.read(content.data(), size);
        std::string ext = std::filesystem::path(filePath).extension().string();
        std::string mime = "application/octet-stream";
        if (ext == ".html" || ext == ".htm") mime = "text/html; charset=utf-8";
        else if (ext == ".css") mime = "text/css";
        else if (ext == ".js") mime = "application/javascript";
        else if (ext == ".json") mime = "application/json";
        else if (ext == ".png") mime = "image/png";
        else if (ext == ".jpg" || ext == ".jpeg") mime = "image/jpeg";
        else if (ext == ".gif") mime = "image/gif";
        else if (ext == ".svg") mime = "image/svg+xml";
        else if (ext == ".ico") mime = "image/x-icon";
        else if (ext == ".pdf") mime = "application/pdf";
        else if (ext == ".txt") mime = "text/plain; charset=utf-8";
        else if (ext == ".xml") mime = "text/xml";
        else if (ext == ".woff2") mime = "font/woff2";
        else if (ext == ".woff") mime = "font/woff";
        httplibRes.set_content(content, mime);
        markSent();
        return nullptr;
      }));

  resObj->define(
      "download",
      gecko::NativeFunction([&httplibRes, markSent](const std::vector<gecko::Value>& args) -> gecko::Value {
        if (args.empty() || !args[0].isString()) {
          throw std::runtime_error("res.download expects a path string");
        }
        std::string filePath = args[0].as<std::string>();
        std::string filename = std::filesystem::path(filePath).filename().string();
        if (args.size() > 1 && args[1].isString()) {
          filename = args[1].as<std::string>();
        }
        httplibRes.set_header("Content-Disposition", "attachment; filename=\"" + filename + "\"");
        std::ifstream file(filePath, std::ios::binary | std::ios::ate);
        if (!file) {
          httplibRes.status = 404;
          httplibRes.set_content("File not found", "text/plain");
          markSent();
          return nullptr;
        }
        auto size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::string content(static_cast<size_t>(size), '\0');
        file.read(content.data(), size);
        httplibRes.set_content(content, "application/octet-stream");
        markSent();
        return nullptr;
      }));

  resObj->define(
      "location",
      gecko::NativeFunction([&httplibRes](const std::vector<gecko::Value>& args) -> gecko::Value {
        if (args.empty() || !args[0].isString()) {
          throw std::runtime_error("res.location expects a URL string");
        }
        httplibRes.set_header("Location", args[0].as<std::string>());
        return nullptr;
      }));

  resObj->exportName("status");
  resObj->exportName("send");
  resObj->exportName("json");
  resObj->exportName("jsonp");
  resObj->exportName("setHeader");
  resObj->exportName("append");
  resObj->exportName("cookie");
  resObj->exportName("clearCookie");
  resObj->exportName("redirect");
  resObj->exportName("type");
  resObj->exportName("format");
  resObj->exportName("sendFile");
  resObj->exportName("download");
  resObj->exportName("location");

  return resObj;
}

void callGeckoHandler(const gecko::Value& handler,
                      const std::vector<gecko::Value>& args,
                      const std::shared_ptr<ServerData>& sd)
{
  if (handler.isNativeFunction()) {
    handler.as<gecko::NativeFunction>()(args);
  }
  else if (handler.isUserFunction()) {
    auto func = handler.as<std::shared_ptr<UserFunction>>();
    Interpreter tempInterp("<http-handler>", &std::cout);
    tempInterp.globals = sd->globals;
    tempInterp.environment = sd->globals;
    tempInterp.moduleCache = sd->creatorInterpreter
                                 ? sd->creatorInterpreter->moduleCache
                                 : std::make_shared<ModuleCache>();
    tempInterp.callUserFunction(func, args, func->closure);
  }
}

void runMiddlewareChain(
    const std::shared_ptr<ServerData>& sd,
    size_t mwIndex,
    const std::vector<size_t>& matchingMW,
    const std::shared_ptr<Environment>& reqObj,
    const std::shared_ptr<Environment>& resObj,
    httplib::Response& httplibRes,
    bool* sentFlag,
    std::function<void()> onDone)
{
  if (*sentFlag) return;

  while (mwIndex < matchingMW.size()) {
    auto& entry = sd->middlewares[matchingMW[mwIndex]];
    if (entry.isErrorHandler) {
      ++mwIndex;
      continue;
    }

    size_t currentIdx = mwIndex;
    ++mwIndex;

    auto nextCalled = std::make_shared<bool>(false);

    gecko::NativeFunction nextFn(
        [sd, currentIdx, matchingMW, reqObj, resObj, &httplibRes, sentFlag, onDone, &mwIndex, nextCalled](
            const std::vector<gecko::Value>&) -> gecko::Value {
          *nextCalled = true;
          runMiddlewareChain(sd, mwIndex, matchingMW, reqObj, resObj, httplibRes, sentFlag, onDone);
          return nullptr;
        });

    try {
      callGeckoHandler(entry.handler, {reqObj, resObj, nextFn}, sd);
    }
    catch (const std::exception& e) {
      // Try error middleware
      bool handled = false;
      for (auto& errMw : sd->middlewares) {
        if (!errMw.isErrorHandler) continue;
        try {
          gecko::NativeFunction errNext([](const std::vector<gecko::Value>&)
                                            -> gecko::Value { return nullptr; });
          auto errHandler = errMw.handler.as<gecko::NativeFunction>();
          errHandler({gecko::Value(std::string(e.what())), reqObj, resObj, errNext});
          if (*sentFlag) { handled = true; break; }
        } catch (...) {}
      }
      if (!handled) {
        httplibRes.status = 500;
        httplibRes.set_content(R"({"error":")" + std::string(e.what()) + "\"}", "application/json");
        *sentFlag = true;
      }
      return;
    }

    if (*sentFlag) return;
    if (!*nextCalled) return;
  }

  onDone();
}

void processRequest(const std::shared_ptr<ServerData>& sd,
                    const httplib::Request& req,
                    httplib::Response& res,
                    const std::string& method,
                    const std::string& path)
{
  std::lock_guard<std::mutex> lock(*sd->mtx);

  bool sentFlag = false;
  auto reqObj = createReqObject(req);
  auto resObj = createResObject(res, &sentFlag);

  // Collect matching middleware
  std::vector<size_t> matchingMW;
  for (size_t i = 0; i < sd->middlewares.size(); ++i) {
    auto& mw = sd->middlewares[i];
    if (mw.isErrorHandler) continue;
    if (path.find(mw.pathPrefix) == 0 || mw.pathPrefix == "/") {
      matchingMW.push_back(i);
    }
  }

  // Find matching route
  ServerRoute* matchedRoute = nullptr;
  std::smatch match;
  for (auto& rt : sd->routes) {
    if (rt.method != method && rt.method != "ALL") continue;
    std::smatch cm;
    if (std::regex_match(path, cm, rt.patternRegex)) {
      matchedRoute = &rt;
      match = cm;
      break;
    }
  }

  // Recreate reqObj with params if route matched
  if (matchedRoute) {
    reqObj = createReqObject(req, matchedRoute->paramNames, match);
  }

  auto onDone = [sd, matchedRoute, &req, &res, &reqObj, &resObj, &sentFlag, &method, &path]() {
    if (sentFlag) return;

    if (matchedRoute) {
      try {
        callGeckoHandler(matchedRoute->handler, {reqObj, resObj}, sd);
      }
      catch (const std::exception& e) {
        bool handled = false;
        for (auto& errMw : sd->middlewares) {
          if (!errMw.isErrorHandler) continue;
          try {
            gecko::NativeFunction errNext([](const std::vector<gecko::Value>&)
                                              -> gecko::Value { return nullptr; });
            auto errHandler = errMw.handler.as<gecko::NativeFunction>();
            errHandler({gecko::Value(std::string(e.what())), reqObj, resObj, errNext});
            if (sentFlag) { handled = true; break; }
          } catch (...) {}
        }
        if (!handled) {
          res.status = 500;
          res.set_content(R"({"error":")" + std::string(e.what()) + "\"}", "application/json");
        }
      }
      return;
    }

    // 404
    res.status = 404;
    std::string contentType = "text/plain";
    std::string body = "Cannot " + method + " " + path;
    if (path.find("/api") == 0 || path.find(".json") != std::string::npos) {
      contentType = "application/json";
      body = R"({"error":"Not found","path":")" + path + "\"}";
    } else if (path.find("/") == 0) {
      contentType = "text/html";
      body = "<!DOCTYPE html><html><head><title>404 Not Found</title>"
             "<style>body{font-family:monospace;padding:2em;}"
             "h1{color:#e74c3c;}</style></head><body>"
             "<h1>404</h1><p>Cannot " + method + " " + path + "</p>"
             "</body></html>";
    }
    res.set_content(body, contentType);
  };

  runMiddlewareChain(sd, 0, matchingMW, reqObj, resObj, res, &sentFlag, onDone);
}

void registerRoute(const std::shared_ptr<ServerData>& sd,
                   const std::string& method,
                   const std::string& pattern,
                   const gecko::Value& handler)
{
  std::vector<std::string> paramNames;
  std::regex patternRegex = makeRouteRegex(pattern, paramNames);
  sd->routes.push_back({method, pattern, paramNames, patternRegex, handler});

  auto routeHandler = [sd, method](const httplib::Request& req, httplib::Response& res) {
    processRequest(sd, req, res, method, req.path);
  };

  auto fallbackHandler = [sd](const httplib::Request& req, httplib::Response& res) {
    processRequest(sd, req, res, req.method, req.path);
  };

  if (method == "ALL") {
    sd->svr->Get(pattern, routeHandler);
    sd->svr->Post(pattern, routeHandler);
    sd->svr->Put(pattern, routeHandler);
    sd->svr->Delete(pattern, routeHandler);
    sd->svr->Patch(pattern, routeHandler);
    sd->svr->Options(pattern, routeHandler);
  } else {
    if (method == "GET") sd->svr->Get(pattern, routeHandler);
    else if (method == "POST") sd->svr->Post(pattern, routeHandler);
    else if (method == "PUT") sd->svr->Put(pattern, routeHandler);
    else if (method == "DELETE") sd->svr->Delete(pattern, routeHandler);
    else if (method == "PATCH") sd->svr->Patch(pattern, routeHandler);
    else if (method == "OPTIONS") sd->svr->Options(pattern, routeHandler);
  }
}

std::shared_ptr<Environment> createServerEnv(std::shared_ptr<ServerData> sd,
                                             std::shared_ptr<Environment> globals)
{
  auto env = std::make_shared<Environment>(globals);

  env->define("get",
              gecko::NativeFunction([sd](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.size() < 2 || !args[0].isString()) {
                  throw std::runtime_error("app.get expects (path, handler)");
                }
                registerRoute(sd, "GET", args[0].as<std::string>(), args[1]);
                return nullptr;
              }));

  env->define("post",
              gecko::NativeFunction([sd](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.size() < 2 || !args[0].isString()) {
                  throw std::runtime_error("app.post expects (path, handler)");
                }
                registerRoute(sd, "POST", args[0].as<std::string>(), args[1]);
                return nullptr;
              }));

  env->define("put",
              gecko::NativeFunction([sd](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.size() < 2 || !args[0].isString()) {
                  throw std::runtime_error("app.put expects (path, handler)");
                }
                registerRoute(sd, "PUT", args[0].as<std::string>(), args[1]);
                return nullptr;
              }));

  env->define("delete",
              gecko::NativeFunction([sd](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.size() < 2 || !args[0].isString()) {
                  throw std::runtime_error("app.delete expects (path, handler)");
                }
                registerRoute(sd, "DELETE", args[0].as<std::string>(), args[1]);
                return nullptr;
              }));

  env->define("patch",
              gecko::NativeFunction([sd](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.size() < 2 || !args[0].isString()) {
                  throw std::runtime_error("app.patch expects (path, handler)");
                }
                registerRoute(sd, "PATCH", args[0].as<std::string>(), args[1]);
                return nullptr;
              }));

  env->define("options",
              gecko::NativeFunction([sd](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.size() < 2 || !args[0].isString()) {
                  throw std::runtime_error("app.options expects (path, handler)");
                }
                registerRoute(sd, "OPTIONS", args[0].as<std::string>(), args[1]);
                return nullptr;
              }));

  env->define("all",
              gecko::NativeFunction([sd](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.size() < 2 || !args[0].isString()) {
                  throw std::runtime_error("app.all expects (path, handler)");
                }
                registerRoute(sd, "ALL", args[0].as<std::string>(), args[1]);
                return nullptr;
              }));

  env->define("use",
              gecko::NativeFunction([sd](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.empty()) {
                  throw std::runtime_error("app.use expects (path?, handler)");
                }
                std::string pathPrefix = "/";
                gecko::Value handler;
                bool isError = false;

                if (args.size() == 1) {
                  handler = args[0];
                } else {
                  if (args[0].isString()) {
                    pathPrefix = args[0].as<std::string>();
                    handler = args[1];
                  } else {
                    handler = args[0];
                  }
                }

                sd->middlewares.push_back({pathPrefix, handler, isError});
                return nullptr;
              }));

  env->define("error",
              gecko::NativeFunction([sd](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.empty()) {
                  throw std::runtime_error("app.error expects an error handler (err, req, res, next)");
                }
                sd->middlewares.push_back({"/", args[0], true});
                return nullptr;
              }));

  env->define("listen",
              gecko::NativeFunction([sd](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.empty() || !args[0].isNumber()) {
                  throw std::runtime_error("app.listen expects (port, host?)");
                }
                int port = static_cast<int>(args[0].as<double>());
                std::string host = "0.0.0.0";
                if (args.size() > 1 && args[1].isString()) {
                  host = args[1].as<std::string>();
                }

                if (sd->listening) {
                  throw std::runtime_error("Server is already listening");
                }

                sd->listening = true;
                sd->creatorInterpreter = Interpreter::current;

                std::cout << "[http] Server listening on http://" << host << ":" << port
                          << std::endl;
                sd->svr->listen(host, port);
                return nullptr;
              }));

  env->define(
      "close",
      gecko::NativeFunction([sd](const std::vector<gecko::Value>& /*args*/) -> gecko::Value {
        if (sd->svr && sd->listening) {
          sd->svr->stop();
          sd->listening = false;
        }
        return nullptr;
      }));

  env->define("routes",
              gecko::NativeFunction([sd](const std::vector<gecko::Value>&) -> gecko::Value {
                auto arr = std::make_shared<std::vector<gecko::Value>>();
                for (const auto& rt : sd->routes) {
                  auto entry = std::make_shared<Environment>();
                  entry->define("method", rt.method);
                  entry->define("path", rt.pattern);
                  entry->exportName("method");
                  entry->exportName("path");
                  arr->push_back(entry);
                }
                return arr;
              }));

  env->define("enableCors",
              gecko::NativeFunction([sd](const std::vector<gecko::Value>&) -> gecko::Value {
                sd->svr->set_pre_routing_handler(
                    [](const httplib::Request& req, httplib::Response& res) -> httplib::Server::HandlerResponse {
                      if (req.method == "OPTIONS") {
                        res.set_header("Access-Control-Allow-Origin", "*");
                        res.set_header("Access-Control-Allow-Methods",
                                       "GET, POST, PUT, DELETE, PATCH, OPTIONS");
                        res.set_header("Access-Control-Allow-Headers", "*");
                        res.set_header("Access-Control-Max-Age", "86400");
                        res.status = 204;
                        return httplib::Server::HandlerResponse::Handled;
                      }
                      // Add CORS headers to all responses
                      res.set_header("Access-Control-Allow-Origin", "*");
                      return httplib::Server::HandlerResponse::Unhandled;
                    });
                return nullptr;
              }));

  env->define("setGlobalPrefix",
              gecko::NativeFunction([sd](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.empty() || !args[0].isString()) {
                  throw std::runtime_error("app.setGlobalPrefix expects a path string");
                }
                std::string prefix = args[0].as<std::string>();
                // Re-register all existing routes with the prefix
                auto oldRoutes = sd->routes;
                for (auto& rt : sd->routes) {
                  rt.pattern = prefix + rt.pattern;
                }
                return nullptr;
              }));

  env->exportName("get");
  env->exportName("post");
  env->exportName("put");
  env->exportName("delete");
  env->exportName("patch");
  env->exportName("options");
  env->exportName("all");
  env->exportName("use");
  env->exportName("error");
  env->exportName("listen");
  env->exportName("close");
  env->exportName("routes");
  env->exportName("enableCors");
  env->exportName("setGlobalPrefix");

  return env;
}

} // namespace

// ──────────────────────────────────────────────
// Factory functions
// ──────────────────────────────────────────────

std::shared_ptr<Environment> createHttpModule(std::shared_ptr<Environment> globals)
{
  auto env = std::make_shared<Environment>(globals);
  env->setPackageName("http");

  // ── Client ──

  env->define("get",
              gecko::NativeFunction([](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.empty() || !args[0].isString()) {
                  throw std::runtime_error("http.get expects a URL string");
                }
                std::string urlStr = args[0].as<std::string>();
                auto url = parseUrl(urlStr);

                httplib::Client cli(url.protocol);
                cli.set_follow_location(true);
                auto res = cli.Get(url.path);
                return handleResponse(res);
              }));

  env->define("post",
              gecko::NativeFunction([](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.size() < 2 || !args[0].isString() || !args[1].isString()) {
                  throw std::runtime_error("http.post expects a URL string and a data string");
                }
                std::string urlStr = args[0].as<std::string>();
                std::string data = args[1].as<std::string>();
                auto url = parseUrl(urlStr);

                httplib::Client cli(url.protocol);
                cli.set_follow_location(true);
                auto res = cli.Post(url.path, data, "application/x-www-form-urlencoded");
                return handleResponse(res);
              }));

  env->define("put",
              gecko::NativeFunction([](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.size() < 2 || !args[0].isString() || !args[1].isString()) {
                  throw std::runtime_error("http.put expects a URL string and a data string");
                }
                std::string urlStr = args[0].as<std::string>();
                std::string data = args[1].as<std::string>();
                auto url = parseUrl(urlStr);

                httplib::Client cli(url.protocol);
                cli.set_follow_location(true);
                auto res = cli.Put(url.path, data, "application/x-www-form-urlencoded");
                return handleResponse(res);
              }));

  env->define("delete",
              gecko::NativeFunction([](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.empty() || !args[0].isString()) {
                  throw std::runtime_error("http.delete expects a URL string");
                }
                std::string urlStr = args[0].as<std::string>();
                auto url = parseUrl(urlStr);

                httplib::Client cli(url.protocol);
                cli.set_follow_location(true);
                auto res = cli.Delete(url.path);
                return handleResponse(res);
              }));

  env->define("request",
              gecko::NativeFunction([](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.size() < 4 || !args[0].isString() || !args[1].isString()) {
                  throw std::runtime_error("http.request expects (method, url, headers, data)");
                }
                std::string method = args[0].as<std::string>();
                std::string urlStr = args[1].as<std::string>();
                auto url = parseUrl(urlStr);

                httplib::Client cli(url.protocol);
                cli.set_follow_location(true);

                httplib::Headers headers;
                if (args[2].isArray()) {
                  auto headerVec = args[2].as<gecko::Array>();
                  for (const auto& h : *headerVec) {
                    if (h.isString()) {
                      std::string headerStr = h.as<std::string>();
                      auto colonPos = headerStr.find(':');
                      if (colonPos != std::string::npos) {
                        std::string key = headerStr.substr(0, colonPos);
                        std::string value = headerStr.substr(colonPos + 1);

                        if (isValidHeaderKey(key)) {
                          headers.insert({key, sanitizeHeaderValue(value)});
                        }
                      }
                    }
                  }
                }

                std::string data;
                if (args[3].isString()) {
                  data = args[3].as<std::string>();
                }

                if (method == "GET") {
                  auto res = cli.Get(url.path, headers);
                  return handleResponse(res);
                }
                else if (method == "POST") {
                  auto res = cli.Post(url.path, headers, data, "text/plain");
                  return handleResponse(res);
                }
                else if (method == "PUT") {
                  auto res = cli.Put(url.path, headers, data, "text/plain");
                  return handleResponse(res);
                }
                else if (method == "DELETE") {
                  auto res = cli.Delete(url.path, headers);
                  return handleResponse(res);
                }

                throw std::runtime_error("Unsupported method: " + method);
              }));

  // Export client names
  env->exportName("get");
  env->exportName("post");
  env->exportName("put");
  env->exportName("delete");
  env->exportName("request");

  // ── Async HTTP Client ──

  auto makeAsyncHttp = [](const std::string& method, bool hasBody) -> gecko::NativeFunction {
    return gecko::NativeFunction(
        [method, hasBody](const std::vector<gecko::Value>& args) -> gecko::Value {
          size_t minArgs = hasBody ? 2 : 1;
          if (args.size() < minArgs || !args[0].isString()) {
            throw std::runtime_error("http." + method + "Async expects a URL string"
                                     + (hasBody ? " and a data string" : ""));
          }
          std::string urlStr = args[0].as<std::string>();
          auto url = parseUrl(urlStr);

          auto promise = std::make_shared<PromiseValue>();
          std::thread([url, promise, args, hasBody, method]() {
            try {
              httplib::Client cli(url.protocol);
              cli.set_follow_location(true);
              httplib::Result res;
              if (method == "GET") {
                res = cli.Get(url.path);
              }
              else if (method == "POST") {
                std::string data = hasBody && args.size() > 1 && args[1].isString()
                                       ? args[1].as<std::string>()
                                       : "";
                res = cli.Post(url.path, data, "application/x-www-form-urlencoded");
              }
              else if (method == "PUT") {
                std::string data = hasBody && args.size() > 1 && args[1].isString()
                                       ? args[1].as<std::string>()
                                       : "";
                res = cli.Put(url.path, data, "application/x-www-form-urlencoded");
              }
              else if (method == "DELETE") {
                res = cli.Delete(url.path);
              }
              auto result = handleResponse(res);
              promise->resolve(std::move(result));
            }
            catch (...) {
              promise->p->set_exception(std::current_exception());
            }
          }).detach();

          return promise;
        });
  };

  env->define("getAsync", makeAsyncHttp("GET", false));
  env->define("postAsync", makeAsyncHttp("POST", true));
  env->define("putAsync", makeAsyncHttp("PUT", true));
  env->define("deleteAsync", makeAsyncHttp("DELETE", false));

  env->exportName("getAsync");
  env->exportName("postAsync");
  env->exportName("putAsync");
  env->exportName("deleteAsync");

  // ── Server ──

  env->define(
      "createServer",
      gecko::NativeFunction([globals](const std::vector<gecko::Value>& /*args*/) -> gecko::Value {
        auto sd = std::make_shared<ServerData>();
        sd->globals = globals;
        sd->creatorInterpreter = Interpreter::current;
        return createServerEnv(sd, globals);
      }));

  env->define("serveStatic",
              gecko::NativeFunction([](const std::vector<gecko::Value>& args) -> gecko::Value {
                if (args.size() < 2 || !args[0].isNumber() || !args[1].isString()) {
                  throw std::runtime_error("http.serveStatic expects (port, directoryPath)");
                }
                int port = static_cast<int>(args[0].as<double>());
                std::string dir = args[1].as<std::string>();

                httplib::Server svr;
                if (!svr.set_mount_point("/", dir)) {
                  throw std::runtime_error("http.serveStatic: invalid directory path");
                }
                std::cout << "[http] Serving static files from " << dir << " on port " << port
                          << std::endl;
                svr.listen("0.0.0.0", port);
                return nullptr;
              }));

  // Export server names
  env->exportName("createServer");
  env->exportName("serveStatic");

  return env;
}

} // namespace module
