/*=============================================================================
    Copyright (c) 2001-2007 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#if !defined(SPIRIT_OMIT_MARCH_24_2007_0802AM)
#define SPIRIT_OMIT_MARCH_24_2007_0802AM

#include <boost/spirit/home/support/component.hpp>
#include <boost/spirit/home/support/unused.hpp>
#include <boost/spirit/home/qi/skip.hpp>

namespace boost { namespace spirit { namespace qi
{
    struct omit_director
    {
        template <typename Component, typename Context, typename Iterator>
        struct attribute
        {
            typedef unused_type type;
        };

        template <
            typename Component
          , typename Iterator, typename Context
          , typename Skipper, typename Attribute>
        static bool parse(
            Component const& component
          , Iterator& first, Iterator const& last
          , Context& context, Skipper const& skipper
          , Attribute&)
        {
            typedef typename
                result_of::subject<Component>::type::director
            director;

            qi::skip(first, last, skipper);
            return director::parse(
                spirit::subject(component), first, last, context, skipper, unused);
        }

        template <typename Component>
        static std::string what(Component const& component)
        {
            std::string result = "omit[";

            typedef typename
                result_of::subject<Component>::type::director
            director;

            result += director::what(subject(component));
            result += "]";
            return result;
        }
    };
}}}

#endif