#ifndef GECKO_VALUE_H
#define GECKO_VALUE_H

#include <functional>
#include <memory>
#include <string>
#include <variant>
#include <vector>

// Forward declarations
struct UserFunction;
struct UserClass;
struct ClassInstance;
struct PromiseValue;
class Environment;

namespace gecko
{

struct Value;

/**
 * @brief Represents the 'null' or 'nil' value in IGecko.
 */
using Nil = std::monostate;

/**
 * @brief Managed array type.
 */
using Array = std::shared_ptr<std::vector<Value>>;

/**
 * @brief Managed object type (represented by an Environment).
 */
using Object = std::shared_ptr<Environment>;

/**
 * @brief Managed promise type for async operations.
 */
using Promise = std::shared_ptr<PromiseValue>;

/**
 * @brief Type for native C++ functions callable from IGecko.
 */
using NativeFunction = std::function<Value(const std::vector<Value>&)>;

/**
 * @brief The variant of all possible IGecko values.
 */
using ValueVariant = std::variant<Nil,
                                  bool,
                                  double,
                                  std::string,
                                  std::shared_ptr<UserFunction>,
                                  std::shared_ptr<UserClass>,
                                  std::shared_ptr<ClassInstance>,
                                  NativeFunction,
                                  Array,
                                  Object,
                                  Promise>;

/**
 * @brief A type-safe container for IGecko values using std::variant.
 */
struct Value
{
  ValueVariant data;

  // Constructors
  Value() : data(Nil{}) {}

  Value(Nil) : data(Nil{}) {}

  Value(bool b) : data(b) {}

  Value(double d) : data(d) {}

  Value(std::string s) : data(std::move(s)) {}

  Value(const char* s) : data(std::string(s)) {}

  Value(std::shared_ptr<UserFunction> f) : data(std::move(f)) {}

  Value(std::shared_ptr<UserClass> c) : data(std::move(c)) {}

  Value(std::shared_ptr<ClassInstance> i) : data(std::move(i)) {}

  Value(NativeFunction f) : data(std::move(f)) {}

  Value(Array a) : data(std::move(a)) {}

  Value(Object o) : data(std::move(o)) {}

  Value(Promise p) : data(std::move(p)) {}

  Value(std::nullptr_t) : data(Nil{}) {}

  // Type checks
  bool isNil() const
  {
    return std::holds_alternative<Nil>(data);
  }

  bool isBool() const
  {
    return std::holds_alternative<bool>(data);
  }

  bool isNumber() const
  {
    return std::holds_alternative<double>(data);
  }

  bool isString() const
  {
    return std::holds_alternative<std::string>(data);
  }

  bool isUserFunction() const
  {
    return std::holds_alternative<std::shared_ptr<UserFunction>>(data);
  }

  bool isUserClass() const
  {
    return std::holds_alternative<std::shared_ptr<UserClass>>(data);
  }

  bool isInstance() const
  {
    return std::holds_alternative<std::shared_ptr<ClassInstance>>(data);
  }

  bool isNativeFunction() const
  {
    return std::holds_alternative<NativeFunction>(data);
  }

  bool isArray() const
  {
    return std::holds_alternative<Array>(data);
  }

  bool isObject() const
  {
    return std::holds_alternative<Object>(data);
  }

  bool isPromise() const
  {
    return std::holds_alternative<Promise>(data);
  }

  // Unsafe access (use with care or after type check)
  template<typename T>
  const T& as() const
  {
    return std::get<T>(data);
  }

  template<typename T>
  T& as()
  {
    return std::get<T>(data);
  }
};

/**
 * @brief Helper for std::visit with multiple lambdas.
 */
template<class... Ts>
struct overloaded : Ts...
{
  using Ts::operator()...;
};
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

} // namespace gecko

#endif // GECKO_VALUE_H
