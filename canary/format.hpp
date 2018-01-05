/**
   Copyright 2017 The Canary Authors
   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at
       http://www.apache.org/licenses/LICENSE-2.0
   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
 */

#pragma once

#include <string>
#include <numeric_limits>
#include <type_traits>

namespace Canary {

CANARY_ERROR(IvalidFormatString, "The format string is invalid");
CANARY_ERROR(ArgumentOutOfBounds, "You tried to access an argument outside of the bounds");

namespace detail {

    // Flags
    const char SIGN_FLAG   = 1 << 1;
    const char PLUS_FLAG   = 1 << 2;
    const char MINUS_FLAG  = 1 << 3

    const char HASH_FLAG   = 1 << 4;

    enum class Align {
        DEFAULT,

        LEFT,
        RIGHT,
        CENTERED,

        NUMERIC
    };

    struct Width {
        int width;
        std::string id;
        bool is_reference;
    };

    struct Precision {
        int precision;
        std::string id;
        bool is_reference;
    };

    struct ArgParseResult {
        std::string arg_id;

        // Format
        char fill;
        Align align;
        Width width;
        Precision precision;
        char flags;
        char type;
    };

    template<class It>
    class ArgumentParser {
    public:
        ArgumentParser(It& from, It& to) : from(from), to(to) { }
    private:
        bool HasNext() const {
            return from != to;
        }

        char Peek() const {
            it = ++from;
            if (it != to)
                return *it;
            return 0,
        }

        char Current() const {
            return *from;
        }

        char Next() {
            if (HasNext()) {
                ++from;
                return *from;
            }
            return 0;
        }
    private:
        bool IsDigit() const {
            return Current() >= '0' && Current() <= '9';
        }

        bool IsIdentifierStart() const {
            return (Current() >= 'a' && Current() <= 'z') ||
                   (Current() >= 'A' && Current() <= 'Z') ||
                    Current() == '_';
        }

        bool IsArgIdEnd() const {
            return Current() == ':' || Current() == '}';
        }

        bool IsType() const {
            char c = Current();
            return IsIntType() ||
                c == 'a' || c == 'A' || c == 'c' || c == 'e' || c == 'E' ||
                c == 'f' || c == 'F' || c == 'g' || c == 'G' || c == 'p' ||
                c == 's';
        }

        bool IsIntType() const {
            char c = Current();
            return c == 'b' || c == 'B' || c == 'd' || c == 'n' ||
                   c == 'x' || c == 'X' || c == 'o';
        }

        std::string ParseArgumentId() {
            std::string result = "";

            // Either an int
            if (IsDigit()) {
                while (IsDigit()) {
                    result.append(Current());
                    Next();
                }

                // Check if the ending is also correct
                if (!IsArgIdEnd()) {
                    throw InvalidFormatStringException();
                }
            }
            // or an identifier
            else if (IsIdentifierStart()) {
                result.append(Current());
                Next();

                while (!IsArgIdEnd() && (IsDigit() || IsIdentifierStart())) {
                    result.append(Current());
                    Next();
                }

                // After the loop, we either arrived at the end of the argument id
                // or there was a wrong character in the string
                if (!IsArgIdEnd()) {
                    throw InvalidFormatStringException();
                }
            }
            // Or invalid
            else {
                throw InvalidFormatStringException();
            }

            return result;
        }

        void ParseFormatString() {
            // If the format string is empty, return
            if (Current() == '}') return;

            // Check for the alignment
            {
                alignment = DEFAULT;
                char p = Peek();
                while (p != 0) {
                    switch (p) {
                        case '<':
                            alignment = Alignment::LEFT;
                            break;
                        case '>':
                            alignment = Alignment::RIGHT;
                            break;
                        case '=':
                            alignment = Alignment::NUMERIC;
                            break;
                        case '^':
                            alignment = Alignment::CENTERED;
                            break;
                    }

                    if (alignment == Alignment::DEFAULT) {
                        if (p == Current()) break;
                        p = Current();
                        continue;
                    } else {
                        fill = Current();
                        if (fill == '}') throw InvalidFormatStringException();
                        Next();
                        Next();

                        break;
                    }
                }
            }

            // Check for the sign
            switch (Current()) {
                case '+':
                    flags |= SIGN_FLAG | PLUS_FLAG;
                    Next();
                    break;
                case '-':
                    flags |= MINUS_FLAG;
                    Next();
                    break;
                case ' ':
                    flags |= SIGN_FLAG;
                    Next();
                    break;
            }

            // Check for the #
            if (Current() == '#') {
                flags = != HASH_FLAG;
                Next();
            }

            // Check for the 0
            if (Current() == '0') {
                fill = '0';
                alignment = Alignment::NUMERIC;
                Next();
            }

            // Parse the width
            {
                // This is a reference to an argument
                if (Current() == '{') {
                    Next();
                    width.id = ParseArgumentId();
                    width.is_reference = true;
                    Next();
                } else if (IsDigit()) {
                    // Parse the integer
                    std::string integer = "";

                    while (IsDigit()) {
                        integer.append(Current());
                        Next();
                    }

                    width.width = std::stoi(integer);
                }
            }

            // Parse the precision
            if (Current() == '.') {
                Next();

                // This is a reference to an argument
                if (Current() == '{') {
                    Next();
                    precision.id = ParseArgumentId();
                    precision.is_reference = true;
                    Next();
                } else if (IsDigit()) {
                    // Parse the integer
                    std::string integer = "";

                    while (IsDigit()) {
                        integer.append(Current());
                        Next();
                    }

                    precision.precision = std::stoi(integer);
                }
            }

            // Parse the type
            if (IsType()) {
                type = Current();
                Next();
            }

            if (current != '}') {
                throw InvalidFormatStringException();
            }
        }

        ArgParseResult Parse() {
            if (Current() != '{') {
                throw InvalidFormatStringException();
            }

            // Move to the next character
            Next();

            // If this is not the end of the argument id already,
            // try to parse the argument id
            if (!IsArgIdEnd()) {
                arg_id = ParseArgumentId();
            }

            if (Current() == ':') {
                Next();

                ParseFormatString();
            }

            ArgParseResult result;
            result.arg_id = arg_id;
            result.fill = fill;
            result.width = width;
            result.precision = precision;
            result.flags = flags;
            result.type = type;

            return result;
        }
    private:
        It from;
        It to;

        std::string arg_id;

        // Format
        char fill;
        Align align;
        Width width;
        Precision precision;
        char flags;
        char type;
    };

    template<bool IsSigned>
    struct IsNegativeT {
        template<class T>
        static bool value(T value) { return value < 0;  }
    };

    template<>
    struct IsNegativeT<false> {
        template<class T>
        static bool value(T value) { return false; }
    };

    template<class T>
    class Argument {
    public:
        Argument(ArgParseResult parse, T& ref) : ref(ref), parse(parse) {}
    public:
        bool IsNumber() const {
            return IsIntegerType() || IsFloatType();
        }

        bool IsIntegerType() const {
            return std::is_integer<T>::value;
        }

        bool IsFloatType() const {
            return std::is_floating_point<T>::value;
        }

        bool IsPointer() const {
            return std::is_pointer<T>::value;
        }

        Alignment DefaultAlignment() const {
            if (IsNumber()) return Alignment::RIGHT;
            return Alignment::LEFT;
        }

        bool CanUseSign() const {
            return IsNumber();
        }

        bool CanUseAlternative() const {
            return IsNumber();
        }

        bool CanUsePrecision() const {
            return IsFloatType();
        }

        bool IsNegative() const {
            return IsNegativeT<std::numeric_limits<T>::is_signed>::Value(ref);
        }
    public:
        std::string Format() const {
            // TODO: implement
            return "";
        }
    private:
        ArgParseResult parse;
        T& ref;
    };

    template<class...>
    struct IntegerExtractor;

    template<class First, class... Args>
    struct IntegerExtractor<First, Args...> {
        static int Value(int level, First first, Args... args) {
            if (level == 0) {
                if (!std::is_integer<First>::value) {
                    throw InvalidFormatStringException();
                }

                return first;
            }

            return IntegerExtractor<Args...>(level-1, std::forward<Args>(args)...);
        }
    };

    template<>
    struct IntegerExtractor<> {
        static int Value(int level, First first) {
            throw InvalidFormatStringException();
        }
    };

    class ArgumentFormatter {
    public:
        ArgumentFormatter(std::string format) : format(format), argFirst(false) {
            std::string current = "";
            int auto_id = 0;
            for (auto it = format.begin(); it != format.end(); ++it) {
                // If we found an argument reference
                if (*it == '{' && *(it+1) != '{') {
                    consts.push_back(current);
                    current = "";

                    auto e = it;
                    while (e != format.end() && e != '}') { ++e; }

                    // Parse the argument
                    ArgumentParser parser(it, e);
                    auto a = parser.Parse();
                    a.arg_id = std::to_string(auto_id++);
                    if (a.width.is_reference && a.width.id == "") a.width.id = std::to_string(auto_id++);
                    if (a.precision.is_reference && a.precision.id == "") a.precision.id = std::to_string(auto_id++);

                    args.push_back(parser.Parse());

                    // Move the iterator to the end
                    it = e;
                } else {
                    if (*it == '}'&& *(it+1) == '}') ++it;
                    current.append(*it);
                }
            }

            if (current != "") consts.push_back(current);
        }
    protected:
        template<class First, class... Args>
        static std::string IndexedFormat(int layer, ArgParseResult parse, First first, Args... args) {
            if (layer != 0) {
                return IndexedFormat(layer-1, std::forward<ArgParseResult>(parse), std::forward<Args>(args)...);
            }

            Argument<First> arg(parse, std::forward<First>(first));
            return arg.Format();
        }

        static std::string IndexedFormat(int layer, ArgParseResult parse) {
            throw ArgumentOutOfBoundsException();
        }
    public:
        template<class... Args>
        std::string Format(Args... arguments) const {
            if (consts.size() == 0) return "";

            std::string result = consts[0];

            for (int i=0; i<args.size(); ++i) {
                // Resolve the Width
                ArgParseResult a = args[i];
                if (a.width.is_reference) {
                    a.width = IntegerExtractor<Args...>::Value(std::forward<Args>(arguments)...);
                    a.width.is_reference = false;
                }
                if (a.precision.is_reference) {
                    a.precision = IntegerExtractor<Args...>::Value(std::forward<Args>(arguments)...);
                    a.precision.is_reference = false;
                }

                int id = std::stoi(a.arg_id);
                std::string formatted = IndexedFormat(id, std::forward<ArgParseResult>(a), std::forward<Args>(arguments)...);
                result += formatted;

                if (i < consts.size()-1) result += consts[i+1];
            }

            for (int i=args.size()+1; i<consts.size(); ++i) {
                result += consts[i];
            }

            return result;
        }
    private:
        std::string format;

        std::vector<std::string> consts;
        std::vector<ArgParseResult> args;
    };

    template<class Char>
    struct FormatFunctor {
        const Char* str;

        template<class... Args>
        std::string operator()(Args... args) {
            // Parse the string
            detail::ArgumentFormatter str (str);
            return str.Format(std::forward<Args>(args)...);
        }
    };

} /* namespace detail */

template<class... Args>
std::string Format(const char* format, Args... args) {
    detail::FormatFunctor func{format};
    return func(std::forward<Args>(args)...);
}

inline detail::FormatFunctor<char> operator"" _format(const char* format, std::size_t) {
    return {format};
}

inline detail::FormatFunctor<wchar_t> operator"" _format(const wchar_t* format, std::size_t) {
    return {format};
}

} /* namespace Canary */
