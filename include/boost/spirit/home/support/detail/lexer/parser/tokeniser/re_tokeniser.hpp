// tokeniser.hpp
// Copyright (c) 2007 Ben Hanson
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file licence_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_LEXER_RE_TOKENISER_HPP
#define BOOST_LEXER_RE_TOKENISER_HPP

#include <map>
#include "num_token.hpp"
#include "../../runtime_error.hpp"
#include "../../size_t.hpp"
#include <sstream>
#include "../../string_token.hpp"
#include "re_tokeniser_helper.hpp"

namespace boost
{
namespace lexer
{
namespace internal
{
template<typename CharT>
class basic_re_tokeniser
{
public:
    typedef basic_num_token<CharT> num_token;
    typedef basic_re_tokeniser_state<CharT> state;
    typedef basic_string_token<CharT> string_token;
    typedef typename string_token::string string;
    typedef std::map<string_token, std::size_t> token_map;
    typedef std::pair<string_token, std::size_t> token_pair;

    static void next (state &state_, token_map &map_, num_token &token_)
    {
        CharT ch_ = 0;
        bool eos_ = state_.next (ch_);

        token_.min_max (0, false, 0);

        while (!eos_ && ch_ == '"')
        {
            state_._in_string ^= 1;
            eos_ = state_.next (ch_);
        }

        if (eos_)
        {
            if (state_._in_string)
            {
                throw runtime_error ("Unexpected end of regex "
                    "(missing '\"').");
            }

            token_.set (num_token::END, null_token);
        }
        else
        {
            if (ch_ == '\\')
            {
                // Even if we are in a string, respect escape sequences...
                escape (state_, map_, token_);
            }
            else if (state_._in_string)
            {
                // All other meta characters lose their special meaning
                // inside a string.
                create_charset_token (string (1, ch_), false, map_, token_);
            }
            else
            {
                // Not an escape sequence and not inside a string, so
                // check for meta characters.
                switch (ch_)
                {
                case '(':
                    token_.set (num_token::OPENPAREN, null_token);
                    ++state_._paren_count;
                    break;
                case ')':
                    --state_._paren_count;

                    if (state_._paren_count < 0)
                    {
                        std::ostringstream ss_;

                        ss_ << "Number of open parenthesis < 0 at index " <<
                            state_._index - 1 << '.';
                        throw runtime_error (ss_.str ().c_str ());
                    }

                    token_.set (num_token::CLOSEPAREN, null_token);
                    break;
                case '?':
                    token_.set (num_token::OPT, null_token);
                    break;
                case '*':
                    token_.set (num_token::ZEROORMORE, null_token);
                    break;
                case '+':
                    token_.set (num_token::ONEORMORE, null_token);
                    break;
                case '{':
                    open_curly (state_, token_);
                    break;
                case '|':
                    token_.set (num_token::OR, null_token);
                    break;
                case '^':
                    token_.set (num_token::CHARSET, bol_token);
                    state_._seen_BOL_assertion = true;
                    break;
                case '$':
                    token_.set (num_token::CHARSET, eol_token);
                    state_._seen_EOL_assertion = true;
                    break;
                case '.':
                {
                    string dot_;

                    if (state_._dot_not_newline)
                    {
                        dot_ = '\n';
                    }

                    create_charset_token (dot_, true, map_, token_);
                    break;
                }
                case '[':
                {
                    charset (state_, map_, token_);
                    break;
                }
                default:
                    if (!state_._case_sensitive &&
                        (std::isupper (ch_, state_._locale) ||
                        std::islower (ch_, state_._locale)))
                    {
                        CharT upper_ = std::toupper (ch_, state_._locale);
                        CharT lower_ = std::tolower (ch_, state_._locale);

                        string str_ (1, upper_);

                        str_ += lower_;
                        create_charset_token (str_, false, map_, token_);
                    }
                    else
                    {
                        create_charset_token (string (1, ch_), false,
                            map_, token_);
                    }

                    break;
                }
            }
        }
    }

private:
    typedef basic_re_tokeniser_helper<CharT> tokeniser_helper;

    static void escape (state &state_, token_map &map_, num_token &token_)
    {
        CharT ch_ = 0;
        std::size_t str_len_ = 0;
        const CharT *str_ = tokeniser_helper::escape_sequence (state_,
            ch_, str_len_);

        if (str_)
        {
            state state2_ (str_ + 1, str_ + str_len_, state_._case_sensitive,
                state_._locale, state_._dot_not_newline);

            charset (state2_, map_, token_);
        }
        else
        {
            create_charset_token (string (1, ch_), false, map_, token_);
        }
    }

    static void charset (state &state_, token_map &map_, num_token &token_)
    {
        string chars_;
        bool negated_ = false;

        tokeniser_helper::charset (state_, chars_, negated_);
        create_charset_token (chars_, negated_, map_, token_);
    }

    static void create_charset_token (const string &charset_,
        const bool negated_, token_map &map_, num_token &token_)
    {
        std::size_t id_ = null_token;
        string_token stok_ (negated_, charset_);

        stok_.remove_duplicates ();
        stok_.normalise ();

        typename token_map::const_iterator iter_ = map_.find (stok_);

        if (iter_ == map_.end ())
        {
            id_ = map_.size ();
            map_.insert (token_pair (stok_, id_));
        }
        else
        {
            id_ = iter_->second;
        }

        token_.set (num_token::CHARSET, id_);
    }

    static void open_curly (state &state_, num_token &token_)
    {
        if (state_.eos ())
        {
            throw runtime_error ("Unexpected end of regex "
                "(missing '}').");
        }
        else if (*state_._curr >= '0' && *state_._curr <= '9')
        {
            repeat_n (state_, token_);
        }
        else
        {
            macro (state_, token_);
        }
    }

    // SYNTAX:
    //   {n[,[n]]}
    // SEMANTIC RULES:
    //   {0} - INVALID (throw exception)
    //   {0,} = *
    //   {0,0} - INVALID (throw exception)
    //   {0,1} = ?
    //   {1,} = +
    //   {min,max} where min == max - {min}
    //   {min,max} where max < min - INVALID (throw exception)
    static void repeat_n (state &state_, num_token &token_)
    {
        CharT ch_ = 0;
        bool eos_ = state_.next (ch_);

        while (!eos_ && ch_ >= '0' && ch_ <= '9')
        {
            token_._min *= 10;
            token_._min += ch_ - '0';
            eos_ = state_.next (ch_);
        }

        if (eos_)
        {
            throw runtime_error ("Unexpected end of regex "
                "(missing '}').");
        }

        bool min_max_ = false;
        bool repeatn_ = true;

        token_._comma = ch_ == ',';

        if (token_._comma)
        {
            eos_ = state_.next (ch_);

            if (eos_)
            {
                throw runtime_error ("Unexpected end of regex "
                    "(missing '}').");
            }

            if (ch_ == '}')
            {
                // Small optimisation: Check for '*' equivalency.
                if (token_._min == 0)
                {
                    token_.set (num_token::ZEROORMORE, null_token);
                    repeatn_ = false;
                }
                // Small optimisation: Check for '+' equivalency.
                else if (token_._min == 1)
                {
                    token_.set (num_token::ONEORMORE, null_token);
                    repeatn_ = false;
                }
            }
            else
            {
                if (ch_ < '0' || ch_ > '9')
                {
                    std::ostringstream ss_;

                    ss_ << "Missing '}' at index " << state_._index - 1 << '.';
                    throw runtime_error (ss_.str ().c_str ());
                }

                min_max_ = true;

                do
                {
                    token_._max *= 10;
                    token_._max += ch_ - '0';
                    eos_ = state_.next (ch_);
                } while (!eos_ && ch_ >= '0' && ch_ <= '9');

                if (eos_)
                {
                    throw runtime_error ("Unexpected end of regex "
                        "(missing '}').");
                }

                // Small optimisation: Check for '?' equivalency.
                if (token_._min == 0 && token_._max == 1)
                {
                    token_.set (num_token::OPT, null_token);
                    repeatn_ = false;
                }
                // Small optimisation: if min == max, then min.
                else if (token_._min == token_._max)
                {
                    token_._comma = false;
                    min_max_ = false;
                    token_._max = 0;
                }
            }
        }

        if (ch_ != '}')
        {
            std::ostringstream ss_;

            ss_ << "Missing '}' at index " << state_._index - 1 << '.';
            throw runtime_error (ss_.str ().c_str ());
        }

        if (repeatn_)
        {
            // SEMANTIC VALIDATION follows:
            // NOTE: {0,} has already become *
            // therefore we don't check for a comma.
            if (token_._min == 0 && token_._max == 0)
            {
                std::ostringstream ss_;

                ss_ << "Cannot have exactly zero repeats preceding index " <<
                    state_._index << '.';
                throw runtime_error (ss_.str ().c_str ());
            }

            if (min_max_ && token_._max < token_._min)
            {
                std::ostringstream ss_;

                ss_ << "Max less than min preceding index " << state_._index
                    << '.';
                throw runtime_error (ss_.str ().c_str ());
            }

            token_.set (num_token::REPEATN, null_token);
        }
    }

    static void macro (state &state_, num_token &token_)
    {
        CharT ch_ = 0;
        bool eos_ = false;
        const CharT *start_ = state_._curr;

        state_.next (ch_);

        if (ch_ != '_' && !(ch_ >= 'A' && ch_ <= 'Z') &&
            !(ch_ >= 'a' && ch_ <= 'z'))
        {
            std::ostringstream ss_;

            ss_ << "Invalid MACRO name at index " <<
                state_._index - 1 << '.';
            throw runtime_error (ss_.str ().c_str ());
        }

        do
        {
            eos_ = state_.next (ch_);

            if (eos_)
            {
                throw runtime_error ("Unexpected end of regex "
                    "(missing '}').");
            }
        } while (ch_ == '_' || ch_ == '-' || ch_ >= 'A' && ch_ <= 'Z' ||
            ch_ >= 'a' && ch_ <= 'z' || ch_ >= '0' && ch_ <= '9');

        if (ch_ != '}')
        {
            std::ostringstream ss_;

            ss_ << "Missing '}' at index " << state_._index - 1 << '.';
            throw runtime_error (ss_.str ().c_str ());
        }

        std::size_t len_ = state_._curr - 1 - start_;

        if (len_ > max_macro_len)
        {
            std::basic_stringstream<CharT> ss_;
            std::ostringstream os_;

            os_ << "MACRO name '";

            while (len_)
            {
                os_ << ss_.narrow (*start_++, ' ');
                --len_;
            }

            os_ << "' too long.";
            throw runtime_error (os_.str ());
        }

        token_.set (num_token::MACRO, null_token);
        ::memcpy (token_._macro, start_, len_ * sizeof(CharT));
        token_._macro[len_] = 0;
    }
};
}
}
}

#endif