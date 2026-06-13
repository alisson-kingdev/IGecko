#include "Runtime.h"

ReturnException::ReturnException(gecko::Value v) : value(std::move(v)) {}

const char* ReturnException::what() const noexcept
{
  return "Return statement executed";
}

UserFunction::UserFunction(std::string n,
                           std::vector<Token> params,
                           std::vector<std::shared_ptr<Stmt>> b,
                           Token decl,
                           std::shared_ptr<Environment> env,
                           bool isStat,
                           bool isGet,
                           bool isSet,
                           bool async)
    : name(std::move(n))
    , parameters(std::move(params))
    , body(std::move(b))
    , declaration(std::move(decl))
    , closure(std::move(env))
    , isStatic(isStat)
    , isGetter(isGet)
    , isSetter(isSet)
    , isAsync(async)
{}

UserClass::UserClass(std::string n,
                     std::shared_ptr<UserClass> super,
                     std::map<std::string, MethodDefinition> m,
                     std::map<std::string, MethodDefinition> sm)
    : name(std::move(n))
    , superclass(std::move(super))
    , methods(std::move(m))
    , staticMethods(std::move(sm))
{}

ClassInstance::ClassInstance(std::shared_ptr<UserClass> k) : klass(std::move(k)) {}
