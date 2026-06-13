#ifndef RUNTIME_H
#define RUNTIME_H

#include <future>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "../../ast/Nodes.h"
#include "Value.h"

class Environment;

using GeckoFunction = gecko::NativeFunction;

class ReturnException : public std::exception
{
public:
  explicit ReturnException(gecko::Value v);
  const char* what() const noexcept override;
  gecko::Value value;
};

struct UserFunction
{
  std::string name;
  std::vector<Token> parameters;
  std::vector<std::shared_ptr<Stmt>> body;
  Token declaration;
  std::shared_ptr<Environment> closure;

  bool isStatic;
  bool isGetter;
  bool isSetter;
  bool isAsync;

  UserFunction(std::string n,
               std::vector<Token> params,
               std::vector<std::shared_ptr<Stmt>> b,
               Token decl,
               std::shared_ptr<Environment> env,
               bool isStat = false,
               bool isGet = false,
               bool isSet = false,
               bool async = false);
  virtual ~UserFunction() = default;
};

struct MethodDefinition
{
  std::shared_ptr<UserFunction> regular;
  std::shared_ptr<UserFunction> getter;
  std::shared_ptr<UserFunction> setter;
};

struct UserClass
{
  std::string name;
  std::shared_ptr<UserClass> superclass;
  std::map<std::string, MethodDefinition> methods;
  std::map<std::string, MethodDefinition> staticMethods;

  UserClass(std::string n,
            std::shared_ptr<UserClass> super,
            std::map<std::string, MethodDefinition> m,
            std::map<std::string, MethodDefinition> sm);
};

struct ClassInstance
{
  std::shared_ptr<UserClass> klass;
  std::map<std::string, gecko::Value> fields;

  explicit ClassInstance(std::shared_ptr<UserClass> k);
};

struct PromiseValue
{
  std::shared_ptr<std::promise<gecko::Value>> p;
  std::shared_ptr<std::future<gecko::Value>> f;

  PromiseValue()
      : p(std::make_shared<std::promise<gecko::Value>>())
      , f(std::make_shared<std::future<gecko::Value>>(p->get_future()))
  {}

  void resolve(gecko::Value val)
  {
    p->set_value(std::move(val));
  }

  gecko::Value get()
  {
    return f->get();
  }
};

using ModuleCache = std::map<std::string, std::shared_ptr<Environment>>;

#endif // RUNTIME_H
