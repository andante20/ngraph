//*****************************************************************************
// Copyright 2017-2019 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//*****************************************************************************

#pragma once

#include <functional>
#include <memory.h>

#include "ngraph/node.hpp"
#include "ngraph/op/constant.hpp"
#include "ngraph/pattern/op/any.hpp"
#include "ngraph/pattern/op/any_of.hpp"
#include "ngraph/pattern/op/label.hpp"
#include "ngraph/pattern/op/skip.hpp"
#include "ngraph/util.hpp"

namespace ngraph
{
    namespace pass
    {
        class GraphRewrite;
    }

    namespace pattern
    {
        using recurrent_graph_rewrite_callback = std::function<bool(class RecurrentMatcher& m)>;
        using RPatternMap = std::map<std::shared_ptr<op::Label>, NodeVector>;

        template <typename T>
        std::function<bool(std::shared_ptr<Node>)> has_class()
        {
            auto pred = [](std::shared_ptr<Node> node) -> bool {
                return std::dynamic_pointer_cast<T>(node) != nullptr;
            };

            return pred;
        }

        namespace op
        {
            class Label;
        }

        /// \brief Matcher matches (compares) two graphs
        ///
        class Matcher
        {
        public:
            using PatternMap = std::map<std::shared_ptr<op::Label>, std::shared_ptr<Node>>;

            /// \brief Constructs a Matcher object
            ///
            /// \param pattern_node is a pattern sub graph that will be matched against input graphs
            /// \param name is a string which is used for logging and disabling a matcher
            /// \param strict_mode forces a matcher to consider shapes and ET of nodes
            Matcher(const std::shared_ptr<Node> pattern_node = nullptr,
                    const std::string& name = "Unnamed",
                    pass::PassPropertyMask property = pass::PassProperty::REGULAR_FUSIONS,
                    bool strict_mode = false)
                : m_pattern_node(pattern_node)
                , m_depth(0)
                , m_name(name)
                , m_property(property)
                , m_strict_mode(strict_mode)
            {
            }

            virtual ~Matcher() {}
            /// \brief Matches a pattern to \p graph_node
            ///
            /// \param graph_node is an input graph to be matched against
            bool match(const std::shared_ptr<Node>& graph_node);

            /// \brief Matches a pattern to \p graph_node
            ///
            /// \param graph_node is an input graph to be matched against
            /// \param previous_matches contains previous mappings from labels to nodes to use
            bool match(const std::shared_ptr<Node>& graph_node, const PatternMap& previous_matches);

            template <typename T>
            static std::shared_ptr<T> unique_match(std::shared_ptr<Node> node)
            {
                std::shared_ptr<T> matched;
                for (auto arg : node->get_arguments())
                {
                    if (auto t_casted = std::dynamic_pointer_cast<T>(arg))
                    {
                        if (matched)
                        {
                            throw ngraph_error("There's more than two arguments of the same type");
                        }
                        else
                        {
                            matched = t_casted;
                        }
                    }
                }
                return matched;
            }

            bool get_property(const pass::PassPropertyMask& prop) const;
            bool is_contained_match(const NodeVector& exclusions = {}, bool ignore_unused = true);
            NodeVector get_matched_nodes() { return m_matched_list; }
            void reset() {}
            std::string get_name() { return m_name; }
            std::shared_ptr<Node> get_pattern() { return m_pattern_node; }
            std::shared_ptr<Node> get_match_root();
            PatternMap get_pattern_map() { return PatternMap{m_pattern_map}; }
            /// \brief Low-level helper to match recurring patterns
            ///
            /// \param graph is a graph to be matched against
            /// \param pattern is a recurring pattern
            /// \param rpattern specifies a node to recur from next
            /// \param patterns a map from labels to matches
            friend op::Label; // TODO: refine to match_class

        protected:
            void add_node(std::shared_ptr<Node> node) { m_matched_list.push_back(node); }
            bool abort_match(size_t watermark, bool matched)
            {
                if (!matched)
                {
                    m_matched_list.erase(m_matched_list.begin() + watermark, m_matched_list.end());
                }
                return matched;
            }

            bool virtual match_node(const std::shared_ptr<Node>& pattern_node,
                                    const std::shared_ptr<Node>& graph_node,
                                    PatternMap& pattern_map);

            virtual bool match_arguments(const std::shared_ptr<Node>& pattern_node,
                                         const std::shared_ptr<Node>& graph_node,
                                         PatternMap& pattern_map);

            std::shared_ptr<Node> m_match_root;
            std::shared_ptr<Node> m_pattern_node;
            PatternMap m_pattern_map;
            NodeVector m_matched_list;

        private:
            static std::string pad(size_t num) { return std::string(num, ' '); }
            bool match_permutation(const NodeVector& pattern_args,
                                   const NodeVector& args,
                                   PatternMap& pattern_map);
            bool match_pattern(const std::shared_ptr<op::Label>& pattern_node,
                               const std::shared_ptr<Node>& graph_node,
                               PatternMap& pattern_map);
            bool match_skip(const std::shared_ptr<op::Skip>& pattern_node,
                            const std::shared_ptr<Node>& graph_node,
                            PatternMap& pattern_map);
            bool match_any(const std::shared_ptr<op::Any>& pattern_node,
                           const std::shared_ptr<Node>& graph_node,
                           PatternMap& pattern_map);
            bool match_any_of(const std::shared_ptr<op::AnyOf>& pattern_node,
                              const std::shared_ptr<Node>& graph_node,
                              PatternMap& pattern_map);

            size_t m_depth;
            std::string m_name;
            pass::PassPropertyMask m_property;
            bool m_strict_mode;
        };

        class RecurrentMatcher
        {
        public:
            /// \brief Constructs a RecurrentMatcher object. Reccurent Matchers are used to match
            /// repeating patterns (e.g. RNN, LSTM, GRU cells)
            ///
            /// \param pattern is a pattern sub graph describing an individual cell
            /// \param rpattern is a (recurring) label to denote which node the next match should start at
            /// \param correlated_patterns is a set of labels whose bound nodes must remain the same across all cells
            // \param is a callback function that will be called on a successful match
            RecurrentMatcher(std::shared_ptr<Node> pattern,
                             std::shared_ptr<op::Label> rpattern,
                             const std::set<std::shared_ptr<op::Label>>& correlated_patterns,
                             recurrent_graph_rewrite_callback callback,
                             pass::PassPropertyMask property = pass::PassProperty::REGULAR_FUSIONS)
                : m_pattern(pattern)
                , m_recurrent_pattern(rpattern)
                , m_correlated_patterns(correlated_patterns)
                , m_callback(callback)
                , m_property(property)
            {
            }

            /// \brief Returns a vector of bound nodes for a given label (used in a pattern
            /// describing an individual cell
            NodeVector get_bound_nodes_for_pattern(std::shared_ptr<op::Label> pattern) const
            {
                if (m_matches.count(pattern) == 0)
                {
                    throw ngraph_error("No bound nodes for a given label");
                }

                return NodeVector{m_matches.at(pattern)};
            }

            size_t get_number_of_recurrent_matches() const
            {
                if (m_matches.size() == 0)
                {
                    return 0;
                }

                return (*m_matches.begin()).second.size();
            }

            size_t get_number_of_bound_labels() const { return m_matches.size(); }
            /// \brief Tries to match a pattern for an individual cell to a given \p graph
            bool match(std::shared_ptr<Node> graph);

            /// \brief Invoked by a pass to process a successful match
            bool process_match();

            bool get_property(const pass::PassPropertyMask& prop) const;

            std::shared_ptr<Node> get_match_root() { return m_match_root; }
        private:
            std::shared_ptr<Node> m_pattern;
            std::shared_ptr<op::Label> m_recurrent_pattern;
            const std::set<std::shared_ptr<op::Label>> m_correlated_patterns;
            RPatternMap m_matches;
            recurrent_graph_rewrite_callback m_callback;
            pass::PassPropertyMask m_property;
            std::shared_ptr<Node> m_match_root;
        };
    }
}
