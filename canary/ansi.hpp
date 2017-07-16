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

namespace Canary {
namespace Ansi {

    namespace detail {

        /**
            Convert an integer into an array of chars
         */
        template<char... Digits>
        struct CompileTimeString;

        template<unsigned Rem, unsigned... Digits>
        struct Explode : Explode<Rem / 10, Rem % 10, Digits...> {};

        template<unsigned... Digits>
        struct Explode<0, Digits...> {
            using Type = CompileTimeString<('0' + Digits)...>;
        };

        template<unsigned v>
        using ToCompileTimeString = typename Explode<v>::Type;

        /**
            Remove zero at the end
         */

        /**
            Concatenate two compile time strings
         */
        template<class...>
        struct ConcatenateImpl;

        template<template<char...> class U, char... S1, template<char...> class V, char... S2, class... W>
        struct ConcatenateImpl<U<S1...>, V<S2...>, W...> {
            using Type = typename ConcatenateImpl<CompileTimeString<S1..., S2...>, W...>::Type;
        };

        template<template<char...> class U, char... S>
        struct ConcatenateImpl<U<S...>> {
            using Type = CompileTimeString<S...>;
        };

        template<template<char...> class U>
        struct ConcatenateImpl<U<>> {
            using Type = CompileTimeString<>;
        };

        template<class... Us>
        using Concatenate = typename ConcatenateImpl<Us...>::Type;

        /**
            CompileTimeString to const char
         */
        template<class U>
        struct ConvertCompileTimeString;

        template<template<char...> class U, char... S>
        struct ConvertCompileTimeString<U<S...>> {
            static const char Value[];
        };

        template<template<char...> class U, char... S>
        const char ConvertCompileTimeString<U<S...>>::Value[] = { S..., 0 };

        /**
            CodeList
         */
        template<unsigned... Codes>
        struct CodeList;

        template<unsigned Code, unsigned... Codes>
        struct CodeList<Code, Codes...> {
            using Type = Concatenate<
                ToCompileTimeString<Code>,
                CompileTimeString<';'>,
                typename CodeList<Codes...>::Type
            >;
        };

        template<unsigned Code>
        struct CodeList<Code> {
            using Type = ToCompileTimeString<Code>;
        };

        /**
            Convert to CodeList
         */
        template<class>
        struct ToCodeListImpl;

        template<class T>
        using ToCodeList = typename ToCodeListImpl<T>::Type;

        template<unsigned... S>
        struct ToCodeListImpl<CodeList<S...>> {
            using Type = CodeList<S...>;
        };

        /**
            Merge two CodeLists
         */
        template<class...>
        struct MergeCodeListsImpl;

        template<template<unsigned...> class L, unsigned... C1, class... K>
        struct MergeCodeListsImpl<L<C1...>, K...> {
            using Result = typename MergeCodeListsImpl<K...>::Type;
            using Type = typename MergeCodeListsImpl<L<C1...>, Result>::Type;
        };

        template<template<unsigned...> class L, unsigned... C1, template<unsigned...> class K, unsigned... C2>
        struct MergeCodeListsImpl<L<C1...>, K<C2...>> {
            using Type = CodeList<C1..., C2...>;
        };

        template<template<unsigned...> class L, unsigned... C>
        struct MergeCodeListsImpl<L<C...>> {
            using Type = CodeList<C...>;
        };

        template<class... Ls>
        using MergeCodeLists = typename MergeCodeListsImpl<ToCodeList<Ls>...>::Type;

        /**
            EcapeCode
         */
        template<class L>
        struct EscapeCode {
            using AnsiCode = Concatenate<
                CompileTimeString<'\033', '['>,
                typename ToCodeList<L>::Type,
                CompileTimeString<'m'>
            >;

            template<class T>
            EscapeCode(T& out) {
                out << ConvertCompileTimeString<AnsiCode>::Value;
            }

            std::string ToString() const {
                return std::string(ConvertCompileTimeString<AnsiCode>::Value);
            }
        };

        template<class... L>
        using EscapeCodes = EscapeCode<MergeCodeLists<L...>>;

        template<class L>
        struct ToCodeListImpl<EscapeCode<L>> {
            using Type = ToCodeList<L>;
        };

        /**
            EscapeSequence

            The key element in the Ansi part. It contains a list of
            Ansi Escape codes and prints it in the constructor to the
            given stream object.

            It also support RAII, meaning that in the destructor the
            reset escape code is written to the output. This can be
            deactivated by calling the CancelReset() method.

            Furthermore, it is possible to directly write an
            escape sequence to a stream with the << operator. In this
            case the reset is deactivated automatically.
         */
        template<class L>
        struct EscapeSequence {
        public:
            struct InnerBase {
                using Pointer = InnerBase*;

                virtual void Print(const std::string& string) = 0;
            };

            template<class T>
            struct Inner : InnerBase {
                //Inner(T value) : value(value) {}
                Inner(T* value) : value(value) {}
                //Inner(T&& value) : value(std::move(value)) {}

                virtual void Print(const std::string& string) {
                    (*value) << string;
                }
            private:
                T* value;
            };
        public:
            EscapeSequence() : reset(false) {}

            template<class T>
            EscapeSequence(T& out) : out(new Inner<T>(&out)) {
                // Print the code
                EscapeCode<L> code(out);
            }

            virtual ~EscapeSequence() {
                if (reset) {
                    // Print reset
                    out->Print("\033[0m");

                    // TODO: test if this is secure and add a state machine
                }
            }

            void CancelReset() {
                reset = false;
            }
        private:
            bool reset=true;
            typename InnerBase::Pointer out;
        };

        /**
            Inline method to use the Ansi styles.

            For example, you can use

                std::cout << Canary::Ansi::Bold() << "Bold text" << Canary::Ansi::Reset();

            to directly print bold text.

            Keep in mind that this will cancel the reset and you have to do this
            by yourself.
         */
        template<class T, class L>
        T& operator<<(T& out, EscapeSequence<L> sequence) {
            EscapeSequence<L> code(out);
            code.CancelReset();
            return out;
        }

        /**
            Implementation to extract the code list
            from an escape sequence.
         */
        template<class L>
        struct ToCodeListImpl<EscapeSequence<L>> {
            using Type = ToCodeList<L>;
        };

        template<class... Ls>
        using EscapeSequences = EscapeSequence<MergeCodeLists<Ls...>>;

    } /* namespace detail */

    template<class... Codes>
    using EscapeSequences = detail::EscapeSequences<Codes...>;

    template<unsigned code>
    using EscapeSequence = detail::EscapeSequence<detail::CodeList<code>>;

    /**
        Short hand to create a custom Ansi Style.

        You hand a list of CodeLists, EscapeSequences or EscapeCodes
        to an alias for the Style template, and the compiler will automatically
        generate a new EscapeSequence from this.

        Example:

            using HeaderStyle = Canary::Ansi::Style<
                Canary::Ansi::Bold,
                Canary::Ansi::GreenForeground
            >;

            // ...

            // Scope based formatting
            {
                HeaderStyle style(std::cout);
                std::cout << "Title" << std::endl;
            }
     */
    template<class... Codes>
    using Style = detail::EscapeSequences<Codes...>;

    // Macro for the definition of the escape codes
    #define CANARY_ESCAPE_CODE(name, number) \
        using name = EscapeSequence<number>;

    /* Styles */
    CANARY_ESCAPE_CODE(Reset, 0);

    CANARY_ESCAPE_CODE(Bold, 1);
    CANARY_ESCAPE_CODE(Faint, 2);
    CANARY_ESCAPE_CODE(Italic, 3);
    CANARY_ESCAPE_CODE(Underline, 4);
    CANARY_ESCAPE_CODE(SlowBlink, 5);
    CANARY_ESCAPE_CODE(RapidBlink, 6);
    CANARY_ESCAPE_CODE(ImageNegative, 7);
    CANARY_ESCAPE_CODE(Conceal, 8);
    CANARY_ESCAPE_CODE(CrossedOut, 9);

    /* Colors */
    CANARY_ESCAPE_CODE(DefaultForeground, 39);
    CANARY_ESCAPE_CODE(BlackForeground, 30);
    CANARY_ESCAPE_CODE(RedForeground, 31);
    CANARY_ESCAPE_CODE(GreenForeground, 32);
    CANARY_ESCAPE_CODE(YellowForeground, 33);
    CANARY_ESCAPE_CODE(BlueForeground, 34);
    CANARY_ESCAPE_CODE(MagentaForeground, 35);
    CANARY_ESCAPE_CODE(CyanForeground, 36);
    CANARY_ESCAPE_CODE(LightGrayForeground, 37);
    CANARY_ESCAPE_CODE(DarkGrayForeground, 90);
    CANARY_ESCAPE_CODE(LightRedForeground, 91);
    CANARY_ESCAPE_CODE(LightGreenForeground, 92);
    CANARY_ESCAPE_CODE(LightYellowForeground, 93);
    CANARY_ESCAPE_CODE(LightBlueForeground, 94);
    CANARY_ESCAPE_CODE(LightMagentaForeground, 95);
    CANARY_ESCAPE_CODE(LightCyanForeground, 96);
    CANARY_ESCAPE_CODE(WhiteForeground, 97);

    CANARY_ESCAPE_CODE(DefaultBackground, 49);
    CANARY_ESCAPE_CODE(BlackBackground, 40);
    CANARY_ESCAPE_CODE(RedBackground, 41);
    CANARY_ESCAPE_CODE(GreenBackground, 42);
    CANARY_ESCAPE_CODE(YellowBackground, 43);
    CANARY_ESCAPE_CODE(BlueBackground, 44);
    CANARY_ESCAPE_CODE(MagentaBackground, 45);
    CANARY_ESCAPE_CODE(CyanBackground, 46);
    CANARY_ESCAPE_CODE(LightGrayBackground, 47);
    CANARY_ESCAPE_CODE(DarkGrayBackground, 100);
    CANARY_ESCAPE_CODE(LightRedBackground, 101);
    CANARY_ESCAPE_CODE(LightGreenBackground, 102);
    CANARY_ESCAPE_CODE(LightYellowBackground, 103);
    CANARY_ESCAPE_CODE(LightBlueBackground, 104);
    CANARY_ESCAPE_CODE(LightMagentaBackground, 105);
    CANARY_ESCAPE_CODE(LightCyanBackground, 106);
    CANARY_ESCAPE_CODE(WhiteBackground, 107);

    #undef CANARY_ESCAPE_CODE

} /* namespace Ansi */
} /* namespace Canary */
